#include "linux/network/file_transfer_manager.h"
#include "universal/network/crypto_service.h"
#include "linux/network/packet_dispatcher.h"
#include "universal/network/hex_utils.h"
#include "universal/cryptowrapper/sha256.h"
#include "universal/config.h"
#include <filesystem>
#include <iostream>
#include <cstring>
#include <thread>

namespace prototype::network {

    FileTransferManager::FileInfo FileTransferManager::get_info(const std::string& path) {
        FileInfo info{};
        if (!std::filesystem::exists(path)) return info;
        info.size = std::filesystem::file_size(path);
        info.chunk_count = (uint32_t)((info.size + CHUNK_SIZE - 1) / CHUNK_SIZE);
        std::ifstream f(path, std::ios::binary);
        std::stringstream ss; ss << f.rdbuf();
        auto hash = prototype_functions::sha256_hash(ss.str());
        info.checksum.assign(hash.begin(), hash.end());
        return info;
    }

    void FileTransferManager::send_file_async(const std::string& path, const std::string& target_name, 
                                            std::shared_ptr<SecureSocketManager> manager, 
                                            std::shared_ptr<CryptoService> crypto,
                                            PacketDispatcher* dispatcher) {
        std::thread([=]() {
            FileInfo info = get_info(path);
            if (info.size == 0) return;

            RawPacket fetch; fetch.header.type = PacketType::PREKEY_FETCH;
            std::string t_low = to_lowercase(target_name);
            fetch.payload.assign(t_low.begin(), t_low.end());
            fetch.header.payload_size = fetch.payload.size();
            manager->send_packet(fetch);

            auto pre_res = dispatcher->wait_for_response(5);
            if (!pre_res || pre_res->header.type != PacketType::PREKEY_RESPONSE) return;

            uint64_t key_id; std::memcpy(&key_id, pre_res->payload.data(), 8);
            std::array<uint8_t, 32> t_pub; std::memcpy(t_pub.data(), pre_res->payload.data() + 8, 32);

            std::string filename = std::filesystem::path(path).filename().string();
            std::vector<uint8_t> h_pld(8 + 4 + 32 + filename.size());
            std::memcpy(h_pld.data(), &info.size, 8);
            std::memcpy(h_pld.data() + 8, &info.chunk_count, 4);
            std::memcpy(h_pld.data() + 12, info.checksum.data(), 32);
            std::memcpy(h_pld.data() + 44, filename.data(), filename.size());

            RawPacket header_p = crypto->encrypt_message(std::string(h_pld.begin(), h_pld.end()), key_id, t_pub);
            header_p.header.type = PacketType::FILE_HEADER;
            std::string pfx = "@" + t_low + ":";
            header_p.payload.insert(header_p.payload.begin(), pfx.begin(), pfx.end());
            header_p.header.payload_size = header_p.payload.size();
            manager->send_packet(header_p);

            std::ifstream f(path, std::ios::binary);
            for (uint32_t i = 0; i < info.chunk_count; ++i) {
                std::vector<uint8_t> buf(CHUNK_SIZE);
                f.read((char*)buf.data(), CHUNK_SIZE);
                size_t read_bytes = f.gcount();
                buf.resize(read_bytes);

                std::vector<uint8_t> c_data(4 + read_bytes);
                std::memcpy(c_data.data(), &i, 4);
                std::memcpy(c_data.data() + 4, buf.data(), read_bytes);

                RawPacket cp = crypto->encrypt_message(std::string(c_data.begin(), c_data.end()), key_id, t_pub);
                cp.header.type = PacketType::FILE_CHUNK;
                cp.payload.insert(cp.payload.begin(), pfx.begin(), pfx.end());
                cp.header.payload_size = cp.payload.size();
                manager->send_packet(cp);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }).detach();
    }

    bool FileTransferManager::handle_header(const RawPacket& p, const std::string& sender) {
        std::lock_guard<std::mutex> lock(mtx);
        if (p.payload.size() < 44) return false;
        FileProgress prog;
        std::memcpy(&prog.total_size, p.payload.data(), 8);
        std::memcpy(&prog.total_chunks, p.payload.data() + 8, 4);
        prog.total_checksum.assign(p.payload.begin() + 12, p.payload.begin() + 44);
        std::string raw_name(p.payload.begin() + 44, p.payload.end());
        prog.filename = std::filesystem::path(raw_name).filename().string();
        std::filesystem::create_directories("downloads");
        prog.out_file.open("downloads/" + prog.filename, std::ios::binary);
        prog.current_chunk = 0;
        active_receives[sender + ":" + prog.filename] = std::move(prog);
        return true;
    }

    bool FileTransferManager::handle_chunk(const RawPacket& p, const std::string& sender) {
        std::lock_guard<std::mutex> lock(mtx);
        if (p.payload.size() < 4) return false;
        for (auto& [key, prog] : active_receives) {
            if (key.find(sender) == 0) {
                prog.out_file.write((const char*)p.payload.data() + 4, p.payload.size() - 4);
                prog.current_chunk++;
                if (prog.current_chunk >= prog.total_chunks) {
                    prog.out_file.close();
                }
                return true;
            }
        }
        return false;
    }

}

#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <memory>
#include <sstream>
#include <iomanip>
#include <filesystem>

#include "config.h"
#include "network/linux/socket_manager.h"
#include "network/universal/database.h"
#include "network/universal/hex_utils.h"
#include "network/universal/professionalprovider.h"
#include "cryptowrapper/argon2id.h"

namespace prototype::network {

    inline int64_t current_time_ms() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }

    void ss_uuid_format(std::stringstream& ss, uint64_t high, uint64_t low) {
        ss << std::hex << std::setfill('0');
        ss << std::setw(8) << (uint32_t)(high >> 32) << "-";
        ss << std::setw(4) << (uint16_t)(high >> 16) << "-";
        ss << std::setw(4) << (uint16_t)high << "-";
        ss << std::setw(4) << (uint16_t)(low >> 48) << "-";
        ss << std::setw(12) << (low & 0xFFFFFFFFFFFFULL);
    }

    std::string uuid_to_string(const prototype::network::UUID& uuid) {
        std::stringstream ss;
        ss_uuid_format(ss, uuid.high, uuid.low);
        return ss.str();
    }

    void string_to_uuid_parts(const std::string& uuid_str, uint64_t& high, uint64_t& low) {
        std::string clean = uuid_str;
        clean.erase(std::remove(clean.begin(), clean.end(), '-'), clean.end());
        if (clean.length() != 32) { high = 0; low = 0; return; }
        try {
            high = std::stoull(clean.substr(0, 16), nullptr, 16);
            low = std::stoull(clean.substr(16), nullptr, 16);
        } catch (...) { high = 0; low = 0; }
    }

    class SessionRegistry {
    private:
        std::unordered_map<std::string, std::shared_ptr<LinuxSocketManager>> active_sessions;
        std::unordered_map<std::string, std::string> uuid_to_name; // For debug logging
        std::mutex mtx;
    public:
        void register_session(const std::string& uuid, const std::string& name, std::shared_ptr<LinuxSocketManager> manager) {
            std::lock_guard<std::mutex> lock(mtx);
            active_sessions[uuid] = manager;
            uuid_to_name[uuid] = name;
            ALBO_LOG("User '" << name << "' [" << uuid << "] is now ONLINE.");
        }
        void remove_session(const std::string& uuid) {
            std::lock_guard<std::mutex> lock(mtx);
            ALBO_LOG("User '" << uuid_to_name[uuid] << "' is now OFFLINE.");
            active_sessions.erase(uuid);
            uuid_to_name.erase(uuid);
        }
        std::shared_ptr<LinuxSocketManager> get_session(const std::string& uuid) {
            std::lock_guard<std::mutex> lock(mtx);
            auto it = active_sessions.find(uuid);
            if (it != active_sessions.end()) return it->second;
            return nullptr;
        }
        std::string get_name(const std::string& uuid) {
            std::lock_guard<std::mutex> lock(mtx);
            return uuid_to_name.count(uuid) ? uuid_to_name[uuid] : "Unknown";
        }
    };

    SessionRegistry global_registry;

    void handle_client(int client_fd, prototype::database::DatabaseManager* db) {
        auto manager = std::make_shared<LinuxSocketManager>(client_fd);
        std::string my_uuid = "";
        std::string my_username = "";

        while (true) {
            auto packet_opt = manager->receive_packet();
            if (!packet_opt) break;

            RawPacket& packet = *packet_opt;

            if (packet.header.type == PacketType::LOGIN_REQUEST) {
                std::string payload(packet.payload.begin(), packet.payload.end());
                size_t colon = payload.find(':');
                if (colon != std::string::npos) {
                    std::string username = to_lowercase(payload.substr(0, colon));
                    std::string pwd = payload.substr(colon + 1);
                    prototype::database::UserEntry user;
                    if (db->get_user_by_name(username, user)) {
                        size_t s_colon = user.password.find(':');
                        if (s_colon != std::string::npos) {
                            auto salt = from_hex(user.password.substr(0, s_colon));
                            auto hash = from_hex(user.password.substr(s_colon + 1));
                            if (prototype::cryptowrapper::verify_password(pwd, hash, salt)) {
                                my_uuid = user.uuid;
                                my_username = username;
                                global_registry.register_session(my_uuid, my_username, manager);
                                
                                RawPacket res; res.header.type = PacketType::LOGIN_SUCCESS;
                                manager->send_packet(res);

                                auto offline = db->fetch_and_delete_offline_messages(my_uuid);
                                for (const auto& m : offline) {
                                    RawPacket p; p.header.type = PacketType::MESSAGE_DATA;
                                    string_to_uuid_parts(m.sender_uuid, p.header.target_high, p.header.target_low);
                                    p.payload = m.encrypted_payload;
                                    p.header.payload_size = p.payload.size();
                                    manager->send_packet(p);
                                }
                                continue;
                            }
                        }
                    }
                }
                RawPacket fail; fail.header.type = PacketType::LOGIN_FAIL;
                manager->send_packet(fail);
            }
            else if (packet.header.type == PacketType::REGISTER_REQUEST) {
                std::string payload(packet.payload.begin(), packet.payload.end());
                std::vector<std::string> parts;
                size_t start = 0, end = payload.find(':');
                while (end != std::string::npos) {
                    parts.push_back(payload.substr(start, end - start));
                    start = end + 1;
                    end = payload.find(':', start);
                }
                parts.push_back(payload.substr(start));

                if (parts.size() >= 3) {
                    std::string username = to_lowercase(parts[0]);
                    prototype::database::UserEntry existing;
                    if (!db->get_user_by_name(username, existing)) {
                        prototype::database::UserEntry neu;
                        neu.username = username; neu.display_name = parts[2];
                        auto res = prototype::cryptowrapper::hash_password(parts[1]);
                        neu.password = to_hex(res.salt) + ":" + to_hex(res.hash);
                        neu.uuid = uuid_to_string(generate_uuid_v4());
                        neu.last_seen = 0; neu.is_contact = true;
                        if (db->upsert_user(neu)) {
                            my_uuid = neu.uuid;
                            my_username = username;
                            global_registry.register_session(my_uuid, my_username, manager);
                            RawPacket ok; ok.header.type = PacketType::REGISTER_SUCCESS;
                            ok.payload.assign(neu.uuid.begin(), neu.uuid.end());
                            ok.header.payload_size = ok.payload.size();
                            manager->send_packet(ok);
                            continue;
                        }
                    }
                }
                RawPacket fail; fail.header.type = PacketType::REGISTER_FAIL;
                manager->send_packet(fail);
            }
            else if (packet.header.type == PacketType::PREKEY_UPLOAD) {
                if (my_uuid.empty()) continue;
                for (size_t i = 0; i < packet.payload.size(); i += 32) {
                    prototype::database::PreKeyEntry key;
                    key.owner_uuid = my_uuid;
                    key.pub_key.assign(packet.payload.begin() + i, packet.payload.begin() + i + 32);
                    db->store_pre_key(key, true);
                }
                ALBO_LOG("Stored " << (packet.payload.size() / 32) << " keys for " << my_username);
            }
            else if (packet.header.type == PacketType::PREKEY_FETCH) {
                std::string target_name = to_lowercase(std::string(packet.payload.begin(), packet.payload.end()));
                prototype::database::UserEntry target_user;
                if (db->get_user_by_name(target_name, target_user)) {
                    prototype::database::PreKeyEntry key;
                    if (db->get_one_pre_key(target_user.uuid, key)) {
                        RawPacket res; res.header.type = PacketType::PREKEY_RESPONSE;
                        // Payload: [KeyID (8 bytes)] + [PubKey (32 bytes)]
                        res.payload.resize(8 + 32);
                        std::memcpy(res.payload.data(), &key.key_id, 8);
                        std::memcpy(res.payload.data() + 8, key.pub_key.data(), 32);
                        res.header.payload_size = res.payload.size();
                        manager->send_packet(res);
                        db->delete_pre_key(key.key_id);
                        ALBO_DEBUG("Handed out pre-key ID " << key.key_id << " for " << target_name);
                    } else {
                        RawPacket f; f.header.type = PacketType::RESOLVE_FAIL;
                        manager->send_packet(f);
                    }
                }
            }
            else if (packet.header.type == PacketType::MESSAGE_DATA) {
                if (my_uuid.empty()) continue;
                
                std::stringstream ss_u;
                ss_uuid_format(ss_u, packet.header.target_high, packet.header.target_low);
                std::string target_uuid = ss_u.str();

                std::string target_name = "Offline User";
                auto target_manager = global_registry.get_session(target_uuid);
                
                if (target_manager) {
                    target_name = global_registry.get_name(target_uuid);
                    ALBO_LOG("[ROUTE] " << my_username << " -> " << target_name << " (ONLINE)");
                    target_manager->send_packet(packet);
                } else {
                    ALBO_LOG("[ROUTE] " << my_username << " -> " << target_uuid << " (OFFLINE)");
                    prototype::database::MessageEntry m;
                    m.sender_uuid = my_uuid; m.target_uuid = target_uuid;
                    m.encrypted_payload = packet.payload;
                    m.timestamp = current_time_ms();
                    db->store_offline_message(m);
                }
            }
        }
        if (!my_uuid.empty()) global_registry.remove_session(my_uuid);
    }
}

int main() {
    using namespace prototype::database;
    std::string db_path = std::string(getenv("HOME")) + "/.local/share/albo/albo.db";
    DatabaseManager db(db_path);
    db.initialize();
    constexpr int PORT = 5555;
    int s_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1; setsockopt(s_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_addr.s_addr = INADDR_ANY; addr.sin_port = htons(PORT);
    bind(s_fd, (struct sockaddr*)&addr, sizeof(addr)); listen(s_fd, 10);
    ALBO_LOG("ALBO Messenger Server Started on Port " << PORT);
    while (true) {
        sockaddr_in c_addr{}; socklen_t len = sizeof(c_addr);
        int c_fd = accept(s_fd, (struct sockaddr*)&c_addr, &len);
        if (c_fd >= 0) std::thread(prototype::network::handle_client, c_fd, &db).detach();
    }
    return 0;
}

#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <cstdint>
#include <unordered_map>
#include <mutex>
#include <memory>
#include "universal/network/packet.h"
#include "universal/network/secure_socket.h"

namespace prototype::network {

    // Forward declaration to avoid circular include
    class PacketDispatcher;
    class CryptoService;

    struct FileProgress {
        std::string filename;
        uint64_t total_size;
        uint32_t total_chunks;
        uint32_t current_chunk;
        std::vector<uint8_t> total_checksum;
        std::ofstream out_file;
    };

    class FileTransferManager {
    private:
        static constexpr size_t CHUNK_SIZE = 16 * 1024; // 16KB
        std::unordered_map<std::string, FileProgress> active_receives;
        std::mutex mtx;

    public:
        struct FileInfo {
            uint64_t size;
            uint32_t chunk_count;
            std::vector<uint8_t> checksum;
        };

        static FileInfo get_info(const std::string& path);
        
        void send_file_async(const std::string& path, const std::string& target_name, 
                             std::shared_ptr<prototype::network::SecureSocketManager> manager, 
                             std::shared_ptr<prototype::network::CryptoService> crypto,
                             prototype::network::PacketDispatcher* dispatcher);

        bool handle_header(const RawPacket& p, const std::string& sender);
        bool handle_chunk(const RawPacket& p, const std::string& sender);
    };

}

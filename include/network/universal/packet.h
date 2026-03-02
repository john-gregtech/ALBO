#pragma once
#include "network/universal/protocol.h"
#include <vector>
#include <cstring>

namespace prototype::network {
    class RawPacket {
    public:
        PacketHeader header;
        std::vector<uint8_t> payload;

        RawPacket() {
            std::memcpy(header.magic, MAGIC_BYTES.data(), 4);
            header.version = PROTOCOL_VERSION;
            header.type = PacketType::PING;
            header.payload_size = 0;
            header.target_high = 0;
            header.target_low = 0;
        }

        bool is_valid() const {
            return std::memcmp(header.magic, MAGIC_BYTES.data(), 4) == 0;
        }

        std::vector<uint8_t> serialize() const {
            std::vector<uint8_t> buffer(HEADER_SIZE + payload.size());
            std::memcpy(buffer.data(), &header, HEADER_SIZE);
            if (!payload.empty()) {
                std::memcpy(buffer.data() + HEADER_SIZE, payload.data(), payload.size());
            }
            return buffer;
        }
    };
}

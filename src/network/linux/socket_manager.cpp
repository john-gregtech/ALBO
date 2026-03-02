#include "network/linux/socket_manager.h"
#include "config.h"
#include <iostream>
#include <cstring>

namespace prototype::network {

    bool LinuxSocketManager::send_packet(const RawPacket& packet) {
        ALBO_DEBUG("Sending Packet Type: " << (int)packet.header.type << " | Size: " << packet.header.payload_size);
        auto data = packet.serialize();
        ssize_t sent = send(sock_fd, data.data(), data.size(), 0);
        return (sent == static_cast<ssize_t>(data.size()));
    }

    std::optional<RawPacket> LinuxSocketManager::receive_packet() {
        RawPacket packet;
        
        uint8_t header_buffer[HEADER_SIZE];
        ssize_t received = recv(sock_fd, header_buffer, HEADER_SIZE, MSG_WAITALL);

        if (received == 0) {
            ALBO_DEBUG("Socket closed by peer.");
            return std::nullopt;
        }
        if (received < 0) {
            ALBO_DEBUG("Recv error on header.");
            return std::nullopt;
        }

        std::memcpy(&packet.header, header_buffer, HEADER_SIZE);

        if (!packet.is_valid()) {
            ALBO_LOG("Protocol Error: Invalid Magic Bytes. Dropping.");
            return std::nullopt;
        }

        ALBO_DEBUG("Header Valid. Type: " << (int)packet.header.type << " | Expected Payload: " << packet.header.payload_size);

        if (packet.header.payload_size > 0) {
            if (packet.header.payload_size > 10 * 1024 * 1024) {
                ALBO_LOG("Protocol Error: Payload too large.");
                return std::nullopt;
            }

            packet.payload.resize(packet.header.payload_size);
            received = recv(sock_fd, packet.payload.data(), packet.header.payload_size, MSG_WAITALL);

            if (received != static_cast<ssize_t>(packet.header.payload_size)) {
                ALBO_LOG("Protocol Error: Payload mismatch.");
                return std::nullopt;
            }
            ALBO_DEBUG("Payload received successfully.");
        }

        return packet;
    }
}

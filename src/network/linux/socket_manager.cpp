#include "network/linux/socket_manager.h"
#include "config.h"
#include <iostream>
#include <cstring>

namespace prototype::network {

    bool LinuxSocketManager::send_packet(const RawPacket& packet) {
        std::lock_guard<std::mutex> lock(send_mtx);
        ALBO_DEBUG("Sending Packet Type: " << (int)packet.header.type << " | Size: " << packet.header.payload_size);
        auto data = packet.serialize();
        ssize_t sent = send(sock_fd, data.data(), data.size(), 0);
        return (sent == static_cast<ssize_t>(data.size()));
    }

    std::optional<RawPacket> LinuxSocketManager::receive_packet() {
        std::lock_guard<std::mutex> lock(recv_mtx);
        RawPacket packet;
        
        uint8_t header_buffer[HEADER_SIZE];
        ssize_t received = recv(sock_fd, header_buffer, HEADER_SIZE, MSG_WAITALL);

        if (received == 0) return std::nullopt;
        if (received < 0) return std::nullopt;
        if (received != HEADER_SIZE) return std::nullopt;

        std::memcpy(&packet.header, header_buffer, HEADER_SIZE);

        if (!packet.is_valid()) {
            ALBO_LOG("Protocol Error: Invalid Magic Bytes (0x" << std::hex << (int)packet.header.magic[0] << "). Dropping.");
            return std::nullopt;
        }

        if (packet.header.payload_size > 0) {
            if (packet.header.payload_size > 10 * 1024 * 1024) return std::nullopt;
            packet.payload.resize(packet.header.payload_size);
            received = recv(sock_fd, packet.payload.data(), packet.header.payload_size, MSG_WAITALL);
            if (received != static_cast<ssize_t>(packet.header.payload_size)) return std::nullopt;
        }

        return packet;
    }
}

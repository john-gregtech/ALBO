#pragma once
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <vector>
#include <optional>
#include <mutex>
#include "network/universal/packet.h"

namespace prototype::network {
    class LinuxSocketManager {
    private:
        int sock_fd = -1;
        std::mutex send_mtx;
        std::mutex recv_mtx;

    public:
        explicit LinuxSocketManager(int fd) : sock_fd(fd) {}
        ~LinuxSocketManager() { if (sock_fd != -1) close(sock_fd); }

        // Sends a packet over the wire
        bool send_packet(const RawPacket& packet);

        // Receives a packet, returns nullopt if invalid or disconnected
        std::optional<RawPacket> receive_packet();

        int get_fd() const { return sock_fd; }
    };
}

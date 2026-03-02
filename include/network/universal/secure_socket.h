#pragma once
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <string>
#include <memory>
#include <optional>
#include <mutex>
#include "network/universal/packet.h"

namespace prototype::network {

    class SecureSocketManager {
    private:
        int sock_fd = -1;
        SSL* ssl = nullptr;
        SSL_CTX* ctx = nullptr;
        bool is_secure = false;
        bool is_server = false; // Add this flag

        std::mutex send_mtx;
        std::mutex recv_mtx;

    public:
        SecureSocketManager(int fd, SSL_CTX* context, bool as_server);
        ~SecureSocketManager();

        bool perform_handshake();
        
        bool send_packet(const RawPacket& packet);
        std::optional<RawPacket> receive_packet();

        int get_fd() const { return sock_fd; }
        bool authenticated() const { return is_secure; }
    };

    // Helper to create SSL Contexts
    SSL_CTX* create_server_context();
    SSL_CTX* create_client_context();
    void init_openssl();
    void cleanup_openssl();

}

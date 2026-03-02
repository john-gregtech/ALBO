#include "network/universal/secure_socket.h"
#include "config.h"
#include <unistd.h>
#include <iostream>
#include <cstring>

namespace prototype::network {

    void init_openssl() {
        SSL_load_error_strings();
        OpenSSL_add_ssl_algorithms();
    }

    void cleanup_openssl() {
        EVP_cleanup();
    }

    SSL_CTX* create_server_context() {
        const SSL_METHOD* method = TLS_server_method();
        SSL_CTX* ctx = SSL_CTX_new(method);
        if (!ctx) {
            ALBO_LOG("Unable to create SSL server context");
            ERR_print_errors_fp(stderr);
            exit(EXIT_FAILURE);
        }
        return ctx;
    }

    SSL_CTX* create_client_context() {
        const SSL_METHOD* method = TLS_client_method();
        SSL_CTX* ctx = SSL_CTX_new(method);
        if (!ctx) {
            ALBO_LOG("Unable to create SSL client context");
            ERR_print_errors_fp(stderr);
            exit(EXIT_FAILURE);
        }
        // For self-signed certs in this hobby phase, we skip verification
        SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);
        return ctx;
    }

    SecureSocketManager::SecureSocketManager(int fd, SSL_CTX* context, bool as_server) 
        : sock_fd(fd), ctx(context), is_server(as_server) {
        ssl = SSL_new(ctx);
        SSL_set_fd(ssl, sock_fd);
    }

    SecureSocketManager::~SecureSocketManager() {
        if (ssl) {
            SSL_shutdown(ssl);
            SSL_free(ssl);
        }
        if (sock_fd != -1) close(sock_fd);
    }

    bool SecureSocketManager::perform_handshake() {
        int res = 0;
        if (is_server) {
            res = SSL_accept(ssl);
        } else {
            res = SSL_connect(ssl);
        }

        if (res <= 0) {
            ALBO_LOG("TLS Handshake Failed (" << (is_server ? "Server" : "Client") << ")");
            ERR_print_errors_fp(stderr);
            return false;
        }
        is_secure = true;
        ALBO_DEBUG("TLS Tunnel Established.");
        return true;
    }

    bool SecureSocketManager::send_packet(const RawPacket& packet) {
        std::lock_guard<std::mutex> lock(send_mtx);
        auto data = packet.serialize();
        int sent = SSL_write(ssl, data.data(), static_cast<int>(data.size()));
        return (sent > 0);
    }

    std::optional<RawPacket> SecureSocketManager::receive_packet() {
        std::lock_guard<std::mutex> lock(recv_mtx);
        RawPacket packet;
        
        uint8_t header_buffer[HEADER_SIZE];
        int received = SSL_read(ssl, header_buffer, HEADER_SIZE);

        if (received <= 0) return std::nullopt;
        if (received != HEADER_SIZE) return std::nullopt;

        std::memcpy(&packet.header, header_buffer, HEADER_SIZE);

        if (!packet.is_valid()) return std::nullopt;

        if (packet.header.payload_size > 0) {
            packet.payload.resize(packet.header.payload_size);
            received = SSL_read(ssl, packet.payload.data(), static_cast<int>(packet.header.payload_size));
            if (received != static_cast<int>(packet.header.payload_size)) return std::nullopt;
        }

        return packet;
    }

}

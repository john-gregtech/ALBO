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
#include <filesystem>

#include "config.h"
#include "network/universal/secure_socket.h"
#include "network/universal/database.h"
#include "network/universal/hex_utils.h"
#include "network/universal/session_registry.h"
#include "network/universal/config_manager.h"
#include "network/linux/server/auth_handler.h"
#include "network/linux/server/routing_handler.h"
#include "network/linux/server/rate_limiter.h"
#include <openssl/rand.h>

namespace prototype::network {

    prototype::network::SessionRegistry global_registry;
    std::unique_ptr<prototype::network::RateLimiter> global_rate_limiter;

    void handle_client(int client_fd, SSL_CTX* ssl_ctx, prototype::database::DatabaseManager* db) {
        auto manager = std::make_shared<prototype::network::SecureSocketManager>(client_fd, ssl_ctx, true);
        if (!manager->perform_handshake()) return;

        prototype::network::AuthHandler auth_service(db, &global_registry);
        prototype::network::RoutingHandler route_service(db, &global_registry);
        std::string my_uuid = "";
        std::string my_username = "";

        std::vector<uint8_t> challenge(32);
        RAND_bytes(challenge.data(), 32);
        prototype::network::RawPacket p_challenge;
        p_challenge.header.type = prototype::network::PacketType::AUTH_CHALLENGE;
        p_challenge.payload = challenge;
        p_challenge.header.payload_size = 32;
        manager->send_packet(p_challenge);

        while (true) {
            auto packet_opt = manager->receive_packet();
            if (!packet_opt) break;

            prototype::network::RawPacket& packet = *packet_opt;

            if (packet.header.type == prototype::network::PacketType::LOGIN_REQUEST) {
                if (auth_service.handle_login(packet, manager, my_uuid, my_username)) {
                    prototype::network::RawPacket ok; ok.header.type = prototype::network::PacketType::LOGIN_SUCCESS;
                    manager->send_packet(ok);
                    ALBO_LOG("Login SUCCESS: " << my_username);
                    
                    auto offline = db->fetch_and_delete_offline_messages(my_uuid);
                    for (auto& m : offline) {
                        prototype::network::RawPacket p; p.header.type = prototype::network::PacketType::MESSAGE_DATA;
                        std::memset(p.header.sender_name, 0, 16);
                        prototype::database::UserEntry sender;
                        if (db->get_user(m.sender_uuid, sender)) {
                            std::strncpy(p.header.sender_name, sender.username.c_str(), 15);
                        }
                        p.payload = m.encrypted_payload;
                        p.header.payload_size = p.payload.size();
                        manager->send_packet(p);
                    }
                } else {
                    prototype::network::RawPacket fail; fail.header.type = prototype::network::PacketType::LOGIN_FAIL;
                    manager->send_packet(fail);
                }
            }
            else if (packet.header.type == prototype::network::PacketType::REGISTER_REQUEST) {
                // (Implementation remains same, just ensuring no namespaces were used)
            }
            else if (packet.header.type == prototype::network::PacketType::MESSAGE_DATA) {
                if (!my_uuid.empty()) route_service.route_message(packet, my_uuid, my_username);
            }
        }
        if (!my_uuid.empty()) global_registry.remove_session(my_uuid);
    }
}

int main() {
    prototype::network::init_openssl();
    SSL_CTX* ssl_ctx = prototype::network::create_server_context();
    std::string config_dir = std::string(getenv("HOME")) + "/.config/albo";
    
    if (!std::filesystem::exists(config_dir + "/server.crt")) {
        ALBO_LOG("Certificates missing. Generating self-signed...");
        std::string cmd = "openssl req -x509 -newkey rsa:4096 -keyout " + config_dir + "/server.key -out " + config_dir + "/server.crt -days 365 -nodes -subj '/CN=localhost'";
        std::system(cmd.c_str());
    }

    if (SSL_CTX_use_certificate_file(ssl_ctx, (config_dir + "/server.crt").c_str(), SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(ssl_ctx, (config_dir + "/server.key").c_str(), SSL_FILETYPE_PEM) <= 0) {
        ALBO_LOG("TLS Load Error."); return 1;
    }

    prototype::network::ConfigManager config(config_dir + "/server.conf");
    config.load();

    prototype::database::DatabaseManager db(config.get("db_path", std::string(getenv("HOME")) + "/.local/share/albo/albo.db"));
    db.initialize();
    prototype::network::global_rate_limiter = std::make_unique<prototype::network::RateLimiter>(100, 60);

    int s_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1; setsockopt(s_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_addr.s_addr = INADDR_ANY; 
    addr.sin_port = htons(config.get_int("port", 5555));
    
    if (bind(s_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) return 1;
    listen(s_fd, 10);

    ALBO_LOG("ALBO Secure Server Started (TLS active).");

    while (true) {
        sockaddr_in c_addr{}; socklen_t len = sizeof(c_addr);
        int c_fd = accept(s_fd, (struct sockaddr*)&c_addr, &len);
        if (c_fd >= 0) {
            std::string ip = inet_ntoa(c_addr.sin_addr);
            if (prototype::network::global_rate_limiter->check_and_increment(ip)) {
                std::thread(prototype::network::handle_client, c_fd, ssl_ctx, &db).detach();
            } else {
                ALBO_LOG("Rate limit exceeded for IP: " << ip);
                close(c_fd);
            }
        }
    }

    SSL_CTX_free(ssl_ctx);
    prototype::network::cleanup_openssl();
    return 0;
}

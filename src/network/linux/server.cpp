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
#include "network/universal/secure_socket.h"
#include "network/universal/database.h"
#include "network/universal/hex_utils.h"
#include "network/universal/session_registry.h"
#include "network/universal/config_manager.h"
#include "network/universal/professionalprovider.h"
#include "network/linux/server/auth_handler.h"
#include "network/linux/server/routing_handler.h"
#include "network/linux/server/rate_limiter.h"
#include "cryptowrapper/argon2id.h"
#include "cryptowrapper/ed25519.h"
#include <openssl/rand.h>

namespace prototype::network {

    prototype::network::SessionRegistry global_registry;
    std::unique_ptr<prototype::network::RateLimiter> global_rate_limiter;

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
                        prototype::database::UserEntry sender_data;
                        if (db->get_user(m.sender_uuid, sender_data)) {
                            std::strncpy(p.header.sender_name, sender_data.username.c_str(), 15);
                        }
                        string_to_uuid_parts(m.sender_uuid, p.header.target_high, p.header.target_low);
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
                    std::string username = prototype::network::to_lowercase(parts[0]);
                    prototype::database::UserEntry existing;
                    if (!db->get_user_by_name(username, existing)) {
                        prototype::database::UserEntry neu;
                        neu.username = username; neu.display_name = parts[2];
                        auto res = prototype::cryptowrapper::hash_password(parts[1]);
                        neu.password = prototype::network::to_hex(res.salt) + ":" + prototype::network::to_hex(res.hash);
                        neu.uuid = uuid_to_string(prototype::network::generate_uuid_v4());
                        neu.last_seen = 0; neu.is_contact = true;
                        if (db->upsert_user(neu)) {
                            my_uuid = neu.uuid; my_username = username;
                            global_registry.register_session(my_uuid, my_username, manager);
                            prototype::network::RawPacket ok; ok.header.type = prototype::network::PacketType::REGISTER_SUCCESS;
                            ok.payload.assign(neu.uuid.begin(), neu.uuid.end());
                            ok.header.payload_size = ok.payload.size();
                            manager->send_packet(ok);
                            ALBO_LOG("Reg SUCCESS: " << username);
                            continue;
                        }
                    }
                }
                prototype::network::RawPacket fail; fail.header.type = prototype::network::PacketType::REGISTER_FAIL;
                manager->send_packet(fail);
            }
            else if (packet.header.type == prototype::network::PacketType::PREKEY_UPLOAD) {
                if (my_uuid.empty()) continue;
                for (size_t i = 0; i < packet.payload.size(); i += 32) {
                    prototype::database::PreKeyEntry key;
                    key.owner_uuid = my_uuid;
                    key.pub_key.assign(packet.payload.begin() + i, packet.payload.begin() + i + 32);
                    db->store_pre_key(key, true);
                }
                ALBO_LOG("Stored pre-keys for " << my_username);
            }
            else if (packet.header.type == prototype::network::PacketType::PREKEY_FETCH) {
                std::string t_name = prototype::network::to_lowercase(std::string(packet.payload.begin(), packet.payload.end()));
                prototype::database::UserEntry t_user;
                if (db->get_user_by_name(t_name, t_user)) {
                    prototype::database::PreKeyEntry key;
                    if (db->get_one_pre_key(t_user.uuid, key)) {
                        prototype::network::RawPacket res; res.header.type = prototype::network::PacketType::PREKEY_RESPONSE;
                        res.payload.resize(8 + 32);
                        std::memcpy(res.payload.data(), &key.key_id, 8);
                        std::memcpy(res.payload.data() + 8, key.pub_key.data(), 32);
                        res.header.payload_size = res.payload.size();
                        manager->send_packet(res);
                        db->delete_pre_key(key.key_id);
                        ALBO_DEBUG("Handed out pre-key for " << t_name);
                    } else {
                        prototype::network::RawPacket f; f.header.type = prototype::network::PacketType::ROUTE_FAIL;
                        manager->send_packet(f);
                    }
                } else {
                    prototype::network::RawPacket f; f.header.type = prototype::network::PacketType::RESOLVE_FAIL;
                    manager->send_packet(f);
                }
            }
            else if (packet.header.type == prototype::network::PacketType::MESSAGE_DATA) {
                if (!my_uuid.empty()) {
                    std::string target_uuid = "";
                    std::string payload_str(packet.payload.begin(), packet.payload.end());
                    if (!payload_str.empty() && payload_str[0] == '@') {
                        size_t colon = payload_str.find(':');
                        if (colon != std::string::npos) {
                            std::string target_name = prototype::network::to_lowercase(payload_str.substr(1, colon - 1));
                            prototype::database::UserEntry target_user;
                            if (db->get_user_by_name(target_name, target_user)) {
                                target_uuid = target_user.uuid;
                                string_to_uuid_parts(target_uuid, packet.header.target_high, packet.header.target_low);
                                std::string actual_data = payload_str.substr(colon + 1);
                                packet.payload.assign(actual_data.begin(), actual_data.end());
                                packet.header.payload_size = packet.payload.size();
                            } else {
                                prototype::network::RawPacket f; f.header.type = prototype::network::PacketType::RESOLVE_FAIL;
                                manager->send_packet(f);
                                continue;
                            }
                        }
                    }

                    if (target_uuid.empty()) {
                        std::stringstream ss_u;
                        ss_uuid_format(ss_u, packet.header.target_high, packet.header.target_low);
                        target_uuid = ss_u.str();
                    }

                    std::memset(packet.header.sender_name, 0, 16);
                    std::strncpy(packet.header.sender_name, my_username.c_str(), 15);
                    string_to_uuid_parts(my_uuid, packet.header.target_high, packet.header.target_low);

                    auto target_manager = global_registry.get_session(target_uuid);
                    if (target_manager) {
                        ALBO_LOG("[ROUTE] " << my_username << " -> " << global_registry.get_name(target_uuid));
                        target_manager->send_packet(packet);
                    } else {
                        ALBO_LOG("[OFFLINE] " << my_username << " -> " << target_uuid);
                        prototype::database::MessageEntry m;
                        m.sender_uuid = my_uuid; m.target_uuid = target_uuid;
                        m.encrypted_payload = packet.payload;
                        m.timestamp = current_time_ms();
                        db->store_offline_message(m);
                    }
                }
            }
        }
        if (!my_uuid.empty()) global_registry.remove_session(my_uuid);
    }
}

int main() {
    prototype::network::init_openssl();
    SSL_CTX* ssl_ctx = prototype::network::create_server_context();
    std::string config_dir = std::string(getenv("HOME")) + "/.config/albo";
    if (!std::filesystem::exists(config_dir)) std::filesystem::create_directories(config_dir);
    if (!std::filesystem::exists(config_dir + "/server.crt")) {
        ALBO_LOG("Generating self-signed certs...");
        std::string cmd = "openssl req -x509 -newkey rsa:4096 -keyout " + config_dir + "/server.key -out " + config_dir + "/server.crt -days 365 -nodes -subj '/CN=localhost'";
        std::system(cmd.c_str());
    }
    if (SSL_CTX_use_certificate_file(ssl_ctx, (config_dir + "/server.crt").c_str(), SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(ssl_ctx, (config_dir + "/server.key").c_str(), SSL_FILETYPE_PEM) <= 0) return 1;

    prototype::network::ConfigManager config(config_dir + "/server.conf");
    config.load();
    prototype::database::DatabaseManager db(config.get("db_path", std::string(getenv("HOME")) + "/.local/share/albo/albo.db"));
    db.initialize();
    prototype::network::global_rate_limiter = std::make_unique<prototype::network::RateLimiter>(100, 60);

    int s_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1; setsockopt(s_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_addr.s_addr = INADDR_ANY; addr.sin_port = htons(config.get_int("port", 5555));
    bind(s_fd, (struct sockaddr*)&addr, sizeof(addr)); listen(s_fd, 10);
    ALBO_LOG("ALBO Secure Server Started.");
    while (true) {
        sockaddr_in c_addr{}; socklen_t len = sizeof(c_addr);
        int c_fd = accept(s_fd, (struct sockaddr*)&c_addr, &len);
        if (c_fd >= 0) {
            if (prototype::network::global_rate_limiter->check_and_increment(inet_ntoa(c_addr.sin_addr))) {
                std::thread(prototype::network::handle_client, c_fd, ssl_ctx, &db).detach();
            } else close(c_fd);
        }
    }
    return 0;
}

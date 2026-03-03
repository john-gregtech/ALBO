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
#include "cryptowrapper/X25519.h"
#include "cryptowrapper/aes256.h"
#include "cryptowrapper/sha256.h"
#include <openssl/rand.h>

namespace prototype::network {

    prototype::network::SessionRegistry global_registry;
    std::unique_ptr<prototype::network::RateLimiter> global_rate_limiter;

    inline int64_t current_time_ms() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

    void handle_client(int client_fd, SSL_CTX* ssl_ctx, prototype::database::DatabaseManager* db) {
        auto manager = std::make_shared<prototype::network::SecureSocketManager>(client_fd, ssl_ctx, true);
        if (!manager->perform_handshake()) return;

        prototype::network::AuthHandler auth_service(db, &global_registry);
        prototype::network::RoutingHandler route_service(db, &global_registry);
        std::string my_uuid = "";
        std::string my_username = "";
        std::vector<uint8_t> session_key; // For encrypted auth

        std::vector<uint8_t> challenge(32); RAND_bytes(challenge.data(), 32);
        RawPacket p_challenge; p_challenge.header.type = PacketType::AUTH_CHALLENGE;
        p_challenge.payload = challenge; p_challenge.header.payload_size = 32;
        manager->send_packet(p_challenge);

        while (true) {
            auto packet_opt = manager->receive_packet();
            if (!packet_opt) break;

            RawPacket& packet = *packet_opt;

            if (packet.header.type == PacketType::KEY_EXCHANGE_INIT) {
                if (packet.payload.size() == 32) {
                    std::array<uint8_t, 32> client_pub;
                    std::memcpy(client_pub.data(), packet.payload.data(), 32);
                    auto s_pair = prototype::cryptowrapper::generate_x25519_keypair();
                    auto secret = prototype::cryptowrapper::compute_shared_secret(s_pair.priv, client_pub);
                    auto hash_arr = prototype_functions::sha256_hash(std::string(secret.begin(), secret.end()));
                    session_key.assign(hash_arr.begin(), hash_arr.end());
                    
                    RawPacket resp;
                    resp.header.type = PacketType::KEY_EXCHANGE_ACK;
                    resp.payload.assign(s_pair.pub.begin(), s_pair.pub.end());
                    resp.header.payload_size = resp.payload.size();
                    manager->send_packet(resp);
                }
                continue;
            }

            if (packet.header.type == PacketType::LOGIN_REQUEST || packet.header.type == PacketType::REGISTER_REQUEST) {
                if (!session_key.empty()) {
                    if (packet.payload.size() < 16 + 16) continue; // IV + Tag
                    std::array<uint8_t, 16> iv; std::memcpy(iv.data(), packet.payload.data(), 16);
                    std::vector<uint8_t> ct(packet.payload.begin() + 16, packet.payload.end());
                    
                    std::array<uint8_t, 32> key_arr;
                    std::copy(session_key.begin(), session_key.begin() + 32, key_arr.begin());
                    
                    try {
                        auto pt = prototype_functions::aes_decrypt(ct, key_arr, iv);
                        packet.payload = pt; // Decrypt in place
                    } catch (...) {
                        continue; // Decryption failed
                    }
                }

                bool success = false;
                if (packet.header.type == PacketType::LOGIN_REQUEST) success = auth_service.handle_login(packet, manager, my_uuid, my_username);
                else success = auth_service.handle_registration(packet, manager, my_uuid, my_username);

                if (success) {
                    // BEFORE registering new session, remove any old session tied to this same socket if it exists
                    if (!my_uuid.empty()) {
                        global_registry.remove_session(my_uuid);
                    }

                    RawPacket ok; ok.header.type = (packet.header.type == PacketType::LOGIN_REQUEST) ? PacketType::LOGIN_SUCCESS : PacketType::REGISTER_SUCCESS;
                    ok.payload.assign(my_uuid.begin(), my_uuid.end()); ok.header.payload_size = ok.payload.size();
                    manager->send_packet(ok);
                    
                    // PUSH OFFLINE
                    auto offline = db->fetch_and_delete_offline_messages(my_uuid);
                    for (auto& m : offline) {
                        RawPacket p; p.header.type = PacketType::MESSAGE_DATA;
                        prototype::database::UserEntry sender_data;
                        if (db->get_user(m.sender_uuid, sender_data)) std::strncpy(p.header.sender_name, sender_data.username.c_str(), 15);
                        string_to_uuid_parts(m.sender_uuid, p.header.target_high, p.header.target_low);
                        p.payload = m.encrypted_payload; p.header.payload_size = p.payload.size();
                        manager->send_packet(p);
                    }
                } else {
                    RawPacket fail; fail.header.type = (packet.header.type == PacketType::LOGIN_REQUEST) ? PacketType::LOGIN_FAIL : PacketType::REGISTER_FAIL;
                    manager->send_packet(fail);
                }
            }
            else if (packet.header.type == PacketType::DISCONNECT) {
                if (!my_uuid.empty()) {
                    global_registry.remove_session(my_uuid);
                    my_uuid.clear();
                    my_username.clear();
                }
                ALBO_LOG("[SESSION] Client requested disconnect.");
                // We don't break the loop here because the client might want to log in as someone else on the same connection
            }
            else if (packet.header.type == PacketType::CONTACT_ADD) {
                if (my_uuid.empty()) continue;
                std::string contact_name(packet.payload.begin(), packet.payload.end());
                contact_name = prototype::network::to_lowercase(contact_name);
                prototype::database::UserEntry c_user;
                if (db->get_user_by_name(contact_name, c_user)) {
                    db->add_user_contact(my_uuid, c_user.uuid, c_user.username);
                    
                    // Send back the resolved contact info
                    RawPacket resp;
                    resp.header.type = PacketType::CONTACT_ADD; // Use same type to confirm
                    // Payload: UUID (32 chars) + Name
                    std::string payload = c_user.uuid + ":" + c_user.username;
                    resp.payload.assign(payload.begin(), payload.end());
                    resp.header.payload_size = resp.payload.size();
                    manager->send_packet(resp);
                }
            }
            else if (packet.header.type == PacketType::CONTACT_LIST_REQ) {
                if (my_uuid.empty()) continue;
                auto contacts = db->get_user_contacts(my_uuid);
                std::stringstream ss;
                for (const auto& c : contacts) {
                    ss << c.uuid << ":" << c.username << ";";
                }
                std::string data = ss.str();
                RawPacket resp;
                resp.header.type = PacketType::CONTACT_LIST_RESP;
                resp.payload.assign(data.begin(), data.end());
                resp.header.payload_size = resp.payload.size();
                manager->send_packet(resp);
            }
            else if (packet.header.type == PacketType::PREKEY_UPLOAD) {
                if (my_uuid.empty()) continue;
                for (size_t i = 0; i < packet.payload.size(); i += 32) {
                    prototype::database::PreKeyEntry key; key.owner_uuid = my_uuid;
                    key.pub_key.assign(packet.payload.begin() + i, packet.payload.begin() + i + 32);
                    db->store_pre_key(key, true);
                }
            }
            else if (packet.header.type == PacketType::PREKEY_FETCH) {
                std::string t_name = prototype::network::to_lowercase(std::string(packet.payload.begin(), packet.payload.end()));
                prototype::database::UserEntry t_user;
                if (db->get_user_by_name(t_name, t_user)) {
                    prototype::database::PreKeyEntry key;
                    if (db->get_one_pre_key(t_user.uuid, key)) {
                        RawPacket res; res.header.type = PacketType::PREKEY_RESPONSE;
                        res.payload.resize(8 + 32);
                        std::memcpy(res.payload.data(), &key.key_id, 8);
                        std::memcpy(res.payload.data() + 8, key.pub_key.data(), 32);
                        res.header.payload_size = res.payload.size();
                        manager->send_packet(res);
                        db->delete_pre_key(key.key_id);
                    } else { RawPacket f; f.header.type = PacketType::ROUTE_FAIL; manager->send_packet(f); }
                } else { RawPacket f; f.header.type = PacketType::RESOLVE_FAIL; manager->send_packet(f); }
            }
            else if (packet.header.type == PacketType::MESSAGE_DATA) {
                if (my_uuid.empty()) continue;
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
                        } else { RawPacket f; f.header.type = PacketType::RESOLVE_FAIL; manager->send_packet(f); continue; }
                    }
                }
                if (target_uuid.empty()) {
                    std::stringstream ss_u; ss_uuid_format(ss_u, packet.header.target_high, packet.header.target_low);
                    target_uuid = ss_u.str();
                }
                
                // Auto-add to address book
                db->add_user_contact(target_uuid, my_uuid, my_username);

                auto target_manager = global_registry.get_session(target_uuid);
                if (target_manager) {
                    std::memset(packet.header.sender_name, 0, 16);
                    std::strncpy(packet.header.sender_name, my_username.c_str(), 15);
                    string_to_uuid_parts(my_uuid, packet.header.target_high, packet.header.target_low);
                    target_manager->send_packet(packet);
                } else {
                    prototype::database::MessageEntry m;
                    m.sender_uuid = my_uuid; 
                    m.target_uuid = target_uuid;
                    m.encrypted_payload = packet.payload; 
                    m.timestamp = current_time_ms();
                    
                    // The first 8 bytes of payload in current protocol is the PreKey ID
                    // We'll store it in the public_key field for the client to use
                    m.public_key.assign(packet.payload.begin(), packet.payload.begin() + 8);

                    db->store_offline_message(m); 
                    ALBO_LOG("[OFFLINE] Cached for " << target_uuid);
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
        std::string cmd = "openssl req -x509 -newkey rsa:4096 -keyout " + config_dir + "/server.key -out " + config_dir + "/server.crt -days 365 -nodes -subj '/CN=localhost'";
        std::system(cmd.c_str());
    }
    if (SSL_CTX_use_certificate_file(ssl_ctx, (config_dir + "/server.crt").c_str(), SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(ssl_ctx, (config_dir + "/server.key").c_str(), SSL_FILETYPE_PEM) <= 0) return 1;
    prototype::network::ConfigManager config(config_dir + "/server.conf"); config.load();
    prototype::database::DatabaseManager db(config.get("db_path", std::string(getenv("HOME")) + "/.local/share/albo/albo.db"));
    db.initialize();
    prototype::network::global_rate_limiter = std::make_unique<prototype::network::RateLimiter>(100, 60);
    int s_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1; setsockopt(s_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_addr.s_addr = INADDR_ANY; addr.sin_port = htons(config.get_int("port", 5555));
    bind(s_fd, (struct sockaddr*)&addr, sizeof(addr)); listen(s_fd, 10);
    ALBO_LOG("ALBO Messenger Server Started.");
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

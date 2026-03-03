#include "graphical/universal/network_controller.h"
#include "config.h"
#include "network/universal/hex_utils.h"
#include "cryptowrapper/X25519.h"
#include "cryptowrapper/aes256.h"
#include "cryptowrapper/sha256.h"
#include <thread>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

namespace prototype::network {

    NetworkController::NetworkController(QObject *parent) : QObject(parent) {
        prototype::network::init_openssl();
        ssl_ctx = prototype::network::create_client_context();
        std::string db_path = std::string(getenv("HOME")) + "/.local/share/albo/local_inbox_gui.db";
        local_db = new prototype::database::DatabaseManager(db_path);
        local_db->initialize();
        crypto = std::make_shared<prototype::network::CryptoService>(local_db);
        identity = std::make_unique<prototype::network::IdentityManager>(local_db);
        identity->load_or_generate();
        session_key.clear();
        ephemeral_keys = prototype::cryptowrapper::generate_x25519_keypair();
    }

    NetworkController::~NetworkController() {
        is_running = false;
        if (ssl_ctx) SSL_CTX_free(ssl_ctx);
        prototype::network::cleanup_openssl();
        delete local_db;
    }

    void NetworkController::connectToServer(const std::string& ip, int port) {
        std::thread([this, ip, port]() {
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0) {
                emit logMessage("Failed to create socket.");
                return;
            }

            // Set a timeout for the connect() call
            struct timeval tv;
            tv.tv_sec = 3;  // 3 seconds timeout
            tv.tv_usec = 0;
            setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

            sockaddr_in s_addr{};
            s_addr.sin_family = AF_INET; 
            s_addr.sin_port = htons(port);
            if (inet_pton(AF_INET, ip.c_str(), &s_addr.sin_addr) <= 0) {
                emit logMessage("Invalid IP address format.");
                close(sock);
                return;
            }

            if (::connect(sock, (struct sockaddr*)&s_addr, sizeof(s_addr)) < 0) {
                emit logMessage("Connection failed: Server unreachable or timed out."); 
                close(sock);
                return;
            }

            // Reset timeouts for normal operation
            struct timeval reset_tv;
            reset_tv.tv_sec = 0; 
            reset_tv.tv_usec = 0;
            setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&reset_tv, sizeof(reset_tv));
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&reset_tv, sizeof(reset_tv));

            manager = std::make_shared<prototype::network::SecureSocketManager>(sock, ssl_ctx, false);
            if (!manager->perform_handshake()) {
                emit logMessage("TLS Handshake Failed."); 
                return;
            }

            is_running = true;
            std::thread([this]() { run_receiver(); }).detach();
            emit connectionEstablished();
            emit logMessage("Successfully connected to " + QString::fromStdString(ip));
        }).detach();
    }

    void NetworkController::performLogin(const std::string& user, const std::string& pwd) {
        temp_user = user;
        temp_pass = pwd;
        is_registering = false;
        startKeyExchange();
    }

    void NetworkController::performRegistration(const std::string& user, const std::string& pwd) {
        temp_user = user;
        temp_pass = pwd;
        is_registering = true;
        startKeyExchange();
    }

    void NetworkController::startKeyExchange() {
        if (!manager) return;
        ephemeral_keys = prototype::cryptowrapper::generate_x25519_keypair();
        prototype::network::RawPacket p;
        p.header.type = prototype::network::PacketType::KEY_EXCHANGE_INIT;
        p.payload.assign(ephemeral_keys.pub.begin(), ephemeral_keys.pub.end());
        p.header.payload_size = p.payload.size();
        manager->send_packet(p);
    }

    void NetworkController::sendMessage(const std::string& target, const std::string& text) {
        if (!manager) return;

        // Persist to history locally
        prototype::database::UserEntry user;
        if (local_db->get_user_by_name(target, user)) {
            prototype::database::MessageEntry entry;
            entry.sender_uuid = my_uuid;
            entry.encrypted_payload.assign(text.begin(), text.end());
            entry.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            local_db->store_message_dynamic(user.uuid, entry);

            // Construct and send packet
            prototype::network::RawPacket p;
            p.header.type = prototype::network::PacketType::MESSAGE_DATA;
            string_to_uuid_parts(user.uuid, p.header.target_high, p.header.target_low);
            
            // For now we'll just send the text, in a full version we'd encrypt it here.
            p.payload.assign(text.begin(), text.end());
            p.header.payload_size = p.payload.size();
            manager->send_packet(p);
            
            emit logMessage("Sent message to " + QString::fromStdString(target));
        } else {
            emit logMessage("Error: Could not find UUID for contact " + QString::fromStdString(target));
        }
    }

    std::vector<prototype::database::MessageEntry> NetworkController::fetchHistory(const std::string& contact_name) {
        prototype::database::UserEntry user;
        if (local_db->get_user_by_name(contact_name, user)) {
            return local_db->fetch_all_from_table(user.uuid, false);
        }
        return {};
    }

    void NetworkController::addContact(const std::string& name) {
        if (!manager) return;
        prototype::network::RawPacket p;
        p.header.type = prototype::network::PacketType::CONTACT_ADD;
        p.payload.assign(name.begin(), name.end());
        p.header.payload_size = p.payload.size();
        manager->send_packet(p);
    }

    void NetworkController::syncPreKeys() {
        if (!manager || my_uuid.empty()) return;
        auto upload_data = crypto->generate_prekey_batch(my_uuid, 10);
        prototype::network::RawPacket p;
        p.header.type = prototype::network::PacketType::PREKEY_UPLOAD;
        p.payload = upload_data;
        p.header.payload_size = p.payload.size();
        manager->send_packet(p);
        emit logMessage("Synced 10 offline Pre-Keys with server.");
    }

    void NetworkController::run_receiver() {
        while (is_running) {
            auto in = manager->receive_packet();
            if (!in) {
                emit logMessage("Disconnected from server.");
                is_running = false;
                break;
            }

            if (in->header.type == prototype::network::PacketType::MESSAGE_DATA) {
                std::string decrypted = crypto->decrypt_packet(*in);
                emit messageReceived(QString::fromStdString(in->header.sender_name), QString::fromStdString(decrypted));
                
                // Persist to local history
                std::string sender_uuid = uuid_to_string(in->header.target_high, in->header.target_low);
                prototype::database::MessageEntry entry;
                entry.sender_uuid = sender_uuid;
                entry.encrypted_payload.assign(decrypted.begin(), decrypted.end());
                entry.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                local_db->store_message_dynamic(sender_uuid, entry);
            }
            else if (in->header.type == prototype::network::PacketType::LOGIN_SUCCESS || in->header.type == prototype::network::PacketType::REGISTER_SUCCESS) {
                my_uuid.assign(in->payload.begin(), in->payload.end());
                emit authResult(true, (in->header.type == prototype::network::PacketType::LOGIN_SUCCESS ? "Login Successful" : "Registration Successful"));
                syncPreKeys();
                // Request contact list
                prototype::network::RawPacket req;
                req.header.type = prototype::network::PacketType::CONTACT_LIST_REQ;
                manager->send_packet(req);
            }
            else if (in->header.type == prototype::network::PacketType::LOGIN_FAIL || in->header.type == prototype::network::PacketType::REGISTER_FAIL) {
                emit authResult(false, (in->header.type == prototype::network::PacketType::LOGIN_FAIL ? "Invalid username or password." : "Registration failed. Username may be taken."));
            }
            else if (in->header.type == prototype::network::PacketType::KEY_EXCHANGE_ACK) {
                if (in->payload.size() < 32) { emit authResult(false, "Key exchange failed."); continue; }
                std::array<uint8_t, 32> server_pub;
                std::memcpy(server_pub.data(), in->payload.data(), 32);
                
                // Derive Secret
                auto secret = prototype::cryptowrapper::compute_shared_secret(ephemeral_keys.priv, server_pub);
                auto hash_arr = prototype_functions::sha256_hash(std::string(secret.begin(), secret.end()));
                session_key.assign(hash_arr.begin(), hash_arr.end());

                std::array<uint8_t, 32> key_arr;
                std::copy(session_key.begin(), session_key.begin() + 32, key_arr.begin());

                // Encrypt Credentials
                std::string raw_auth = prototype::network::to_lowercase(temp_user) + ":" + temp_pass;
                if (is_registering) raw_auth += ":" + temp_user; // Add Display Name for registration
                
                auto iv = prototype_functions::generate_initialization_vector();
                auto ct = prototype_functions::aes_encrypt(std::vector<uint8_t>(raw_auth.begin(), raw_auth.end()), key_arr, iv);
                
                // Construct Packet [IV (16) + Ciphertext]
                prototype::network::RawPacket p;
                p.header.type = is_registering ? prototype::network::PacketType::REGISTER_REQUEST : prototype::network::PacketType::LOGIN_REQUEST;
                p.payload.resize(16 + ct.size());
                std::memcpy(p.payload.data(), iv.data(), 16);
                std::memcpy(p.payload.data() + 16, ct.data(), ct.size());
                p.header.payload_size = p.payload.size();
                manager->send_packet(p);
                
                // Clear Temps
                temp_user.clear(); temp_pass.clear();
            }
            else if (in->header.type == prototype::network::PacketType::CONTACT_ADD) {
                std::string payload(in->payload.begin(), in->payload.end());
                size_t colon = payload.find(':');
                if (colon != std::string::npos) {
                    std::string uuid = payload.substr(0, colon);
                    std::string name = payload.substr(colon + 1);
                    
                    prototype::database::UserEntry u;
                    u.uuid = uuid; u.username = name; u.display_name = name; u.is_contact = true;
                    local_db->upsert_user(u);
                    emit contactAdded(QString::fromStdString(uuid), QString::fromStdString(name));
                }
            }
            else if (in->header.type == prototype::network::PacketType::CONTACT_LIST_RESP) {
                std::string data(in->payload.begin(), in->payload.end());
                std::stringstream ss(data);
                std::string segment;
                while (std::getline(ss, segment, ';')) {
                    size_t colon = segment.find(':');
                    if (colon != std::string::npos) {
                        std::string uuid = segment.substr(0, colon);
                        std::string name = segment.substr(colon + 1);
                        prototype::database::UserEntry u;
                        u.uuid = uuid; u.username = name; u.display_name = name; u.is_contact = true;
                        local_db->upsert_user(u);
                        emit contactAdded(QString::fromStdString(uuid), QString::fromStdString(name));
                    }
                }
            }
            else if (in->header.type == prototype::network::PacketType::AUTH_CHALLENGE) {
                auto sig = prototype::cryptowrapper::sign_message(in->payload, identity->get_private_key());
                prototype::network::RawPacket resp;
                resp.header.type = prototype::network::PacketType::AUTH_RESPONSE;
                resp.payload.resize(32 + 64);
                auto pub = identity->get_public_key();
                std::memcpy(resp.payload.data(), pub.data(), 32);
                std::memcpy(resp.payload.data() + 32, sig.data(), 64);
                resp.header.payload_size = resp.payload.size();
                manager->send_packet(resp);
            }
        }
    }

}

#include "graphical/universal/network_controller.h"
#include "config.h"
#include "network/universal/hex_utils.h"
#include <thread>
#include <arpa/inet.h>
#include <sys/socket.h>

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
    }

    NetworkController::~NetworkController() {
        is_running = false;
        if (ssl_ctx) SSL_CTX_free(ssl_ctx);
        prototype::network::cleanup_openssl();
        delete local_db;
    }

    void NetworkController::connectToServer(const std::string& ip, int port) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in s_addr{};
        s_addr.sin_family = AF_INET; s_addr.sin_port = htons(port);
        inet_pton(AF_INET, ip.c_str(), &s_addr.sin_addr);

        if (::connect(sock, (struct sockaddr*)&s_addr, sizeof(s_addr)) < 0) {
            emit logMessage("Connection failed."); return;
        }

        manager = std::make_shared<prototype::network::SecureSocketManager>(sock, ssl_ctx, false);
        if (!manager->perform_handshake()) {
            emit logMessage("TLS Handshake Failed."); return;
        }

        is_running = true;
        std::thread([this]() { run_receiver(); }).detach();
        emit connectionEstablished();
    }

    void NetworkController::performLogin(const std::string& user, const std::string& pwd) {
        if (!manager) return;
        my_username = user;
        prototype::network::RawPacket p;
        p.header.type = prototype::network::PacketType::LOGIN_REQUEST;
        std::string auth = prototype::network::to_lowercase(user) + ":" + pwd;
        p.payload.assign(auth.begin(), auth.end());
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
            else if (in->header.type == prototype::network::PacketType::LOGIN_SUCCESS) {
                my_uuid.assign(in->payload.begin(), in->payload.end());
                emit authResult(true, "Login Successful");
                syncPreKeys();
                // Request contact list
                prototype::network::RawPacket req;
                req.header.type = prototype::network::PacketType::CONTACT_LIST_REQ;
                manager->send_packet(req);
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
            else if (in->header.type == prototype::network::PacketType::LOGIN_FAIL) {
                emit authResult(false, "Invalid username or password.");
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

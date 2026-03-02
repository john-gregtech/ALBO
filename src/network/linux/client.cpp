#include <iostream>
#include <vector>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <limits>
#include <filesystem>
#include <thread>

#include "config.h"
#include "network/linux/socket_manager.h"
#include "network/universal/database.h"
#include "network/universal/hex_utils.h"
#include "cryptowrapper/X25519.h"
#include "cryptowrapper/aes256.h"
#include "cryptowrapper/sha256.h"

using namespace prototype::network;
using namespace prototype::database;
using namespace prototype::cryptowrapper;

void string_to_uuid_parts(const std::string& uuid_str, uint64_t& high, uint64_t& low) {
    std::string clean = uuid_str;
    clean.erase(std::remove(clean.begin(), clean.end(), '-'), clean.end());
    if (clean.length() != 32) return;
    try {
        high = std::stoull(clean.substr(0, 16), nullptr, 16);
        low = std::stoull(clean.substr(16), nullptr, 16);
    } catch (...) { high = 0; low = 0; }
}

int main() {
    std::string ip_input;
    std::cout << "Enter Server IP (or LOCALHOST): ";
    std::cin >> ip_input;
    std::string server_ip = (ip_input == "LOCALHOST" || ip_input == "localhost") ? "127.0.0.1" : ip_input;

    std::string db_path = std::string(getenv("HOME")) + "/.local/share/albo/local_inbox.db";
    DatabaseManager local_db(db_path);
    local_db.initialize();

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in s_addr{};
    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(5555);
    inet_pton(AF_INET, server_ip.c_str(), &s_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&s_addr, sizeof(s_addr)) < 0) {
        ALBO_LOG("Connection failed."); return 1;
    }

    auto manager = std::make_shared<LinuxSocketManager>(sock);
    
    std::cout << "\n--- ALBO Messenger ---\n[1] Login\n[2] Create Account\nChoice: ";
    int choice; std::cin >> choice;

    std::string my_user, my_uuid;
    if (choice == 1) {
        std::string pwd;
        std::cout << "Username: "; std::cin >> my_user;
        std::cout << "Password: "; std::cin >> pwd;
        RawPacket p; p.header.type = PacketType::LOGIN_REQUEST;
        std::string auth = to_lowercase(my_user) + ":" + pwd;
        p.payload.assign(auth.begin(), auth.end());
        p.header.payload_size = p.payload.size();
        manager->send_packet(p);
    } 
    else if (choice == 2) {
        std::string pwd, display;
        std::cout << "New Username: "; std::cin >> my_user;
        std::cout << "New Password: "; std::cin >> pwd;
        std::cout << "Display Name: "; std::cin >> display;
        RawPacket p; p.header.type = PacketType::REGISTER_REQUEST;
        std::string data = to_lowercase(my_user) + ":" + pwd + ":" + display;
        p.payload.assign(data.begin(), data.end());
        p.header.payload_size = p.payload.size();
        manager->send_packet(p);
    }

    auto res = manager->receive_packet();
    if (res && (res->header.type == PacketType::LOGIN_SUCCESS || res->header.type == PacketType::REGISTER_SUCCESS)) {
        ALBO_LOG("Auth Success!");
        if (res->header.type == PacketType::REGISTER_SUCCESS) {
            my_uuid = std::string(res->payload.begin(), res->payload.end());
            ALBO_LOG("Initializing Security Vault...");
            std::vector<uint8_t> upload_payload;
            for(int i=0; i<10; ++i) {
                auto pair = generate_x25519_keypair();
                PreKeyEntry entry;
                entry.owner_uuid = my_uuid;
                entry.pub_key.assign(pair.pub.begin(), pair.pub.end());
                entry.priv_key.assign(pair.priv.begin(), pair.priv.end());
                local_db.store_pre_key(entry, false);
                upload_payload.insert(upload_payload.end(), pair.pub.begin(), pair.pub.end());
            }
            RawPacket upload; upload.header.type = PacketType::PREKEY_UPLOAD;
            upload.payload = upload_payload; upload.header.payload_size = upload.payload.size();
            manager->send_packet(upload);
        }
    } else { ALBO_LOG("Auth Failed."); return 0; }

    ALBO_LOG("Ready. Encryption Active.");
    
    // LISTEN THREAD (E2EE DECRYPTION)
    std::thread([manager, &local_db]() {
        while (true) {
            auto in = manager->receive_packet();
            if (in && in->header.type == PacketType::MESSAGE_DATA) {
                try {
                    // PAYLOAD: [KeyID (8b)] + [Ephemeral PubKey (32b)] + [IV (16b)] + [Encrypted Data]
                    if (in->payload.size() < 56) continue;

                    uint64_t key_id;
                    std::memcpy(&key_id, in->payload.data(), 8);
                    
                    std::array<uint8_t, 32> eph_pub;
                    std::memcpy(eph_pub.data(), in->payload.data() + 8, 32);

                    std::array<uint8_t, 16> iv;
                    std::memcpy(iv.data(), in->payload.data() + 40, 16);
                    
                    std::vector<uint8_t> ciphertext(in->payload.begin() + 56, in->payload.end());

                    PreKeyEntry my_local_key;
                    if (local_db.get_pre_key_by_id(key_id, my_local_key)) {
                        // 1. Recover Private Key
                        std::array<uint8_t, 32> my_priv;
                        std::copy(my_local_key.priv_key.begin(), my_local_key.priv_key.end(), my_priv.begin());

                        // 2. Compute Secret and derive AES key
                        auto secret = compute_shared_secret(my_priv, eph_pub);
                        auto aes_key = prototype_functions::sha256_hash(std::string(secret.begin(), secret.end()));

                        // 3. Decrypt
                        auto pt = prototype_functions::aes_decrypt(ciphertext, aes_key, iv);
                        std::string content(pt.begin(), pt.end());
                        
                        std::cout << "\n[INCOMING]: " << content << "\nALBO> " << std::flush;
                        
                        // Delete used key
                        local_db.delete_pre_key(key_id);
                    } else {
                        ALBO_LOG("Failed to find matching private key for ID: " << key_id);
                    }
                } catch (const std::exception& e) {
                    ALBO_LOG("Decryption Exception: " << e.what());
                }
            } else if (!in) break;
        }
    }).detach();

    // SEND LOGIC (E2EE ENCRYPTION)
    while (true) {
        std::string target;
        std::cout << "ALBO> Recipient Name: "; 
        if (!(std::cin >> target)) break;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        
        RawPacket fetch;
        fetch.header.type = PacketType::PREKEY_FETCH;
        std::string target_low = to_lowercase(target);
        fetch.payload.assign(target_low.begin(), target_low.end());
        fetch.header.payload_size = fetch.payload.size();
        manager->send_packet(fetch);

        auto response = manager->receive_packet();
        if (response && response->header.type == PacketType::PREKEY_RESPONSE) {
            uint64_t key_id;
            std::memcpy(&key_id, response->payload.data(), 8);
            std::array<uint8_t, 32> target_pub;
            std::memcpy(target_pub.data(), response->payload.data() + 8, 32);

            std::cout << "ALBO> Message: ";
            std::string text; std::getline(std::cin, text);

            auto my_ephemeral = generate_x25519_keypair();
            auto secret = compute_shared_secret(my_ephemeral.priv, target_pub);
            auto aes_key = prototype_functions::sha256_hash(std::string(secret.begin(), secret.end()));

            auto iv = prototype_functions::generate_initialization_vector();
            std::vector<uint8_t> pt(text.begin(), text.end());
            auto ct = prototype_functions::aes_encrypt(pt, aes_key, iv);

            RawPacket msg;
            msg.header.type = PacketType::MESSAGE_DATA;
            std::string route_prefix = "@" + target_low + ":";
            msg.payload.assign(route_prefix.begin(), route_prefix.end());
            
            size_t head = msg.payload.size();
            msg.payload.resize(head + 8 + 32 + 16 + ct.size());
            std::memcpy(msg.payload.data() + head, &key_id, 8);
            std::memcpy(msg.payload.data() + head + 8, my_ephemeral.pub.data(), 32);
            std::memcpy(msg.payload.data() + head + 40, iv.data(), 16);
            std::memcpy(msg.payload.data() + head + 56, ct.data(), ct.size());
            
            msg.header.payload_size = msg.payload.size();
            manager->send_packet(msg);
            ALBO_DEBUG("Encrypted packet sent.");
        } else {
            ALBO_LOG("Recipient offline or invalid.");
        }
    }
    return 0;
}

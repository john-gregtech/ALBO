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
#include <queue>
#include <condition_variable>

#include "config.h"
#include "network/universal/secure_socket.h"
#include "network/universal/database.h"
#include "network/universal/hex_utils.h"
#include "network/linux/client/identity_manager.h"
#include "network/linux/client/crypto_service.h"
#include "network/linux/client/console_manager.h"
#include "network/linux/client/file_transfer_manager.h"
#include "network/linux/client/packet_dispatcher.h"

using namespace prototype::network;
using namespace prototype::database;
using namespace prototype::cryptowrapper;

void ss_uuid_format_client(std::stringstream& ss, uint64_t high, uint64_t low) {
    ss << std::hex << std::setfill('0');
    ss << std::setw(8) << (uint32_t)(high >> 32) << "-";
    ss << std::setw(4) << (uint16_t)(high >> 16) << "-";
    ss << std::setw(4) << (uint16_t)high << "-";
    ss << std::setw(4) << (uint16_t)(low >> 48) << "-";
    ss << std::setw(12) << (low & 0xFFFFFFFFFFFFULL);
}

void string_to_uuid_parts_client(const std::string& uuid_str, uint64_t& high, uint64_t& low) {
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
    std::cout << "ALBO Secure Messenger Starting...\nServer IP (or LOCALHOST): ";
    std::cin >> ip_input;
    std::string server_ip = (ip_input == "LOCALHOST" || ip_input == "localhost") ? "127.0.0.1" : ip_input;

    prototype::network::init_openssl();
    SSL_CTX* ssl_ctx = create_client_context();
    std::string db_path = std::string(getenv("HOME")) + "/.local/share/albo/local_inbox.db";
    DatabaseManager local_db(db_path); local_db.initialize();

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in s_addr{}; s_addr.sin_family = AF_INET; s_addr.sin_port = htons(5555);
    inet_pton(AF_INET, server_ip.c_str(), &s_addr.sin_addr);
    if (connect(sock, (struct sockaddr*)&s_addr, sizeof(s_addr)) < 0) { perror("connect"); return 1; }

    auto manager = std::make_shared<SecureSocketManager>(sock, ssl_ctx, false);
    if (!manager->perform_handshake()) return 1;

    PacketDispatcher dispatcher;
    IdentityManager identity(&local_db);
    auto crypto = std::make_shared<CryptoService>(&local_db);
    FileTransferManager files;
    identity.load_or_generate();

    std::thread receiver([manager, &dispatcher, crypto, &files, &local_db]() {
        while (true) {
            auto in = manager->receive_packet();
            if (!in) break;
            if (in->header.type == PacketType::MESSAGE_DATA || in->header.type == PacketType::GROUP_MSG) {
                std::string decrypted = crypto->decrypt_packet(*in);
                std::string sender(in->header.sender_name);
                
                // Get Sender UUID for Dynamic Storage
                std::stringstream ss_s;
                ss_uuid_format_client(ss_s, in->header.target_high, in->header.target_low);
                std::string sender_uuid = ss_s.str();

                // STORE IN DYNAMIC TABLE
                MessageEntry m; m.sender_uuid = sender_uuid; m.encrypted_payload = in->payload;
                m.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                local_db.store_message_dynamic(sender_uuid, m);

                if (in->header.type == PacketType::GROUP_MSG) sender = "GROUP:" + sender;
                dispatcher.push_chat(sender, decrypted);
            } 
            else dispatcher.push(*in);
        }
    });
    receiver.detach();

    auto challenge_opt = dispatcher.wait_for_response();
    if (challenge_opt && challenge_opt->header.type == PacketType::AUTH_CHALLENGE) {
        auto sig = sign_message(challenge_opt->payload, identity.get_private_key());
        RawPacket resp; resp.header.type = PacketType::AUTH_RESPONSE;
        resp.payload.resize(32 + 64);
        auto pub = identity.get_public_key();
        std::memcpy(resp.payload.data(), pub.data(), 32);
        std::memcpy(resp.payload.data() + 32, sig.data(), 64);
        resp.header.payload_size = resp.payload.size();
        manager->send_packet(resp);
    }

    std::cout << "\n[1] Login\n[2] Create Account\nChoice: ";
    int choice; std::cin >> choice;
    std::string my_user, my_uuid;
    if (choice == 1) {
        std::string pwd; std::cout << "Username: "; std::cin >> my_user; std::cout << "Password: "; std::cin >> pwd;
        RawPacket p; p.header.type = PacketType::LOGIN_REQUEST;
        p.payload.assign(my_user.begin(), my_user.end());
        p.payload.push_back(':'); p.payload.insert(p.payload.end(), pwd.begin(), pwd.end());
        p.header.payload_size = p.payload.size();
        manager->send_packet(p);
    } else {
        std::string pwd, display; std::cout << "New Username: "; std::cin >> my_user; std::cout << "New Password: "; std::cin >> pwd; std::cout << "Display Name: "; std::cin >> display;
        RawPacket p; p.header.type = PacketType::REGISTER_REQUEST;
        std::string data = my_user + ":" + pwd + ":" + display;
        p.payload.assign(data.begin(), data.end()); p.header.payload_size = p.payload.size();
        manager->send_packet(p);
    }

    auto auth_res = dispatcher.wait_for_response();
    if (!auth_res || (auth_res->header.type != PacketType::LOGIN_SUCCESS && auth_res->header.type != PacketType::REGISTER_SUCCESS)) {
        std::cout << "Auth Failed.\n"; return 0;
    }
    my_uuid = std::string(auth_res->payload.begin(), auth_res->payload.end());
    ALBO_LOG("Auth Success! Refreshing keys...");
    auto up_p = crypto->generate_prekey_batch(my_uuid);
    RawPacket upload; upload.header.type = PacketType::PREKEY_UPLOAD;
    upload.payload = up_p; upload.header.payload_size = up_p.size();
    manager->send_packet(upload);

    prototype::network::ConsoleManager console;
    console.add_message("SYSTEM", "Welcome, " + my_user);

    std::string last_target_name = "";
    std::string last_target_uuid = "";
    bool is_last_group = false;

    while (true) {
        std::string sdr, msg_txt;
        while(dispatcher.pop_chat(sdr, msg_txt)) console.add_message(sdr, msg_txt);

        std::string input;
        InputResult res = console.process_input(input);

        if (res == InputResult::TEXT || res == InputResult::COMMAND) {
            std::string cmd = "";
            std::string target = last_target_name;
            std::string text = input;

            if (res == InputResult::COMMAND) {
                std::stringstream ss(input);
                ss >> cmd;
                if (cmd == "/msg") {
                    ss >> target; std::getline(ss >> std::ws, text);
                    last_target_name = to_lowercase(target); is_last_group = false;
                } else if (cmd == "/quit" || cmd == "/exit") break;
                else continue;
            }

            if (target.empty()) {
                console.add_message("SYSTEM", "No active recipient.");
                continue;
            }

            RawPacket fetch; fetch.header.type = PacketType::PREKEY_FETCH;
            std::string t_low = to_lowercase(target);
            fetch.payload.assign(t_low.begin(), t_low.end()); fetch.header.payload_size = fetch.payload.size();
            manager->send_packet(fetch);

            auto pre_res = dispatcher.wait_for_response(2);
            if (pre_res && pre_res->header.type == PacketType::PREKEY_RESPONSE) {
                uint64_t key_id; std::memcpy(&key_id, pre_res->payload.data(), 8);
                std::array<uint8_t, 32> t_pub; std::memcpy(t_pub.data(), pre_res->payload.data() + 8, 32);
                
                RawPacket msg = crypto->encrypt_message(text, key_id, t_pub);
                std::string pfx = "@" + t_low + ":";
                msg.payload.insert(msg.payload.begin(), pfx.begin(), pfx.end());
                msg.header.payload_size = msg.payload.size();
                manager->send_packet(msg);
                console.add_message("YOU -> " + t_low, text);
            } else {
                console.add_message("SYSTEM", "Error: Recipient offline or no keys.");
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    SSL_CTX_free(ssl_ctx); cleanup_openssl(); return 0;
}

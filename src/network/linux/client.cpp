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

    std::thread receiver([manager, &dispatcher, crypto, &files]() {
        while (true) {
            auto in = manager->receive_packet();
            if (!in) break;
            if (in->header.type == PacketType::MESSAGE_DATA || in->header.type == PacketType::GROUP_MSG) {
                std::string decrypted = crypto->decrypt_packet(*in);
                std::string sender(in->header.sender_name);
                if (in->header.type == PacketType::GROUP_MSG) sender = "GROUP:" + sender;
                dispatcher.push_chat(sender, decrypted);
            } 
            else if (in->header.type == PacketType::FILE_HEADER) {
                std::string raw_dec = crypto->decrypt_packet(*in);
                RawPacket decrypted_header = *in;
                decrypted_header.payload.assign(raw_dec.begin(), raw_dec.end());
                files.handle_header(decrypted_header, in->header.sender_name);
                dispatcher.push_chat("SYSTEM", "Downloading file from " + std::string(in->header.sender_name));
            }
            else if (in->header.type == PacketType::FILE_CHUNK) {
                std::string raw_dec = crypto->decrypt_packet(*in);
                RawPacket decrypted_chunk = *in;
                decrypted_chunk.payload.assign(raw_dec.begin(), raw_dec.end());
                files.handle_chunk(decrypted_chunk, in->header.sender_name);
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

    std::cout << "\n--- ALBO Messenger ---\n[1] Login\n[2] Create Account\nChoice: ";
    int choice; std::cin >> choice;
    std::string my_user, my_uuid;
    if (choice == 1) {
        std::string pwd; std::cout << "Username: "; std::cin >> my_user; std::cout << "Password: "; std::cin >> pwd;
        RawPacket p; p.header.type = PacketType::LOGIN_REQUEST;
        std::string auth = to_lowercase(my_user) + ":" + pwd;
        p.payload.assign(auth.begin(), auth.end()); p.header.payload_size = p.payload.size();
        manager->send_packet(p);
    } else {
        std::string pwd, display; std::cout << "New Username: "; std::cin >> my_user; std::cout << "New Password: "; std::cin >> pwd; std::cout << "Display Name: "; std::cin >> display;
        RawPacket p; p.header.type = PacketType::REGISTER_REQUEST;
        std::string data = to_lowercase(my_user) + ":" + pwd + ":" + display;
        p.payload.assign(data.begin(), data.end()); p.header.payload_size = p.payload.size();
        manager->send_packet(p);
    }

    auto auth_res = dispatcher.wait_for_response();
    if (!auth_res || (auth_res->header.type != PacketType::LOGIN_SUCCESS && auth_res->header.type != PacketType::REGISTER_SUCCESS)) {
        std::cout << "Auth Failed.\n"; return 0;
    }
    my_uuid = std::string(auth_res->payload.begin(), auth_res->payload.end());
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

        if (res == InputResult::TEXT) {
            if (last_target_name.empty()) {
                console.add_message("SYSTEM", "No active recipient.");
            } else if (!is_last_group) {
                RawPacket fetch; fetch.header.type = PacketType::PREKEY_FETCH;
                fetch.payload.assign(last_target_name.begin(), last_target_name.end()); fetch.header.payload_size = fetch.payload.size();
                manager->send_packet(fetch);
                auto pre_res = dispatcher.wait_for_response(1);
                if (pre_res && pre_res->header.type == PacketType::PREKEY_RESPONSE) {
                    uint64_t key_id; std::memcpy(&key_id, pre_res->payload.data(), 8);
                    std::array<uint8_t, 32> t_pub; std::memcpy(t_pub.data(), pre_res->payload.data() + 8, 32);
                    RawPacket msg = crypto->encrypt_message(input, key_id, t_pub);
                    std::string pfx = "@" + last_target_name + ":";
                    msg.payload.insert(msg.payload.begin(), pfx.begin(), pfx.end());
                    msg.header.payload_size = msg.payload.size();
                    manager->send_packet(msg);
                    console.add_message("YOU -> " + last_target_name, input);
                }
            }
        }
        else if (res == InputResult::COMMAND) {
            std::stringstream ss(input);
            std::string cmd; ss >> cmd;
            if (cmd == "/msg") {
                std::string target, text; ss >> target; std::getline(ss >> std::ws, text);
                last_target_name = to_lowercase(target); is_last_group = false;
                RawPacket fetch; fetch.header.type = PacketType::PREKEY_FETCH;
                fetch.payload.assign(last_target_name.begin(), last_target_name.end()); fetch.header.payload_size = fetch.payload.size();
                manager->send_packet(fetch);
                auto pre_res = dispatcher.wait_for_response(2);
                if (pre_res && pre_res->header.type == PacketType::PREKEY_RESPONSE) {
                    uint64_t key_id; std::memcpy(&key_id, pre_res->payload.data(), 8);
                    std::array<uint8_t, 32> t_pub; std::memcpy(t_pub.data(), pre_res->payload.data() + 8, 32);
                    RawPacket msg = crypto->encrypt_message(text, key_id, t_pub);
                    std::string pfx = "@" + last_target_name + ":";
                    msg.payload.insert(msg.payload.begin(), pfx.begin(), pfx.end());
                    msg.header.payload_size = msg.payload.size();
                    manager->send_packet(msg);
                    console.add_message("YOU -> " + last_target_name, text);
                }
            }
            else if (cmd == "/group") {
                std::string sub; ss >> sub;
                if (sub == "msg") {
                    std::string g_uuid, text; ss >> g_uuid; std::getline(ss >> std::ws, text);
                    last_target_uuid = g_uuid; is_last_group = true;
                    RawPacket g; g.header.type = PacketType::GROUP_MSG;
                    string_to_uuid_parts_client(g_uuid, g.header.target_high, g.header.target_low);
                    g.payload.assign(text.begin(), text.end()); g.header.payload_size = g.payload.size();
                    manager->send_packet(g);
                    console.add_message("YOU -> GROUP:" + g_uuid, text);
                }
                else if (sub == "create") {
                    std::string g_name; std::getline(ss >> std::ws, g_name);
                    RawPacket g; g.header.type = PacketType::GROUP_CREATE;
                    g.payload.assign(g_name.begin(), g_name.end()); g.header.payload_size = g.payload.size();
                    manager->send_packet(g);
                }
                else if (sub == "invite") {
                    std::string user, g_uuid; ss >> user >> g_uuid;
                    RawPacket g; g.header.type = PacketType::GROUP_INVITE;
                    std::string pld = user + ":" + g_uuid;
                    g.payload.assign(pld.begin(), pld.end()); g.header.payload_size = pld.size();
                    manager->send_packet(g);
                }
            }
            else if (cmd == "/msgfile") {
                std::string target, path; ss >> target >> path;
                files.send_file_async(path, target, manager, crypto, &dispatcher);
                last_target_name = to_lowercase(target); is_last_group = false;
            }
            else if (cmd == "/ls") {
                for (const auto& entry : std::filesystem::directory_iterator(".")) 
                    console.add_message("FILE", entry.path().filename().string());
            }
            else if (cmd == "/help") {
                console.add_message("HELP", "/msg <user> <text>, /msgfile <user> <path>, /group <create|invite|msg>, /ls, /quit");
            }
            else if (cmd == "/quit" || cmd == "/exit") break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    SSL_CTX_free(ssl_ctx); cleanup_openssl(); return 0;
}

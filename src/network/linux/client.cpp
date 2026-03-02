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

using namespace prototype::network;
using namespace prototype::database;
using namespace prototype::cryptowrapper;

class PacketDispatcher {
public:
    std::queue<RawPacket> response_queue;
    std::mutex mtx;
    std::condition_variable cv;
    // Separate queue for chat messages to be consumed by main loop
    std::queue<std::string> chat_messages; 

    void push(RawPacket p) { 
        std::lock_guard<std::mutex> lock(mtx); 
        response_queue.push(p); 
        cv.notify_one(); 
    }
    
    void push_chat(const std::string& msg) {
        std::lock_guard<std::mutex> lock(mtx);
        chat_messages.push(msg);
    }

    std::optional<RawPacket> wait_for_response(int timeout_sec = 5) {
        std::unique_lock<std::mutex> lock(mtx);
        if (cv.wait_for(lock, std::chrono::seconds(timeout_sec), [this] { return !response_queue.empty(); })) {
            RawPacket p = response_queue.front(); response_queue.pop(); return p;
        }
        return std::nullopt;
    }
    
    bool pop_chat(std::string& out_msg) {
        std::lock_guard<std::mutex> lock(mtx);
        if (chat_messages.empty()) return false;
        out_msg = chat_messages.front();
        chat_messages.pop();
        return true;
    }
};

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
    CryptoService crypto(&local_db);
    identity.load_or_generate();

    std::thread receiver([manager, &dispatcher, &crypto]() {
        while (true) {
            auto in = manager->receive_packet();
            if (!in) break;
            if (in->header.type == PacketType::MESSAGE_DATA || in->header.type == PacketType::GROUP_MSG) {
                std::string decrypted = crypto.decrypt_packet(*in);
                std::string sender(in->header.sender_name);
                std::string display = "[" + sender + "]: " + decrypted;
                if (in->header.type == PacketType::GROUP_MSG) display = "[GROUP:" + sender + "]: " + decrypted;
                dispatcher.push_chat(display);
            } else {
                dispatcher.push(*in);
            }
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
    if (auth_res && (auth_res->header.type == PacketType::LOGIN_SUCCESS || auth_res->header.type == PacketType::REGISTER_SUCCESS)) {
        my_uuid = std::string(auth_res->payload.begin(), auth_res->payload.end());
        auto up_p = crypto.generate_prekey_batch(my_uuid);
        RawPacket upload; upload.header.type = PacketType::PREKEY_UPLOAD;
        upload.payload = up_p; upload.header.payload_size = up_p.size();
        manager->send_packet(upload);
    } else { std::cout << "Auth Failed.\n"; return 0; }

    prototype::network::ConsoleManager console;
    console.add_message("SYSTEM", "Welcome, " + my_user);

    std::string last_target_name = "";
    std::string last_target_uuid = "";
    bool is_last_group = false;

    while (true) {
        // 1. Poll for incoming chat messages (Non-blocking)
        std::string incoming;
        while(dispatcher.pop_chat(incoming)) {
            // Parse sender/text manually if needed, or just display raw string
            // Assuming dispatcher sends formatted string "[sender]: text"
            // We need to split it if we want strict add_message(sender, text)
            // For now, simpler:
            size_t colon = incoming.find("]: ");
            if (colon != std::string::npos) {
                std::string sender = incoming.substr(1, colon - 1); // remove [
                std::string text = incoming.substr(colon + 3);
                console.add_message(sender, text);
            } else {
                console.add_message("SYSTEM", incoming);
            }
        }

        // 2. Process Input (Non-blocking due to VMIN=0)
        std::string input;
        InputResult res = console.process_input(input);

        if (res == InputResult::TEXT) {
            if (last_target_name.empty()) {
                console.add_message("SYSTEM", "No active recipient. Use /msg <user> or /group msg <uuid>");
            } else if (is_last_group) {
                RawPacket g; g.header.type = PacketType::GROUP_MSG;
                string_to_uuid_parts_client(last_target_uuid, g.header.target_high, g.header.target_low);
                g.payload.assign(input.begin(), input.end()); g.header.payload_size = g.payload.size();
                manager->send_packet(g);
                console.add_message("YOU -> GROUP:" + last_target_name, input);
            } else {
                RawPacket fetch; fetch.header.type = PacketType::PREKEY_FETCH;
                fetch.payload.assign(last_target_name.begin(), last_target_name.end());
                fetch.header.payload_size = fetch.payload.size();
                manager->send_packet(fetch);
                auto pre_res = dispatcher.wait_for_response(1);
                if (pre_res && pre_res->header.type == PacketType::PREKEY_RESPONSE) {
                    uint64_t key_id; std::memcpy(&key_id, pre_res->payload.data(), 8);
                    std::array<uint8_t, 32> t_pub; std::memcpy(t_pub.data(), pre_res->payload.data() + 8, 32);
                    RawPacket msg = crypto.encrypt_message(input, key_id, t_pub);
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
                last_target_name = to_lowercase(target);
                is_last_group = false;
                
                RawPacket fetch; fetch.header.type = PacketType::PREKEY_FETCH;
                fetch.payload.assign(last_target_name.begin(), last_target_name.end());
                fetch.header.payload_size = fetch.payload.size();
                manager->send_packet(fetch);
                auto pre_res = dispatcher.wait_for_response(2);
                if (pre_res && pre_res->header.type == PacketType::PREKEY_RESPONSE) {
                    uint64_t key_id; std::memcpy(&key_id, pre_res->payload.data(), 8);
                    std::array<uint8_t, 32> t_pub; std::memcpy(t_pub.data(), pre_res->payload.data() + 8, 32);
                    RawPacket msg = crypto.encrypt_message(text, key_id, t_pub);
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
                    last_target_name = "Group"; last_target_uuid = g_uuid; is_last_group = true;
                    RawPacket g; g.header.type = PacketType::GROUP_MSG;
                    string_to_uuid_parts_client(g_uuid, g.header.target_high, g.header.target_low);
                    g.payload.assign(text.begin(), text.end()); g.header.payload_size = g.payload.size();
                    manager->send_packet(g);
                    console.add_message("YOU -> GROUP", text);
                }
            }
            else if (cmd == "/quit" || cmd == "/exit") break;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // CPU saver
    }

    SSL_CTX_free(ssl_ctx);
    cleanup_openssl();
    return 0;
}

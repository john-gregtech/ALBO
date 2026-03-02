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
    void push(RawPacket p) { std::lock_guard<std::mutex> lock(mtx); response_queue.push(p); cv.notify_one(); }
    std::optional<RawPacket> wait_for_response(int timeout_sec = 5) {
        std::unique_lock<std::mutex> lock(mtx);
        if (cv.wait_for(lock, std::chrono::seconds(timeout_sec), [this] { return !response_queue.empty(); })) {
            RawPacket p = response_queue.front(); response_queue.pop(); return p;
        }
        return std::nullopt;
    }
};

int main() {
    std::string ip_input;
    std::cout << "ALBO Secure Messenger Starting...\nServer IP (or LOCALHOST): ";
    std::cin >> ip_input;
    std::string server_ip = (ip_input == "LOCALHOST" || ip_input == "localhost") ? "127.0.0.1" : ip_input;

    init_openssl();
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

    // 1. Initial Handshake & Login (CLI Mode)
    auto challenge_opt = manager->receive_packet();
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

    std::cout << "[1] Login\n[2] Create Account\nChoice: ";
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

    auto auth_res = manager->receive_packet(); // Use direct recv for initial flow
    if (!auth_res || (auth_res->header.type != PacketType::LOGIN_SUCCESS && auth_res->header.type != PacketType::REGISTER_SUCCESS)) {
        std::cout << "Auth Failed.\n"; return 0;
    }
    if (auth_res->header.type == PacketType::REGISTER_SUCCESS) {
        my_uuid = std::string(auth_res->payload.begin(), auth_res->payload.end());
        auto up_p = crypto.generate_prekey_batch(my_uuid);
        RawPacket upload; upload.header.type = PacketType::PREKEY_UPLOAD;
        upload.payload = up_p; upload.header.payload_size = up_p.size();
        manager->send_packet(upload);
    }

    // 2. SWITCH TO TUI MODE
    ConsoleManager console;
    console.add_message("SYSTEM", "Welcome to ALBO Messenger, " + my_user);
    console.add_message("SYSTEM", "Use /msg <name> <text> to chat.");

    bool running = true;
    std::thread receiver([manager, &dispatcher, &crypto, &console]() {
        while (true) {
            auto in = manager->receive_packet();
            if (!in) { console.add_message("SYSTEM", "Disconnected."); break; }

            if (in->header.type == PacketType::MESSAGE_DATA) {
                std::string decrypted = crypto.decrypt_packet(*in);
                std::string sender(in->header.sender_name);
                console.add_message(sender, decrypted);
            } else {
                dispatcher.push(*in);
            }
        }
    });
    receiver.detach();

    while (running) {
        std::string input;
        InputResult res = console.process_input(input);

        if (res == InputResult::TEXT) {
            console.add_message("YOU", input);
            // (Need a "current chat" context to send raw text, for now we require /msg)
        }
        else if (res == InputResult::COMMAND) {
            std::stringstream ss(input);
            std::string cmd; ss >> cmd;
            if (cmd == "/msg") {
                std::string target, text;
                ss >> target; std::getline(ss >> std::ws, text);
                
                // Fetch and Send E2EE
                RawPacket fetch; fetch.header.type = PacketType::PREKEY_FETCH;
                std::string t_low = to_lowercase(target);
                fetch.payload.assign(t_low.begin(), t_low.end());
                fetch.header.payload_size = fetch.payload.size();
                manager->send_packet(fetch);

                auto pre_res = dispatcher.wait_for_response(2);
                if (pre_res && pre_res->header.type == PacketType::PREKEY_RESPONSE) {
                    uint64_t key_id; std::memcpy(&key_id, pre_res->payload.data(), 8);
                    std::array<uint8_t, 32> t_pub; std::memcpy(t_pub.data(), pre_res->payload.data() + 8, 32);
                    RawPacket msg = crypto.encrypt_message(text, key_id, t_pub);
                    std::string pfx = "@" + t_low + ":";
                    msg.payload.insert(msg.payload.begin(), pfx.begin(), pfx.end());
                    msg.header.payload_size = msg.payload.size();
                    manager->send_packet(msg);
                } else {
                    console.add_message("SYSTEM", "Error: Recipient offline or key fetch failed.");
                }
            }
            else if (cmd == "/quit" || cmd == "/exit") running = false;
            else if (cmd == "/clear") { /* clear history logic */ }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    SSL_CTX_free(ssl_ctx);
    cleanup_openssl();
    return 0;
}

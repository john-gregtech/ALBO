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

class PacketDispatcher {
public:
    std::queue<prototype::network::RawPacket> response_queue;
    std::mutex mtx;
    std::condition_variable cv;
    void push(prototype::network::RawPacket p) { std::lock_guard<std::mutex> lock(mtx); response_queue.push(p); cv.notify_one(); }
    std::optional<prototype::network::RawPacket> wait_for_response(int timeout_sec = 5) {
        std::unique_lock<std::mutex> lock(mtx);
        if (cv.wait_for(lock, std::chrono::seconds(timeout_sec), [this] { return !response_queue.empty(); })) {
            prototype::network::RawPacket p = response_queue.front(); response_queue.pop(); return p;
        }
        return std::nullopt;
    }
};

int main() {
    std::string ip_input;
    std::cout << "Enter Server IP (or LOCALHOST): ";
    std::cin >> ip_input;
    std::string server_ip = (ip_input == "LOCALHOST" || ip_input == "localhost") ? "127.0.0.1" : ip_input;

    prototype::network::init_openssl();
    SSL_CTX* ssl_ctx = prototype::network::create_client_context();

    std::string db_path = std::string(getenv("HOME")) + "/.local/share/albo/local_inbox.db";
    prototype::database::DatabaseManager local_db(db_path);
    local_db.initialize();

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in s_addr{};
    s_addr.sin_family = AF_INET; s_addr.sin_port = htons(5555);
    inet_pton(AF_INET, server_ip.c_str(), &s_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&s_addr, sizeof(s_addr)) < 0) { perror("connect"); return 1; }

    auto manager = std::make_shared<prototype::network::SecureSocketManager>(sock, ssl_ctx, false);
    if (!manager->perform_handshake()) { ALBO_LOG("TLS Handshake Failed."); return 1; }

    PacketDispatcher dispatcher;
    prototype::network::IdentityManager identity(&local_db);
    prototype::network::CryptoService crypto(&local_db);
    identity.load_or_generate();

    std::thread receiver([manager, &dispatcher, &crypto, &local_db]() {
        while (true) {
            auto in = manager->receive_packet();
            if (!in) break;

            if (in->header.type == prototype::network::PacketType::MESSAGE_DATA) {
                std::string decrypted = crypto.decrypt_packet(*in);
                std::string sender(in->header.sender_name);
                std::cout << "\n[" << sender << "]: " << decrypted << "\nALBO> " << std::flush;
            } else dispatcher.push(*in);
        }
    });
    receiver.detach();

    auto challenge_opt = dispatcher.wait_for_response();
    if (challenge_opt && challenge_opt->header.type == prototype::network::PacketType::AUTH_CHALLENGE) {
        auto sig = prototype::cryptowrapper::sign_message(challenge_opt->payload, identity.get_private_key());
        prototype::network::RawPacket resp; resp.header.type = prototype::network::PacketType::AUTH_RESPONSE;
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
        prototype::network::RawPacket p; p.header.type = prototype::network::PacketType::LOGIN_REQUEST;
        std::string auth = prototype::network::to_lowercase(my_user) + ":" + pwd;
        p.payload.assign(auth.begin(), auth.end()); p.header.payload_size = p.payload.size();
        manager->send_packet(p);
    } else {
        std::string pwd, display; std::cout << "New Username: "; std::cin >> my_user; std::cout << "New Password: "; std::cin >> pwd; std::cout << "Display Name: "; std::cin >> display;
        prototype::network::RawPacket p; p.header.type = prototype::network::PacketType::REGISTER_REQUEST;
        std::string data = prototype::network::to_lowercase(my_user) + ":" + pwd + ":" + display;
        p.payload.assign(data.begin(), data.end()); p.header.payload_size = p.payload.size();
        manager->send_packet(p);
    }

    auto auth_res = dispatcher.wait_for_response();
    if (auth_res && (auth_res->header.type == prototype::network::PacketType::LOGIN_SUCCESS || auth_res->header.type == prototype::network::PacketType::REGISTER_SUCCESS)) {
        ALBO_LOG("Auth Success!");
        if (auth_res->header.type == prototype::network::PacketType::REGISTER_SUCCESS) {
            my_uuid = std::string(auth_res->payload.begin(), auth_res->payload.end());
            ALBO_LOG("Uploading Pre-Keys...");
            auto up_p = crypto.generate_prekey_batch(my_uuid);
            prototype::network::RawPacket upload; upload.header.type = prototype::network::PacketType::PREKEY_UPLOAD;
            upload.payload = up_p; upload.header.payload_size = up_p.size();
            manager->send_packet(upload);
        }
    } else { ALBO_LOG("Auth Failed."); return 0; }

    while (true) {
        std::cout << "ALBO> Recipient Name: ";
        std::string target; if (!(std::cin >> target)) break;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        
        prototype::network::RawPacket fetch; fetch.header.type = prototype::network::PacketType::PREKEY_FETCH;
        std::string t_low = prototype::network::to_lowercase(target);
        fetch.payload.assign(t_low.begin(), t_low.end()); fetch.header.payload_size = fetch.payload.size();
        manager->send_packet(fetch);

        auto pre_res = dispatcher.wait_for_response();
        if (pre_res && pre_res->header.type == prototype::network::PacketType::PREKEY_RESPONSE) {
            uint64_t key_id; std::memcpy(&key_id, pre_res->payload.data(), 8);
            std::array<uint8_t, 32> t_pub; std::memcpy(t_pub.data(), pre_res->payload.data() + 8, 32);
            
            std::cout << "ALBO> Message: "; std::string text; std::getline(std::cin, text);
            
            prototype::network::RawPacket msg = crypto.encrypt_message(text, key_id, t_pub);
            std::string pfx = "@" + t_low + ":";
            msg.payload.insert(msg.payload.begin(), pfx.begin(), pfx.end());
            msg.header.payload_size = msg.payload.size();
            manager->send_packet(msg);
        } else { ALBO_LOG("Recipient offline or invalid."); }
    }

    SSL_CTX_free(ssl_ctx);
    prototype::network::cleanup_openssl();
    return 0;
}

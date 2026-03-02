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
#include "network/linux/socket_manager.h"
#include "network/universal/database.h"
#include "network/universal/hex_utils.h"
#include "cryptowrapper/X25519.h"
#include "cryptowrapper/aes256.h"
#include "cryptowrapper/sha256.h"
#include "cryptowrapper/ed25519.h"

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
    s_addr.sin_family = AF_INET; s_addr.sin_port = htons(5555);
    inet_pton(AF_INET, server_ip.c_str(), &s_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&s_addr, sizeof(s_addr)) < 0) { perror("connect"); return 1; }

    auto manager = std::make_shared<LinuxSocketManager>(sock);
    PacketDispatcher dispatcher;

    std::thread receiver([manager, &dispatcher, &local_db]() {
        while (true) {
            auto in = manager->receive_packet();
            if (!in) break;

            if (in->header.type == PacketType::MESSAGE_DATA) {
                try {
                    if (in->payload.size() < 56) continue;
                    uint64_t key_id; std::memcpy(&key_id, in->payload.data(), 8);
                    std::array<uint8_t, 32> eph_pub; std::memcpy(eph_pub.data(), in->payload.data() + 8, 32);
                    std::array<uint8_t, 16> iv; std::memcpy(iv.data(), in->payload.data() + 40, 16);
                    std::vector<uint8_t> ciphertext(in->payload.begin() + 56, in->payload.end());

                    PreKeyEntry my_local_key;
                    if (local_db.get_pre_key_by_id(key_id, my_local_key)) {
                        std::array<uint8_t, 32> my_priv;
                        std::copy(my_local_key.priv_key.begin(), my_local_key.priv_key.end(), my_priv.begin());
                        auto secret = compute_shared_secret(my_priv, eph_pub);
                        auto aes_key = prototype_functions::sha256_hash(std::string(secret.begin(), secret.end()));
                        auto pt = prototype_functions::aes_decrypt(ciphertext, aes_key, iv);
                        
                        // EXTRACT SENDER NAME FROM HEADER
                        std::string sender(in->header.sender_name);
                        if (sender.empty()) sender = "Unknown";

                        std::cout << "\n[" << sender << "]: " << std::string(pt.begin(), pt.end()) << "\nALBO> " << std::flush;
                        local_db.delete_pre_key(key_id);
                    }
                } catch (...) {}
            } else dispatcher.push(*in);
        }
    });
    receiver.detach();

    auto challenge_opt = dispatcher.wait_for_response();
    if (challenge_opt && challenge_opt->header.type == PacketType::AUTH_CHALLENGE) {
        auto identity = generate_ed25519_keypair();
        auto sig = sign_message(challenge_opt->payload, identity.priv);
        RawPacket resp; resp.header.type = PacketType::AUTH_RESPONSE;
        resp.payload.resize(32 + 64);
        std::memcpy(resp.payload.data(), identity.pub.data(), 32);
        std::memcpy(resp.payload.data() + 32, sig.data(), 64);
        resp.header.payload_size = resp.payload.size();
        manager->send_packet(resp);
    }

    std::cout << "\n--- ALBO Messenger ---\n[1] Login\n[2] Create Account\nChoice: ";
    int choice; std::cin >> choice;
    std::string my_user;
    if (choice == 1) {
        std::string pwd; std::cout << "Username: "; std::cin >> my_user; std::cout << "Password: "; std::cin >> pwd;
        RawPacket p; p.header.type = PacketType::LOGIN_REQUEST;
        std::string auth = to_lowercase(my_user) + ":" + pwd;
        p.payload.assign(auth.begin(), auth.end()); p.header.payload_size = p.payload.size();
        manager->send_packet(p);
    } else if (choice == 2) {
        std::string pwd, display; std::cout << "New Username: "; std::cin >> my_user; std::cout << "New Password: "; std::cin >> pwd; std::cout << "Display Name: "; std::cin >> display;
        RawPacket p; p.header.type = PacketType::REGISTER_REQUEST;
        std::string data = to_lowercase(my_user) + ":" + pwd + ":" + display;
        p.payload.assign(data.begin(), data.end()); p.header.payload_size = p.payload.size();
        manager->send_packet(p);
    }

    auto auth_res = dispatcher.wait_for_response();
    if (auth_res && (auth_res->header.type == PacketType::LOGIN_SUCCESS || auth_res->header.type == PacketType::REGISTER_SUCCESS)) {
        ALBO_LOG("Auth Success!");
    } else { ALBO_LOG("Auth Failed."); return 0; }

    while (true) {
        std::cout << "ALBO> Recipient Name: ";
        std::string target; if (!(std::cin >> target)) break;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        
        RawPacket fetch; fetch.header.type = PacketType::PREKEY_FETCH;
        std::string t_low = to_lowercase(target);
        fetch.payload.assign(t_low.begin(), t_low.end()); fetch.header.payload_size = fetch.payload.size();
        manager->send_packet(fetch);

        auto pre_res = dispatcher.wait_for_response();
        if (pre_res && pre_res->header.type == PacketType::PREKEY_RESPONSE) {
            uint64_t key_id; std::memcpy(&key_id, pre_res->payload.data(), 8);
            std::array<uint8_t, 32> t_pub; std::memcpy(t_pub.data(), pre_res->payload.data() + 8, 32);
            std::cout << "ALBO> Message: "; std::string text; std::getline(std::cin, text);
            auto my_eph = generate_x25519_keypair();
            auto secret = compute_shared_secret(my_eph.priv, t_pub);
            auto aes_k = prototype_functions::sha256_hash(std::string(secret.begin(), secret.end()));
            auto iv = prototype_functions::generate_initialization_vector();
            auto ct = prototype_functions::aes_encrypt(std::vector<uint8_t>(text.begin(), text.end()), aes_k, iv);
            RawPacket msg; msg.header.type = PacketType::MESSAGE_DATA;
            std::string pfx = "@" + t_low + ":"; msg.payload.assign(pfx.begin(), pfx.end());
            size_t h = msg.payload.size(); msg.payload.resize(h + 8 + 32 + 16 + ct.size());
            std::memcpy(msg.payload.data() + h, &key_id, 8);
            std::memcpy(msg.payload.data() + h + 8, my_eph.pub.data(), 32);
            std::memcpy(msg.payload.data() + h + 40, iv.data(), 16);
            std::memcpy(msg.payload.data() + h + 56, ct.data(), ct.size());
            msg.header.payload_size = msg.payload.size(); manager->send_packet(msg);
        } else { ALBO_LOG("Recipient offline or invalid."); }
    }
    return 0;
}

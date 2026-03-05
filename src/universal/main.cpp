#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <chrono>
#include <sstream>
#include <filesystem>

#include "universal/config.h"
#include "universal/network/database.h"
#include "universal/network/professionalprovider.h"
#include "universal/cryptowrapper/argon2id.h"

// Simple helper to get current time in MS
int64_t current_time_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

// Convert bytes to a hex string
std::string to_hex(const uint8_t* data, size_t len) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i) {
        ss << std::setw(2) << (int)data[i];
    }
    return ss.str();
}

std::string to_hex(const std::vector<uint8_t>& data) {
    return to_hex(data.data(), data.size());
}

// Helper to convert hex string back to bytes
std::vector<uint8_t> from_hex(std::string hex) {
    if (hex.size() >= 2 && hex.substr(0, 2) == "0x") hex = hex.substr(2);
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        uint8_t byte = (uint8_t)strtol(byteString.c_str(), nullptr, 16);
        bytes.push_back(byte);
    }
    return bytes;
}

// Convert our UUID struct into a standard string format (8-4-4-4-12)
std::string uuid_to_string_fixed(const prototype::network::UUID& uuid) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    ss << std::setw(8) << (uint32_t)(uuid.high >> 32) << "-";
    ss << std::setw(4) << (uint16_t)(uuid.high >> 16) << "-";
    ss << std::setw(4) << (uint16_t)uuid.high << "-";
    ss << std::setw(4) << (uint16_t)(uuid.low >> 48) << "-";
    ss << std::setw(12) << (uuid.low & 0xFFFFFFFFFFFFULL);
    return ss.str();
}

void print_sim_help() {
    std::cout << "\n--- ALBO Robustness & Versatility Simulation ---\n";
    std::cout << "Auth Commands: login <user> <pwd>, logout\n";
    std::cout << "User Commands: adduser <user> <pwd> <display>, users, finduser <user>\n";
    std::cout << "Inbox Commands: send <target_uuid> <msg>, history (my inbox)\n";
    std::cout << "--------------------------------------------------\n";
}

int main() {
    using namespace prototype::database;
    using namespace prototype::network;
    using namespace prototype::cryptowrapper;
    
#ifdef _WIN32
    const char* appdata = getenv("APPDATA");
    std::string db_path = std::string(appdata ? appdata : "C:") + "/" + ALBO_APP_DIR + "/" + ALBO_SIM_DB_NAME;
#else
    std::string db_path = std::string(getenv("HOME")) + "/.local/share/" + ALBO_APP_DIR + "/" + ALBO_SIM_DB_NAME;
#endif
    DatabaseManager db(db_path);
    db.initialize();

    std::string line;
    std::string current_user_uuid = "";
    std::string current_user_name = "Guest";

    std::cout << "ALBO Full-Scale Simulation Environment (Dynamic Tables).\n";
    print_sim_help();

    while (true) {
        std::cout << "\n[" << current_user_name << " @ SIM]> ";
        if (!std::getline(std::cin, line) || line == "exit") break;
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string cmd; ss >> cmd;

        try {
            if (cmd == "login") {
                std::string username, pwd; ss >> username >> pwd;
                UserEntry u;
                if (db.get_user_by_name(username, u)) {
                    size_t colon_pos = u.password.find(':');
                    if (colon_pos != std::string::npos) {
                        auto salt = from_hex(u.password.substr(0, colon_pos));
                        auto hash = from_hex(u.password.substr(colon_pos + 1));
                        if (prototype::cryptowrapper::verify_password(pwd, hash, salt)) {
                            current_user_uuid = u.uuid;
                            current_user_name = u.username;
                            std::cout << "Logged in!\n";
                        } else std::cout << "Wrong password.\n";
                    }
                } else std::cout << "User not found.\n";
            }
            else if (cmd == "adduser") {
                UserEntry u; std::string pwd; ss >> u.username >> pwd >> u.display_name;
                UserEntry dummy;
                if (db.get_user_by_name(u.username, dummy)) { std::cout << "User exists!\n"; continue; }
                auto res = prototype::cryptowrapper::hash_password(pwd);
                u.password = to_hex(res.salt) + ":" + to_hex(res.hash);
                u.uuid = uuid_to_string_fixed(prototype::network::generate_uuid_v4());
                u.last_seen = current_time_ms(); u.is_contact = true;
                if (db.upsert_user(u)) std::cout << "User created: " << u.uuid << "\n";
            }
            else if (cmd == "users") {
                auto users = db.list_all_users();
                for (const auto& u : users) std::cout << u.username << " | " << u.uuid << "\n";
            }
            else if (cmd == "send") {
                if (current_user_uuid.empty()) { std::cout << "Log in first!\n"; continue; }
                std::string to_uuid, text; ss >> to_uuid; std::getline(ss >> std::ws, text);
                MessageEntry m; m.sender_uuid = current_user_uuid;
                m.encrypted_payload.assign(text.begin(), text.end());
                m.timestamp = current_time_ms();
                if (db.store_message_dynamic(to_uuid, m)) std::cout << "Stored in " << to_uuid << "'s table.\n";
            }
            else if (cmd == "history") {
                if (current_user_uuid.empty()) { std::cout << "Log in first!\n"; continue; }
                auto msgs = db.fetch_all_from_table(current_user_uuid, false);
                std::cout << "--- Inbox for " << current_user_name << " ---\n";
                for (const auto& m : msgs) {
                    std::string c(m.encrypted_payload.begin(), m.encrypted_payload.end());
                    std::cout << "[" << m.timestamp << "] FROM " << m.sender_uuid << ": " << c << "\n";
                }
            }
            else if (cmd == "wipe") {
                db.wipe_all_data(); current_user_uuid = ""; current_user_name = "Guest";
            }
            else if (cmd == "/help" || cmd == "help") print_sim_help();
        } catch (const std::exception& e) { std::cout << "Error: " << e.what() << "\n"; }
    }
    return 0;
}

#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <chrono>
#include <sstream>

#include "network/universal/database.h"
#include "network/universal/professionalprovider.h"
#include "cryptowrapper/argon2id.h"

// Simple helper to get current time in MS
int64_t current_time_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

// Convert bytes to a hex string
std::string to_hex(const uint8_t* data, size_t len) {
    std::stringstream ss;
    for (size_t i = 0; i < len; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)data[i];
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
std::string uuid_to_string(const prototype::network::UUID& uuid) {
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
    std::cout << "\n--- ALBO Robustness & Versatility Simulation (Argon2id Active) ---\n";
    std::cout << "Auth Commands:\n";
    std::cout << "  login <uuid> <password>        - Log in with Argon2id verification\n";
    std::cout << "  logout                        - Revert to Guest\n";
    std::cout << "\nUser Commands:\n";
    std::cout << "  adduser <pwd> <name> <status>  - Create a user (hashes pwd with Argon2id)\n";
    std::cout << "  users                          - List all users\n";
    std::cout << "  finduser <uuid>                - Search user details (shows salt/hash)\n";
    std::cout << "\nArgon2id Direct Test:\n";
    std::cout << "  hash <password>                - Manually hash a password\n";
    std::cout << "\nMessage Commands:\n";
    std::cout << "  send <to_uuid> <msg>, chat <uuid2>, history\n";
    std::cout << "\nMaintenance Commands:\n";
    std::cout << "  wipe, init, sql <query>, help, exit\n";
    std::cout << "------------------------------------------------------------------\n";
}

int main() {
    using namespace prototype::database;
    using namespace prototype::network;
    using namespace prototype::cryptowrapper;
    
    DatabaseManager db("albo_simulation.db");
    db.initialize();

    std::string line;
    std::string current_user_uuid = "";
    std::string current_user_name = "Guest";

    std::cout << "ALBO Full-Scale Simulation Environment.\n";
    print_sim_help();

    while (true) {
        std::cout << "\n[" << current_user_name << " @ SIM]> ";
        if (!std::getline(std::cin, line) || line == "exit") break;
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string cmd;
        ss >> cmd;

        try {
            if (cmd == "login") {
                std::string uuid, pwd; ss >> uuid >> pwd;
                UserEntry u;
                if (db.get_user(uuid, u)) {
                    // Extract salt and hash from the stored "password" field
                    // Format in DB: salt_hex:hash_hex
                    size_t colon_pos = u.password.find(':');
                    if (colon_pos != std::string::npos) {
                        auto salt = from_hex(u.password.substr(0, colon_pos));
                        auto hash = from_hex(u.password.substr(colon_pos + 1));
                        
                        if (verify_password(pwd, hash, salt)) {
                            current_user_uuid = uuid;
                            current_user_name = u.display_name;
                            std::cout << "Successfully logged in as " << current_user_name << " (Argon2id Verified)\n";
                        } else {
                            std::cout << "Login failed: Incorrect Password.\n";
                        }
                    } else {
                        std::cout << "Login failed: Stored password format is invalid.\n";
                    }
                } else {
                    std::cout << "Login failed: Invalid UUID.\n";
                }
            }
            else if (cmd == "hash") {
                std::string pwd; ss >> pwd;
                auto res = hash_password(pwd);
                std::cout << "Salt: " << to_hex(res.salt) << "\n";
                std::cout << "Hash: " << to_hex(res.hash) << "\n";
            }
            else if (cmd == "adduser") {
                UserEntry u;
                std::string pwd; ss >> pwd >> u.display_name;
                std::getline(ss >> std::ws, u.status_text);
                
                // Hash the password
                auto res = hash_password(pwd);
                u.password = to_hex(res.salt) + ":" + to_hex(res.hash);
                
                u.uuid = uuid_to_string(generate_uuid_v4());
                u.last_seen = current_time_ms();
                u.public_key_hex = "0xFEEDFACE"; 
                u.is_contact = true;
                
                if (db.upsert_user(u)) {
                    std::cout << "User '" << u.display_name << "' created.\n";
                    std::cout << "UUID: " << u.uuid << "\n";
                }
            }
            else if (cmd == "users") {
                auto users = db.list_all_users();
                for (const auto& u : users) std::cout << u.uuid << " | " << u.display_name << "\n";
            }
            else if (cmd == "finduser") {
                std::string uuid; ss >> uuid;
                UserEntry u;
                if (db.get_user(uuid, u)) {
                    std::cout << "UUID:   " << u.uuid << "\n";
                    std::cout << "Name:   " << u.display_name << "\n";
                    std::cout << "Status: " << u.status_text << "\n";
                    std::cout << "Stored Credential (Salt:Hash): " << u.password << "\n";
                } else std::cout << "Not found.\n";
            }
            else if (cmd == "logout") {
                current_user_uuid = ""; current_user_name = "Guest";
                std::cout << "Logged out.\n";
            }
            else if (cmd == "send") {
                if (current_user_uuid.empty()) { std::cout << "Log in first!\n"; continue; }
                std::string to, msg_text;
                ss >> to; std::getline(ss >> std::ws, msg_text);
                MessageEntry m;
                m.sender_uuid = current_user_uuid; m.target_uuid = to;
                m.timestamp = current_time_ms(); m.is_read = false;
                m.encrypted_payload.assign(msg_text.begin(), msg_text.end());
                m.iv.fill(0x55);
                db.store_message(m); std::cout << "Sent.\n";
            }
            else if (cmd == "history") {
                if (current_user_uuid.empty()) { std::cout << "Log in first!\n"; continue; }
                auto msgs = db.get_messages_by_contact(current_user_uuid, 50);
                for (const auto& m : msgs) {
                    std::string content(m.encrypted_payload.begin(), m.encrypted_payload.end());
                    std::cout << "[" << m.timestamp << "] " << (m.sender_uuid == current_user_uuid ? "Me" : "Them") << ": " << content << "\n";
                }
            }
            else if (cmd == "chat") {
                if (current_user_uuid.empty()) { std::cout << "Log in first!\n"; continue; }
                std::string u2; ss >> u2;
                auto msgs = db.get_chat_history(current_user_uuid, u2, 50);
                std::cout << "--- Conversation with " << u2 << " ---\n";
                for (const auto& m : msgs) {
                    std::string content(m.encrypted_payload.begin(), m.encrypted_payload.end());
                    std::cout << "[" << m.timestamp << "] " << (m.sender_uuid == current_user_uuid ? "Me" : "Them") << ": " << content << "\n";
                }
            }
            else if (cmd == "clear") {
                std::string uuid; ss >> uuid;
                if (db.clear_messages(uuid)) std::cout << "Messages for contact " << uuid << " cleared.\n";
            }
            else if (cmd == "wipe") {
                db.wipe_all_data(); current_user_uuid = ""; current_user_name = "Guest";
                std::cout << "Wiped.\n";
            }
            else if (cmd == "help") print_sim_help();
        } catch (const std::exception& e) { std::cout << "Error: " << e.what() << "\n"; }
    }
    return 0;
}

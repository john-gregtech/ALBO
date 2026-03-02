#include "network/linux/server/auth_handler.h"
#include "network/universal/hex_utils.h"
#include "network/universal/professionalprovider.h" // For UUID generation
#include "cryptowrapper/argon2id.h"
#include "config.h"
#include <cstring>
#include <sstream>

namespace prototype::network {

    AuthHandler::AuthHandler(prototype::database::DatabaseManager* database, prototype::network::SessionRegistry* reg) 
        : db(database), registry(reg) {}

    // Helper from server.cpp
    static void ss_uuid_format_auth(std::stringstream& ss, uint64_t high, uint64_t low) {
        ss << std::hex << std::setfill('0');
        ss << std::setw(8) << (uint32_t)(high >> 32) << "-";
        ss << std::setw(4) << (uint16_t)(high >> 16) << "-";
        ss << std::setw(4) << (uint16_t)high << "-";
        ss << std::setw(4) << (uint16_t)(low >> 48) << "-";
        ss << std::setw(12) << (low & 0xFFFFFFFFFFFFULL);
    }

    static std::string uuid_to_string_auth(const prototype::network::UUID& uuid) {
        std::stringstream ss;
        ss_uuid_format_auth(ss, uuid.high, uuid.low);
        return ss.str();
    }

    bool AuthHandler::handle_login(const prototype::network::RawPacket& packet, std::shared_ptr<prototype::network::SecureSocketManager> manager, std::string& out_uuid, std::string& out_username) {
        std::string payload(packet.payload.begin(), packet.payload.end());
        size_t colon = payload.find(':');
        if (colon == std::string::npos) return false;

        std::string username = prototype::network::to_lowercase(payload.substr(0, colon));
        std::string pwd = payload.substr(colon + 1);
        
        prototype::database::UserEntry user;
        if (db->get_user_by_name(username, user)) {
            size_t s_colon = user.password.find(':');
            if (s_colon != std::string::npos) {
                auto salt = prototype::network::from_hex(user.password.substr(0, s_colon));
                auto hash = prototype::network::from_hex(user.password.substr(s_colon + 1));
                if (prototype::cryptowrapper::verify_password(pwd, hash, salt)) {
                    if (registry->is_online(user.uuid)) {
                        ALBO_LOG("Login REJECTED: " << username << " already online.");
                        return false;
                    }
                    out_uuid = user.uuid;
                    out_username = username;
                    registry->register_session(out_uuid, out_username, manager);
                    return true;
                }
            }
        }
        return false;
    }

    bool AuthHandler::handle_registration(const prototype::network::RawPacket& packet, std::shared_ptr<prototype::network::SecureSocketManager> manager, std::string& out_uuid, std::string& out_username) {
        std::string payload(packet.payload.begin(), packet.payload.end());
        std::vector<std::string> parts;
        size_t start = 0, end = payload.find(':');
        while (end != std::string::npos) {
            parts.push_back(payload.substr(start, end - start));
            start = end + 1;
            end = payload.find(':', start);
        }
        parts.push_back(payload.substr(start));

        if (parts.size() < 3) return false;

        std::string username = prototype::network::to_lowercase(parts[0]);
        prototype::database::UserEntry existing;
        if (!db->get_user_by_name(username, existing)) {
            prototype::database::UserEntry neu;
            neu.username = username;
            neu.display_name = parts[2];
            
            auto res = prototype::cryptowrapper::hash_password(parts[1]);
            neu.password = prototype::network::to_hex(res.salt) + ":" + prototype::network::to_hex(res.hash);
            neu.uuid = uuid_to_string_auth(prototype::network::generate_uuid_v4());
            neu.last_seen = 0;
            neu.is_contact = true;

            if (db->upsert_user(neu)) {
                out_uuid = neu.uuid;
                out_username = username;
                registry->register_session(out_uuid, out_username, manager);
                return true;
            }
        }
        return false;
    }

}

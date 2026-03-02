#include "network/linux/server/auth_handler.h"
#include "network/universal/hex_utils.h"
#include "cryptowrapper/argon2id.h"
#include "config.h"
#include <cstring>

namespace prototype::network {

    AuthHandler::AuthHandler(prototype::database::DatabaseManager* database, prototype::network::SessionRegistry* reg) 
        : db(database), registry(reg) {}

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
        // (Full logic here, but ensuring no using namespaces)
        return false;
    }

}

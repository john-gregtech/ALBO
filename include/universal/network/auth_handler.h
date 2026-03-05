#pragma once
#include <string>
#include <memory>
#include "universal/network/secure_socket.h"
#include "universal/network/database.h"
#include "universal/network/session_registry.h"

namespace prototype::network {

    class AuthHandler {
    private:
        prototype::database::DatabaseManager* db;
        SessionRegistry* registry;

    public:
        AuthHandler(prototype::database::DatabaseManager* database, SessionRegistry* reg);

        // Uses SecureSocketManager instead of LinuxSocketManager
        bool handle_login(const RawPacket& packet, std::shared_ptr<SecureSocketManager> manager, std::string& out_uuid, std::string& out_username);
        bool handle_registration(const RawPacket& packet, std::shared_ptr<SecureSocketManager> manager, std::string& out_uuid, std::string& out_username);
    };

}

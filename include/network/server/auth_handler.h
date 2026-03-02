#pragma once
#include <string>
#include <memory>
#include "network/linux/socket_manager.h"
#include "network/universal/database.h"
#include "network/universal/session_registry.h"

namespace prototype::network {

    class AuthHandler {
    private:
        prototype::database::DatabaseManager* db;
        SessionRegistry* registry;

    public:
        AuthHandler(prototype::database::DatabaseManager* database, SessionRegistry* reg);

        bool handle_login(const RawPacket& packet, std::shared_ptr<LinuxSocketManager> manager, std::string& out_uuid, std::string& out_username);
        bool handle_registration(const RawPacket& packet, std::shared_ptr<LinuxSocketManager> manager, std::string& out_uuid, std::string& out_username);
    };

}

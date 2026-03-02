#pragma once
#include <string>
#include <memory>
#include "network/universal/secure_socket.h"
#include "network/universal/database.h"
#include "network/universal/session_registry.h"

namespace prototype::network {

    class RoutingHandler {
    private:
        prototype::database::DatabaseManager* db;
        SessionRegistry* registry;

    public:
        RoutingHandler(prototype::database::DatabaseManager* database, SessionRegistry* reg);

        bool route_message(RawPacket& packet, const std::string& sender_uuid, const std::string& sender_name);
        bool handle_prekey_fetch(const RawPacket& packet, std::shared_ptr<SecureSocketManager> manager);
    };

}

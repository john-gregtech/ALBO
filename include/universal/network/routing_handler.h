#pragma once
#include <string>
#include <memory>
#include "universal/network/secure_socket.h"
#include "universal/network/database.h"
#include "universal/network/session_registry.h"

namespace prototype::network {

    class RoutingHandler {
    private:
        prototype::database::DatabaseManager* db;
        SessionRegistry* registry;

    public:
        RoutingHandler(prototype::database::DatabaseManager* database, SessionRegistry* reg);

        bool route_message(RawPacket& packet, const std::string& sender_uuid, const std::string& sender_name);
        bool broadcast_to_group(RawPacket& packet, const std::string& group_uuid, const std::string& sender_uuid, const std::string& sender_name);
        bool handle_prekey_fetch(const RawPacket& packet, std::shared_ptr<SecureSocketManager> manager);
    };

}

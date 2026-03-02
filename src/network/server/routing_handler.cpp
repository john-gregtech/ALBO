#include "network/linux/server/routing_handler.h"
#include "network/universal/hex_utils.h"
#include "config.h"
#include <sstream>
#include <cstring>

namespace prototype::network {

    static void ss_uuid_format_internal(std::stringstream& ss, uint64_t high, uint64_t low) {
        ss << std::hex << std::setfill('0');
        ss << std::setw(8) << (uint32_t)(high >> 32) << "-";
        ss << std::setw(4) << (uint16_t)(high >> 16) << "-";
        ss << std::setw(4) << (uint16_t)high << "-";
        ss << std::setw(4) << (uint16_t)(low >> 48) << "-";
        ss << std::setw(12) << (low & 0xFFFFFFFFFFFFULL);
    }

    RoutingHandler::RoutingHandler(prototype::database::DatabaseManager* database, prototype::network::SessionRegistry* reg)
        : db(database), registry(reg) {}

    bool RoutingHandler::route_message(prototype::network::RawPacket& packet, const std::string& sender_uuid, const std::string& sender_name) {
        std::stringstream ss_u;
        ss_uuid_format_internal(ss_u, packet.header.target_high, packet.header.target_low);
        std::string target_uuid = ss_u.str();

        std::memset(packet.header.sender_name, 0, 16);
        std::strncpy(packet.header.sender_name, sender_name.c_str(), 15);

        auto target_manager = registry->get_session(target_uuid);
        if (target_manager) {
            return target_manager->send_packet(packet);
        } else {
            return true;
        }
    }

}

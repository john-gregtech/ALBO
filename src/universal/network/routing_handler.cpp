#include "universal/network/routing_handler.h"
#include "universal/network/hex_utils.h"
#include "universal/network/professionalprovider.h"
#include "universal/config.h"
#include <sstream>
#include <cstring>
#include <chrono>

namespace prototype::network {

    static void ss_uuid_format_internal(std::stringstream& ss, uint64_t high, uint64_t low) {
        ss << std::hex << std::setfill('0');
        ss << std::setw(8) << (uint32_t)(high >> 32) << "-";
        ss << std::setw(4) << (uint16_t)(high >> 16) << "-";
        ss << std::setw(4) << (uint16_t)high << "-";
        ss << std::setw(4) << (uint16_t)(low >> 48) << "-";
        ss << std::setw(12) << (low & 0xFFFFFFFFFFFFULL);
    }

    static void string_to_uuid_parts_internal(const std::string& uuid_str, uint64_t& high, uint64_t& low) {
        std::string clean = uuid_str;
        clean.erase(std::remove(clean.begin(), clean.end(), '-'), clean.end());
        if (clean.length() != 32) return;
        high = std::stoull(clean.substr(0, 16), nullptr, 16);
        low = std::stoull(clean.substr(16), nullptr, 16);
    }

    RoutingHandler::RoutingHandler(prototype::database::DatabaseManager* database, SessionRegistry* reg)
        : db(database), registry(reg) {}

    bool RoutingHandler::route_message(RawPacket& packet, const std::string& sender_uuid, const std::string& sender_name) {
        std::stringstream ss_u;
        ss_uuid_format_internal(ss_u, packet.header.target_high, packet.header.target_low);
        std::string target_uuid = ss_u.str();

        std::memset(packet.header.sender_name, 0, 16);
        std::strncpy(packet.header.sender_name, sender_name.c_str(), 15);
        string_to_uuid_parts_internal(sender_uuid, packet.header.target_high, packet.header.target_low);

        auto target_manager = registry->get_session(target_uuid);
        if (target_manager) {
            return target_manager->send_packet(packet);
        } else {
            prototype::database::MessageEntry m;
            m.sender_uuid = sender_uuid;
            m.target_uuid = target_uuid;
            m.encrypted_payload = packet.payload;
            m.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            return db->store_offline_message(m);
        }
    }

    bool RoutingHandler::broadcast_to_group(RawPacket& packet, const std::string& group_uuid, const std::string& sender_uuid, const std::string& sender_name) {
        auto members = db->get_group_members(group_uuid);
        bool all_ok = true;

        for (const auto& member_uuid : members) {
            if (member_uuid == sender_uuid) continue; // Don't echo back to sender

            // Prepare packet for THIS member
            RawPacket copy = packet;
            std::memset(copy.header.sender_name, 0, 16);
            std::strncpy(copy.header.sender_name, sender_name.c_str(), 15);
            string_to_uuid_parts_internal(sender_uuid, copy.header.target_high, copy.header.target_low);

            auto target_manager = registry->get_session(member_uuid);
            if (target_manager) {
                if (!target_manager->send_packet(copy)) all_ok = false;
            } else {
                prototype::database::MessageEntry m;
                m.sender_uuid = sender_uuid;
                m.target_uuid = member_uuid;
                m.encrypted_payload = copy.payload;
                m.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                if (!db->store_offline_message(m)) all_ok = false;
            }
        }
        return all_ok;
    }

}

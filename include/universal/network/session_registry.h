#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include <memory>
#include "universal/network/secure_socket.h"
#include "universal/config.h"

namespace prototype::network {

    class SessionRegistry {
    private:
        std::unordered_map<std::string, std::shared_ptr<SecureSocketManager>> active_sessions;
        std::unordered_map<std::string, std::string> uuid_to_name;
        std::mutex mtx;

    public:
        void register_session(const std::string& uuid, const std::string& name, std::shared_ptr<SecureSocketManager> manager) {
            std::lock_guard<std::mutex> lock(mtx);
            active_sessions[uuid] = manager;
            uuid_to_name[uuid] = name;
            ALBO_LOG("Session Registered: " << name << " [" << uuid << "]");
        }

        void remove_session(const std::string& uuid) {
            std::lock_guard<std::mutex> lock(mtx);
            if (uuid_to_name.count(uuid)) {
                ALBO_LOG("Session Removed: " << uuid_to_name[uuid] << " [" << uuid << "]");
                active_sessions.erase(uuid);
                uuid_to_name.erase(uuid);
            }
        }

        void force_disconnect_all(const std::string& uuid) {
            std::lock_guard<std::mutex> lock(mtx);
            if (active_sessions.count(uuid)) {
                ALBO_LOG("Force Disconnect: " << (uuid_to_name.count(uuid) ? uuid_to_name[uuid] : uuid));
                // In a real system, we might want to send a 'kick' packet here
                // but since the loop will fail on next recv/send, we just clear registry
                active_sessions.erase(uuid);
                uuid_to_name.erase(uuid);
            }
        }

        std::shared_ptr<SecureSocketManager> get_session(const std::string& uuid) {
            std::lock_guard<std::mutex> lock(mtx);
            auto it = active_sessions.find(uuid);
            if (it != active_sessions.end()) return it->second;
            return nullptr;
        }

        bool is_online(const std::string& uuid) {
            std::lock_guard<std::mutex> lock(mtx);
            return active_sessions.count(uuid) > 0;
        }

        std::string get_name(const std::string& uuid) {
            std::lock_guard<std::mutex> lock(mtx);
            return uuid_to_name.count(uuid) ? uuid_to_name[uuid] : "Unknown";
        }
    };

}

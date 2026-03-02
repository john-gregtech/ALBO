#include <queue>
#include <vector>
#include <array>
#include <cctype>
#include <cstdint>
#include <string>
#include <iostream>

#include <thread>
#include <mutex>
#include <atomic>

#include <sqlite3.h>

namespace prototype::database {

    struct MessageEntry {
        uint64_t id;
        std::string sender_uuid;
        std::string target_uuid;
        std::vector<uint8_t> encrypted_payload;
        std::array<uint8_t, 16> iv;
        int64_t timestamp;
        bool is_read;
    };

    struct UserEntry {
        std::string uuid;
        std::string display_name;
        std::string public_key_hex;
        std::string password; // Plaintext for simulation as requested
        int64_t last_seen;
        bool is_contact;
        std::string status_text;
    };

    class DatabaseManager {
    private:
        sqlite3* db = nullptr;
        std::string db_path;
        std::mutex db_mutex;

        bool execute_raw(const std::string& sql);

    public:
        explicit DatabaseManager(const std::string& path);
        ~DatabaseManager();

        // Prevent copying
        DatabaseManager(const DatabaseManager&) = delete;
        DatabaseManager& operator=(const DatabaseManager&) = delete;

        bool initialize();
        
        // Messaging specific functions
        bool store_message(const MessageEntry& msg);
        std::vector<MessageEntry> get_messages_by_contact(const std::string& contact_uuid, int limit = 50);
        std::vector<MessageEntry> get_chat_history(const std::string& u1, const std::string& u2, int limit = 50);
        bool clear_messages(const std::string& contact_uuid);
        bool wipe_all_data();

        // User/Contact management
        bool upsert_user(const UserEntry& user);
        bool get_user(const std::string& uuid, UserEntry& out_user);
        std::vector<UserEntry> list_all_users();
        
        // Generic execution with binding support (simplified for now)
        void execute_sql(const std::string& sql_command);
    };
}
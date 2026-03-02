#pragma once
#include <vector>
#include <array>
#include <cstdint>
#include <string>
#include <iostream>
#include <thread>
#include <mutex>
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
        std::string username;
        std::string display_name;
        std::string public_key_hex;
        std::string password;
        int64_t last_seen;
        bool is_contact;
    };

    struct PreKeyEntry {
        uint64_t key_id;
        std::string owner_uuid;
        std::vector<uint8_t> pub_key;
        std::vector<uint8_t> priv_key;
    };

    struct GroupEntry {
        std::string group_uuid;
        std::string group_name;
        std::string admin_uuid;
        int64_t created_at;
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

        DatabaseManager(const DatabaseManager&) = delete;
        DatabaseManager& operator=(const DatabaseManager&) = delete;

        bool initialize();
        
        // 1-on-1 Messaging
        bool store_message(const MessageEntry& msg);
        std::vector<MessageEntry> get_messages_by_contact(const std::string& contact_uuid, int limit = 50);
        std::vector<MessageEntry> get_chat_history(const std::string& u1, const std::string& u2, int limit = 50);
        bool clear_messages(const std::string& contact_uuid);
        bool wipe_all_data();

        // User Management
        bool upsert_user(const UserEntry& user);
        bool get_user(const std::string& uuid, UserEntry& out_user);
        bool get_user_by_name(const std::string& username, UserEntry& out_user);
        std::vector<UserEntry> list_all_users();

        // Offline / Pre-Keys
        bool store_offline_message(const MessageEntry& msg);
        std::vector<MessageEntry> fetch_and_delete_offline_messages(const std::string& target_uuid);
        bool store_pre_key(const PreKeyEntry& key, bool is_server_side);
        bool get_one_pre_key(const std::string& owner_uuid, PreKeyEntry& out_key);
        bool get_pre_key_by_id(uint64_t key_id, PreKeyEntry& out_key);
        bool delete_pre_key(uint64_t key_id);

        // Group Management
        bool create_group(const GroupEntry& group);
        bool add_group_member(const std::string& group_uuid, const std::string& user_uuid);
        bool remove_group_member(const std::string& group_uuid, const std::string& user_uuid);
        std::vector<std::string> get_group_members(const std::string& group_uuid);
        std::vector<GroupEntry> get_user_groups(const std::string& user_uuid);
        bool is_group_admin(const std::string& group_uuid, const std::string& user_uuid);
        
        void execute_sql(const std::string& sql_command);
    };
}

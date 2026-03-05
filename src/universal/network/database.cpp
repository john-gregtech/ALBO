#include "universal/network/database.h"
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <filesystem>

namespace prototype::database {

    DatabaseManager::DatabaseManager(const std::string& path) : db_path(path) {
        std::filesystem::path p(db_path);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
        if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
            std::string error = sqlite3_errmsg(db);
            sqlite3_close(db);
            throw std::runtime_error("Failed to open database: " + error);
        }
    }

    DatabaseManager::~DatabaseManager() {
        if (db) sqlite3_close(db);
    }

    bool DatabaseManager::execute_raw(const std::string& sql) {
        char* errMsg = nullptr;
        if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::string error = errMsg;
            sqlite3_free(errMsg);
            return false;
        }
        return true;
    }

    bool DatabaseManager::initialize() {
        std::lock_guard<std::mutex> lock(db_mutex);
        const char* schema = R"(
            CREATE TABLE IF NOT EXISTS users (
                uuid TEXT PRIMARY KEY,
                username TEXT UNIQUE NOT NULL,
                display_name TEXT,
                public_key_hex TEXT,
                password TEXT,
                last_seen INTEGER,
                is_contact INTEGER DEFAULT 0
            );
            CREATE TABLE IF NOT EXISTS pre_keys (
                key_id INTEGER PRIMARY KEY AUTOINCREMENT,
                owner_uuid TEXT NOT NULL,
                pub_key BLOB NOT NULL,
                priv_key BLOB
            );
            CREATE TABLE IF NOT EXISTS groups (
                group_uuid TEXT PRIMARY KEY,
                group_name TEXT NOT NULL,
                admin_uuid TEXT NOT NULL,
                created_at INTEGER NOT NULL
            );
            CREATE TABLE IF NOT EXISTS group_members (
                group_uuid TEXT NOT NULL,
                user_uuid TEXT NOT NULL,
                PRIMARY KEY (group_uuid, user_uuid)
            );
            CREATE TABLE IF NOT EXISTS user_contacts (
                owner_uuid TEXT NOT NULL,
                contact_uuid TEXT NOT NULL,
                contact_username TEXT NOT NULL,
                PRIMARY KEY (owner_uuid, contact_uuid)
            );
            CREATE TABLE IF NOT EXISTS messages (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                sender_uuid TEXT NOT NULL,
                target_uuid TEXT NOT NULL,
                encrypted_payload BLOB NOT NULL,
                timestamp INTEGER NOT NULL,
                public_key BLOB
            );
        )";
        return execute_raw(schema);
    }

    bool DatabaseManager::create_user_inbox_table(const std::string& uuid) {
        std::string sql = "CREATE TABLE IF NOT EXISTS \"" + uuid + "\" ("
                          "encrypted_payload BLOB, "
                          "timestamp INTEGER, "
                          "public_key BLOB, "
                          "sender_uuid TEXT);";
        return execute_raw(sql);
    }

    bool DatabaseManager::store_message_dynamic(const std::string& table_name, const MessageEntry& msg) {
        std::lock_guard<std::mutex> lock(db_mutex);
        create_user_inbox_table(table_name);
        std::string sql = "INSERT INTO \"" + table_name + "\" (encrypted_payload, timestamp, public_key, sender_uuid) VALUES (?, ?, ?, ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_blob(stmt, 1, msg.encrypted_payload.data(), (int)msg.encrypted_payload.size(), SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 2, msg.timestamp);
        sqlite3_bind_blob(stmt, 3, msg.public_key.data(), (int)msg.public_key.size(), SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, msg.sender_uuid.c_str(), -1, SQLITE_STATIC);
        bool res = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        return res;
    }

    std::vector<MessageEntry> DatabaseManager::fetch_all_from_table(const std::string& table_name, bool delete_after) {
        std::lock_guard<std::mutex> lock(db_mutex);
        std::vector<MessageEntry> msgs;
        std::string sql = "SELECT encrypted_payload, timestamp, public_key, sender_uuid FROM \"" + table_name + "\";";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return msgs;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            MessageEntry m;
            const uint8_t* p = (const uint8_t*)sqlite3_column_blob(stmt, 0);
            m.encrypted_payload.assign(p, p + sqlite3_column_bytes(stmt, 0));
            m.timestamp = sqlite3_column_int64(stmt, 1);
            const uint8_t* pk = (const uint8_t*)sqlite3_column_blob(stmt, 2);
            m.public_key.assign(pk, pk + sqlite3_column_bytes(stmt, 2));
            m.sender_uuid = (const char*)sqlite3_column_text(stmt, 3);
            msgs.push_back(std::move(m));
        }
        sqlite3_finalize(stmt);
        if (delete_after && !msgs.empty()) {
            std::string drop = "DROP TABLE \"" + table_name + "\";";
            execute_raw(drop);
        }
        return msgs;
    }

    bool DatabaseManager::store_message(const MessageEntry& msg) {
        std::lock_guard<std::mutex> lock(db_mutex);
        const char* sql = "INSERT INTO messages (sender_uuid, target_uuid, encrypted_payload, timestamp, public_key) VALUES (?, ?, ?, ?, ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, msg.sender_uuid.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, msg.target_uuid.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_blob(stmt, 3, msg.encrypted_payload.data(), (int)msg.encrypted_payload.size(), SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 4, msg.timestamp);
        sqlite3_bind_blob(stmt, 5, msg.public_key.data(), (int)msg.public_key.size(), SQLITE_STATIC);
        bool res = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        return res;
    }

    std::vector<MessageEntry> DatabaseManager::get_messages_by_contact(const std::string& contact_uuid, int limit) {
        return fetch_all_from_table(contact_uuid, false);
    }

    std::vector<MessageEntry> DatabaseManager::get_chat_history(const std::string& u1, const std::string& u2, int limit) {
        std::lock_guard<std::mutex> lock(db_mutex);
        std::vector<MessageEntry> msgs;
        const char* sql = "SELECT sender_uuid, target_uuid, encrypted_payload, timestamp FROM messages "
                          "WHERE (sender_uuid = ? AND target_uuid = ?) OR (sender_uuid = ? AND target_uuid = ?) "
                          "ORDER BY timestamp ASC LIMIT ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return msgs;
        sqlite3_bind_text(stmt, 1, u1.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, u2.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, u2.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, u1.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 5, limit);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            MessageEntry m;
            m.sender_uuid = (const char*)sqlite3_column_text(stmt, 0);
            m.target_uuid = (const char*)sqlite3_column_text(stmt, 1);
            const uint8_t* p = (const uint8_t*)sqlite3_column_blob(stmt, 2);
            m.encrypted_payload.assign(p, p + sqlite3_column_bytes(stmt, 2));
            m.timestamp = sqlite3_column_int64(stmt, 3);
            msgs.push_back(std::move(m));
        }
        sqlite3_finalize(stmt);
        return msgs;
    }

    bool DatabaseManager::clear_messages(const std::string& contact_uuid) {
        std::string sql = "DROP TABLE IF EXISTS \"" + contact_uuid + "\";";
        return execute_raw(sql);
    }

    bool DatabaseManager::get_user_by_name(const std::string& username, UserEntry& out_user) {
        std::lock_guard<std::mutex> lock(db_mutex);
        const char* sql = "SELECT uuid, display_name, password FROM users WHERE username = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            out_user.username = username;
            out_user.uuid = (const char*)sqlite3_column_text(stmt, 0);
            out_user.display_name = (const char*)sqlite3_column_text(stmt, 1);
            out_user.password = (const char*)sqlite3_column_text(stmt, 2);
            sqlite3_finalize(stmt); return true;
        }
        sqlite3_finalize(stmt); return false;
    }

    bool DatabaseManager::get_user(const std::string& uuid, UserEntry& out_user) {
        std::lock_guard<std::mutex> lock(db_mutex);
        const char* sql = "SELECT username, display_name, password FROM users WHERE uuid = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, uuid.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            out_user.uuid = uuid;
            out_user.username = (const char*)sqlite3_column_text(stmt, 0);
            out_user.display_name = (const char*)sqlite3_column_text(stmt, 1);
            out_user.password = (const char*)sqlite3_column_text(stmt, 2);
            sqlite3_finalize(stmt); return true;
        }
        sqlite3_finalize(stmt); return false;
    }

    bool DatabaseManager::upsert_user(const UserEntry& user) {
        std::lock_guard<std::mutex> lock(db_mutex);
        const char* sql = "INSERT OR REPLACE INTO users (uuid, username, display_name, password, last_seen, is_contact) VALUES (?, ?, ?, ?, ?, ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, user.uuid.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, user.username.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, user.display_name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, user.password.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 5, user.last_seen);
        sqlite3_bind_int(stmt, 6, user.is_contact ? 1 : 0);
        bool res = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt); return res;
    }

    bool DatabaseManager::get_pre_key_by_id(uint64_t key_id, PreKeyEntry& out_key) {
        std::lock_guard<std::mutex> lock(db_mutex);
        const char* sql = "SELECT pub_key, priv_key FROM pre_keys WHERE key_id = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_int64(stmt, 1, key_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            out_key.key_id = key_id;
            const uint8_t* p = (const uint8_t*)sqlite3_column_blob(stmt, 0);
            out_key.pub_key.assign(p, p + sqlite3_column_bytes(stmt, 0));
            const uint8_t* pr = (const uint8_t*)sqlite3_column_blob(stmt, 1);
            if (pr) out_key.priv_key.assign(pr, pr + sqlite3_column_bytes(stmt, 1));
            sqlite3_finalize(stmt); return true;
        }
        sqlite3_finalize(stmt); return false;
    }

    bool DatabaseManager::store_pre_key(const PreKeyEntry& key, bool is_server_side) {
        std::lock_guard<std::mutex> lock(db_mutex);
        const char* sql = "INSERT INTO pre_keys (owner_uuid, pub_key, priv_key) VALUES (?, ?, ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, key.owner_uuid.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_blob(stmt, 2, key.pub_key.data(), (int)key.pub_key.size(), SQLITE_STATIC);
        if (!is_server_side) sqlite3_bind_blob(stmt, 3, key.priv_key.data(), (int)key.priv_key.size(), SQLITE_STATIC);
        else sqlite3_bind_null(stmt, 3);
        bool res = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt); return res;
    }

    bool DatabaseManager::delete_pre_key(uint64_t key_id) {
        std::lock_guard<std::mutex> lock(db_mutex);
        std::string sql = "DELETE FROM pre_keys WHERE key_id = " + std::to_string(key_id) + ";";
        return execute_raw(sql);
    }

    bool DatabaseManager::get_one_pre_key(const std::string& owner_uuid, PreKeyEntry& out_key) {
        std::lock_guard<std::mutex> lock(db_mutex);
        const char* sql = "SELECT key_id, pub_key FROM pre_keys WHERE owner_uuid = ? LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, owner_uuid.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            out_key.key_id = sqlite3_column_int64(stmt, 0);
            const uint8_t* p = (const uint8_t*)sqlite3_column_blob(stmt, 1);
            out_key.pub_key.assign(p, p + sqlite3_column_bytes(stmt, 1));
            sqlite3_finalize(stmt); return true;
        }
        sqlite3_finalize(stmt); return false;
    }

    std::vector<UserEntry> DatabaseManager::list_all_users() {
        std::lock_guard<std::mutex> lock(db_mutex);
        std::vector<UserEntry> res;
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, "SELECT uuid, username FROM users;", -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                UserEntry u; u.uuid = (const char*)sqlite3_column_text(stmt, 0); u.username = (const char*)sqlite3_column_text(stmt, 1);
                res.push_back(u);
            }
        }
        sqlite3_finalize(stmt); return res;
    }

    bool DatabaseManager::wipe_all_data() { return execute_raw("DELETE FROM users; DELETE FROM pre_keys; DELETE FROM groups; DELETE FROM group_members;"); }
    
    bool DatabaseManager::store_offline_message(const MessageEntry& msg) {
        if (msg.target_uuid.empty()) return false;
        return store_message_dynamic(msg.target_uuid, msg);
    }

    std::vector<MessageEntry> DatabaseManager::fetch_and_delete_offline_messages(const std::string& target_uuid) {
        return fetch_all_from_table(target_uuid, true);
    }

    bool DatabaseManager::create_group(const GroupEntry& g) { 
        std::lock_guard<std::mutex> lock(db_mutex);
        const char* sql = "INSERT INTO groups (group_uuid, group_name, admin_uuid, created_at) VALUES (?, ?, ?, ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, g.group_uuid.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, g.group_name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, g.admin_uuid.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 4, g.created_at);
        bool res = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt); return res;
    }
    bool DatabaseManager::add_group_member(const std::string& g, const std::string& u) {
        std::lock_guard<std::mutex> lock(db_mutex);
        const char* sql = "INSERT OR IGNORE INTO group_members (group_uuid, user_uuid) VALUES (?, ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, g.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, u.c_str(), -1, SQLITE_STATIC);
        bool res = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt); return res;
    }
    std::vector<std::string> DatabaseManager::get_group_members(const std::string& g) {
        std::lock_guard<std::mutex> lock(db_mutex);
        std::vector<std::string> res;
        const char* sql = "SELECT user_uuid FROM group_members WHERE group_uuid = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, g.c_str(), -1, SQLITE_STATIC);
            while (sqlite3_step(stmt) == SQLITE_ROW) res.push_back((const char*)sqlite3_column_text(stmt, 0));
        }
        sqlite3_finalize(stmt); return res;
    }
    bool DatabaseManager::is_group_admin(const std::string& g, const std::string& u) {
        std::lock_guard<std::mutex> lock(db_mutex);
        const char* sql = "SELECT 1 FROM groups WHERE group_uuid = ? AND admin_uuid = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, g.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, u.c_str(), -1, SQLITE_STATIC);
        bool res = (sqlite3_step(stmt) == SQLITE_ROW);
        sqlite3_finalize(stmt); return res;
    }

    bool DatabaseManager::add_user_contact(const std::string& owner, const std::string& contact, const std::string& c_name) {
        std::lock_guard<std::mutex> lock(db_mutex);
        const char* sql = "INSERT OR IGNORE INTO user_contacts (owner_uuid, contact_uuid, contact_username) VALUES (?, ?, ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, owner.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, contact.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, c_name.c_str(), -1, SQLITE_STATIC);
        bool res = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt); return res;
    }

    std::vector<UserEntry> DatabaseManager::get_user_contacts(const std::string& owner) {
        std::lock_guard<std::mutex> lock(db_mutex);
        std::vector<UserEntry> res;
        const char* sql = "SELECT contact_uuid, contact_username FROM user_contacts WHERE owner_uuid = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, owner.c_str(), -1, SQLITE_STATIC);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                UserEntry u;
                u.uuid = (const char*)sqlite3_column_text(stmt, 0);
                u.username = (const char*)sqlite3_column_text(stmt, 1);
                u.display_name = u.username;
                res.push_back(u);
            }
        }
        sqlite3_finalize(stmt); return res;
    }

    void DatabaseManager::execute_sql(const std::string& s) {}
}

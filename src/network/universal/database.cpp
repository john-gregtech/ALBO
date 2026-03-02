#include "network/universal/database.h"
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
            std::cerr << "Database Error: " << error << "\n";
            return false;
        }
        return true;
    }

    bool DatabaseManager::initialize() {
        std::lock_guard<std::mutex> lock(db_mutex);
        const char* schema = R"(
            CREATE TABLE IF NOT EXISTS messages (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                sender_uuid TEXT NOT NULL,
                target_uuid TEXT NOT NULL,
                encrypted_payload BLOB NOT NULL,
                iv BLOB NOT NULL,
                timestamp INTEGER NOT NULL,
                is_read INTEGER DEFAULT 0
            );
            CREATE TABLE IF NOT EXISTS offline_messages (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                sender_uuid TEXT NOT NULL,
                target_uuid TEXT NOT NULL,
                encrypted_payload BLOB NOT NULL,
                iv BLOB NOT NULL,
                timestamp INTEGER NOT NULL
            );
            CREATE TABLE IF NOT EXISTS pre_keys (
                key_id INTEGER PRIMARY KEY AUTOINCREMENT,
                owner_uuid TEXT NOT NULL,
                pub_key BLOB NOT NULL,
                priv_key BLOB
            );
            CREATE TABLE IF NOT EXISTS users (
                uuid TEXT PRIMARY KEY,
                username TEXT UNIQUE NOT NULL,
                display_name TEXT,
                public_key_hex TEXT,
                password TEXT,
                last_seen INTEGER,
                is_contact INTEGER DEFAULT 0
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
        )";
        return execute_raw(schema);
    }

    bool DatabaseManager::store_message(const MessageEntry& msg) {
        std::lock_guard<std::mutex> lock(db_mutex);
        const char* sql = "INSERT INTO messages (sender_uuid, target_uuid, encrypted_payload, iv, timestamp, is_read) VALUES (?, ?, ?, ?, ?, ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, msg.sender_uuid.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, msg.target_uuid.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_blob(stmt, 3, msg.encrypted_payload.data(), (int)msg.encrypted_payload.size(), SQLITE_STATIC);
        sqlite3_bind_blob(stmt, 4, msg.iv.data(), 16, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 5, msg.timestamp);
        sqlite3_bind_int(stmt, 6, msg.is_read ? 1 : 0);
        bool res = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        return res;
    }

    bool DatabaseManager::store_offline_message(const MessageEntry& msg) {
        std::lock_guard<std::mutex> lock(db_mutex);
        const char* sql = "INSERT INTO offline_messages (sender_uuid, target_uuid, encrypted_payload, iv, timestamp) VALUES (?, ?, ?, ?, ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, msg.sender_uuid.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, msg.target_uuid.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_blob(stmt, 3, msg.encrypted_payload.data(), (int)msg.encrypted_payload.size(), SQLITE_STATIC);
        sqlite3_bind_blob(stmt, 4, msg.iv.data(), 16, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 5, msg.timestamp);
        bool res = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        return res;
    }

    std::vector<MessageEntry> DatabaseManager::fetch_and_delete_offline_messages(const std::string& target_uuid) {
        std::lock_guard<std::mutex> lock(db_mutex);
        std::vector<MessageEntry> msgs;
        const char* sql = "SELECT sender_uuid, target_uuid, encrypted_payload, iv, timestamp FROM offline_messages WHERE target_uuid = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return msgs;
        sqlite3_bind_text(stmt, 1, target_uuid.c_str(), -1, SQLITE_STATIC);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            MessageEntry m;
            m.sender_uuid = (const char*)sqlite3_column_text(stmt, 0);
            m.target_uuid = (const char*)sqlite3_column_text(stmt, 1);
            const uint8_t* p = (const uint8_t*)sqlite3_column_blob(stmt, 2);
            m.encrypted_payload.assign(p, p + sqlite3_column_bytes(stmt, 2));
            std::memcpy(m.iv.data(), sqlite3_column_blob(stmt, 3), 16);
            m.timestamp = sqlite3_column_int64(stmt, 4);
            msgs.push_back(std::move(m));
        }
        sqlite3_finalize(stmt);
        execute_raw("DELETE FROM offline_messages WHERE target_uuid = '" + target_uuid + "';");
        return msgs;
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
        sqlite3_finalize(stmt);
        return res;
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

    bool DatabaseManager::delete_pre_key(uint64_t key_id) {
        std::lock_guard<std::mutex> lock(db_mutex);
        std::string sql = "DELETE FROM pre_keys WHERE key_id = " + std::to_string(key_id) + ";";
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
        if (!res) std::cerr << "[DB Error] upsert_user FAILED: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_finalize(stmt); return res;
    }

    std::vector<UserEntry> DatabaseManager::list_all_users() {
        std::lock_guard<std::mutex> lock(db_mutex);
        std::vector<UserEntry> res;
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, "SELECT uuid, username, display_name FROM users;", -1, &stmt, nullptr) != SQLITE_OK) return res;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            UserEntry u; u.uuid = (const char*)sqlite3_column_text(stmt, 0);
            u.username = (const char*)sqlite3_column_text(stmt, 1);
            u.display_name = (const char*)sqlite3_column_text(stmt, 2);
            res.push_back(u);
        }
        sqlite3_finalize(stmt); return res;
    }

    std::vector<MessageEntry> DatabaseManager::get_messages_by_contact(const std::string& contact_uuid, int limit) {
        std::lock_guard<std::mutex> lock(db_mutex);
        std::vector<MessageEntry> res;
        const char* sql = "SELECT sender_uuid, target_uuid, encrypted_payload, timestamp FROM messages WHERE sender_uuid = ? OR target_uuid = ? ORDER BY timestamp DESC LIMIT ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return res;
        sqlite3_bind_text(stmt, 1, contact_uuid.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, contact_uuid.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, limit);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            MessageEntry m;
            m.sender_uuid = (const char*)sqlite3_column_text(stmt, 0);
            m.target_uuid = (const char*)sqlite3_column_text(stmt, 1);
            const uint8_t* p = (const uint8_t*)sqlite3_column_blob(stmt, 2);
            m.encrypted_payload.assign(p, p + sqlite3_column_bytes(stmt, 2));
            m.timestamp = sqlite3_column_int64(stmt, 3);
            res.push_back(m);
        }
        sqlite3_finalize(stmt); return res;
    }

    std::vector<MessageEntry> DatabaseManager::get_chat_history(const std::string& u1, const std::string& u2, int limit) {
        std::lock_guard<std::mutex> lock(db_mutex);
        std::vector<MessageEntry> res;
        const char* sql = "SELECT sender_uuid, target_uuid, encrypted_payload, timestamp FROM messages WHERE (sender_uuid = ? AND target_uuid = ?) OR (sender_uuid = ? AND target_uuid = ?) ORDER BY timestamp ASC LIMIT ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return res;
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
            res.push_back(m);
        }
        sqlite3_finalize(stmt); return res;
    }

    bool DatabaseManager::create_group(const GroupEntry& group) {
        std::lock_guard<std::mutex> lock(db_mutex);
        const char* sql = "INSERT INTO groups (group_uuid, group_name, admin_uuid, created_at) VALUES (?, ?, ?, ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, group.group_uuid.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, group.group_name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, group.admin_uuid.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 4, group.created_at);
        bool res = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        return res;
    }

    bool DatabaseManager::add_group_member(const std::string& group_uuid, const std::string& user_uuid) {
        std::lock_guard<std::mutex> lock(db_mutex);
        const char* sql = "INSERT OR IGNORE INTO group_members (group_uuid, user_uuid) VALUES (?, ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, group_uuid.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, user_uuid.c_str(), -1, SQLITE_STATIC);
        bool res = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        return res;
    }

    bool DatabaseManager::remove_group_member(const std::string& group_uuid, const std::string& user_uuid) {
        std::lock_guard<std::mutex> lock(db_mutex);
        const char* sql = "DELETE FROM group_members WHERE group_uuid = ? AND user_uuid = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, group_uuid.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, user_uuid.c_str(), -1, SQLITE_STATIC);
        bool res = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        return res;
    }

    std::vector<std::string> DatabaseManager::get_group_members(const std::string& group_uuid) {
        std::lock_guard<std::mutex> lock(db_mutex);
        std::vector<std::string> res;
        const char* sql = "SELECT user_uuid FROM group_members WHERE group_uuid = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return res;
        sqlite3_bind_text(stmt, 1, group_uuid.c_str(), -1, SQLITE_STATIC);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            res.push_back((const char*)sqlite3_column_text(stmt, 0));
        }
        sqlite3_finalize(stmt);
        return res;
    }

    std::vector<GroupEntry> DatabaseManager::get_user_groups(const std::string& user_uuid) {
        std::lock_guard<std::mutex> lock(db_mutex);
        std::vector<GroupEntry> res;
        const char* sql = "SELECT g.group_uuid, g.group_name, g.admin_uuid, g.created_at FROM groups g "
                          "JOIN group_members gm ON g.group_uuid = gm.group_uuid WHERE gm.user_uuid = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return res;
        sqlite3_bind_text(stmt, 1, user_uuid.c_str(), -1, SQLITE_STATIC);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            GroupEntry g;
            g.group_uuid = (const char*)sqlite3_column_text(stmt, 0);
            g.group_name = (const char*)sqlite3_column_text(stmt, 1);
            g.admin_uuid = (const char*)sqlite3_column_text(stmt, 2);
            g.created_at = sqlite3_column_int64(stmt, 3);
            res.push_back(g);
        }
        sqlite3_finalize(stmt);
        return res;
    }

    bool DatabaseManager::is_group_admin(const std::string& group_uuid, const std::string& user_uuid) {
        std::lock_guard<std::mutex> lock(db_mutex);
        const char* sql = "SELECT 1 FROM groups WHERE group_uuid = ? AND admin_uuid = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, group_uuid.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, user_uuid.c_str(), -1, SQLITE_STATIC);
        bool res = (sqlite3_step(stmt) == SQLITE_ROW);
        sqlite3_finalize(stmt);
        return res;
    }

    bool DatabaseManager::wipe_all_data() { return execute_raw("DELETE FROM users; DELETE FROM messages; DELETE FROM offline_messages; DELETE FROM pre_keys; DELETE FROM groups; DELETE FROM group_members;"); }
    bool DatabaseManager::clear_messages(const std::string& contact_uuid) { return execute_raw("DELETE FROM messages WHERE sender_uuid = '" + contact_uuid + "' OR target_uuid = '" + contact_uuid + "';"); }
    void DatabaseManager::execute_sql(const std::string& sql_command) {
        std::lock_guard<std::mutex> lock(db_mutex);
        char* err = nullptr;
        if (sqlite3_exec(db, sql_command.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
            std::cerr << "SQL Error: " << err << std::endl;
            sqlite3_free(err);
        }
    }
}

#include "network/universal/database.h"
#include <iostream>
#include <stdexcept>
#include <cstring>

namespace prototype::database {

    DatabaseManager::DatabaseManager(const std::string& path) : db_path(path) {
        if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
            std::string error = sqlite3_errmsg(db);
            sqlite3_close(db);
            throw std::runtime_error("Failed to open database: " + error);
        }
    }

    DatabaseManager::~DatabaseManager() {
        if (db) {
            sqlite3_close(db);
        }
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

            CREATE TABLE IF NOT EXISTS users (
                uuid TEXT PRIMARY KEY,
                display_name TEXT,
                public_key_hex TEXT,
                password TEXT,
                last_seen INTEGER,
                is_contact INTEGER DEFAULT 0,
                status_text TEXT
            );
        )";

        return execute_raw(schema);
    }

    bool DatabaseManager::store_message(const MessageEntry& msg) {
        std::lock_guard<std::mutex> lock(db_mutex);
        
        const char* sql = "INSERT INTO messages (sender_uuid, target_uuid, encrypted_payload, iv, timestamp, is_read) "
                          "VALUES (?, ?, ?, ?, ?, ?);";
        
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, msg.sender_uuid.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, msg.target_uuid.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_blob(stmt, 3, msg.encrypted_payload.data(), static_cast<int>(msg.encrypted_payload.size()), SQLITE_STATIC);
        sqlite3_bind_blob(stmt, 4, msg.iv.data(), 16, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 5, msg.timestamp);
        sqlite3_bind_int(stmt, 6, msg.is_read ? 1 : 0);

        bool success = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        return success;
    }

    std::vector<MessageEntry> DatabaseManager::get_messages_by_contact(const std::string& contact_uuid, int limit) {
        std::lock_guard<std::mutex> lock(db_mutex);
        std::vector<MessageEntry> results;

        const char* sql = "SELECT id, sender_uuid, target_uuid, encrypted_payload, iv, timestamp, is_read "
                          "FROM messages WHERE sender_uuid = ? OR target_uuid = ? "
                          "ORDER BY timestamp DESC LIMIT ?;";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return results;

        sqlite3_bind_text(stmt, 1, contact_uuid.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, contact_uuid.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, limit);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            MessageEntry entry;
            entry.id = sqlite3_column_int64(stmt, 0);
            entry.sender_uuid = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            entry.target_uuid = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));

            const uint8_t* p_ptr = static_cast<const uint8_t*>(sqlite3_column_blob(stmt, 3));
            entry.encrypted_payload.assign(p_ptr, p_ptr + sqlite3_column_bytes(stmt, 3));

            const uint8_t* iv_ptr = static_cast<const uint8_t*>(sqlite3_column_blob(stmt, 4));
            std::memcpy(entry.iv.data(), iv_ptr, 16);

            entry.timestamp = sqlite3_column_int64(stmt, 5);
            entry.is_read = (sqlite3_column_int(stmt, 6) != 0);
            results.push_back(std::move(entry));
        }
        sqlite3_finalize(stmt);
        return results;
    }

    std::vector<MessageEntry> DatabaseManager::get_chat_history(const std::string& u1, const std::string& u2, int limit) {
        std::lock_guard<std::mutex> lock(db_mutex);
        std::vector<MessageEntry> results;

        const char* sql = "SELECT id, sender_uuid, target_uuid, encrypted_payload, iv, timestamp, is_read "
                          "FROM messages WHERE (sender_uuid = ? AND target_uuid = ?) "
                          "OR (sender_uuid = ? AND target_uuid = ?) "
                          "ORDER BY timestamp ASC LIMIT ?;";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return results;

        sqlite3_bind_text(stmt, 1, u1.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, u2.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, u2.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, u1.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 5, limit);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            MessageEntry entry;
            entry.id = sqlite3_column_int64(stmt, 0);
            entry.sender_uuid = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            entry.target_uuid = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));

            const uint8_t* p_ptr = static_cast<const uint8_t*>(sqlite3_column_blob(stmt, 3));
            entry.encrypted_payload.assign(p_ptr, p_ptr + sqlite3_column_bytes(stmt, 3));

            const uint8_t* iv_ptr = static_cast<const uint8_t*>(sqlite3_column_blob(stmt, 4));
            std::memcpy(entry.iv.data(), iv_ptr, 16);

            entry.timestamp = sqlite3_column_int64(stmt, 5);
            entry.is_read = (sqlite3_column_int(stmt, 6) != 0);
            results.push_back(std::move(entry));
        }
        sqlite3_finalize(stmt);
        return results;
    }

    bool DatabaseManager::clear_messages(const std::string& contact_uuid) {
        std::lock_guard<std::mutex> lock(db_mutex);
        const char* sql = "DELETE FROM messages WHERE sender_uuid = ? OR target_uuid = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, contact_uuid.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, contact_uuid.c_str(), -1, SQLITE_STATIC);
        bool success = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        return success;
    }

    bool DatabaseManager::wipe_all_data() {
        std::lock_guard<std::mutex> lock(db_mutex);
        return execute_raw("DELETE FROM messages; DELETE FROM users;");
    }

    bool DatabaseManager::upsert_user(const UserEntry& user) {
        std::lock_guard<std::mutex> lock(db_mutex);
        const char* sql = "INSERT OR REPLACE INTO users (uuid, display_name, public_key_hex, password, last_seen, is_contact, status_text) "
                          "VALUES (?, ?, ?, ?, ?, ?, ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, user.uuid.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, user.display_name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, user.public_key_hex.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, user.password.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 5, user.last_seen);
        sqlite3_bind_int(stmt, 6, user.is_contact ? 1 : 0);
        sqlite3_bind_text(stmt, 7, user.status_text.c_str(), -1, SQLITE_STATIC);

        bool success = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        return success;
    }

    bool DatabaseManager::get_user(const std::string& uuid, UserEntry& out_user) {
        std::lock_guard<std::mutex> lock(db_mutex);
        const char* sql = "SELECT display_name, public_key_hex, last_seen, is_contact, status_text, password FROM users WHERE uuid = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, uuid.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            out_user.uuid = uuid;
            out_user.display_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            out_user.public_key_hex = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            out_user.last_seen = sqlite3_column_int64(stmt, 2);
            out_user.is_contact = (sqlite3_column_int(stmt, 3) != 0);
            out_user.status_text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            out_user.password = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            sqlite3_finalize(stmt);
            return true;
        }
        sqlite3_finalize(stmt);
        return false;
    }

    std::vector<UserEntry> DatabaseManager::list_all_users() {
        std::lock_guard<std::mutex> lock(db_mutex);
        std::vector<UserEntry> results;
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, "SELECT uuid, display_name, status_text FROM users;", -1, &stmt, nullptr) != SQLITE_OK) return results;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            UserEntry u;
            u.uuid = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            u.display_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            u.status_text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            results.push_back(u);
        }
        sqlite3_finalize(stmt);
        return results;
    }

    void DatabaseManager::execute_sql(const std::string& sql_command) {
        std::lock_guard<std::mutex> lock(db_mutex);
        char* errMsg = nullptr;
        if (sqlite3_exec(db, sql_command.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::cout << "Raw SQL error: " << errMsg << "\n";
            sqlite3_free(errMsg);
        }
    }
}

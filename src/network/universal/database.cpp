#include "network/universal/database.h"

namespace prototype::database {

    /*
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
            age INTEGER
        );
    )";
    char* errMsg = nullptr;

    if (sqlite3_exec(db, sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string error = errMsg;
        sqlite3_free(errMsg);
        throw std::runtime_error(error);
    }
    */

    void execute_sql(const std::string& db_file, const std::string& sql_command)
    {
        // Toggle debug/output at compile time
        constexpr bool debug = false;

        sqlite3* db = nullptr;

        if (sqlite3_open(db_file.c_str(), &db) != SQLITE_OK) {
            if constexpr (debug)
                std::cout << "Open error: " << sqlite3_errmsg(db) << "\n";
            sqlite3_close(db);
            return;
        }

        sqlite3_stmt* stmt = nullptr;

        if (sqlite3_prepare_v2(db, sql_command.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            if constexpr (debug)
                std::cout << "SQL error: " << sqlite3_errmsg(db) << "\n";
            sqlite3_close(db);
            return;
        }

        int column_count = sqlite3_column_count(stmt);
        int rc;
        bool has_rows = false;

        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            has_rows = true;

            if constexpr (debug) {
                for (int i = 0; i < column_count; ++i) {
                    const char* text =
                        reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));

                    std::cout << (text ? text : "NULL");

                    if (i < column_count - 1)
                        std::cout << " | ";
                }
                std::cout << "\n";
            }
        }

        if (rc != SQLITE_DONE) {
            if constexpr (debug)
                std::cout << "Execution error: " << sqlite3_errmsg(db) << "\n";
        }
        else if (!has_rows) {
            if constexpr (debug)
                std::cout << "OK\n";
        }

        sqlite3_finalize(stmt);
        sqlite3_close(db);
    }
}
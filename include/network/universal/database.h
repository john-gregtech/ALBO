#include <queue>
#include <vector>
#include <array>
#include <cctype>
#include <string>

#include <thread>
#include <mutex>
#include <atomic>

#include <sqlite3.h>

namespace prototype::database {

    class SqlJob { // <- sql job btwW
    private:
        bool isSanitized = false;
        bool isDone = false;
        std::string command{};
    public:
        SqlJob() = default;
        SqlJob& operator=(const SqlJob& other) {
            if (this != &other) {          // avoid self-assignment
                command = other.command;   // copy the command
                isDone = false;            // reset isDone
                isSanitized = other.isSanitized; // optional: copy sanitized state
            }
        return *this;
        }

        void sanatizeCommand();


    
    }

    class DatabaseManager {
    private:
        
    public:
        DatabaseManager();
        ~DatabaseManager();

        DatabaseManager(const DatabaseManager&) = delete;
        DatabaseManager& operator=(const DatabaseManager&) = delete;

        DatabaseManager(DatabaseManager&&) = delete;
        DatabaseManager& operator=(const DatabaseManager&&) = delete;
    }
    
    bool create_database(const char* name);
    bool table_exists(const char* name);
}
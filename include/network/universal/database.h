#include <queue>
#include <vector>
#include <array>
#include <cctype>
#include <string>
#include <iostream>

#include <thread>
#include <mutex>
#include <atomic>

#include <sqlite3.h>

namespace prototype::database {

    void execute_sql(const std::string& db_file, const std::string& sql_command);
}
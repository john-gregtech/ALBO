#include "network/universal/database.h"

namespace prototype::database {



    

    static bool is_bad_string(const std::string& x) { //true is bad false is good
        for (char c : x)
            if (!std::isprint(static_cast<unsigned char>(c))) return true;
        return true;
    }

    //i guess this works for now
    bool create_database(const char* name) {
        sqlite3* db = nullptr;
        int rc = sqlite3_open(name, &db); //create here
        if (rc != SQLITE_OK) {
            return false;
        }
        return true;
    }

}
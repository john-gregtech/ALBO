#include "debugging/debug.h"

namespace error {
    void log(const std::string& e, std::string end = "\n") {
        std::cerr << "LOG: " << e << end;
    }
    void warn(const std::string& e, std::string end = "\n") {
        std::cerr << "WARNING: " << e << end;
    }
    void error(const std::string& e, std::string end = "\n") {
        std::cerr << "ERROR: " << e << end;
    }
    void fatal(const std::string& e, std::string end = "\n") {
        std::cerr << "FATAL: " << e << end;
        std::abort();
    }

    
    // log, warning, error, fatal
}
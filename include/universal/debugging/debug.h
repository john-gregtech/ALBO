#include <iostream>
#include <cstdlib>
#include <string>



namespace error {
    void log(const std::string& e, std::string end = "\n");
    void warn(const std::string& e, std::string end = "\n");
    void error(const std::string& e, std::string end = "\n");
    void fatal(const std::string& e, std::string end = "\n");

    
    // log, warning, error, fatal
}
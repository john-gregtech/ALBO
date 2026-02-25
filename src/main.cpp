
#include <iostream>
#include <vector>
#include <string>

#include "network/universal/database.h"
#include "cryptowrapper/password.h"

int command_parse(const std::string& x) {
    prototype_functions::randomByteGen(32);
    size_t seperation{};
    // bool multi = false;

    if (x == "exit") return 0;
    
    for (size_t i{}; i < x.size(); ++i) {
        if (x.at(i) == ',') {
            seperation = i;
            // multi = true;
            break;
        }
    }
    if (seperation) {
        std::string parse(x.begin() + seperation + 2, x.end());
        std::string command(x.begin(), x.begin() + seperation);
        std::cout << parse << "\n";
        if (command == "sql") {
            prototype::database::execute_sql("test.db", parse);
        }
    }
    return 1;
}

int main() {
    std::string command{};

    while (true) {
        std::cout << "> ";
        std::getline(std::cin, command);

        if (command_parse(command) == 0) break;
    }
}
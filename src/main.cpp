#include <iostream>
#include <vector>
#include <array>
#include <sstream>
#include <stdexcept>
#include <bits/stdc++.h>
#include <iomanip>

#include "cryptowrapper/sha256.h"
#include "cryptowrapper/password.h"

template <unsigned int arrayLength> 
std::string hexarraytostring(const std::array<unsigned char, arrayLength>& data) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');

    for (unsigned int i = 0; i < arrayLength; ++i) {
        // Cast to int to print numeric value instead of character
        ss << std::setw(2) << static_cast<int>(data[i]);
    }

    return ss.str();
}
std::string hexvectortostring(const std::vector<unsigned char>& data) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');

    for (unsigned int i = 0; i < data.size(); ++i) {
        // Cast to int to print numeric value instead of character
        ss << std::setw(2) << static_cast<int>(data[i]);
    }

    return ss.str();
}
int main() {
    std::vector<unsigned char> my_pepper{}; 
    std::vector<unsigned char> my_salt{}; 
    std::vector<unsigned char> my_password{};
    my_pepper = prototype_functions::randomByteGen(10);
    my_salt = prototype_functions::randomByteGen(2);
    my_password = prototype_functions::randomByteGen(2);

    prototype_functions::generatePassword(my_password, my_salt, my_pepper);
    std::cout << hexvectortostring(my_pepper) << "\n";
    

    std::string message = "This is a handshake";
    std::getline(std::cin, message);
    std::string stringhash{};
    std::array<unsigned char, 32> hash{};

    hash = prototype_functions::sha256_hash(message);

    stringhash = hexarraytostring<32>(hash);
    std::cout << stringhash << std::endl;
}
#include <iostream>
#include <vector>
#include <array>
#include <sstream>
#include <stdexcept>
#include <iomanip>

#include "cryptowrapper/sha256.h"

template <unsigned int x>
std::string hexarraytostring(const std::array<unsigned char, x>& data) {
    std::stringstream ss;
    ss << std::hex;

    for (int i = 0; i < x; ++i) {
        ss << std::setw(2) << std::setfill('0') << data.at(i);
    }
    return ss.str();
}

int main() {
    std::string message = "This is a handshake";
    std::string stringhash{};
    std::array<unsigned char, 32> hash{};

    hash = prototype_functions::sha256_hash(message);

    stringhash = hexarraytostring<32>(hash);
    std::cout << stringhash << std::endl;
}
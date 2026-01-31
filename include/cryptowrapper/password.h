//This fileset will directly be used for salt and pepper plus sha256 on passwords

#pragma once
#include <openssl/rand.h>
#include <vector>
#include <array>
#include <iostream>

namespace prototype_functions {
    std::vector<uint8_t> randomByteGen(uint32_t randomLengthSize);
    std::vector<uint8_t> generatePassword(
        const std::vector<uint8_t>& password,
        const std::vector<uint8_t>& salt,
        const std::vector<uint8_t>& pepper 
    );
}

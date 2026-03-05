#pragma once
#include <openssl/evp.h>
#include <vector>
#include <array>
#include <string>
#include <iostream>

namespace prototype_functions {
    std::array<uint8_t, 32> sha256_hash(
        const std::string& input
    );
}
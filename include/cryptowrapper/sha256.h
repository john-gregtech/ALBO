#pragma once
#include <openssl/evp.h>
#include <vector>
#include <array>
#include <string>
#include <iostream>



namespace prototype_functions {
    std::array<unsigned char, 32> sha256_hash(
        const std::string& input
    );
}
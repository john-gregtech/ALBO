#pragma once
#include <openssl/sha.h>
#include <vector>
#include <array>
#include <string>
#include <iostream>
namespace prototype_functions {
    std::array<unsigned char, SHA256_DIGEST_LENGTH> sha256_hash(
        const std::string& input
    );
}
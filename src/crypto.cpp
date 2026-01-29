#include "cryptowrapper/crypto.h"

namespace prototype_functions {
    std::array<unsigned char, SHA256_DIGEST_LENGTH> sha256_hash(
        const std::string& input
    ) {
        std::array<unsigned char, SHA256_DIGEST_LENGTH> hash{};

        SHA256_CTX ctx;
        
        if (SHA256_Init(&ctx) != 1) {
            throw std::runtime_error("SHA256_Init has failed");
        }
        if (SHA256_Update(&ctx, input.data(), input.size()) != 1) {
            throw std::runtime_error("SHA256_Update has failed");
        }     
        if (SHA256_Final(hash.data(), &ctx) != 1) {
            throw std::runtime_error("SHA256_Final has failed");
        }

        return hash;
    }
}
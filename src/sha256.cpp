#include "cryptowrapper/sha256.h"

namespace prototype_functions {
    //OLD uses illedgidly decaricated sha256 library
    /*
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
    */
    std::array<unsigned char, 32> sha256_hash(
        const std::string& input
    ) {
        std::array<unsigned char, 32> hash{};
        unsigned int hash_len = 0;

        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        if (!ctx)
            throw std::runtime_error("Failed to create EVP_MD_CTX");

        if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
            EVP_MD_CTX_free(ctx);
            throw std::runtime_error("EVP_DigestInit_ex has failed");
        }

        if (EVP_DigestUpdate(ctx, input.data(), input.size()) != 1) {
            EVP_MD_CTX_free(ctx);
            throw std::runtime_error("EVP_DigestUpdate has failed");
        }

        if (EVP_DigestFinal_ex(ctx, hash.data(), &hash_len) != 1) {
            EVP_MD_CTX_free(ctx);
            throw std::runtime_error("EVP_DigestFinal_ex has failed");
        }

        if (hash_len != hash.size()) {
            EVP_MD_CTX_free(ctx);
            throw std::runtime_error("Unexpected hash length");
        }

        EVP_MD_CTX_free(ctx);

        return hash;
    }
}
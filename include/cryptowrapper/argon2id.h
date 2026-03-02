#pragma once
#include <vector>
#include <array>
#include <cstdint>
#include <string>

namespace prototype::cryptowrapper {

    struct Argon2idParams {
        uint32_t t_cost = 3;          // iterations
        uint32_t m_cost = 64 * 1024;  // 64 MB (in KB)
        uint32_t parallelism = 4;     // threads
        uint32_t salt_len = 16;
        uint32_t hash_len = 32;
    };

    struct Argon2idResult {
        std::vector<uint8_t> hash;
        std::vector<uint8_t> salt;
    };

    /**
     * @brief Hashes a password using Argon2id.
     * @return A struct containing the resulting hash and the random salt used.
     */
    Argon2idResult hash_password(const std::string& password, const Argon2idParams& params = Argon2idParams());

    /**
     * @brief Verifies a password against an existing hash and salt.
     */
    bool verify_password(const std::string& password, const std::vector<uint8_t>& hash, const std::vector<uint8_t>& salt, const Argon2idParams& params = Argon2idParams());

}
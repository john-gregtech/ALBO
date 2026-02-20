#include "cryptowrapper/argon2id.h"

std::vector<uint8_t> argonidhash(const std::vector<uint8_t>& input) {
    // ---- Parameters (safe defaults for testing) ----
    constexpr uint32_t t_cost = 2;          // iterations
    constexpr uint32_t m_cost = 1 << 16;    // 64 MB
    constexpr uint32_t parallelism = 1;     // threads
    constexpr size_t hash_len = 32;         // 256-bit output
    constexpr size_t salt_len = 16;

    // ---- Generate cryptographically secure salt ----
    std::vector<uint8_t> salt(salt_len);

    if (RAND_bytes(salt.data(), static_cast<int>(salt.size())) != 1) {
        throw std::runtime_error("OpenSSL RAND_bytes failed");
    }

    std::vector<uint8_t> hash(hash_len);

    int result = argon2id_hash_raw(
        t_cost,
        m_cost,
        parallelism,
        input.data(),
        input.size(),
        salt.data(),
        salt.size(),
        hash.data(),
        hash.size()
    );

    if (result != ARGON2_OK) {
        throw std::runtime_error(argon2_error_message(result));
    }

    return hash;
}
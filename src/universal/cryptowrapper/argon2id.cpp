#include "universal/cryptowrapper/argon2id.h"
#include "universal/cryptowrapper/secure_mem.h"
#include <argon2.h>
#include <openssl/rand.h>
#include <stdexcept>
#include <iostream>

namespace prototype::cryptowrapper {

    Argon2idResult hash_password(const std::string& password, const Argon2idParams& params) {
        Argon2idResult result;
        result.salt.resize(params.salt_len);

        // Generate a random salt
        if (RAND_bytes(result.salt.data(), static_cast<int>(result.salt.size())) != 1) {
            throw std::runtime_error("OpenSSL RAND_bytes failed for salt generation");
        }

        result.hash.resize(params.hash_len);

        // Compute Argon2id hash
        int rc = argon2id_hash_raw(
            params.t_cost,
            params.m_cost,
            params.parallelism,
            password.data(),
            password.size(),
            result.salt.data(),
            result.salt.size(),
            result.hash.data(),
            result.hash.size()
        );

        if (rc != ARGON2_OK) {
            throw std::runtime_error(std::string("Argon2id hashing failed: ") + argon2_error_message(rc));
        }

        return result;
    }

    bool verify_password(const std::string& password, const std::vector<uint8_t>& hash, const std::vector<uint8_t>& salt, const Argon2idParams& params) {
        std::vector<uint8_t> verification_hash(hash.size());

        // Recompute Argon2id hash with the given salt and parameters
        int rc = argon2id_hash_raw(
            params.t_cost,
            params.m_cost,
            params.parallelism,
            password.data(),
            password.size(),
            salt.data(),
            salt.size(),
            verification_hash.data(),
            verification_hash.size()
        );

        if (rc != ARGON2_OK) {
            return false;
        }

        // Constant-time comparison to prevent timing attacks
        bool matches = (hash.size() == verification_hash.size()) &&
                       (CRYPTO_memcmp(hash.data(), verification_hash.data(), hash.size()) == 0);

        // Securely wipe the temporary hash buffer
        secure_erase(verification_hash.data(), verification_hash.size());

        return matches;
    }

}

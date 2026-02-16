#include <iostream>
#include <string>

extern "C" {
#include <argon2.h>
}

int main()
{
    const std::string password = "testpassword";

    // Argon2 parameters
    const uint32_t t_cost = 2;          // iterations
    const uint32_t m_cost = 1 << 16;    // 64 MB
    const uint32_t parallelism = 1;
    const size_t salt_len = 16;
    const size_t hash_len = 32;

    
    uint8_t salt[salt_len] = {0}; // quick & dirty zero salt (DO NOT use in production)

    char encoded[128];

    int result = argon2id_hash_encoded(
        t_cost,
        m_cost,
        parallelism,
        password.c_str(),
        password.size(),
        salt,
        salt_len,
        hash_len,
        encoded,
        sizeof(encoded)
    );

    if (result != ARGON2_OK) {
        std::cerr << "Hashing failed: "
                  << argon2_error_message(result) << "\n";
        return 1;
    }

    std::cout << "Encoded hash:\n" << encoded << "\n";

    // Verify
    int verify = argon2id_verify(
        encoded,
        password.c_str(),
        password.size()
    );

    if (verify == ARGON2_OK)
        std::cout << "Verification successful.\n";
    else
        std::cout << "Verification failed.\n";

    return 0;
}
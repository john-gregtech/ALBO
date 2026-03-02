#pragma once
#include <array>
#include <vector>
#include <cstdint>
#include <string>

namespace prototype::cryptowrapper {

    struct Ed25519KeyPair {
        std::array<uint8_t, 32> priv{};
        std::array<uint8_t, 32> pub{};
    };

    /**
     * @brief Generates a long-term Ed25519 identity keypair.
     */
    Ed25519KeyPair generate_ed25519_keypair();

    /**
     * @brief Signs a message (or challenge) using an Ed25519 private key.
     * @return A 64-byte digital signature.
     */
    std::array<uint8_t, 64> sign_message(const std::vector<uint8_t>& message, const std::array<uint8_t, 32>& priv_key);

    /**
     * @brief Verifies an Ed25519 signature against a message and public key.
     */
    bool verify_signature(const std::vector<uint8_t>& message, const std::array<uint8_t, 64>& signature, const std::array<uint8_t, 32>& pub_key);

}

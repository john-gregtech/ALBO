#pragma once

#include <array>
#include <vector>
#include <cstdint>
#include <memory>

namespace prototype::cryptowrapper {
    struct X25519KeyPair {
        std::array<uint8_t, 32> priv{};
        std::array<uint8_t, 32> pub{};
    };

    /**
     * @brief Generates a fresh ephemeral X25519 keypair for an echemeral handshake.
     */
    X25519KeyPair generate_x25519_keypair();
    
    /**
     * @brief Computes the shared secret (32 bytes) between a local private key and a remote public key.
     */
    std::array<uint8_t, 32> compute_shared_secret(
        const std::array<uint8_t, 32>& my_priv,
        const std::array<uint8_t, 32>& peer_pub
    );
}
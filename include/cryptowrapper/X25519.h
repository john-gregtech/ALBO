#pragma once

#include <array>
#include <vector>
#include <string>
#include <iostream>
#include <openssl/kdf.h>
#include <openssl/evp.h>

namespace prototype_functions {
    struct X25519KeyPair {
        std::array<uint8_t, 32> priv{};
        std::array<uint8_t, 32> pub{};
    };

    X25519KeyPair x25519_generate_keypair();
    
    std::array<uint8_t, 32> x25519_shared_secret(
        const std::array<uint8_t, 32>& my_priv,
        const std::array<uint8_t, 32>& peer_pub
    );
    
}
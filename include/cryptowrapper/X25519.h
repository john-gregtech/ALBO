#pragma once

#include <array>
#include <vector>
#include <string>
#include <iostream>

#include <openssl/kdf.h>
#include <openssl/evp.h>

namespace prototype_functions {
    struct X25519KeyPair {
        std::array<unsigned char, 32> priv{};
        std::array<unsigned char, 32> pub{};
    };

    X25519KeyPair x25519_generate_keypair();
    
    std::array<unsigned char, 32> x25519_shared_secret(
        const std::array<unsigned char, 32>& my_priv,
        const std::array<unsigned char, 32>& peer_pub
    );
    
}
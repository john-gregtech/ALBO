#include "cryptowrapper/X25519.h"
#include "cryptowrapper/secure_mem.h"
#include <openssl/evp.h>
#include <stdexcept>
#include <cstring>

namespace prototype::cryptowrapper {

    X25519KeyPair generate_x25519_keypair() {
        X25519KeyPair pair;
        EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, nullptr);
        if (!ctx) throw std::runtime_error("Failed to create X25519 context");

        EVP_PKEY* pkey = nullptr;
        if (EVP_PKEY_keygen_init(ctx) <= 0 || EVP_PKEY_keygen(ctx, &pkey) <= 0) {
            EVP_PKEY_CTX_free(ctx);
            throw std::runtime_error("Failed to generate X25519 key");
        }

        size_t len = 32;
        EVP_PKEY_get_raw_private_key(pkey, pair.priv.data(), &len);
        EVP_PKEY_get_raw_public_key(pkey, pair.pub.data(), &len);

        EVP_PKEY_free(pkey);
        EVP_PKEY_CTX_free(ctx);
        return pair;
    }

    std::array<uint8_t, 32> compute_shared_secret(const std::array<uint8_t, 32>& my_priv, const std::array<uint8_t, 32>& peer_pub) {
        std::array<uint8_t, 32> secret{};
        
        // Load private key
        EVP_PKEY* my_pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, nullptr, my_priv.data(), 32);
        // Load peer's public key
        EVP_PKEY* their_pkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, nullptr, peer_pub.data(), 32);

        if (!my_pkey || !their_pkey) {
            if (my_pkey) EVP_PKEY_free(my_pkey);
            if (their_pkey) EVP_PKEY_free(their_pkey);
            throw std::runtime_error("Failed to load X25519 keys for secret computation");
        }

        EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(my_pkey, nullptr);
        size_t secret_len = 32;
        if (EVP_PKEY_derive_init(ctx) <= 0 ||
            EVP_PKEY_derive_set_peer(ctx, their_pkey) <= 0 ||
            EVP_PKEY_derive(ctx, secret.data(), &secret_len) <= 0) {
            
            EVP_PKEY_CTX_free(ctx);
            EVP_PKEY_free(my_pkey);
            EVP_PKEY_free(their_pkey);
            throw std::runtime_error("Failed to derive X25519 shared secret");
        }

        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(my_pkey);
        EVP_PKEY_free(their_pkey);
        return secret;
    }
}

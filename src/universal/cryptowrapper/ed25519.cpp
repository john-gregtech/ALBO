#include "universal/cryptowrapper/ed25519.h"
#include <openssl/evp.h>
#include <stdexcept>

namespace prototype::cryptowrapper {

    Ed25519KeyPair generate_ed25519_keypair() {
        Ed25519KeyPair pair;
        EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr);
        if (!ctx) throw std::runtime_error("Failed to create Ed25519 context");

        EVP_PKEY* pkey = nullptr;
        if (EVP_PKEY_keygen_init(ctx) <= 0 || EVP_PKEY_keygen(ctx, &pkey) <= 0) {
            EVP_PKEY_CTX_free(ctx);
            throw std::runtime_error("Failed to generate Ed25519 key");
        }

        size_t len = 32;
        EVP_PKEY_get_raw_private_key(pkey, pair.priv.data(), &len);
        EVP_PKEY_get_raw_public_key(pkey, pair.pub.data(), &len);

        EVP_PKEY_free(pkey);
        EVP_PKEY_CTX_free(ctx);
        return pair;
    }

    std::array<uint8_t, 64> sign_message(const std::vector<uint8_t>& message, const std::array<uint8_t, 32>& priv_key) {
        std::array<uint8_t, 64> signature{};
        EVP_PKEY* pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, priv_key.data(), 32);
        if (!pkey) throw std::runtime_error("Failed to load Ed25519 private key");

        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        size_t sig_len = 64;

        if (EVP_DigestSignInit(ctx, nullptr, nullptr, nullptr, pkey) <= 0 ||
            EVP_DigestSign(ctx, signature.data(), &sig_len, message.data(), message.size()) <= 0) {
            EVP_MD_CTX_free(ctx);
            EVP_PKEY_free(pkey);
            throw std::runtime_error("Ed25519 signing failed");
        }

        EVP_MD_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return signature;
    }

    bool verify_signature(const std::vector<uint8_t>& message, const std::array<uint8_t, 64>& signature, const std::array<uint8_t, 32>& pub_key) {
        EVP_PKEY* pkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, pub_key.data(), 32);
        if (!pkey) return false;

        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        bool result = (EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, pkey) > 0 &&
                       EVP_DigestVerify(ctx, signature.data(), 64, message.data(), message.size()) > 0);

        EVP_MD_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return result;
    }
}

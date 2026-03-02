#include "cryptowrapper/aes256.h"
#include <stdexcept>
#include <cstring>

namespace prototype_functions {

    std::array<uint8_t, 16> generate_initialization_vector() {
        std::array<uint8_t, 16> iv{};
        // AES-256-GCM standard IV length is 12, but 16 is acceptable
        if (RAND_bytes(iv.data(), 16) != 1) 
            throw std::runtime_error("RAND_bytes for IV generation failed");
        return iv;
    }
    std::array<uint8_t, 32> generate_key() {
        std::array<uint8_t, 32> key{};
        
        if (RAND_bytes(key.data(), 32) != 1)
            throw std::runtime_error("RAND_bytes for key generation failed");
        return key;
    }

    std::vector<uint8_t> aes_encrypt(
        const std::vector<uint8_t>& plaintext, 
        const std::array<uint8_t, 32>& key,
        const std::array<uint8_t, 16>& iv
    ) {
        // Output format: [Ciphertext][16-byte Authentication Tag]
        std::vector<uint8_t> ciphertext(plaintext.size() + 16);
        
        EvpCtxPtr ctx(EVP_CIPHER_CTX_new());
        if (!ctx)
            throw std::runtime_error("EVP_CIPHER_CTX_new has failed");
        
        int len = 0;
        int ciphertext_len = 0;

        // Using GCM for Authenticated Encryption
        if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) {
            throw std::runtime_error("EVP_EncryptInit_ex (GCM) failed");
        }

        // Set IV length (default is 12, we provide 16 for backward compatibility with signature)
        if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, iv.size(), NULL) != 1) {
            throw std::runtime_error("Failed to set IV length for GCM");
        }

        if (EVP_EncryptInit_ex(ctx.get(), NULL, NULL, key.data(), iv.data()) != 1) {
            throw std::runtime_error("EVP_EncryptInit_ex (Key/IV) failed");
        }

        if (EVP_EncryptUpdate(ctx.get(), ciphertext.data(), &len, plaintext.data(), plaintext.size()) != 1) {
            throw std::runtime_error("EVP_EncryptUpdate failed");
        }
        ciphertext_len = len;

        // Finalize encryption (no padding needed for GCM)
        if (EVP_EncryptFinal_ex(ctx.get(), ciphertext.data() + len, &len) != 1) {
            throw std::runtime_error("EVP_EncryptFinal failed");
        }
        ciphertext_len += len;

        // Get authentication tag (16 bytes)
        std::array<uint8_t, 16> tag{};
        if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, 16, tag.data()) != 1) {
            throw std::runtime_error("Failed to retrieve authentication tag");
        }

        // Append tag to ciphertext
        std::memcpy(ciphertext.data() + ciphertext_len, tag.data(), 16);
        ciphertext.resize(ciphertext_len + 16);
        
        return ciphertext;
    }

    std::vector<uint8_t> aes_decrypt(
        const std::vector<uint8_t>& ciphertext,
        const std::array<uint8_t, 32>& key,
        const std::array<uint8_t, 16>& iv
    ) {
        if (ciphertext.size() < 16) {
            throw std::runtime_error("Ciphertext too short (missing GCM tag)");
        }

        size_t actual_ciphertext_len = ciphertext.size() - 16;
        std::vector<uint8_t> plaintext(actual_ciphertext_len);

        EvpCtxPtr ctx(EVP_CIPHER_CTX_new());
        if (!ctx) 
            throw std::runtime_error("EVP_CIPHER_CTX_new has failed");

        int len = 0;
        int plaintext_len = 0;

        if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) {
            throw std::runtime_error("EVP_DecryptInit_ex (GCM) failed");
        }

        if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, iv.size(), NULL) != 1) {
            throw std::runtime_error("Failed to set IV length for GCM");
        }

        if (EVP_DecryptInit_ex(ctx.get(), NULL, NULL, key.data(), iv.data()) != 1) {
            throw std::runtime_error("EVP_DecryptInit_ex (Key/IV) failed");
        }

        if (EVP_DecryptUpdate(ctx.get(), plaintext.data(), &len, ciphertext.data(), actual_ciphertext_len) != 1) {
            throw std::runtime_error("EVP_DecryptUpdate failed");
        }
        plaintext_len = len;

        // Set the expected tag (last 16 bytes of ciphertext)
        if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, 16, const_cast<uint8_t*>(ciphertext.data() + actual_ciphertext_len)) != 1) {
            throw std::runtime_error("Failed to set authentication tag for verification");
        }

        // Finalize decryption (checks tag integrity)
        if (EVP_DecryptFinal_ex(ctx.get(), plaintext.data() + len, &len) <= 0) {
            // Authentication failed - secure the memory immediately
            prototype::cryptowrapper::secure_erase(plaintext.data(), plaintext.size());
            throw std::runtime_error("Authentication failed: Data may be corrupted or key/IV is incorrect");
        }
        plaintext_len += len;
        plaintext.resize(plaintext_len);

        return plaintext;
    }
    
    //This function is ai generated by OpenAI chatgpt and will be removed
    void openssl_sanity_check()
    {
        std::cout << "OpenSSL version (macro): "
                << OPENSSL_VERSION_TEXT << '\n';

        std::cout << "OpenSSL version (runtime): "
                << OpenSSL_version(OPENSSL_VERSION) << '\n';
    }
}
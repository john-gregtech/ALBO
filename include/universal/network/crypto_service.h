#pragma once
#include <vector>
#include <string>
#include <array>
#include "universal/network/packet.h"
#include "universal/network/database.h"

namespace prototype::network {

    class CryptoService {
    private:
        prototype::database::DatabaseManager* local_db;

    public:
        explicit CryptoService(prototype::database::DatabaseManager* db);

        // Generates and saves a batch of pre-keys
        std::vector<uint8_t> generate_prekey_batch(const std::string& my_uuid, int count = 10);

        // Encrypts a message using a fetched pre-key
        RawPacket encrypt_message(const std::string& text, uint64_t key_id, const std::array<uint8_t, 32>& target_pub);

        // Decrypts an incoming message packet
        std::string decrypt_packet(const RawPacket& packet);
    };

}

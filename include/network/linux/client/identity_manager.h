#pragma once
#include <string>
#include <vector>
#include <memory>
#include "cryptowrapper/ed25519.h"
#include "network/universal/database.h"

namespace prototype::network {

    class IdentityManager {
    private:
        prototype::cryptowrapper::Ed25519KeyPair identity_keys;
        prototype::database::DatabaseManager* local_db;

    public:
        explicit IdentityManager(prototype::database::DatabaseManager* db);

        bool load_or_generate();
        std::array<uint8_t, 32> get_public_key() const { return identity_keys.pub; }
        std::array<uint8_t, 32> get_private_key() const { return identity_keys.priv; }
    };

}

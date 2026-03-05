#include "universal/network/identity_manager.h"
#include "universal/config.h"

namespace prototype::network {

    IdentityManager::IdentityManager(prototype::database::DatabaseManager* db) : local_db(db) {}

    bool IdentityManager::load_or_generate() {
        // In a real version, we'd query the DB for existing keys
        // For this preview, we always generate a fresh one
        identity_keys = prototype::cryptowrapper::generate_ed25519_keypair();
        ALBO_LOG("Client Identity Keypair Ready.");
        return true;
    }

}

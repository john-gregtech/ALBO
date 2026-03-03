#include "network/linux/client/crypto_service.h"
#include "cryptowrapper/X25519.h"
#include "cryptowrapper/aes256.h"
#include "cryptowrapper/sha256.h"
#include "config.h"
#include <cstring>

namespace prototype::network {

    CryptoService::CryptoService(prototype::database::DatabaseManager* db) : local_db(db) {}

    std::vector<uint8_t> CryptoService::generate_prekey_batch(const std::string& my_uuid, int count) {
        std::vector<uint8_t> upload_payload;
        for(int i=0; i<count; ++i) {
            auto pair = prototype::cryptowrapper::generate_x25519_keypair();
            prototype::database::PreKeyEntry e;
            e.owner_uuid = my_uuid;
            e.pub_key.assign(pair.pub.begin(), pair.pub.end());
            e.priv_key.assign(pair.priv.begin(), pair.priv.end());
            local_db->store_pre_key(e, false);
            upload_payload.insert(upload_payload.end(), pair.pub.begin(), pair.pub.end());
        }
        return upload_payload;
    }

    RawPacket CryptoService::encrypt_message(const std::string& text, uint64_t key_id, const std::array<uint8_t, 32>& target_pub) {
        auto my_eph = prototype::cryptowrapper::generate_x25519_keypair();
        auto secret = prototype::cryptowrapper::compute_shared_secret(my_eph.priv, target_pub);
        auto aes_k = prototype_functions::sha256_hash(std::string(secret.begin(), secret.end()));
        auto iv = prototype_functions::generate_initialization_vector();
        auto ct = prototype_functions::aes_encrypt(std::vector<uint8_t>(text.begin(), text.end()), aes_k, iv);

        RawPacket msg;
        msg.header.type = PacketType::MESSAGE_DATA;
        msg.payload.resize(8 + 32 + 16 + ct.size());
        std::memcpy(msg.payload.data(), &key_id, 8);
        std::memcpy(msg.payload.data() + 8, my_eph.pub.data(), 32);
        std::memcpy(msg.payload.data() + 40, iv.data(), 16);
        std::memcpy(msg.payload.data() + 56, ct.data(), ct.size());
        msg.header.payload_size = msg.payload.size();
        return msg;
    }

    std::string CryptoService::decrypt_packet(const RawPacket& packet) {
        if (packet.payload.size() < 56) return std::string(packet.payload.begin(), packet.payload.end());
        uint64_t key_id; std::memcpy(&key_id, packet.payload.data(), 8);
        std::array<uint8_t, 32> eph_pub; std::memcpy(eph_pub.data(), packet.payload.data() + 8, 32);
        std::array<uint8_t, 16> iv; std::memcpy(iv.data(), packet.payload.data() + 40, 16);
        std::vector<uint8_t> ct(packet.payload.begin() + 56, packet.payload.end());

        prototype::database::PreKeyEntry my_key;
        if (local_db->get_pre_key_by_id(key_id, my_key)) {
            std::array<uint8_t, 32> my_priv;
            std::copy(my_key.priv_key.begin(), my_key.priv_key.end(), my_priv.begin());
            auto sec = prototype::cryptowrapper::compute_shared_secret(my_priv, eph_pub);
            auto aes_k = prototype_functions::sha256_hash(std::string(sec.begin(), sec.end()));
            auto pt = prototype_functions::aes_decrypt(ct, aes_k, iv);
            local_db->delete_pre_key(key_id);
            return std::string(pt.begin(), pt.end());
        }
        return std::string(packet.payload.begin(), packet.payload.end());
    }

}

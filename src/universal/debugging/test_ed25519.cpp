#include <iostream>
#include <vector>
#include <string>
#include "universal/cryptowrapper/ed25519.h"
#include "universal/network/hex_utils.h"

int main() {
    using namespace prototype::cryptowrapper;
    using namespace prototype::network;

    std::cout << "--- Ed25519 Sanity Test ---" << std::endl;

    // 1. Generate Keypair
    auto pair = generate_ed25519_keypair();
    std::cout << "[Step 1] Keypair Generated." << std::endl;
    std::cout << "  Pub:  " << to_hex(pair.pub.data(), 32) << std::endl;

    // 2. Sign a Message
    std::string msg_str = "ALBO Identity Challenge 12345";
    std::vector<uint8_t> msg(msg_str.begin(), msg_str.end());
    auto sig = sign_message(msg, pair.priv);
    std::cout << "[Step 2] Message Signed." << std::endl;
    std::cout << "  Sig:  " << to_hex(sig.data(), 64) << std::endl;

    // 3. Verify Signature
    bool ok = verify_signature(msg, sig, pair.pub);
    std::cout << "[Step 3] Verification: " << (ok ? "PASSED" : "FAILED") << std::endl;

    // 4. Test Tamper Resistance
    msg[0] ^= 0xFF; // Tamper with the message
    bool tamper_ok = verify_signature(msg, sig, pair.pub);
    std::cout << "[Step 4] Tamper Test: " << (!tamper_ok ? "PASSED (Detection Successful)" : "FAILED (Tamper missed!)") << std::endl;

    if (ok && !tamper_ok) {
        std::cout << "\nALL ED25519 TESTS PASSED." << std::endl;
        return 0;
    }
    return 1;
}

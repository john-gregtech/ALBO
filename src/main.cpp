#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>

#include "cryptowrapper/aes256.h"
#include "cryptowrapper/sha256.h"

// Helper to convert bytes to a hex string (e.g., 0xABC123)
std::string to_hex(const uint8_t* data, size_t len) {
    std::stringstream ss;
    ss << "0x";
    for (size_t i = 0; i < len; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)data[i];
    }
    return ss.str();
}

std::string to_hex(const std::vector<uint8_t>& data) {
    return to_hex(data.data(), data.size());
}

// Helper to convert hex string (with or without 0x) back to bytes
std::vector<uint8_t> from_hex(std::string hex) {
    if (hex.size() >= 2 && hex.substr(0, 2) == "0x") {
        hex = hex.substr(2);
    }
    
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        uint8_t byte = (uint8_t)strtol(byteString.c_str(), nullptr, 16);
        bytes.push_back(byte);
    }
    return bytes;
}

void print_help() {
    std::cout << "\n--- AES-256-GCM Test Utility ---\n";
    std::cout << "Commands:\n";
    std::cout << "  keygen          - Generate a random 32-byte key and 16-byte IV\n";
    std::cout << "  encrypt <text>  - Encrypt text (requires prior keygen or manual set)\n";
    std::cout << "  decrypt <hex>   - Decrypt hex ciphertext (requires prior keygen/set)\n";
    std::cout << "  setkey <hex>    - Manually set the 32-byte hex key\n";
    std::cout << "  setiv <hex>     - Manually set the 16-byte hex IV\n";
    std::cout << "  exit            - Close the program\n";
    std::cout << "--------------------------------\n";
}

int main() {
    std::string line;
    std::array<uint8_t, 32> current_key = {0};
    std::array<uint8_t, 16> current_iv = {0};
    bool key_ready = false;

    print_help();

    while (true) {
        std::cout << "\nALBO-TEST> ";
        if (!std::getline(std::cin, line) || line == "exit") break;
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string cmd;
        ss >> cmd;

        try {
            if (cmd == "keygen") {
                current_key = prototype_functions::generate_key();
                current_iv = prototype_functions::generate_initialization_vector();
                std::cout << "Generated Key: " << to_hex(current_key.data(), 32) << "\n";
                std::cout << "Generated IV:  " << to_hex(current_iv.data(), 16) << "\n";
                key_ready = true;
            } 
            else if (cmd == "setkey") {
                std::string hex; ss >> hex;
                auto bytes = from_hex(hex);
                if (bytes.size() != 32) {
                    std::cout << "Error: Key must be 32 bytes (64 hex chars)\n";
                } else {
                    std::copy(bytes.begin(), bytes.end(), current_key.begin());
                    std::cout << "Key updated.\n";
                    key_ready = true;
                }
            }
            else if (cmd == "setiv") {
                std::string hex; ss >> hex;
                auto bytes = from_hex(hex);
                if (bytes.size() != 16) {
                    std::cout << "Error: IV must be 16 bytes (32 hex chars)\n";
                } else {
                    std::copy(bytes.begin(), bytes.end(), current_iv.begin());
                    std::cout << "IV updated.\n";
                }
            }
            else if (cmd == "encrypt") {
                if (!key_ready) {
                    std::cout << "Error: Generate or set a key first.\n";
                    continue;
                }
                std::string text;
                std::getline(ss >> std::ws, text); // Read the rest of the line
                
                std::vector<uint8_t> pt(text.begin(), text.end());
                auto ct = prototype_functions::aes_encrypt(pt, current_key, current_iv);
                
                std::cout << "Ciphertext (GCM): " << to_hex(ct) << "\n";
                std::cout << "(Note: Last 16 bytes are the Auth Tag)\n";
            }
            else if (cmd == "decrypt") {
                if (!key_ready) {
                    std::cout << "Error: Generate or set a key first.\n";
                    continue;
                }
                std::string hex; ss >> hex;
                auto ct = from_hex(hex);
                
                auto pt = prototype_functions::aes_decrypt(ct, current_key, current_iv);
                std::string result(pt.begin(), pt.end());
                std::cout << "Decrypted Text: " << result << "\n";
            }
            else if (cmd == "help") {
                print_help();
            }
            else {
                std::cout << "Unknown command. Type 'help' for options.\n";
            }
        } catch (const std::exception& e) {
            std::cout << "CRITICAL ERROR: " << e.what() << "\n";
        }
    }

    return 0;
}

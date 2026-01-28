#include <iostream>
#include <array>
#include <sstream>
#include <vector>
#include <stdexcept>
#include <string>
#include <limits>
#include <iomanip>

#include "cryptowrapper/aes256.h"

void print_hex(const std::vector<unsigned char>& value) {
    for (unsigned char i : value) {
        std::cout << int(i) << ' ';
    }
    std::cout << "\n"; 
}
void print_key(const std::array<unsigned char, 32>& value) {
    for (unsigned char i : value) {
        std::cout << int(i) << ' ';
    }
    std::cout << "\n"; 
}
void print_iv(const std::array<unsigned char, 16>& value) {
    for (unsigned char i : value) {
        std::cout << int(i) << ' ';
    }
    std::cout << "\n"; 
}
std::string userInput() {
    std::string x;
    std::getline(std::cin, x);
    return x;
}
std::array<unsigned char, 32> input_key() {
    std::array<unsigned char, 32> key{};
    std::string line;

    std::cout << "Enter 32 numbers for the key (space-separated, 0-255):\n";
    std::getline(std::cin, line);
    std::istringstream iss(line);
    std::vector<int> numbers;
    int n;

    while (iss >> n) {
        if (n < 0 || n > 255)
            throw std::runtime_error("Numbers must be between 0 and 255");
        numbers.push_back(n);
    }

    if (numbers.size() != 32)
        throw std::runtime_error("You must enter exactly 32 numbers");

    for (size_t i = 0; i < 32; ++i) {
        key[i] = static_cast<unsigned char>(numbers[i]);
    }

    return key;
}

//everything under this is ai slop

// Convert vector of bytes to hex string
std::string to_hex_string(const std::vector<unsigned char>& data) {
    std::ostringstream oss;
    for (auto b : data)
        oss << std::hex << std::setw(2) << std::setfill('0') << int(b);
    return oss.str();
}

// Convert hex string to vector of bytes
std::vector<unsigned char> hex_to_bytes(const std::string& hex) {
    if (hex.size() % 2 != 0)
        throw std::runtime_error("Invalid hex string length");
    std::vector<unsigned char> bytes;
    for (size_t i = 0; i < hex.size(); i += 2) {
        unsigned int byte;
        std::istringstream(hex.substr(i, 2)) >> std::hex >> byte;
        bytes.push_back(static_cast<unsigned char>(byte));
    }
    return bytes;
}

int main() {
    prototype_functions::openssl_sanity_check();

    std::array<unsigned char, 32> key{};
    bool key_set = false;

    bool running = true;
    while (running) {
        std::cout << "\nMenu:\n";
        std::cout << "1) Show current key\n";
        std::cout << "2) Input key (hex format, 64 chars)\n";
        std::cout << "3) Generate new key\n";
        std::cout << "4) Encrypt message\n";
        std::cout << "5) Decrypt message\n";
        std::cout << "6) Exit\n";
        std::cout << "Choose an option: ";

        int choice;
        std::cin >> choice;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        try {
            switch (choice) {
                case 1:
                    if (key_set) {
                        std::vector<unsigned char> key_vec(key.begin(), key.end());
                        std::cout << "Current key (hex): " << to_hex_string(key_vec) << "\n";
                    } else {
                        std::cout << "No key set.\n";
                    }
                    break;

                case 2: { // Input key in hex
                    std::cout << "Enter 32-byte key in hex (64 chars): ";
                    std::string hex;
                    std::getline(std::cin, hex);
                    if (hex.size() != 64)
                        throw std::runtime_error("Key must be exactly 64 hex characters");

                    auto bytes = hex_to_bytes(hex);
                    std::copy(bytes.begin(), bytes.end(), key.begin());
                    key_set = true;
                    break;
                }

                case 3: // Generate key
                    key = prototype_functions::generate_key();
                    key_set = true;
                    {
                        std::vector<unsigned char> key_vec(key.begin(), key.end());
                        std::cout << "Generated key (hex): " << to_hex_string(key_vec) << "\n";
                    }
                    break;

                case 4: { // Encrypt
                    if (!key_set) {
                        std::cout << "No key set. Generating key...\n";
                        key = prototype_functions::generate_key();
                        key_set = true;
                        std::vector<unsigned char> key_vec(key.begin(), key.end());
                        std::cout << "Generated key (hex): " << to_hex_string(key_vec) << "\n";
                    }

                    std::cout << "Enter message to encrypt: ";
                    std::string raw = userInput();
                    std::vector<unsigned char> plaintext(raw.begin(), raw.end());

                    std::array<unsigned char, 16> iv = prototype_functions::generate_initialization_vector();
                    auto ciphertext = prototype_functions::aes_encrypt(plaintext, key, iv);

                    std::vector<unsigned char> iv_vec(iv.begin(), iv.end());
                    std::string iv_hex = to_hex_string(iv_vec);
                    std::string ct_hex = to_hex_string(ciphertext);

                    std::cout << "Encrypted output (IV,Ciphertext): " << iv_hex << "," << ct_hex << "\n";
                    break;
                }

                case 5: { // Decrypt
                    if (!key_set) {
                        std::cout << "Error: No key set. Cannot decrypt.\n";
                        break;
                    }

                    std::cout << "Enter encrypted text (IVhex,Ciphertexthex): ";
                    std::string input;
                    std::getline(std::cin, input);

                    auto comma_pos = input.find(',');
                    if (comma_pos == std::string::npos)
                        throw std::runtime_error("Invalid format, missing comma");

                    std::string iv_hex = input.substr(0, comma_pos);
                    std::string ct_hex = input.substr(comma_pos + 1);

                    auto iv_bytes = hex_to_bytes(iv_hex);
                    auto ct_bytes = hex_to_bytes(ct_hex);

                    if (iv_bytes.size() != 16)
                        throw std::runtime_error("IV must be 16 bytes");

                    std::array<unsigned char, 16> iv{};
                    std::copy(iv_bytes.begin(), iv_bytes.end(), iv.begin());

                    auto decrypted = prototype_functions::aes_decrypt(ct_bytes, key, iv);
                    std::string message(decrypted.begin(), decrypted.end());
                    std::cout << "Decrypted message: " << message << "\n";
                    break;
                }

                case 6:
                    running = false;
                    break;

                default:
                    std::cout << "Invalid option\n";
            }
        } catch (const std::exception &e) {
            std::cout << "Error: " << e.what() << "\n";
        }
    }

    std::cout << "Exiting program.\n";
    return 0;
}
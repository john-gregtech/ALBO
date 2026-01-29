#include <iostream>
#include <vector>
#include <array>
#include <sstream>
#include <stdexcept>
#include <iomanip>
#include "cryptowrapper/X25519.h" // your existing X25519 header

// --- Hex helpers ---
std::string byte_to_hex(unsigned char b) {
    std::ostringstream oss;
    oss << std::hex << std::setw(2) << std::setfill('0')
        << static_cast<int>(b);
    return oss.str();
}

unsigned char hex_to_byte(const std::string& hex) {
    if (hex.size() != 2) throw std::invalid_argument("Must be 2 chars");
    return static_cast<unsigned char>(std::stoi(hex, nullptr, 16));
}

std::string bytearray_to_hexstring(const std::vector<unsigned char>& data) {
    std::string converted;
    for (unsigned char i : data) converted += byte_to_hex(i);
    return converted;
}

std::vector<unsigned char> hexstring_to_bytearray(const std::string& hex) {
    if (hex.size() % 2 != 0) throw std::invalid_argument("Hex string must be even length");
    std::vector<unsigned char> bytes;
    bytes.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2)
        bytes.push_back(hex_to_byte(hex.substr(i, 2)));
    return bytes;
}

template <int x>
std::vector<unsigned char> arraytovector(const std::array<unsigned char, x>& data) {
    return std::vector<unsigned char>(data.begin(), data.end());
}

template <int x>
std::array<unsigned char, x> vectortoarray(const std::vector<unsigned char>& v) {
    if (v.size() != x) throw std::invalid_argument("Vector size does not match array size");
    std::array<unsigned char, x> arr{};
    std::copy(v.begin(), v.end(), arr.begin());
    return arr;
}

// --- Main program ---
int main() {
    prototype_functions::X25519KeyPair my_keys;
    bool running = true;

    while (running) {
        std::cout << "\nX25519 32-byte generator:\n";
        std::cout << "1 - Generate your key pair\n";
        std::cout << "2 - Display key pair\n";
        std::cout << "3 - Compute shared secret from external hex\n";
        std::cout << "4 - Compute shared secret with random peer\n";
        std::cout << "5 - Manually input your private & public key\n";
        std::cout << "6 - Exit\n";
        int input;
        std::cin >> input;

        switch (input) {
            case 1:
                my_keys = prototype_functions::x25519_generate_keypair();
                std::cout << "Key pair generated.\n";
                break;

            case 2:
                std::cout << "Priv: " << bytearray_to_hexstring(arraytovector<32>(my_keys.priv))
                          << "\nPub: " << bytearray_to_hexstring(arraytovector<32>(my_keys.pub))
                          << "\n";
                break;

            case 3: {
                std::string publickey_hex;
                std::cout << "Paste external public key in hex (64 chars): ";
                std::cin >> publickey_hex;
                if (publickey_hex.length() != 64) {
                    std::cout << "Error: wrong length\n";
                    break;
                }

                try {
                    auto peer_pub = vectortoarray<32>(hexstring_to_bytearray(publickey_hex));
                    auto secret = prototype_functions::x25519_shared_secret(my_keys.priv, peer_pub);
                    std::cout << "Shared secret: "
                              << bytearray_to_hexstring(arraytovector<32>(secret)) << "\n";
                } catch (const std::exception& e) {
                    std::cout << "Error: " << e.what() << "\n";
                }

                break;
            }

            case 4: {
                auto peer = prototype_functions::x25519_generate_keypair();
                std::cout << "Random peer pub: "
                          << bytearray_to_hexstring(arraytovector<32>(peer.pub)) << "\n";
                auto secret = prototype_functions::x25519_shared_secret(my_keys.priv, peer.pub);
                std::cout << "Shared secret: "
                          << bytearray_to_hexstring(arraytovector<32>(secret)) << "\n";
                break;
            }

            case 5: {
                std::string priv_hex, pub_hex;
                std::cout << "Enter your private key in hex (64 chars): ";
                std::cin >> priv_hex;
                if (priv_hex.length() != 64) {
                    std::cout << "Error: wrong length\n";
                    break;
                }

                std::cout << "Enter your public key in hex (64 chars): ";
                std::cin >> pub_hex;
                if (pub_hex.length() != 64) {
                    std::cout << "Error: wrong length\n";
                    break;
                }

                try {
                    my_keys.priv = vectortoarray<32>(hexstring_to_bytearray(priv_hex));
                    my_keys.pub  = vectortoarray<32>(hexstring_to_bytearray(pub_hex));
                    std::cout << "Keys manually set.\n";
                } catch (const std::exception& e) {
                    std::cout << "Error: " << e.what() << "\n";
                }
                break;
            }

            case 6:
                running = false;
                break;

            default:
                std::cout << "Invalid choice\n";
                break;
        }
    }

    return 0;
}

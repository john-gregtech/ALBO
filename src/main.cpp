#include <iostream>
#include <array>
#include <vector>
#include <string>

#include "cryptowrapper/aes256.h"

void print_hex(const std::vector<unsigned char>& value) {
    for (unsigned char i : value) {
        std::cout << "0x" << int(i) << ' ';
    }
    std::cout << "\n"; 
}


int main() {
    std::cout << "test" << std::endl;
    prototype_functions::openssl_sanity_check();

    std::array<unsigned char, 32> my_key{};
    std::array<unsigned char, 16> iv{};

    my_key = prototype_functions::generate_key();
    iv = prototype_functions::generate_initialization_vector();
    
    std::string raw =  "this is a secret message";
    std::vector<unsigned char> my_message(raw.begin(), raw.end());
    
    std::vector<unsigned char> my_secret = prototype_functions::aes_encrypt(my_message, my_key, iv);
    std::vector<unsigned char> desecritized = prototype_functions::aes_decrypt(my_secret, my_key, iv);
    
    std::cout << "\n\n\n\n";
    print_hex(my_secret);
    std::cout << "\n";
    std::string original_message(desecritized.begin(), desecritized.end());
    print_hex(desecritized);
    std::cout << original_message << std::endl;

    return 0;
}
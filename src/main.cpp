#include <iostream>
#include <array>
#include "cryptowrapper/aes256.h"

int main() {
    std::cout << "test" << std::endl;

    prototype_functions::openssl_sanity_check();
    std::array<unsigned char, 32> my_key{};

    my_key = prototype_functions::generate_key();
    for (unsigned char i : my_key) {
        std::cout << i;
    }
    std::cout << '\n';

    return 0;
}
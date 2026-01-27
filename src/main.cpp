#include <iostream>
#include "cryptowrapper/aes256.h"

int main() {
    std::cout << "test" << std::endl;

    prototype_functions::openssl_sanity_check();

    return 0;
}
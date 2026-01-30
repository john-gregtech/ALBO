//This fileset will directly be used for salt and pepper plus sha256 on passwords

#pragma once
#include <openssl/rand.h>
#include <vector>
#include <array>
#include <iostream>

namespace prototype_functions {
    std::vector<unsigned char> randomByteGen(unsigned int randomLengthSize);
}

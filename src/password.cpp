#include "cryptowrapper/password.h"

namespace prototype_functions {

    //Workflow
    //This code is to be used as a a salt and pepper setup for password creating and checking
    //so i just learnt what pepper actually is

    // unsigned char pepperGen() {
    //     unsigned char pepper;
    //     RAND_bytes(&pepper, 1);
    //     return pepper;
    // }
    //i chose std::array over vector as it wont change
    
    std::vector<unsigned char> randomByteGen(unsigned int randomLengthSize) {
        std::vector<unsigned char> pepper(randomLengthSize);
        if (RAND_bytes(pepper.data(), randomLengthSize) != 1)
            throw std::runtime_error("Error when RAND_bytes in pepperGen()");
        return pepper;
    }
    

}
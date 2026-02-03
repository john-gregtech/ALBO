#include "cryptowrapper/password.h"

namespace prototype_functions {

    //Workflow
    //This code is to be used as a a salt and pepper setup for password creating and checking
    //so i just learnt what pepper actually is

    // uint8_t pepperGen() {
    //     uint8_t pepper;
    //     RAND_bytes(&pepper, 1);
    //     return pepper;
    // }
    //i chose std::array over vector as it wont change
    
    std::vector<uint8_t> randomByteGen(uint32_t randomLengthSize) {
        std::vector<uint8_t> pepper(randomLengthSize);
        if (RAND_bytes(pepper.data(), randomLengthSize) != 1)
            throw std::runtime_error("Error when RAND_bytes in pepperGen()");
        return pepper;
    }
    //no cheating with google for this function
    std::vector<uint8_t> generatePassword(
        const std::vector<uint8_t>& password,
        const std::vector<uint8_t>& salt,
        const std::vector<uint8_t>& pepper 
    ) {
        size_t totalPasswordLength = password.size() + salt.size() + pepper.size();
        //i guess you put salt on first so password salt pepper
        std::vector<uint8_t> seasonedPassword(totalPasswordLength);
        std::cout << totalPasswordLength << std::endl;
        //combine vectors here
        for (size_t i{}; i < totalPasswordLength; ++i) {
            if (i >= password.size() + salt.size()) {
                seasonedPassword.at(i) = pepper.at(i - password.size() - salt.size());
            } else if (i >= password.size()) {
                seasonedPassword.at(i) = salt.at(i - password.size());
            } else {
                seasonedPassword.at(i) = password.at(i);
            }
        }
        return seasonedPassword;
    }
    

}
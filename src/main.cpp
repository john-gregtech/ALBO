#include <cryptowrapper/argon2id.h>
#include <iostream>
#include <vector>
#include <string>

int main() {
    std::string test = "crazy frog was made in sweden.";
    std::vector<uint8_t> rawtext(test.begin(), test.end());

    std::vector<uint8_t> ciphertext = argonidhash(rawtext);
    std::cout << ciphertext.data() << std::endl;
}
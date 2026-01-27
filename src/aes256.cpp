#include "cryptowrapper/aes256.h"

namespace prototype_functions {
    void openssl_sanity_check()
    {
        std::cout << "OpenSSL version (macro): "
                << OPENSSL_VERSION_TEXT << '\n';

        std::cout << "OpenSSL version (runtime): "
                << OpenSSL_version(OPENSSL_VERSION) << '\n';
    }
}
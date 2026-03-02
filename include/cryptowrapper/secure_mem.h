#pragma once
#include <cstddef>

namespace prototype::cryptowrapper {
    /**
     * @brief Securely erases memory to prevent sensitive data from remaining in RAM.
     * Uses platform-specific calls (explicit_bzero on Linux, SecureZeroMemory on Windows)
     * to ensure the compiler does not optimize away the operation.
     */
    void secure_erase(void* ptr, size_t size);
}

#include "universal/cryptowrapper/secure_mem.h"
#include <windows.h>

namespace prototype::cryptowrapper {
    void secure_erase(void* ptr, size_t size) {
        if (ptr && size > 0) {
            SecureZeroMemory(ptr, size);
        }
    }
}

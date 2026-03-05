#include "universal/cryptowrapper/secure_mem.h"
#include <string.h>

namespace prototype::cryptowrapper {
    void secure_erase(void* ptr, size_t size) {
        if (ptr && size > 0) {
            explicit_bzero(ptr, size);
        }
    }
}

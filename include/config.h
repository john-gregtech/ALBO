#pragma once

// Toggle this to ON to see high-signal networking and logic logs
#define ALBO_DEBUG_VERBOSE ON

#ifdef ALBO_DEBUG_VERBOSE
    #include <iostream>
    #define ALBO_LOG(x) std::cout << "[LOG] " << x << std::endl
    #define ALBO_DEBUG(x) std::cout << "[DEBUG] " << x << std::endl
#else
    #define ALBO_LOG(x)
    #define ALBO_DEBUG(x)
#endif

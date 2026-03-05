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

// --- Network Presets ---
#define ALBO_DEFAULT_PORT 6000
#define ALBO_DEFAULT_IP "127.0.0.1"

// --- File & Database Presets ---
#define ALBO_SERVER_RAR_FILE "large.rar"
#define ALBO_GUI_DB_NAME "local_inbox_gui.db"
#define ALBO_SIM_DB_NAME "albo_sim.db"
#define ALBO_APP_DIR "albo"

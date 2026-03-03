cmake_minimum_required(VERSION 3.16)

project(ALBO
    VERSION 1.0.0
    LANGUAGES CXX
)

# ---- Export compile_commands.json ----
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- C++20 ----
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Discovery ----
find_package(OpenSSL REQUIRED COMPONENTS Crypto SSL)
find_package(SQLite3 REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets)

# ---- Library Sources (Shared across platforms) ----
set(CRYPTO_SOURCES
    src/sha256.cpp
    src/aes256.cpp
    src/X25519.cpp
    src/argon2id.cpp
    src/network/universal/ed25519.cpp
)

set(NETWORK_SOURCES_UNIX
    src/network/universal/secure_socket.cpp
    src/network/universal/database.cpp
    src/network/universal/config_manager.cpp
    src/network/universal/professionalprovider.cpp
    src/network/linux/socket_manager.cpp
    src/network/linux/secure_mem.cpp
)

set(NETWORK_SOURCES_WIN
    src/network/universal/secure_socket.cpp
    src/network/universal/database.cpp
    src/network/universal/config_manager.cpp
    src/network/universal/professionalprovider.cpp
    src/network/win/network_boiler.cpp
    src/network/win/secure_mem.cpp
)

# ---- Executables ----

# 1. ALBO Testing (Simulation)
add_executable(ALBO
    src/main.cpp
    ${CRYPTO_SOURCES}
    ${NETWORK_SOURCES_UNIX}
)
target_link_libraries(ALBO PRIVATE OpenSSL::Crypto OpenSSL::SSL SQLite::SQLite3 argon2)
target_include_directories(ALBO PRIVATE ${PROJECT_SOURCE_DIR}/include)

# 2. LINUX SERVER
add_executable(LINUXSERVER
    src/network/linux/server.cpp
    src/network/server/auth_handler.cpp
    src/network/server/routing_handler.cpp
    src/network/server/rate_limiter.cpp
    ${CRYPTO_SOURCES}
    ${NETWORK_SOURCES_UNIX}
)
target_link_libraries(LINUXSERVER PRIVATE OpenSSL::Crypto OpenSSL::SSL SQLite::SQLite3 argon2)
target_include_directories(LINUXSERVER PRIVATE ${PROJECT_SOURCE_DIR}/include)

# 3. LINUX CLIENT (TUI)
add_executable(LINUXCLIENT
    src/network/linux/client.cpp
    src/network/client/identity_manager.cpp
    src/network/client/crypto_service.cpp
    src/network/client/console_manager.cpp
    src/network/client/file_transfer_manager.cpp
    ${CRYPTO_SOURCES}
    ${NETWORK_SOURCES_UNIX}
)
target_link_libraries(LINUXCLIENT PRIVATE OpenSSL::Crypto OpenSSL::SSL SQLite::SQLite3 argon2)
target_include_directories(LINUXCLIENT PRIVATE ${PROJECT_SOURCE_DIR}/include)

# 4. LINUX CLIENT GUI (Qt)
add_executable(LINUX_CLIENT_GUI
    src/graphical/linux/main.cpp
    src/graphical/universal/main_window.cpp
    src/graphical/universal/network_controller.cpp
    src/graphical/universal/login_dialog.cpp
    include/graphical/universal/main_window.h
    include/graphical/universal/network_controller.h
    include/graphical/universal/login_dialog.h
    src/network/client/identity_manager.cpp
    src/network/client/crypto_service.cpp
    src/network/client/file_transfer_manager.cpp
    ${CRYPTO_SOURCES}
    ${NETWORK_SOURCES_UNIX}
)
target_link_libraries(LINUX_CLIENT_GUI PRIVATE 
    OpenSSL::Crypto OpenSSL::SSL SQLite::SQLite3 argon2 Qt6::Widgets
)
target_include_directories(LINUX_CLIENT_GUI PRIVATE 
    ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/include/graphical
)
set_target_properties(LINUX_CLIENT_GUI PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON)

# 5. WINDOWS CLIENT
if (WIN32)
    add_executable(WINCLIENT
        src/network/win/client.cpp
        ${CRYPTO_SOURCES}
        ${NETWORK_SOURCES_WIN}
    )
    target_link_libraries(WINCLIENT PRIVATE OpenSSL::Crypto OpenSSL::SSL SQLite::SQLite3 argon2 ws2_32 kernel32 user32)
    target_include_directories(WINCLIENT PRIVATE ${PROJECT_SOURCE_DIR}/include)

    add_executable(WINSERVER
        src/network/win/server.cpp
        ${CRYPTO_SOURCES}
        ${NETWORK_SOURCES_WIN}
    )
    target_link_libraries(WINSERVER PRIVATE OpenSSL::Crypto OpenSSL::SSL SQLite::SQLite3 argon2 ws2_32 kernel32 user32)
    target_include_directories(WINSERVER PRIVATE ${PROJECT_SOURCE_DIR}/include)
endif()

# Global Output settings
set_target_properties(ALBO PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
set_target_properties(LINUXSERVER PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
set_target_properties(LINUXCLIENT PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
set_target_properties(LINUX_CLIENT_GUI PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
if (WIN32)
    set_target_properties(WINCLIENT PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
    set_target_properties(WINSERVER PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
endif()

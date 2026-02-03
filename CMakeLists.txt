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

# ---- Compiler warnings ----
if (MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ---- OpenSSL discovery ----
# Prefer config packages, fall back to module mode
set(OpenSSL_USE_STATIC_LIBS OFF)
set(OpenSSL_USE_MSVC_STATIC_RT OFF)

find_package(OpenSSL REQUIRED COMPONENTS Crypto)

# ---- Executables ----
add_executable(ALBO
    src/main.cpp
    src/sha256.cpp
    src/aes256.cpp
    src/X25519.cpp
    src/password.cpp
)

if (WIN32)
    add_executable(WINCLIENT
        src/sha256.cpp
        src/aes256.cpp
        src/X25519.cpp
        src/password.cpp
        src/network/win/client.cpp
    )

    add_executable(WINSERVER
        src/sha256.cpp
        src/aes256.cpp
        src/X25519.cpp
        src/password.cpp
        src/network/win/server.cpp
    )
elseif (UNIX)
    add_executable(LINUXCLIENT
        src/sha256.cpp
        src/aes256.cpp
        src/X25519.cpp
        src/password.cpp
        src/network/linux/client.cpp
    )

    add_executable(LINUXSERVER
        src/sha256.cpp
        src/aes256.cpp
        src/X25519.cpp
        src/password.cpp
        src/network/linux/server.cpp
    )
endif()

# ---- Include directories ----
target_include_directories(ALBO
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

if (WIN32)
    target_include_directories(WINCLIENT
        PRIVATE
            ${PROJECT_SOURCE_DIR}/include
    )
    target_include_directories(WINSERVER
        PRIVATE
            ${PROJECT_SOURCE_DIR}/include
    )
elseif (UNIX)
    # for unix later
    target_include_directories(LINUXCLIENT
        PRIVATE
            ${PROJECT_SOURCE_DIR}/include
    )
    target_include_directories(LINUXSERVER
        PRIVATE
            ${PROJECT_SOURCE_DIR}/include
    )
endif()

# ---- Link libraries ----
target_link_libraries(ALBO
    PRIVATE
        OpenSSL::Crypto
)

if (WIN32)
    target_link_libraries(WINCLIENT
        PRIVATE
            OpenSSL::Crypto
            user32
            kernel32
            ws2_32
    )
    target_link_libraries(WINSERVER
        PRIVATE
            OpenSSL::Crypto
            user32
            kernel32
            ws2_32
    )
elseif (UNIX)
    # same as above
    target_link_libraries(LINUXCLIENT
        PRIVATE
            OpenSSL::Crypto
    )
    target_link_libraries(LINUXSERVER
        PRIVATE
            OpenSSL::Crypto
    )
endif()

# ---- Platform defines ----
if (WIN32)
    target_compile_definitions(ALBO PRIVATE ALBO_PLATFORM_WINDOWS)
elseif (UNIX)
    target_compile_definitions(ALBO PRIVATE ALBO_PLATFORM_UNIX)
endif()

# ---- Output directories ----
set_target_properties(ALBO PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin
)
if (WIN32)
    set_target_properties(WINCLIENT PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin
    )

    set_target_properties(WINSERVER PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin
    )
elseif (UNIX) 
    set_target_properties(LINUXCLIENT PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin
    )

    set_target_properties(LINUXSERVER PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin
    )
endif(`)

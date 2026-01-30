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

# ---- Executable ----
add_executable(ALBO
    src/main.cpp
    src/sha256.cpp
    src/aes256.cpp
    src/X25519.cpp
    src/password.cpp
)

# ---- Includes ----
target_include_directories(ALBO
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

# ---- Link OpenSSL ----
target_link_libraries(ALBO
    PRIVATE
        OpenSSL::Crypto
)

# ---- Platform defines ----
if (WIN32)
    target_compile_definitions(ALBO PRIVATE ALBO_PLATFORM_WINDOWS)
elseif (UNIX)
    target_compile_definitions(ALBO PRIVATE ALBO_PLATFORM_UNIX)
endif()

# ---- Output ----
set_target_properties(ALBO PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin
)

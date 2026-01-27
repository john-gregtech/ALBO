cmake_minimum_required(VERSION 3.16)

project(ALBO
    VERSION 1.0.0
    LANGUAGES CXX
)

# ---- C++ Standard ----
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Compiler warnings ----
if (MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ---- OpenSSL ----
find_package(OpenSSL REQUIRED)

# ---- Sources ----
add_executable(ALBO
    src/main.cpp
    src/crypto.cpp
    src/aes256.cpp
    src/X25519.cpp
)

# ---- Includes ----
target_include_directories(ALBO
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

# ---- Link libraries ----
# Order matters for MinGW
target_link_libraries(ALBO
    PRIVATE
        OpenSSL::Crypto
        OpenSSL::SSL
)

# ---- Platform defines ----
if (WIN32)
    target_compile_definitions(ALBO PRIVATE ALBO_PLATFORM_WINDOWS)
else()
    target_compile_definitions(ALBO PRIVATE ALBO_PLATFORM_UNIX)
endif()

# ---- Output directory ----
set_target_properties(ALBO PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin
)
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

# ---- Include Path (Base include/ directory) ----
set(BASE_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)

# ---- External Libraries ----
add_subdirectory("external libraries/openGL_GUI")

# ---- Library Sources (Shared across platforms) ----
set(CRYPTO_SOURCES
    src/universal/cryptowrapper/sha256.cpp
    src/universal/cryptowrapper/aes256.cpp
    src/universal/cryptowrapper/X25519.cpp
    src/universal/cryptowrapper/argon2id.cpp
    src/universal/cryptowrapper/ed25519.cpp
)

# Shared logic for client/server across platforms
set(UNIVERSAL_NETWORK_SOURCES
    src/universal/network/secure_socket.cpp
    src/universal/network/database.cpp
    src/universal/network/config_manager.cpp
    src/universal/network/professionalprovider.cpp
    src/universal/network/auth_handler.cpp
    src/universal/network/routing_handler.cpp
    src/universal/network/rate_limiter.cpp
    src/universal/network/identity_manager.cpp
    src/universal/network/crypto_service.cpp
)

set(NETWORK_SOURCES_UNIX
    ${UNIVERSAL_NETWORK_SOURCES}
    src/linux/network/socket_manager.cpp
    src/linux/network/secure_mem.cpp
)

set(NETWORK_SOURCES_WIN
    ${UNIVERSAL_NETWORK_SOURCES}
    src/win/network/network_boiler.cpp
    src/win/network/secure_mem.cpp
)

# ---- Executables ----

if (UNIX OR WIN32)
    # 1. ALBO Testing (Simulation)
    add_executable(ALBO
        src/universal/main.cpp
        ${CRYPTO_SOURCES}
    )
    if (UNIX)
        target_sources(ALBO PRIVATE ${NETWORK_SOURCES_UNIX})
    else()
        target_sources(ALBO PRIVATE ${NETWORK_SOURCES_WIN})
        target_link_libraries(ALBO PRIVATE ws2_32)
    endif()
    target_link_libraries(ALBO PRIVATE OpenSSL::Crypto OpenSSL::SSL SQLite::SQLite3 argon2)
    target_include_directories(ALBO PRIVATE ${BASE_INCLUDE_DIR})

    # 2. ALBO NEW GUI (Replacing Qt with OpenGLGUI)
    add_executable(ALBO_GUI
        src/universal/main_gl.cpp
        ${CRYPTO_SOURCES}
    )
    if (UNIX)
        target_sources(ALBO_GUI PRIVATE ${NETWORK_SOURCES_UNIX})
    else()
        target_sources(ALBO_GUI PRIVATE ${NETWORK_SOURCES_WIN})
        target_link_libraries(ALBO_GUI PRIVATE ws2_32)
    endif()
    target_link_libraries(ALBO_GUI PRIVATE OpenGLGUI OpenSSL::Crypto OpenSSL::SSL SQLite::SQLite3 argon2)
    target_include_directories(ALBO_GUI PRIVATE ${BASE_INCLUDE_DIR})
endif()

if (UNIX)
    # 3. LINUX SERVER
    add_executable(LINUXSERVER
        src/linux/network/server.cpp
        ${CRYPTO_SOURCES}
        ${NETWORK_SOURCES_UNIX}
    )
    target_link_libraries(LINUXSERVER PRIVATE OpenSSL::Crypto OpenSSL::SSL SQLite::SQLite3 argon2)
    target_include_directories(LINUXSERVER PRIVATE ${BASE_INCLUDE_DIR})

    # 4. LINUX CLIENT (TUI)
    add_executable(LINUXCLIENT
        src/linux/network/client.cpp
        src/linux/network/console_manager.cpp
        src/linux/network/file_transfer_manager.cpp
        ${CRYPTO_SOURCES}
        ${NETWORK_SOURCES_UNIX}
    )
    target_link_libraries(LINUXCLIENT PRIVATE OpenSSL::Crypto OpenSSL::SSL SQLite::SQLite3 argon2)
    target_include_directories(LINUXCLIENT PRIVATE ${BASE_INCLUDE_DIR})
endif()

# 5. WINDOWS CLIENTS
if (WIN32)
    add_executable(WINCLIENT
        src/win/network/client.cpp
        ${CRYPTO_SOURCES}
        ${NETWORK_SOURCES_WIN}
    )
    target_link_libraries(WINCLIENT PRIVATE OpenSSL::Crypto OpenSSL::SSL SQLite::SQLite3 argon2 ws2_32 kernel32 user32)
    target_include_directories(WINCLIENT PRIVATE ${BASE_INCLUDE_DIR})

    add_executable(WINSERVER
        src/win/network/server.cpp
        ${CRYPTO_SOURCES}
        ${NETWORK_SOURCES_WIN}
    )
    target_link_libraries(WINSERVER PRIVATE OpenSSL::Crypto OpenSSL::SSL SQLite::SQLite3 argon2 ws2_32 kernel32 user32)
    target_include_directories(WINSERVER PRIVATE ${BASE_INCLUDE_DIR})
endif()

# Global Output settings
if (UNIX)
    set_target_properties(ALBO PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
    set_target_properties(ALBO_GUI PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
    if (TARGET LINUXSERVER)
        set_target_properties(LINUXSERVER PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
    endif()
    if (TARGET LINUXCLIENT)
        set_target_properties(LINUXCLIENT PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
    endif()
endif()

if (WIN32)
    set(WIN_DLLS 
        "C:/msys64/mingw64/bin/libssl-3-x64.dll"
        "C:/msys64/mingw64/bin/libcrypto-3-x64.dll"
        "C:/msys64/mingw64/bin/libsqlite3-0.dll"
        "C:/msys64/mingw64/bin/libargon2.dll"
        "C:/msys64/mingw64/bin/libstdc++-6.dll"
        "C:/msys64/mingw64/bin/libgcc_s_seh-1.dll"
        "C:/msys64/mingw64/bin/libwinpthread-1.dll"
    )

    macro(setup_windows_target TARGET_NAME)
        if (TARGET ${TARGET_NAME})
            set_target_properties(${TARGET_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin/${TARGET_NAME})
            foreach(DLL ${WIN_DLLS})
                add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DLL}" "$<TARGET_FILE_DIR:${TARGET_NAME}>"
                )
            endforeach()
        endif()
    endmacro()

    setup_windows_target(ALBO)
    setup_windows_target(ALBO_GUI)
    setup_windows_target(WINCLIENT)
    setup_windows_target(WINSERVER)
endif()

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

# ---- Include Path (Base include/ directory) ----
set(BASE_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)

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
endif()

if (UNIX)
    # 2. LINUX SERVER
    add_executable(LINUXSERVER
        src/linux/network/server.cpp
        ${CRYPTO_SOURCES}
        ${NETWORK_SOURCES_UNIX}
    )
    target_link_libraries(LINUXSERVER PRIVATE OpenSSL::Crypto OpenSSL::SSL SQLite::SQLite3 argon2)
    target_include_directories(LINUXSERVER PRIVATE ${BASE_INCLUDE_DIR})

    # 3. LINUX CLIENT (TUI)
    add_executable(LINUXCLIENT
        src/linux/network/client.cpp
        src/linux/network/console_manager.cpp
        src/linux/network/file_transfer_manager.cpp
        ${CRYPTO_SOURCES}
        ${NETWORK_SOURCES_UNIX}
    )
    target_link_libraries(LINUXCLIENT PRIVATE OpenSSL::Crypto OpenSSL::SSL SQLite::SQLite3 argon2)
    target_include_directories(LINUXCLIENT PRIVATE ${BASE_INCLUDE_DIR})

    # 4. LINUX CLIENT GUI (Qt)
    add_executable(LINUX_CLIENT_GUI
        src/linux/graphical/main.cpp
        src/universal/graphical/main_window.cpp
        src/universal/graphical/network_controller.cpp
        src/universal/graphical/login_dialog.cpp
        # Include headers for AUTOMOC
        include/universal/graphical/main_window.h
        include/universal/graphical/network_controller.h
        include/universal/graphical/login_dialog.h
        src/linux/network/file_transfer_manager.cpp
        ${CRYPTO_SOURCES}
        ${NETWORK_SOURCES_UNIX}
    )
    target_link_libraries(LINUX_CLIENT_GUI PRIVATE 
        OpenSSL::Crypto OpenSSL::SSL SQLite::SQLite3 argon2 Qt6::Widgets
    )
    target_include_directories(LINUX_CLIENT_GUI PRIVATE ${BASE_INCLUDE_DIR})
    set_target_properties(LINUX_CLIENT_GUI PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON)
endif()

# 5. WINDOWS CLIENT
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

    # Windows GUI Client
    if (Qt6_FOUND)
        add_executable(WIN_CLIENT_GUI WIN32
            src/linux/graphical/main.cpp
            src/universal/graphical/main_window.cpp
            src/universal/graphical/network_controller.cpp
            src/universal/graphical/login_dialog.cpp
            # Include headers for AUTOMOC
            include/universal/graphical/main_window.h
            include/universal/graphical/network_controller.h
            include/universal/graphical/login_dialog.h
            src/linux/network/file_transfer_manager.cpp
            ${CRYPTO_SOURCES}
            ${NETWORK_SOURCES_WIN}
        )
        target_link_libraries(WIN_CLIENT_GUI PRIVATE 
            OpenSSL::Crypto OpenSSL::SSL SQLite::SQLite3 argon2 Qt6::Widgets ws2_32 kernel32 user32
        )
        target_include_directories(WIN_CLIENT_GUI PRIVATE ${BASE_INCLUDE_DIR})
        set_target_properties(WIN_CLIENT_GUI PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON)
    endif()
endif()

# Global Output settings
if (UNIX)
    if (TARGET ALBO)
        set_target_properties(ALBO PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
    endif()
    if (TARGET LINUXSERVER)
        set_target_properties(LINUXSERVER PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
    endif()
    if (TARGET LINUXCLIENT)
        set_target_properties(LINUXCLIENT PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
    endif()
    if (TARGET LINUX_CLIENT_GUI)
        set_target_properties(LINUX_CLIENT_GUI PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
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
    setup_windows_target(WINCLIENT)
    setup_windows_target(WINSERVER)

    if (TARGET WIN_CLIENT_GUI)
        set_target_properties(WIN_CLIENT_GUI PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin/WIN_CLIENT_GUI)
        # For Qt, we use windeployqt
        add_custom_command(TARGET WIN_CLIENT_GUI POST_BUILD
            COMMAND C:/msys64/mingw64/bin/windeployqt6.exe "$<TARGET_FILE:WIN_CLIENT_GUI>"
        )
        # Also copy our non-Qt dependencies
        foreach(DLL ${WIN_DLLS})
            add_custom_command(TARGET WIN_CLIENT_GUI POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DLL}" "$<TARGET_FILE_DIR:WIN_CLIENT_GUI>"
            )
        endforeach()
    endif()
endif()

cmake_minimum_required(VERSION 3.26)
project(OpenGLGUI_Project LANGUAGES CXX C)

# Set C++ standard to 20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Address FreeType/Dependency compatibility for modern CMake (3.27+)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# ------------------------
# Dependencies (FetchContent)
# ------------------------
include(FetchContent)

# GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.8
)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# FreeType
FetchContent_Declare(
    freetype
    GIT_REPOSITORY https://github.com/freetype/freetype.git
    GIT_TAG VER-2-13-0
)
FetchContent_MakeAvailable(freetype)

# stb_image
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
)
FetchContent_MakeAvailable(stb)

# ------------------------
# Library Target (The static library part)
# ------------------------

# GLAD (Static in deps)
set(GLAD_DIR "${CMAKE_SOURCE_DIR}/deps/glad")
add_library(glad STATIC "${GLAD_DIR}/src/glad.c")
target_include_directories(glad PUBLIC "${GLAD_DIR}/include")

# Core Library Files
set(LIB_SOURCES
    "src/GUILibrary.cpp"
    "src/RenderManager.cpp"
    "src/InputManager.cpp"
    "src/TextManager.cpp"
)

add_library(OpenGLGUI STATIC ${LIB_SOURCES})

# Public headers for users of the library
target_include_directories(OpenGLGUI PUBLIC 
    "${CMAKE_SOURCE_DIR}/include"
)

# Private headers for internal building
target_include_directories(OpenGLGUI PRIVATE 
    ${glfw_SOURCE_DIR}/include
    ${freetype_SOURCE_DIR}/include
    ${stb_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/deps/glad/include
)

# Dependencies required by the library
target_link_libraries(OpenGLGUI PUBLIC 
    glfw 
    glad
    freetype
)

if(WIN32)
    target_link_libraries(OpenGLGUI PUBLIC opengl32)
else()
    target_link_libraries(OpenGLGUI PUBLIC GL X11 pthread dl)
endif()

# ------------------------
# Demo Target (The example app)
# ------------------------

add_executable(OpenGLGUI_Demo "src/main.cpp")
target_link_libraries(OpenGLGUI_Demo PRIVATE OpenGLGUI)

# Ensure the demo can find headers
target_include_directories(OpenGLGUI_Demo PRIVATE "include")

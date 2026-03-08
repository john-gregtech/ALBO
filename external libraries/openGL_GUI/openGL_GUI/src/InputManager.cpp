#include "InputManager.hpp"

std::map<int, bool> InputManager::keys;
std::map<int, bool> InputManager::keysPressed;
std::map<int, bool> InputManager::mouseButtons;
double InputManager::mouseX = 0;
double InputManager::mouseY = 0;
std::string InputManager::charBuffer = "";

void InputManager::init(GLFWwindow* window) {
    glfwSetKeyCallback(window, keyCallback);
    glfwSetCharCallback(window, charCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
}

bool InputManager::isKeyDown(int key) {
    return keys[key];
}

bool InputManager::isKeyPressed(int key) {
    bool pressed = keysPressed[key];
    keysPressed[key] = false; // Reset after reading
    return pressed;
}

bool InputManager::isMouseButtonDown(int button) {
    return mouseButtons[button];
}

void InputManager::getMousePos(double& x, double& y) {
    x = mouseX;
    y = mouseY;
}

std::string InputManager::getAndClearCharBuffer() {
    std::string buffer = charBuffer;
    charBuffer = "";
    return buffer;
}

void InputManager::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        keys[key] = true;
        keysPressed[key] = true;
    } else if (action == GLFW_RELEASE) {
        keys[key] = false;
    }
}

void InputManager::charCallback(GLFWwindow* window, unsigned int codepoint) {
    if (codepoint < 128) {
        charBuffer += (char)codepoint;
    }
}

void InputManager::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (action == GLFW_PRESS) mouseButtons[button] = true;
    else if (action == GLFW_RELEASE) mouseButtons[button] = false;
}

void InputManager::cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    mouseX = xpos;
    mouseY = ypos;
}

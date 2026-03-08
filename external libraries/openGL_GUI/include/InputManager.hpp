#pragma once
#include <GLFW/glfw3.h>
#include <map>
#include <string>

class InputManager {
public:
    static void init(GLFWwindow* window);
    static bool isKeyDown(int key);
    static bool isMouseButtonDown(int button);
    static void getMousePos(double& x, double& y);
    
    // Character input
    static std::string getAndClearCharBuffer();
    static bool isKeyPressed(int key); // One-shot trigger

private:
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void charCallback(GLFWwindow* window, unsigned int codepoint);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);

    static std::map<int, bool> keys;
    static std::map<int, bool> keysPressed;
    static std::map<int, bool> mouseButtons;
    static double mouseX, mouseY;
    static std::string charBuffer;
};

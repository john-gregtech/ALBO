#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "RenderManager.hpp"
#include <GLFW/glfw3.h>

struct WindowDecoration {
    bool enabled = false;
    float height = 0.06f; 
    Color backgroundColor = {0.1f, 0.1f, 0.1f, 1.0f};
    Color titleColor = Color::White();
    std::string title = "Window";
    bool showControls = true;
};

struct MenuBar {
    bool visible = true;
    float height = 0.05f;
    Color backgroundColor = {0.15f, 0.15f, 0.15f, 1.0f};
    Color textColor = Color::White();
};

struct MenuItem {
    std::string name;
    std::function<void()> action;
};

class GUILibrary {
public:
    GUILibrary(int width, int height, const std::string& title, bool decorated = true, const std::string& fontPath = "");
    ~GUILibrary();

    void run();
    void addElement(std::unique_ptr<Shape> element);
    void setWindowSize(int width, int height);
    void setWindowIcon(const std::string& path);
    
    // Custom Border & Menu Setup
    void enableCustomDecoration(const WindowDecoration& config);
    void updateDecorationPositions();
    void setControlsVisible(bool visible);
    
    // Menu Bar Methods
    void setMenuBarVisible(bool visible);
    void addMenuItem(const std::string& name, std::function<void()> action);

    // System Utilities
    static void hideConsole();

private:
    void handleWindowDragging();
    void setupDefaultControls();
    void updateMenuBarItems();

    GLFWwindow* window;
    std::vector<std::unique_ptr<Shape>> elements;
    std::vector<std::unique_ptr<Shape>> decorationElements; 
    std::vector<std::unique_ptr<Button>> menuBarButtons;
    
    WindowDecoration decorationConfig;
    MenuBar menuBarConfig;
    std::vector<MenuItem> menuItems;

    bool initialized = false;
    
    // Dragging State
    bool isDragging = false;
    double dragStartX, dragStartY;
    int windowStartX, windowStartY;
};

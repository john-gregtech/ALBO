#include "GUILibrary.hpp"
#include "InputManager.hpp"
#include <glad/gl.h>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

#include "TextManager.hpp"
#include <stb_image.h>

GUILibrary::GUILibrary(int width, int height, const std::string& title, bool decorated, const std::string& fontPath) {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DECORATED, decorated ? GLFW_TRUE : GLFW_FALSE);

    window = glfwCreateWindow(width, height, title.c_str(), NULL, NULL);
    if (!window) {
        glfwTerminate();
        return;
    }

    glfwMakeContextCurrent(window);

    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return;
    }

    InputManager::init(window);
    RenderManager::init();
    if (!fontPath.empty()) {
        TextManager::init(fontPath);
    }
    initialized = true;
    
    isDragging = false;
    decorationConfig.title = title;
}

void GUILibrary::setWindowIcon(const std::string& path) {
    if (!window) return;
    GLFWimage images[1];
    int width, height, channels;
    stbi_set_flip_vertically_on_load(false);
    unsigned char* pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (pixels) {
        images[0].width = width;
        images[0].height = height;
        images[0].pixels = pixels;
        glfwSetWindowIcon(window, 1, images);
        stbi_image_free(pixels);
    }
}

GUILibrary::~GUILibrary() {
    glfwTerminate();
}

void GUILibrary::hideConsole() {
#ifdef _WIN32
    HWND hWnd = GetConsoleWindow();
    if (hWnd) ShowWindow(hWnd, SW_HIDE);
#endif
}

void GUILibrary::setControlsVisible(bool visible) {
    decorationConfig.showControls = visible;
    decorationElements.clear();
    if (visible) setupDefaultControls();
}

void GUILibrary::setMenuBarVisible(bool visible) {
    menuBarConfig.visible = visible;
}

void GUILibrary::addMenuItem(const std::string& name, std::function<void()> action) {
    menuItems.push_back({name, action});
    updateMenuBarItems();
}

void GUILibrary::updateMenuBarItems() {
    menuBarButtons.clear();
    
    // Calculate scale to fit menu bar height (with padding)
    // 48px * 0.002 = 0.096 NDC height for 1.0 scale
    // scale = target_height / 0.096
    float paddingY = menuBarConfig.height * 0.2f;
    float targetTextHeight = menuBarConfig.height - (paddingY * 2.0f);
    float calculatedScale = targetTextHeight / 0.096f;

    for (const auto& item : menuItems) {
        auto btn = std::make_unique<Button>();
        
        // Measure text width at the calculated scale
        Vec2 textSize = TextManager::getTextSize(item.name, calculatedScale);
        float paddingX = 0.04f;
        
        btn->config.text = item.name;
        btn->config.textScale = calculatedScale;
        btn->config.backgroundColor = Color::Transparent();
        btn->config.foregroundColor = menuBarConfig.textColor;
        btn->config.posMode = PositionMode::Fixed;
        btn->config.size = {textSize.x + paddingX, menuBarConfig.height};
        btn->config.hAlign = HorizontalAlignment::Center;
        btn->config.vAlign = VerticalAlignment::Center;
        btn->config.onClick = item.action;
        
        menuBarButtons.push_back(std::move(btn));
    }
}

void GUILibrary::enableCustomDecoration(const WindowDecoration& config) {
    decorationConfig = config;
    decorationConfig.enabled = true;
    setupDefaultControls();
}

void GUILibrary::setupDefaultControls() {
    if (!decorationConfig.showControls) return;
    for (int i = 0; i < 3; ++i) {
        auto btn = std::make_unique<Button>();
        btn->config.posMode = PositionMode::Fixed;
        if (i == 0) { 
            btn->config.text = "X"; 
            btn->config.backgroundColor = {0.8f, 0.2f, 0.2f, 1.0f}; 
            btn->config.onClick = [this](){ glfwSetWindowShouldClose(window, GLFW_TRUE); }; 
        }
        else if (i == 1) { 
            btn->config.text = "[]"; 
            btn->config.backgroundColor = {0.4f, 0.4f, 0.4f, 1.0f}; 
            btn->config.onClick = [this](){ 
                if (glfwGetWindowAttrib(window, GLFW_MAXIMIZED)) glfwRestoreWindow(window); 
                else glfwMaximizeWindow(window); 
            }; 
        }
        else { 
            btn->config.text = "_"; 
            btn->config.backgroundColor = {0.4f, 0.4f, 0.4f, 1.0f}; 
            btn->config.onClick = [this](){ glfwIconifyWindow(window); }; 
        }
        decorationElements.push_back(std::move(btn));
    }
    updateDecorationPositions();
}

void GUILibrary::updateDecorationPositions() {
    int w, h;
    glfwGetWindowSize(window, &w, &h);
    float aspect = (float)w / (float)h;

    float unitWidth = 0.06f; 
    float btnHeight = decorationConfig.height * 0.8f;
    float topY = 1.0f - decorationConfig.height;

    if (decorationElements.size() >= 3) {
        decorationElements[0]->config.pos = {aspect - unitWidth * 1.1f, topY + (decorationConfig.height - btnHeight)/2.0f};
        decorationElements[0]->config.size = {unitWidth, btnHeight};
        decorationElements[1]->config.pos = {aspect - unitWidth * 2.2f, topY + (decorationConfig.height - btnHeight)/2.0f};
        decorationElements[1]->config.size = {unitWidth, btnHeight};
        decorationElements[2]->config.pos = {aspect - unitWidth * 3.3f, topY + (decorationConfig.height - btnHeight)/2.0f};
        decorationElements[2]->config.size = {unitWidth, btnHeight};
    }

    // Update Menu Bar positions
    float menuY = topY;
    if (menuBarConfig.visible) {
        menuY -= menuBarConfig.height;
        float currentX = -aspect + 0.01f;
        for (auto& btn : menuBarButtons) {
            btn->config.pos = {currentX, menuY};
            currentX += btn->config.size.x + 0.01f;
        }
    }
}

void GUILibrary::handleWindowDragging() {
    if (!decorationConfig.enabled) return;
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    int w, h;
    glfwGetWindowSize(window, &w, &h);
    float normalizedY = (float)(1.0 - (my / (h / 2.0)));
    bool isDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

    if (isDown && !isDragging) {
        if (normalizedY >= (1.0f - (decorationConfig.height * 2.0f))) {
             if (glfwGetWindowAttrib(window, GLFW_MAXIMIZED)) glfwRestoreWindow(window);
             isDragging = true;
             glfwGetCursorPos(window, &dragStartX, &dragStartY);
        }
    }

    if (isDragging) {
        if (isDown) {
            double currentX, currentY;
            glfwGetCursorPos(window, &currentX, &currentY);
            int wx, wy;
            glfwGetWindowPos(window, &wx, &wy);
            int deltaX = (int)(currentX - dragStartX);
            int deltaY = (int)(currentY - dragStartY);
            if (deltaX != 0 || deltaY != 0) glfwSetWindowPos(window, wx + deltaX, wy + deltaY);
        } else isDragging = false;
    }
}

void GUILibrary::addElement(std::unique_ptr<Shape> element) {
    elements.push_back(std::move(element));
}

void GUILibrary::run() {
    if (!initialized) return;
    while (!glfwWindowShouldClose(window)) {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        updateDecorationPositions();
        RenderManager::clear({0.15f, 0.15f, 0.15f, 1.0f});
        RenderManager::setScrollOffset({0, 0});

        for (auto& element : elements) element->update();
        for (auto& deco : decorationElements) deco->update();
        for (auto& menuBtn : menuBarButtons) if (menuBarConfig.visible) menuBtn->update();
        
        handleWindowDragging();

        for (auto& element : elements) element->draw();

        if (decorationConfig.enabled) {
            float aspect = (float)width / (float)height;
            // Title Bar Background
            PrimitiveConfig bar;
            bar.pos = {-aspect, 1.0f - decorationConfig.height};
            bar.size = {aspect * 2.0f, decorationConfig.height};
            bar.backgroundColor = decorationConfig.backgroundColor;
            bar.posMode = PositionMode::Fixed;
            RenderManager::drawRect(bar);

            // Title Text (Left aligned)
            RenderManager::drawText(decorationConfig.title, {-aspect + 0.02f, 1.0f - decorationConfig.height}, 0.4f, decorationConfig.titleColor, HorizontalAlignment::Left, VerticalAlignment::Center, {aspect, decorationConfig.height});

            // Menu Bar Background
            if (menuBarConfig.visible) {
                PrimitiveConfig menuBar;
                menuBar.pos = {-aspect, 1.0f - decorationConfig.height - menuBarConfig.height};
                menuBar.size = {aspect * 2.0f, menuBarConfig.height};
                menuBar.backgroundColor = menuBarConfig.backgroundColor;
                menuBar.posMode = PositionMode::Fixed;
                RenderManager::drawRect(menuBar);
                
                for (auto& menuBtn : menuBarButtons) menuBtn->draw();
            }
        }
        for (auto& deco : decorationElements) deco->draw();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}

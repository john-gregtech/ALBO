#pragma once
#include <glad/gl.h>
#include <vector>
#include <string>
#include <functional>
#include <algorithm>

struct Color { 
    float r, g, b, a; 
    static Color White() { return {1.0f, 1.0f, 1.0f, 1.0f}; }
    static Color Black() { return {0.0f, 0.0f, 0.0f, 1.0f}; }
    static Color Gray() { return {0.5f, 0.5f, 0.5f, 1.0f}; }
    static Color Transparent() { return {0.0f, 0.0f, 0.0f, 0.0f}; }
    static Color Red() { return {1.0f, 0.0f, 0.0f, 1.0f}; }
    static Color Green() { return {0.0f, 1.0f, 0.0f, 1.0f}; }
    static Color Blue() { return {0.0f, 0.0f, 1.0f, 1.0f}; }
};

struct Vec2 { float x, y; };

enum class PositionMode {
    Static,
    Absolute,
    Fixed
};

enum class HorizontalAlignment {
    Left,
    Center,
    Right
};

enum class VerticalAlignment {
    Top,
    Center,
    Bottom
};

struct PrimitiveConfig {
    Vec2 pos = {0, 0};
    Vec2 size = {0, 0};
    PositionMode posMode = PositionMode::Static;
    
    Color foregroundColor = Color::Black();
    Color backgroundColor = Color::Gray();
    
    float borderThickness = 0.0f;
    Color borderColor = Color::Transparent();
    float borderRadius = 0.0f;
    
    float internalMargin = 0.0f;
    
    std::string text = "";
    float textScale = 0.5f;
    HorizontalAlignment hAlign = HorizontalAlignment::Left;
    VerticalAlignment vAlign = VerticalAlignment::Center;
    
    // Callbacks
    std::function<void()> onClick = nullptr;
    std::function<void()> onPressDown = nullptr;
    std::function<void()> onPressUp = nullptr;
    std::function<void()> onHover = nullptr;
    
    // States
    bool isPressed = false;
    bool isHovered = false;
    
    // Specialized
    std::string placeholder = "";
    std::string* dataField = nullptr;
    unsigned int textureID = 0;

    // InputField Specific
    bool isMultiline = false;
    char maskChar = '\0';
    Vec2 internalScroll = {0, 0};
};

class RenderManager {
public:
    static void init();
    static void clear(Color color);
    static void setScrollOffset(Vec2 offset) { scrollOffset = offset; }
    static Vec2 getScrollOffset() { return scrollOffset; }
    
    static void drawRect(const PrimitiveConfig& config);
    static void drawCircle(Vec2 pos, float radius, Color color, int segments = 32);
    static void drawTriangle(Vec2 p1, Vec2 p2, Vec2 p3, Color color);
    
    static void drawText(const std::string& text, Vec2 pos, float scale, Color color, 
                         HorizontalAlignment hAlign = HorizontalAlignment::Left, 
                         VerticalAlignment vAlign = VerticalAlignment::Top,
                         Vec2 boxSize = {0,0});
    static void drawImage(unsigned int textureID, Vec2 pos, Vec2 size);
    static unsigned int loadTexture(const char* path);

private:
    static unsigned int shaderProgram;
    static unsigned int vao, vbo;
    static Vec2 scrollOffset;
    static unsigned int createShader(const char* vertexSrc, const char* fragmentSrc);
};

class Shape {
public:
    PrimitiveConfig config;
    virtual void update() {}
    virtual void draw() = 0;
    virtual ~Shape() = default;

protected:
    bool wasMouseDownLastFrame = false;
    bool wasHoveredLastFrame = false;
};

class Rect : public Shape {
public:
    void draw() override { RenderManager::drawRect(config); }
};

class Circle : public Shape {
public:
    float radius = 0.1f;
    void draw() override { RenderManager::drawCircle(config.pos, radius, config.backgroundColor); }
};

class Triangle : public Shape {
public:
    Vec2 p1, p2, p3;
    void draw() override { RenderManager::drawTriangle(p1, p2, p3, config.backgroundColor); }
};

class Image : public Shape {
public:
    void draw() override { RenderManager::drawImage(config.textureID, config.pos, config.size); }
};

class Button : public Shape {
public:
    bool checkHover();
    void update() override;
    void draw() override;
};

class InputField : public Shape {
public:
    bool isFocused = false;
    size_t cursorIndex = 0;
    void update() override;
    void draw() override;
};

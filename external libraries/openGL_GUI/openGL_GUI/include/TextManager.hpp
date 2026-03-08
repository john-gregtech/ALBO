#pragma once
#include <glad/gl.h>
#include <map>
#include <string>
#include "RenderManager.hpp"

struct Character {
    unsigned int TextureID; 
    Vec2         Size;      
    Vec2         Bearing;   
    unsigned int Advance;   
};

class TextManager {
public:
    static void init(const std::string& fontPath);
    static void renderText(const std::string& text, Vec2 pos, float scale, Color color, 
                           HorizontalAlignment hAlign = HorizontalAlignment::Left, 
                           VerticalAlignment vAlign = VerticalAlignment::Top,
                           Vec2 boxSize = {0,0});
    static Vec2 getTextSize(const std::string& text, float scale);
    static Vec2 getCursorCoords(const std::string& text, size_t cursorIndex, Vec2 pos, float scale,
                                HorizontalAlignment hAlign = HorizontalAlignment::Left,
                                VerticalAlignment vAlign = VerticalAlignment::Top,
                                Vec2 boxSize = {0,0});

private:
    static std::map<char, Character> Characters;
    static unsigned int vao, vbo;
    static unsigned int shaderProgram;
    static float lineHeight;
};

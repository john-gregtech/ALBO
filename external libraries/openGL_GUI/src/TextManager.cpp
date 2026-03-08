#include "TextManager.hpp"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <iostream>
#include <GLFW/glfw3.h>
#include <vector>
#include <sstream>

std::map<char, Character> TextManager::Characters;
unsigned int TextManager::vao = 0;
unsigned int TextManager::vbo = 0;
unsigned int TextManager::shaderProgram = 0;
float TextManager::lineHeight = 0.0f;

const char* textVertexShader = R"(
#version 330 core
layout (location = 0) in vec4 vertex; 
out vec2 TexCoords;
uniform vec2 uResolution;
void main() {
    float aspect = uResolution.x / uResolution.y;
    gl_Position = vec4(vertex.x / aspect, vertex.y, 0.0, 1.0);
    TexCoords = vertex.zw;
})";

const char* textFragmentShader = R"(
#version 330 core
in vec2 TexCoords;
out vec4 color;
uniform sampler2D text;
uniform vec3 textColor;
void main() {    
    vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, TexCoords).r);
    color = vec4(textColor, 1.0) * sampled;
})";

void TextManager::init(const std::string& fontPath) {
    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        std::cerr << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
        return;
    }

    FT_Face face;
    if (FT_New_Face(ft, fontPath.c_str(), 0, &face)) {
        std::cerr << "ERROR::FREETYPE: Failed to load font: " << fontPath << std::endl;
        FT_Done_FreeType(ft);
        return;
    }

    FT_Set_Pixel_Sizes(face, 0, 48);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); 

    for (unsigned char c = 0; c < 128; c++) {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
            std::cerr << "ERROR::FREETYTPE: Failed to load Glyph" << std::endl;
            continue;
        }
        unsigned int texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, face->glyph->bitmap.width, face->glyph->bitmap.rows, 0, GL_RED, GL_UNSIGNED_BYTE, face->glyph->bitmap.buffer);
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        Character character = {
            texture, 
            { (float)face->glyph->bitmap.width, (float)face->glyph->bitmap.rows },
            { (float)face->glyph->bitmap_left, (float)face->glyph->bitmap_top },
            (unsigned int)face->glyph->advance.x
        };
        Characters.insert(std::pair<char, Character>(c, character));
    }
    lineHeight = (float)face->size->metrics.height / 64.0f;
    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    // Shaders
    unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &textVertexShader, NULL);
    glCompileShader(vs);
    unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &textFragmentShader, NULL);
    glCompileShader(fs);
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vs);
    glAttachShader(shaderProgram, fs);
    glLinkProgram(shaderProgram);
    glDeleteShader(vs);
    glDeleteShader(fs);

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

Vec2 TextManager::getTextSize(const std::string& text, float scale) {
    if (Characters.empty()) return {0, 0};
    float baseScale = 0.002f; 
    float totalW = 0;
    float currentW = 0;
    int lineCount = 0;

    if (text.empty()) return {0, lineHeight * baseScale * scale};

    std::stringstream ss(text);
    std::string line;
    while (std::getline(ss, line, '\n')) {
        lineCount++;
        float lw = 0;
        for (auto const& c : line) {
            lw += (Characters[c].Advance >> 6) * baseScale * scale;
        }
        totalW = std::max(totalW, lw);
    }
    if (text.back() == '\n') lineCount++;

    return {totalW, lineCount * lineHeight * baseScale * scale};
}

Vec2 TextManager::getCursorCoords(const std::string& text, size_t cursorIndex, Vec2 pos, float scale,
                                HorizontalAlignment hAlign, VerticalAlignment vAlign, Vec2 boxSize) {
    if (Characters.empty()) return pos;
    float baseScale = 0.002f;
    float unitLineHeight = lineHeight * baseScale * scale;

    std::vector<std::string> lines;
    std::stringstream ss(text);
    std::string line;
    while (std::getline(ss, line, '\n')) {
        lines.push_back(line);
    }
    if (!text.empty() && text.back() == '\n') lines.push_back("");
    if (text.empty()) lines.push_back("");

    Vec2 totalSize = getTextSize(text, scale);

    float startY = pos.y;
    if (boxSize.y > 0) {
        if (vAlign == VerticalAlignment::Top) startY = pos.y + boxSize.y - unitLineHeight;
        else if (vAlign == VerticalAlignment::Center) startY = pos.y + (boxSize.y + totalSize.y) / 2.0f - unitLineHeight;
        else if (vAlign == VerticalAlignment::Bottom) startY = pos.y + totalSize.y - unitLineHeight;
    }

    // Find which line the cursor is on
    size_t currentIdx = 0;
    float currentY = startY;
    for (const auto& l : lines) {
        size_t lineLen = l.length();
        // If cursor is within this line or at the end of it
        if (cursorIndex >= currentIdx && cursorIndex <= currentIdx + lineLen) {
            float currentX = pos.x;
            if (boxSize.x > 0) {
                float lw = 0;
                for(auto c : l) lw += (Characters[c].Advance >> 6) * baseScale * scale;
                if (hAlign == HorizontalAlignment::Center) currentX = pos.x + (boxSize.x - lw) / 2.0f;
                else if (hAlign == HorizontalAlignment::Right) currentX = pos.x + (boxSize.x - lw);
            }

            // Calculate X offset within line
            float xOffset = 0;
            std::string sub = l.substr(0, cursorIndex - currentIdx);
            for(char c : sub) xOffset += (Characters[c].Advance >> 6) * baseScale * scale;

            return {currentX + xOffset, currentY};
        }
        currentIdx += lineLen + 1; // +1 for the newline character
        currentY -= unitLineHeight;
    }

    return pos;
}

void TextManager::renderText(const std::string& text, Vec2 pos, float scale, Color color, 
                             HorizontalAlignment hAlign, VerticalAlignment vAlign, Vec2 boxSize) {
    if (Characters.empty()) return;
    
    glUseProgram(shaderProgram);
    glUniform3f(glGetUniformLocation(shaderProgram, "textColor"), color.r, color.g, color.b);
    int winW, winH;
    glfwGetWindowSize(glfwGetCurrentContext(), &winW, &winH);
    glUniform2f(glGetUniformLocation(shaderProgram, "uResolution"), (float)winW, (float)winH);

    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(vao);

    float baseScale = 0.002f;
    
    std::vector<std::string> lines;
    std::stringstream ss(text);
    std::string line;
    while (std::getline(ss, line, '\n')) {
        lines.push_back(line);
    }
    if (!text.empty() && text.back() == '\n') lines.push_back("");
    if (text.empty()) lines.push_back("");

    Vec2 totalSize = getTextSize(text, scale);
    float unitLineHeight = lineHeight * baseScale * scale;

    // Calculate initial Y (baseline of the first line)
    float startY = pos.y;
    if (boxSize.y > 0) {
        if (vAlign == VerticalAlignment::Top) {
            startY = pos.y + boxSize.y - unitLineHeight;
        } else if (vAlign == VerticalAlignment::Center) {
            startY = pos.y + (boxSize.y + totalSize.y) / 2.0f - unitLineHeight;
        } else if (vAlign == VerticalAlignment::Bottom) {
            startY = pos.y + totalSize.y - unitLineHeight;
        }
    }

    float currentY = startY;

    for (const auto& l : lines) {
        float currentX = pos.x;
        if (boxSize.x > 0) {
            float lw = 0;
            for(auto c : l) lw += (Characters[c].Advance >> 6) * baseScale * scale;
            
            if (hAlign == HorizontalAlignment::Center) {
                currentX = pos.x + (boxSize.x - lw) / 2.0f;
            } else if (hAlign == HorizontalAlignment::Right) {
                currentX = pos.x + (boxSize.x - lw);
            }
        }

        float x = currentX;
        for (auto const& c : l) {
            Character ch = Characters[c];
            float xpos = x + ch.Bearing.x * baseScale * scale;
            float ypos = currentY - (ch.Size.y - ch.Bearing.y) * baseScale * scale;
            float w = ch.Size.x * baseScale * scale;
            float h = ch.Size.y * baseScale * scale;

            float vertices[6][4] = {
                { xpos,     ypos + h,   0.0f, 0.0f },            
                { xpos,     ypos,       0.0f, 1.0f },
                { xpos + w, ypos,       1.0f, 1.0f },
                { xpos,     ypos + h,   0.0f, 0.0f },
                { xpos + w, ypos,       1.0f, 1.0f },
                { xpos + w, ypos + h,   1.0f, 0.0f }           
            };
            glBindTexture(GL_TEXTURE_2D, ch.TextureID);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices); 
            glDrawArrays(GL_TRIANGLES, 0, 6);
            x += (ch.Advance >> 6) * baseScale * scale;
        }
        currentY -= unitLineHeight; 
    }
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

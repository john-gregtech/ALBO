#include "RenderManager.hpp"
#include "InputManager.hpp"
#include "TextManager.hpp"
#include <GLFW/glfw3.h>
#include <cmath>
#include <iostream>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

unsigned int RenderManager::loadTexture(const char* path) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char *data = stbi_load(path, &width, &height, &nrChannels, 0);
    if (data) {
        GLenum format = GL_RGB;
        if (nrChannels == 4) format = GL_RGBA;

        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    } else {
        std::cerr << "Failed to load texture: " << path << std::endl;
    }
    stbi_image_free(data);
    return textureID;
}

unsigned int RenderManager::shaderProgram = 0;
unsigned int RenderManager::vao = 0;
unsigned int RenderManager::vbo = 0;
static unsigned int textureShaderProgram = 0;
static unsigned int sdfRectShaderProgram = 0;
Vec2 RenderManager::scrollOffset = {0, 0};

const char* sdfRectVertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
out vec2 FragPos;
uniform vec2 uPos;
uniform vec2 uSize;
uniform vec2 uResolution;
void main() {
    float aspect = uResolution.x / uResolution.y;
    gl_Position = vec4(aPos.x / aspect, aPos.y, 0.0, 1.0);
    FragPos = aPos;
})";

const char* sdfRectFragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
in vec2 FragPos;
uniform vec2 uPos;
uniform vec2 uSize;
uniform float uRadius;
uniform vec4 uBgColor;
uniform vec4 uBorderColor;
uniform float uBorderThickness;

float sdRoundedBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + r;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

void main() {
    vec2 center = uPos + uSize * 0.5;
    vec2 halfSize = uSize * 0.5;
    float dist = sdRoundedBox(FragPos - center, halfSize, uRadius);
    
    float smoothing = 0.002;
    float fillAlpha = 1.0 - smoothstep(-smoothing, smoothing, dist);
    
    // Border logic
    float borderDist = abs(dist + uBorderThickness * 0.5) - uBorderThickness * 0.5;
    float borderAlpha = 1.0 - smoothstep(-smoothing, smoothing, borderDist);

    vec4 color = uBgColor;
    color.a *= fillAlpha;
    
    if (uBorderThickness > 0.0) {
        vec4 bColor = uBorderColor;
        bColor.a *= borderAlpha;
        // Blend border over background
        color = mix(color, bColor, borderAlpha);
    }

    if (color.a <= 0.0) discard;
    FragColor = color;
})";

const char* textureVertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;
out vec2 TexCoord;
uniform vec2 uResolution;
void main() {
    float aspect = uResolution.x / uResolution.y;
    gl_Position = vec4(aPos.x / aspect, aPos.y, 0.0, 1.0);
    TexCoord = aTexCoord;
})";

const char* textureFragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
in vec2 TexCoord;
uniform sampler2D uTexture;
void main() {
    FragColor = texture(uTexture, TexCoord);
})";

const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
uniform vec2 uResolution;
void main() {
    // Convert NDC to pixel space, apply aspect ratio correction, or just keep it simple:
    // We want the Y axis to stay -1 to 1, but X axis to be relative to aspect ratio
    float aspect = uResolution.x / uResolution.y;
    gl_Position = vec4(aPos.x / aspect, aPos.y, 0.0, 1.0);
})";

const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
uniform vec4 uColor;
void main() {
    FragColor = uColor;
})";

void RenderManager::init() {
    shaderProgram = createShader(vertexShaderSource, fragmentShaderSource);
    textureShaderProgram = createShader(textureVertexShaderSource, textureFragmentShaderSource);
    sdfRectShaderProgram = createShader(sdfRectVertexShaderSource, sdfRectFragmentShaderSource);
    
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void RenderManager::clear(Color color) {
    glClearColor(color.r, color.g, color.b, color.a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void RenderManager::drawRect(const PrimitiveConfig& config) {
    float pos_x = config.pos.x;
    float pos_y = config.pos.y;

    if (config.posMode == PositionMode::Static) {
        pos_x += RenderManager::scrollOffset.x;
        pos_y += RenderManager::scrollOffset.y;
    }

    float width = config.size.x;
    float height = config.size.y;

    // Use full quad, fragment shader will handle the rounding/clipping
    float vertices[] = {
        pos_x, pos_y,
        pos_x + width, pos_y,
        pos_x + width, pos_y + height,
        pos_x, pos_y,
        pos_x + width, pos_y + height,
        pos_x, pos_y + height
    };

    glUseProgram(sdfRectShaderProgram);
    int w, h;
    glfwGetWindowSize(glfwGetCurrentContext(), &w, &h);
    glUniform2f(glGetUniformLocation(sdfRectShaderProgram, "uResolution"), (float)w, (float)h);
    glUniform2f(glGetUniformLocation(sdfRectShaderProgram, "uPos"), pos_x, pos_y);
    glUniform2f(glGetUniformLocation(sdfRectShaderProgram, "uSize"), width, height);
    glUniform1f(glGetUniformLocation(sdfRectShaderProgram, "uRadius"), config.borderRadius);
    glUniform4f(glGetUniformLocation(sdfRectShaderProgram, "uBgColor"), config.backgroundColor.r, config.backgroundColor.g, config.backgroundColor.b, config.backgroundColor.a);
    glUniform4f(glGetUniformLocation(sdfRectShaderProgram, "uBorderColor"), config.borderColor.r, config.borderColor.g, config.borderColor.b, config.borderColor.a);
    glUniform1f(glGetUniformLocation(sdfRectShaderProgram, "uBorderThickness"), config.borderThickness);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void RenderManager::drawCircle(Vec2 pos, float radius, Color color, int segments) {
    // Basic circle without mode support for now
    std::vector<float> vertices;
    for (int i = 0; i <= segments; ++i) {
        float theta = 2.0f * 3.1415926f * float(i) / float(segments);
        vertices.push_back(pos.x + radius * cosf(theta));
        vertices.push_back(pos.y + radius * sinf(theta));
    }
    glUseProgram(shaderProgram);
    glUniform4f(glGetUniformLocation(shaderProgram, "uColor"), color.r, color.g, color.b, color.a);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLE_FAN, 0, segments + 1);
}

void RenderManager::drawTriangle(Vec2 p1, Vec2 p2, Vec2 p3, Color color) {
    float vertices[] = { p1.x, p1.y, p2.x, p2.y, p3.x, p3.y };
    glUseProgram(shaderProgram);
    glUniform4f(glGetUniformLocation(shaderProgram, "uColor"), color.r, color.g, color.b, color.a);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void RenderManager::drawImage(unsigned int textureID, Vec2 pos, Vec2 size) {
    float pos_x = pos.x;
    float pos_y = pos.y;

    float width = size.x;
    float height = size.y;

    float vertices[] = {
        // pos      // tex
        pos_x, pos_y,          0.0f, 1.0f,
        pos_x + width, pos_y,  1.0f, 1.0f,
        pos_x + width, pos_y + height, 1.0f, 0.0f,
        pos_x, pos_y,          0.0f, 1.0f,
        pos_x + width, pos_y + height, 1.0f, 0.0f,
        pos_x, pos_y + height, 0.0f, 0.0f
    };

    glUseProgram(textureShaderProgram);
    int w, h;
    glfwGetWindowSize(glfwGetCurrentContext(), &w, &h);
    glUniform2f(glGetUniformLocation(textureShaderProgram, "uResolution"), (float)w, (float)h);
    
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindTexture(GL_TEXTURE_2D, textureID);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    glDisableVertexAttribArray(1);
}

void RenderManager::drawText(const std::string& text, Vec2 pos, float scale, Color color, 
                         HorizontalAlignment hAlign, VerticalAlignment vAlign, Vec2 boxSize) {
    TextManager::renderText(text, pos, scale, color, hAlign, vAlign, boxSize);
}

unsigned int RenderManager::createShader(const char* vSrc, const char* fSrc) {
    unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vSrc, NULL);
    glCompileShader(vs);
    unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fSrc, NULL);
    glCompileShader(fs);
    unsigned int program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs); glDeleteShader(fs);
    return program;
}

// Button Implementation
bool Button::checkHover() {
    double mx, my;
    InputManager::getMousePos(mx, my);
    int w, h;
    glfwGetWindowSize(glfwGetCurrentContext(), &w, &h);
    float aspect = (float)w / (float)h;
    float normalizedX = (float)((mx / (w / 2.0)) - 1.0) * aspect;
    float normalizedY = (float)(1.0 - (my / (h / 2.0)));

    float finalX = config.pos.x;
    float finalY = config.pos.y;
    if (config.posMode == PositionMode::Static) {
        finalX += RenderManager::getScrollOffset().x;
        finalY += RenderManager::getScrollOffset().y;
    }

    return (normalizedX >= finalX && normalizedX <= finalX + config.size.x && 
            normalizedY >= finalY && normalizedY <= finalY + config.size.y);
}

void Button::update() {
    bool isHoveredThisFrame = checkHover();
    bool isMouseDownThisFrame = InputManager::isMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT);

    config.isHovered = isHoveredThisFrame;
    config.isPressed = isHoveredThisFrame && isMouseDownThisFrame;

    if (isHoveredThisFrame && !wasHoveredLastFrame) {
        if (config.onHover) config.onHover();
    }

    if (isHoveredThisFrame && isMouseDownThisFrame && !wasMouseDownLastFrame) {
        if (config.onPressDown) config.onPressDown();
    }

    if (wasMouseDownLastFrame && !isMouseDownThisFrame) {
        if (config.onPressUp) config.onPressUp();
        if (isHoveredThisFrame && config.onClick) {
            config.onClick();
        }
    }

    if (config.isPressed) config.backgroundColor = {0.8f, 0.2f, 0.2f, 1.0f};
    else if (config.isHovered) config.backgroundColor = {0.6f, 0.6f, 0.6f, 1.0f};
    else config.backgroundColor = {0.4f, 0.4f, 0.4f, 1.0f};

    wasHoveredLastFrame = isHoveredThisFrame;
    wasMouseDownLastFrame = isMouseDownThisFrame;
}

void Button::draw() {
    RenderManager::drawRect(config);
    
    float x = config.pos.x;
    float y = config.pos.y;

    if (config.posMode == PositionMode::Static) {
        x += RenderManager::getScrollOffset().x;
        y += RenderManager::getScrollOffset().y;
    }

    // Use full box size for alignment calculation
    RenderManager::drawText(config.text, {x, y}, config.textScale, config.foregroundColor, config.hAlign, config.vAlign, config.size);
}

// InputField Implementation
void InputField::update() {
    if (InputManager::isMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT)) {
        double mx, my;
        InputManager::getMousePos(mx, my);
        int w, h;
        glfwGetWindowSize(glfwGetCurrentContext(), &w, &h);
        float aspect = (float)w / (float)h;
        float normalizedX = (float)((mx / (w / 2.0)) - 1.0) * aspect;
        float normalizedY = (float)(1.0 - (my / (h / 2.0)));

        float finalX = config.pos.x;
        float finalY = config.pos.y;
        if (config.posMode == PositionMode::Static) {
            finalX += RenderManager::getScrollOffset().x;
            finalY += RenderManager::getScrollOffset().y;
        }

        bool previouslyFocused = isFocused;
        isFocused = (normalizedX >= finalX && normalizedX <= finalX + config.size.x && 
                     normalizedY >= finalY && normalizedY <= finalY + config.size.y);
        
        if (isFocused && !previouslyFocused && config.dataField) {
            cursorIndex = config.dataField->length();
        }
    }

    if (isFocused && config.dataField) {
        std::string& data = *(config.dataField);
        
        // Ensure cursor is in bounds
        if (cursorIndex > data.length()) cursorIndex = data.length();

        // Handle character input at cursor
        std::string input = InputManager::getAndClearCharBuffer();
        if (!input.empty()) {
            data.insert(cursorIndex, input);
            cursorIndex += input.length();
        }

        // Handle Enter for multiline
        if (config.isMultiline && InputManager::isKeyPressed(GLFW_KEY_ENTER)) {
            data.insert(cursorIndex, "\n");
            cursorIndex++;
        }

        // Handle Backspace
        if (InputManager::isKeyPressed(GLFW_KEY_BACKSPACE)) {
            if (cursorIndex > 0) {
                data.erase(cursorIndex - 1, 1);
                cursorIndex--;
            }
        }

        // Handle Delete
        if (InputManager::isKeyPressed(GLFW_KEY_DELETE)) {
            if (cursorIndex < data.length()) {
                data.erase(cursorIndex, 1);
            }
        }

        // Handle Arrow Left/Right
        if (InputManager::isKeyPressed(GLFW_KEY_LEFT)) {
            if (cursorIndex > 0) cursorIndex--;
        }
        if (InputManager::isKeyPressed(GLFW_KEY_RIGHT)) {
            if (cursorIndex < data.length()) cursorIndex++;
        }

        // Handle Arrow Up/Down (Multiline logic)
        if (config.isMultiline) {
            if (InputManager::isKeyPressed(GLFW_KEY_UP)) {
                // Find start of current line
                size_t lineStart = data.find_last_of('\n', cursorIndex == 0 ? 0 : cursorIndex - 1);
                if (lineStart != std::string::npos) {
                    size_t col = cursorIndex - (lineStart + 1);
                    // Find start of previous line
                    size_t prevLineStart = (lineStart == 0) ? std::string::npos : data.find_last_of('\n', lineStart - 1);
                    size_t prevLineActualStart = (prevLineStart == std::string::npos) ? 0 : prevLineStart + 1;
                    size_t prevLineLen = lineStart - prevLineActualStart;
                    cursorIndex = prevLineActualStart + std::min(col, prevLineLen);
                } else if (cursorIndex > 0) {
                    // We are on first line, but maybe there is no \n yet or we are at start
                    // No previous line to go to
                }
            }
            if (InputManager::isKeyPressed(GLFW_KEY_DOWN)) {
                size_t nextLineStart = data.find('\n', cursorIndex);
                if (nextLineStart != std::string::npos) {
                    size_t lineStart = data.find_last_of('\n', cursorIndex == 0 ? 0 : cursorIndex - 1);
                    size_t actualLineStart = (lineStart == std::string::npos) ? 0 : lineStart + 1;
                    size_t col = cursorIndex - actualLineStart;
                    
                    size_t followingLineEnd = data.find('\n', nextLineStart + 1);
                    if (followingLineEnd == std::string::npos) followingLineEnd = data.length();
                    
                    size_t nextLineLen = followingLineEnd - (nextLineStart + 1);
                    cursorIndex = (nextLineStart + 1) + std::min(col, nextLineLen);
                }
            }
        }
    }
}

void InputField::draw() {
    PrimitiveConfig drawCfg = config;
    if (isFocused) {
        drawCfg.borderColor = {0.2f, 0.5f, 1.0f, 1.0f};
        if (drawCfg.borderThickness < 0.002f) drawCfg.borderThickness = 0.002f;
    }
    RenderManager::drawRect(drawCfg);

    // Setup Clipping (glScissor uses screen pixels)
    int winW, winH;
    glfwGetWindowSize(glfwGetCurrentContext(), &winW, &winH);
    float aspect = (float)winW / (float)winH;

    // Convert NDC to Screen Space for Scissor
    float margin = config.internalMargin;
    float clipX = ((config.pos.x + margin) / aspect + 1.0f) / 2.0f * (float)winW;
    float clipY = (config.pos.y + margin + 1.0f) / 2.0f * (float)winH;
    float clipW = ((config.size.x - 2.0f * margin) / aspect) / 2.0f * (float)winW;
    float clipH = (config.size.y - 2.0f * margin) / 2.0f * (float)winH;

    if (clipW > 0 && clipH > 0) {
        glEnable(GL_SCISSOR_TEST);
        glScissor((int)clipX, (int)clipY, (int)clipW, (int)clipH);

        float textX = config.pos.x + margin + 0.01f;
        float textY = config.pos.y + (config.size.y / 2.0f) - 0.015f;

        if (config.posMode == PositionMode::Static) {
            textX += RenderManager::getScrollOffset().x;
            textY += RenderManager::getScrollOffset().y;
        }

        if (config.dataField && !config.dataField->empty()) {
            std::string displayText = *config.dataField;
            if (config.maskChar != '\0') {
                displayText = std::string(displayText.length(), config.maskChar);
            }
            RenderManager::drawText(displayText, {config.pos.x + margin, config.pos.y + margin}, 0.5f, config.foregroundColor, config.hAlign, config.vAlign, {config.size.x - 2*margin, config.size.y - 2*margin});
            
            // Render Cursor if focused
            if (isFocused) {
                std::string textBeforeCursor = displayText.substr(0, cursorIndex);
                // We need to handle multiline for cursor pos too
                size_t lastNL = textBeforeCursor.find_last_of('\n');
                std::string currentLineBefore = (lastNL == std::string::npos) ? textBeforeCursor : textBeforeCursor.substr(lastNL + 1);
                int lineCountBefore = 0;
                for(char c : textBeforeCursor) if(c == '\n') lineCountBefore++;

                Vec2 beforeSize = TextManager::getTextSize(currentLineBefore, 0.5f);
                Vec2 fullTextSize = TextManager::getTextSize(displayText, 0.5f);
                
                // This is a rough alignment matching TextManager's internal logic
                float cursorX = config.pos.x + margin + beforeSize.x;
                float cursorY = config.pos.y + margin + (config.size.y + fullTextSize.y)/2.0f - (lineCountBefore + 1) * (fullTextSize.y / (float)(std::count(displayText.begin(), displayText.end(), '\n') + 1));
                
                // Adjust for alignment (assuming Left/Top for simplicity in cursor math for now)
                // In a full impl, we'd replicate the alignment logic exactly.
                
                PrimitiveConfig cursorCfg;
                cursorCfg.pos = {cursorX, cursorY};
                cursorCfg.size = {0.002f, 0.04f}; // Thin line
                cursorCfg.backgroundColor = config.foregroundColor;
                cursorCfg.posMode = config.posMode;
                RenderManager::drawRect(cursorCfg);
            }
        } else {
            RenderManager::drawText(config.placeholder, {config.pos.x + margin, config.pos.y + margin}, 0.5f, {0.5f, 0.5f, 0.5f, 1.0f}, config.hAlign, config.vAlign, {config.size.x - 2*margin, config.size.y - 2*margin});
            
            if (isFocused) {
                PrimitiveConfig cursorCfg;
                cursorCfg.pos = {config.pos.x + margin, config.pos.y + config.size.y/2.0f - 0.02f};
                cursorCfg.size = {0.002f, 0.04f};
                cursorCfg.backgroundColor = config.foregroundColor;
                cursorCfg.posMode = config.posMode;
                RenderManager::drawRect(cursorCfg);
            }
        }

        glDisable(GL_SCISSOR_TEST);
    }
}

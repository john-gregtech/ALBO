# OpenGL GUI Library (C++20)

A lightweight, targeted OpenGL wrapper designed for modern GUI design. Features high-quality SDF rendering for rounded corners, a robust event-driven component system, and full FreeType text support.

## Features
- **Modern Rendering:** SDF-based primitives with anti-aliased rounded corners and borders.
- **Advanced Text:** Full multiline support with Horizontal (Left/Center/Right) and Vertical (Top/Center/Bottom) alignment.
- **Interactive Components:** Custom Buttons and Input Fields (with masking, cursors, and navigation).
- **Custom Windowing:** Support for borderless windows with a built-in draggable title bar and system controls.
- **Cross-Platform:** Built using CMake with automated dependency management via FetchContent.

## Prerequisites
- **C++ Standard:** C++20 or higher.
- **Graphics API:** OpenGL 3.3+ Core Profile.
- **Build System:** CMake 3.26+.
- **Dependencies:** GLFW, GLAD, FreeType, stb_image (All except GLAD are fetched automatically via CMake).

## Integration Guide

### 1. Project Structure
To use this as a library, include the following directories in your project:
- `include/`: All `.hpp` header files.
- `src/`: All `.cpp` source files.
- `deps/glad/`: Local GLAD loader.
- `res/`: Resource files (fonts).

### 2. CMake Setup
Add the library directory to your project and link against the `OpenGLGUI` target. This will automatically include all necessary headers and dependencies (GLFW, FreeType, etc.).

```cmake
# In your project's CMakeLists.txt
add_subdirectory(path/to/openGL_GUI)

# Link to your executable
target_link_libraries(your_project PRIVATE OpenGLGUI)
```

### 3. Basic Usage Example

```cpp
#include "GUILibrary.hpp"

int main() {
    // 1. Initialize with font support
    std::string fontPath = "res/fonts/NotoSans-Regular.ttf";
    GUILibrary gui(1280, 720, "My Application", true, fontPath);

    // 2. Create a component configuration
    auto btn = std::make_unique<Button>();
    btn->config.pos = {-0.2f, -0.1f};
    btn->config.size = {0.4f, 0.2f};
    btn->config.text = "Click Me";
    btn->config.backgroundColor = Color::Blue();
    btn->config.borderRadius = 0.02f; // Smooth rounded corners
    
    // 3. Attach a callback
    btn->config.onClick = []() {
        std::cout << "Action Triggered!" << std::endl;
    };

    // 4. Add to the GUI stack
    gui.addElement(std::move(btn));

    // 5. Run the main loop
    gui.run();

    return 0;
}
```

## Component Reference

### PrimitiveConfig
All components are styled using the `PrimitiveConfig` struct:
- `pos`: Vec2 coordinates (-1.0 to 1.0, aspect-corrected).
- `size`: Vec2 dimensions.
- `backgroundColor` / `foregroundColor`: Color with alpha support.
- `borderThickness` / `borderColor`: Customizable borders.
- `borderRadius`: Controls corner curvature (0.0 = sharp, >0.0 = rounded).
- `hAlign` / `vAlign`: Text alignment enums.

### Supported Components
- `Rect`: Basic shape for panels and backgrounds.
- `Circle`: Basic circular primitive.
- `Triangle`: Basic three-point primitive.
- `Button`: Interactive element with hover/click states.
- `InputField`: Full text entry supporting multiline and password masking (`maskChar`).
- `Image`: Texture rendering primitive.

## Custom Window Decorations
If using a borderless window (`decorated = false` in constructor), enable the custom title bar:

```cpp
WindowDecoration deco;
deco.enabled = true;
deco.title = "Custom App";
deco.height = 0.07f;
gui.enableCustomDecoration(deco);
```

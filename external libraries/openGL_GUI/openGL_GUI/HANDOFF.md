# OpenGL GUI Library Project Handoff

## Current Status
The project has evolved into a feature-rich, event-driven C++20 GUI library.
*   **SDF Rendering:** Completed migration to SDF-based primitive rendering, enabling high-quality **Rounded Corners** and pixel-perfect borders.
*   **Advanced Text:** Full alignment control (H/V) and multiline support are implemented.
*   **Interactive Menu Bar:** Dynamic menu bar that automatically sizes itself and its buttons based on text content.
*   **Robust Input:** Input fields now support cursors, arrow-key navigation, text masking, and multiline editing.
*   **Window Management:** Precise borderless window dragging and functional system controls (Min/Max/Close).

## Project Architecture
The project is split into two distinct CMake targets:
1.  **OpenGLGUI (Static Library):** Contains the core logic and components. It excludes `main.cpp` so it can be linked into other projects without `main` function conflicts.
2.  **OpenGLGUI_Demo (Executable):** A demonstration application that uses `main.cpp` to showcase library features.

### Implemented Files:
*   `GUILibrary.hpp/cpp`: High-level entry point, manages window state and custom title/menu bars.
*   `RenderManager.hpp/cpp`: Modern SDF-based primitive renderer.
*   `TextManager.hpp/cpp`: FreeType-based glyph engine with alignment and measurement logic.
*   `InputManager.hpp/cpp`: Unified event stream for mouse, keyboard, and character data.
*   `main.cpp`: Demo application code (not part of the library target).

## Immediate Next Steps
1.  **Coordinate Mapping:** Introduce a `CoordinateSystem` enum or toggle to allow users to work in **Pixels** instead of NDC units.
2.  **Z-Indexing:** Add a `zIndex` to `PrimitiveConfig` and sort the element stack before rendering to ensure correct layering.
3.  **Dropdown Components:** Develop a `Dropdown` class that leverages the new Z-indexing to show menu lists on top of the UI.
4.  **Cursor System:** Implement `glfwSetCursor` logic within the `GUILibrary` main loop to provide visual feedback (pointers vs text beams).

## Technical Details & Build
*   **Build Command:** `cmake -S . -B build -G "MinGW Makefiles"` then `cmake --build build`.
*   **Dependencies:** Managed via `FetchContent` (GLFW, FreeType, STB). GLAD is provided locally in `deps/glad`.
*   **Standard:** C++20.

# OpenGL GUI Library Project Outline

## Objective
Create a targeted C++20 OpenGL library wrapper specifically designed for GUI development, supporting both Windows and Linux. The library provides high-level abstractions via a unified configuration system while maintaining performance and cross-platform compatibility.

## Key Features

### 1. Rendering Capabilities
*   **Unified Primitive Styling:** All elements are configured via `PrimitiveConfig`, supporting:
    *   Foreground/Background colors with full Alpha Blending.
    *   Customizable borders (thickness and color).
    *   Internal margins for content spacing.
    *   **SDF Rounded Corners:** Smooth, anti-aliased rounded corners using Signed Distance Fields.
    *   **Positioning Modes:** Support for `Static` (scrollable), `Fixed` (locked to viewport), and `Absolute`.
*   **Primitive Shapes:** Rectangles, Circles, and Triangles.
*   **Text Rendering:** Full FreeType integration with **Horizontal and Vertical Alignment** and **Multiline** support.
*   **Image Support:** stb_image integration for loading and rendering textures.

### 2. GUI Components & Event System
*   **Granular Event Callbacks:** `onPressDown`, `onPressUp`, `onClick`, and `onHover`.
*   **State Tracking:** Persistent `isPressed` and `isHovered` states.
*   **Input Fields:** Interactive text entry with placeholder support, **Masking (passwords)**, **Interactive Cursor**, and **Arrow-key Navigation**.
*   **Custom Window Decoration:** Integrated title bar with Close, Maximize, and Minimize controls, plus a functional window drag zone.
*   **Menu Bar:** Dynamic menu bar with auto-scaling text and auto-sizing buttons based on content.

### 3. Architecture & Performance
*   **Encapsulation:** `GUILibrary` class manages the lifecycle, window hints, and element stack.
*   **Aspect Ratio Correction:** Automatic coordinate normalization ensures UI elements maintain proportions across different window sizes.
*   **Real-time Updating:** Non-blocking loop for dynamic UI feedback.
*   **Multithreaded Foundation:** Supported via external data binding.

## Implementation Phases

### Phase 1: Foundation (Complete)
*   [x] Basic GLFW/GLAD setup.
*   [x] Unified `PrimitiveConfig` architecture.
*   [x] Alpha blending and transparency support.
*   [x] Borderless window support.
*   [x] Custom window dragging and controls.

### Phase 2: Core GUI Features (Complete)
*   [x] FreeType glyph rendering with alignment.
*   [x] Aspect-corrected coordinate system.
*   [x] Texture/image loading and rendering.
*   [x] Keyboard input handling for `InputField`.
*   [x] Dynamic Menu Bar system.

### Phase 3: Advanced Features (In Progress)
*   [x] Implement Shader-based Rounded Corners (SDF).
*   [ ] Coordinate mapping system (Pixel space to OpenGL space).
*   [ ] Z-indexing for element layering.
*   [ ] Dropdown/Popup menu components.

### Phase 4: Finalization
*   [ ] Static/Shared library packaging.
*   [ ] Linux/Windows cross-validation.
*   [ ] Documentation and integration examples.

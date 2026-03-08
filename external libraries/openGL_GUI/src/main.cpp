#include "GUILibrary.hpp"
#include <iostream>
#include <string>

int main() {
    GUILibrary::hideConsole();
    std::string fontPath = "C:/Users/linuxnoob/Documents/c++/openGL_GUI/res/fonts/NotoSans-Regular.ttf";
    
    // 1. Initialize borderless window
    GUILibrary gui(1000, 800, "Pro GUI Designer", false, fontPath);

    // 2. Configure Title Bar
    WindowDecoration deco;
    deco.enabled = true;
    deco.height = 0.07f;
    deco.title = "Studio OpenGL v1.0";
    deco.backgroundColor = {0.1f, 0.1f, 0.1f, 1.0f};
    deco.titleColor = {0.9f, 0.9f, 0.9f, 1.0f};
    gui.enableCustomDecoration(deco);

    // 3. Add Menu Items
    gui.addMenuItem("File", []() { std::cout << "Menu: File clicked" << std::endl; });
    gui.addMenuItem("View", []() { std::cout << "Menu: View clicked" << std::endl; });
    gui.addMenuItem("Help", []() { std::cout << "Menu: Help clicked" << std::endl; });

    // 4. Add some UI content
    static std::string noteData = "Welcome to the new Studio interface.\nThis is a multiline InputField.\nAlignment is set to TOP.";
    auto noteInput = std::make_unique<InputField>();
    noteInput->config.pos = {-0.9f, -0.2f};
    noteInput->config.size = {1.0f, 0.6f};
    noteInput->config.dataField = &noteData;
    noteInput->config.isMultiline = true;
    noteInput->config.vAlign = VerticalAlignment::Top;
    noteInput->config.backgroundColor = {0.15f, 0.15f, 0.15f, 1.0f};
    noteInput->config.foregroundColor = {0.8f, 0.8f, 0.8f, 1.0f};
    noteInput->config.internalMargin = 0.02f;
    gui.addElement(std::move(noteInput));

    auto actionBtn = std::make_unique<Button>();
    actionBtn->config.pos = {0.2f, 0.1f};
    actionBtn->config.size = {0.4f, 0.12f};
    actionBtn->config.text = "Toggle Menu";
    actionBtn->config.backgroundColor = {0.2f, 0.4f, 0.7f, 1.0f};
    actionBtn->config.foregroundColor = Color::White();
    static bool menuVisible = true;
    actionBtn->config.onClick = [&gui]() { 
        menuVisible = !menuVisible;
        gui.setMenuBarVisible(menuVisible);
        std::cout << "Menu visibility toggled: " << menuVisible << std::endl;
    };
    gui.addElement(std::move(actionBtn));

    // 5. Run the app
    gui.run();

    return 0;
}

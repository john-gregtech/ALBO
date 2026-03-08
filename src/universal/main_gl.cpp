#include "GUILibrary.hpp"
#include <iostream>
#include <string>
#include <vector>

// Note: In a real implementation, we would include ALBO headers here
// e.g. #include "network/socket_manager.hpp"

int main() {
    // 1. System setup
    GUILibrary::hideConsole();
    
    // Path to the font inside the external library folder
    std::string fontPath = "external libraries/openGL_GUI/res/fonts/NotoSans-Regular.ttf";
    
    // 2. Initialize ALBO Pro Interface
    GUILibrary gui(1280, 720, "ALBO Secure Interface", false, fontPath);

    // 3. Custom Title Bar & Styling
    WindowDecoration deco;
    deco.enabled = true;
    deco.title = "ALBO | Quantum Secure Network v1.0";
    deco.backgroundColor = {0.05f, 0.05f, 0.05f, 1.0f};
    deco.titleColor = {0.0f, 0.8f, 1.0f, 1.0f}; // Cyber blue
    gui.enableCustomDecoration(deco);

    // 4. Menu System
    gui.addMenuItem("Terminal", []() { std::cout << "Switching to Terminal view..." << std::endl; });
    gui.addMenuItem("Network", []() { std::cout << "Opening Network Topology..." << std::endl; });
    gui.addMenuItem("Nodes", []() { std::cout << "Scanning active nodes..." << std::endl; });
    gui.addMenuItem("Security", []() { std::cout << "Running security audit..." << std::endl; });

    // 5. Main Content Area (Unified Log/Console)
    static std::string terminalData = 
        "> ALBO v1.0.0 Online\n"
        "> Initializing X25519 Handshake... [OK]\n"
        "> AES-256-GCM Layer: Active\n"
        "> Secure Vault: Locked\n"
        "> Ready for commands.";

    auto terminal = std::make_unique<InputField>();
    terminal->config.pos = {-0.98f, -0.85f};
    terminal->config.size = {1.96f, 1.6f};
    terminal->config.dataField = &terminalData;
    terminal->config.isMultiline = true;
    terminal->config.vAlign = VerticalAlignment::Top;
    terminal->config.backgroundColor = {0.02f, 0.02f, 0.02f, 0.9f};
    terminal->config.foregroundColor = {0.0f, 1.0f, 0.5f, 1.0f}; // Matrix Green
    terminal->config.borderRadius = 0.015f;
    terminal->config.internalMargin = 0.02f;
    gui.addElement(std::move(terminal));

    // 6. Interaction
    auto logoutBtn = std::make_unique<Button>();
    logoutBtn->config.pos = {0.6f, -0.95f};
    logoutBtn->config.size = {0.35f, 0.08f};
    logoutBtn->config.text = "TERMINATE SESSION";
    logoutBtn->config.backgroundColor = {0.4f, 0.1f, 0.1f, 1.0f};
    logoutBtn->config.foregroundColor = Color::White();
    logoutBtn->config.borderRadius = 0.01f;
    logoutBtn->config.onClick = []() {
        std::cout << "Session termination sequence initiated..." << std::endl;
        // In real app: trigger secure shutdown
    };
    gui.addElement(std::move(logoutBtn));

    // 7. Execution
    gui.run();

    return 0;
}

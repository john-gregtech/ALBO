#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <termios.h>

namespace prototype::network {

    enum class InputResult {
        NOTHING,
        TEXT,
        COMMAND,
        SCROLL_UP,
        SCROLL_DOWN
    };

    class ConsoleManager {
    private:
        struct termios orig_termios;
        std::vector<std::string> message_history;
        std::string current_input;
        std::mutex mtx;
        
        int screen_height;
        int screen_width;

        void setup_terminal();
        void restore_terminal();
        void update_screen_size();

    public:
        ConsoleManager();
        ~ConsoleManager();

        // Adds a message to the top display area
        void add_message(const std::string& sender, const std::string& text);
        
        // Non-blocking check for key presses
        InputResult process_input(std::string& out_str);

        // Re-draws the entire UI
        void redraw();

        // Clear the input line
        void clear_input();
    };

}

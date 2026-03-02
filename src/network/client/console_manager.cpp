#include "network/linux/client/console_manager.h"
#include <iostream>
#include <unistd.h>
#include <sys/ioctl.h>
#include <algorithm>

namespace prototype::network {

    ConsoleManager::ConsoleManager() {
        setup_terminal();
        update_screen_size();
        redraw();
    }

    ConsoleManager::~ConsoleManager() {
        restore_terminal();
    }

    void ConsoleManager::setup_terminal() {
        tcgetattr(STDIN_FILENO, &orig_termios);
        struct termios raw = orig_termios;
        raw.c_lflag &= ~(ECHO | ICANON); // Turn off echo and line-buffering
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
        std::cout << "\033[?1049h"; // Use alternate screen buffer
    }

    void ConsoleManager::restore_terminal() {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        std::cout << "\033[?1049l"; // Return to main screen
    }

    void ConsoleManager::update_screen_size() {
        struct winsize w;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
        screen_height = w.ws_row;
        screen_width = w.ws_col;
    }

    void ConsoleManager::add_message(const std::string& sender, const std::string& text) {
        std::lock_guard<std::mutex> lock(mtx);
        // If the sender name looks like a group (e.g. "[Group: name]"), it will be displayed here
        message_history.push_back("[" + sender + "]: " + text);
        if (message_history.size() > 500) message_history.erase(message_history.begin());
        redraw();
    }

    void ConsoleManager::redraw() {
        update_screen_size();
        std::cout << "\033[H\033[J"; // Clear and Home

        // Draw Message Area (Top)
        int msg_count = std::min((int)message_history.size(), screen_height - 4);
        int start_idx = (int)message_history.size() - msg_count;

        for (int i = 0; i < msg_count; ++i) {
            std::cout << "\033[" << (i + 1) << ";1H" << message_history[start_idx + i];
        }

        // Draw Divider
        std::cout << "\033[" << (screen_height - 2) << ";1H" << std::string(screen_width, '-');

        // Draw Input Line (Bottom)
        std::cout << "\033[" << (screen_height - 1) << ";1H" << "ALBO> " << current_input << "\033[K";
        std::cout << std::flush;
    }

    InputResult ConsoleManager::process_input(std::string& out_str) {
        char c;
        if (read(STDIN_FILENO, &c, 1) == 1) {
            if (c == '\n' || c == '\r') {
                out_str = current_input;
                current_input.clear();
                redraw();
                if (!out_str.empty() && out_str[0] == '/') return InputResult::COMMAND;
                return InputResult::TEXT;
            } else if (c == 127 || c == 8) { // Backspace
                if (!current_input.empty()) current_input.pop_back();
                redraw();
            } else if (c == 27) { // Escape Sequences (Arrows)
                char seq[3];
                // Read next two chars of sequence
                if (read(STDIN_FILENO, &seq[0], 1) == 1 && read(STDIN_FILENO, &seq[1], 1) == 1) {
                    if (seq[1] == 'A') return InputResult::SCROLL_UP;
                    if (seq[1] == 'B') return InputResult::SCROLL_DOWN;
                }
            } else {
                current_input += c;
                redraw();
            }
        }
        return InputResult::NOTHING;
    }

}

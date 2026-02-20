// server.cpp super chopped ai generated code
#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <math.h>
#include <thread>
#include <mutex>
#include <memory>
#include <condition_variable>
#include <format>   
#include <cstring>
#include "network/universal/professionalprovider.h"

class Stopwatch {
    using Clock = std::chrono::steady_clock;
    Clock::time_point last_mark;

public:
    // Starts or resets the timer
    void start() {
        last_mark = Clock::now();
    }

    // Returns time since last start() or last lap() in nanoseconds
    int64_t lap() {
        auto now = Clock::now();
        auto diff = std::chrono::duration_cast<std::chrono::nanoseconds>(now - last_mark).count();
        last_mark = now; // Update the mark for the next lap
        return diff;
    }
};


template <typename T>
static void printVector(const std::vector<T>& data) {
    int j = -1;
    for (T i : data) {
        if (++j % 16 == 0) {
            std::cout << " ";
        }
        if (j % 32 == 0) {
            std::cout << "\n";
        }
        std::cout << std::format("{:02x}", (char)i) << "";
    }
    std::cout << "\n";
}

int main() {
    Stopwatch sw;
    Stopwatch total;
    sw.start();
    total.start();
    prototype::network::FileReadStream fs = prototype::network::FileReadStream("large.rar");
    fs.start_thread();
    std::vector<char> chunk{};
    // std::cout << (bool)fs->isDone() << " This is the the isDone()\n";
    sw.lap();
    total.lap();
    while (!fs.isDone()) {
        fs.getTransferBuffer(chunk);
        //std::cout << fs.getTransferBuffer(chunk) << "if fail -> ";
        //std::cout << "CHUNK: " << sw.lap()/1000 << "us\n";
        //std::cout << "================================================\n";
        //printVector<char>(chunk);
        // std::cout << "AAA" << ++i << "\n";
    }
    std::cout << "Total Time: " << total.lap()/1000 << "us\n";

    



}

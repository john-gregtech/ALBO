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
#include "cryptowrapper/sha256.h"

//informal version not encrypted
class MessagePacket {
private:
    uint8_t message_type; //We will figure out how to use this later, 0 for raw, 1 for file, 2 empty idk
    std::vector<uint8_t> data{}; //Data is stored as btyes
    std::array<uint8_t, 32> checksum{};
    int64_t time = 0; //Time that the message was instantiated
    bool hasSent = false;
public:
    explicit MessagePacket(
        uint8_t type, 
        const std::vector<uint8_t>& _data
    ) : data(_data), message_type(type)
     {
        std::string temp(data.begin(), data.end());
        checksum = prototype_functions::sha256_hash(temp);
        explicit_bzero(temp.data(), temp.size());
        time = std::chrono::duration_cast<std::chrono::milliseconds>(
           std::chrono::system_clock::now().time_since_epoch()
       ).count();   
        //i wont add error checking yet as idk what its gonna be for bounds
    }
    ~MessagePacket() {
        if (!data.empty()) {
            explicit_bzero(data.data(), data.size());
        }
    }
    void setAsSent() {
        hasSent = true;
    }
    std::array<uint8_t, 32> getChecksum() {
        return checksum;
    }
    bool getStatus() {
        return hasSent;
    }
    MessagePacket(const MessagePacket&) = delete;
    MessagePacket& operator=(const MessagePacket&) = delete;

    MessagePacket(MessagePacket&&) = default;
    MessagePacket& operator=(MessagePacket&&) = default;
};

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
class FileReadStream {
private:
    static constexpr size_t chunkSize = 16 * 1024;

    std::ifstream file;
    std::vector<char> transferBuffer; 

    std::mutex mtx;
    std::condition_variable cv;
    std::thread readWorker;
    
    bool dataReadyForPickup = false; // Controls the flow between threads
    bool fileFullyRead      = false;
    size_t chunkCount       = 0;
    size_t lastChunkSize    = 0;
    size_t totalFileSize    = 0;

    void calculateChunkData() {
        file.seekg(0, std::ios::end);
        totalFileSize = static_cast<size_t>(file.tellg());
        file.seekg(0, std::ios::beg);

        if (totalFileSize == 0) return;

        chunkCount = (totalFileSize + chunkSize - 1) / chunkSize;
        lastChunkSize = totalFileSize % chunkSize;
        if (lastChunkSize == 0) lastChunkSize = chunkSize;
    }

    void scanChunk() {
        for (size_t currentChunk = 1; currentChunk <= chunkCount; ++currentChunk) {
            size_t sizeToRead = (currentChunk == chunkCount) ? lastChunkSize : chunkSize;
            
            // 1. High Performance Bulk Read
            std::vector<char> localBuffer(sizeToRead);
            file.read(localBuffer.data(), sizeToRead);

            // 2. Synchronize and Transfer
            {
                std::unique_lock<std::mutex> lock(mtx);
                // Wait until the main thread has picked up the PREVIOUS chunk
                cv.wait(lock, [this] { return !dataReadyForPickup; });

                // FAST: Move the data instead of copying it byte-by-byte
                transferBuffer = std::move(localBuffer);
                dataReadyForPickup = true;
            }
            // 3. Signal the main thread that data is ready
            cv.notify_one();
        }

        std::lock_guard<std::mutex> lock(mtx);
        fileFullyRead = true;
        cv.notify_one();
    }

public:
    explicit FileReadStream(const std::string& _fn) : file(_fn, std::ios::binary) {
        if (!file.is_open()) {
            std::cerr << "Error, File failed to open\n";
            return;
        }
        calculateChunkData();
    }

    ~FileReadStream() {
        if (readWorker.joinable()) readWorker.join();
        if (file.is_open()) file.close();
    }

    // Returns 1 if buffer was filled, 0 if file is done
    int getTransferBuffer(std::vector<char>& outBuffer) {
        std::unique_lock<std::mutex> lock(mtx);
        
        // Wait until there is data OR the file is finished
        cv.wait(lock, [this] { return dataReadyForPickup || fileFullyRead; });

        if (dataReadyForPickup) {
            outBuffer = std::move(transferBuffer); // Hand off to caller
            dataReadyForPickup = false;
            lock.unlock(); // Unlock before notifying to avoid "wake-and-block"
            cv.notify_one(); // Tell the worker it can read the NEXT chunk
            return 1;
        }

        return 0; // File is fully read
    }

    void start_thread() {
        readWorker = std::thread(&FileReadStream::scanChunk, this);
    }

    bool isDone() {
        std::lock_guard<std::mutex> lock(mtx);
        return fileFullyRead && !dataReadyForPickup;
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
    FileReadStream fs = FileReadStream("large.rar");
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

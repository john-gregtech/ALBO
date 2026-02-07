// server.cpp super chopped ai generated code
#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <math.h>
#include <thread>
#include <mutex>
#include <memory>

class FileReadStream {
private:
    static constexpr size_t chunkSize = 16 * 1024;

    std::string fn = "";
    std::ifstream file;

    std::vector<int> readBuffer;       //buffer that reads the file
    std::vector<int> transferBuffer;   //this one takes the data from readBuffer

    std::thread readWorker;
    
    bool transferLock       = true; //we do first round lock cause we didnt do a do while
    bool readReady          = false;
    bool readDone           = false; //this is for later if we need to check if the thread is ready to transfer
    bool fileFullyRead      = false;
    size_t currentChunk     = 0; //done
    size_t chunkCount       = 0; //done
    size_t lastChunkSize    = 0; //done
    size_t totalFileSize    = 0; //done

    size_t getFileSize() {
        file.seekg(0, std::ios::end);
        return static_cast<size_t>(file.tellg());
    }
    void calculateChunkData() {
        totalFileSize = getFileSize();
        chunkCount = static_cast<size_t>(std::ceil(static_cast<double>(totalFileSize)/static_cast<double>(chunkSize)));
        lastChunkSize = totalFileSize-(chunkCount-1)*chunkSize;
    }
    //the function that is actually threaded
    void scanChunk() {
        std::cout << "\nH" << chunkCount << "\n";
        file.seekg(0, std::ios::beg);
        for (currentChunk = 1; currentChunk <= chunkCount; ++currentChunk) {

            readDone = false;
            readBuffer.clear();

            if (currentChunk != chunkCount) {
                readBuffer.resize(lastChunkSize);
                for (size_t byteCursor = 0; byteCursor < lastChunkSize; ++byteCursor) {
                    readBuffer.at(byteCursor) = file.get();
                }
            } else {
                readBuffer.resize(chunkSize);
                for (size_t byteCursor = 0; byteCursor < chunkSize; ++byteCursor) {
                    readBuffer.at(byteCursor) = file.get();
                }
            }
            readDone = true;
            while (transferLock) {
                std::this_thread::sleep_for(std::chrono::microseconds(2));
            }
            transferBuffer.assign(readBuffer.begin(), readBuffer.end());
            
            transferLock = true;
        }
        fileFullyRead = true;
    }
public:
    explicit FileReadStream(const std::string& _fn) : file(_fn, std::ios::binary | std::ios::ate) {
        fn = _fn;
        if (!file.is_open()) {
            std::cerr << "Error, File failed to open";
        }
        calculateChunkData();
    }
    ~FileReadStream() {
        file.close();
        if (!file.is_open()) {
            std::cerr << "File has been closed\n";
        } else {
            std::cerr << "Not closed oh o\n";
        }
        join_thread();
    }
    int getTransferBuffer(std::vector<int>* _buffer) {
        while (!transferLock) {

        }
        if (!transferLock) {
            return 0;
        } else {
            *_buffer = transferBuffer;
            transferLock = false;
        }
        return 1;
    }
    void start_thread() {
        readWorker = std::thread(&FileReadStream::scanChunk, this);
    }
    void join_thread() {
        if (readWorker.joinable()) readWorker.join();
    }
    bool isReadyToTransfer() {
        return readDone;
    }
    bool isDone() { //lol at the name mismatch
        return fileFullyRead;
    }
    
};

int main() {
    std::unique_ptr<FileReadStream> fs = std::make_unique<FileReadStream>("large.rar");
    fs->start_thread();
    std::vector<int> chunk{};
    // std::cout << (bool)fs->isDone() << " This is the the isDone()\n";
    int i{};
    while (!fs->isDone()) {
        std::cout << fs->getTransferBuffer(&chunk) << "if fail -> ";
        std::cout << "AAA" << ++i << "\n";
    }

    



}

// server.cpp super chopped ai generated code
#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <math.h>
#include <thread>
#include <mutex>

//basically it needs to be threaded so there will be a producer and consumer thread
//consumer will be the main thread
//producer will be the file reader thread
//seekg to got eg seekg(-1, std::ios::cur) is move back one byte

class FileStream {
private:
    static constexpr size_t chunkSize = 16 * 1024;

    std::string fn = "";
    std::ifstream file;

    std::vector<int> readBuffer;       //buffer that reads the file
    std::vector<int> transferBuffer;   //this one takes the data from readBuffer
    
    bool transferLock = false;
    bool readDone = false;
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

    //This function will be the function to be called as a thread
    void scanChunk() {
        //goto start
        file.seekg(0, std::ios::beg);
        //first which chunk are we on
        //if chunkcount is 1 then lastchunksize is the first chunk so then we will use that
        //calculate chunk data was pre called instantly after the file was opened
        for (currentChunk = 1; currentChunk <= chunkCount; ++currentChunk) {
            //we have the parts that read now we need to actually put it into the buffer
            //first we clear the buffer
            readDone = false;
            readBuffer.clear();

            //std::cout << currentChunk << std::endl;
            if (currentChunk == chunkCount) {
                readBuffer.resize(lastChunkSize);
                for (size_t byteCursor = 0; byteCursor < lastChunkSize; ++byteCursor) {
                    readBuffer.at(byteCursor) = file.get();
                    std::cout << readBuffer.at(byteCursor);
                }
            } else {
                readBuffer.resize(chunkSize);
                for (size_t byteCursor = 0; byteCursor < chunkSize; ++byteCursor) {
                    readBuffer.at(byteCursor) = file.get();
                    std::cout << readBuffer.at(byteCursor);
                }
            }
            readDone = true;
            //now we have a full buffer
            //this is the point where we wait
            std::cout << "At lock\n";
            while (transferLock) {
                //wait code
                std::this_thread::sleep_for(std::chrono::microseconds(2));
            }
            transferBuffer.assign(readBuffer.begin(), readBuffer.end());
            
            transferLock = true; //imagine it gets checked now
            std::cout << "Past lock\n";
        }
        
    }
public:
    explicit FileStream(const std::string& _fn) : file(_fn, std::ios::binary | std::ios::ate) {
        fn = _fn;
        if (!file.is_open()) {
            std::cerr << "Error, File failed to open somehow";
        }
        std::cerr << "working and good\n";
        calculateChunkData();
        scanChunk();
    }
    ~FileStream() {
        file.close();
        if (!file.is_open()) {
            std::cerr << "File has been closed\n";
        } else {
            std::cerr << "Not closed oh o\n";
        }
        //also close threads here incase they are still runnning
    }
    
};


int main() {
    FileStream fs("yummy.txt");
    
}

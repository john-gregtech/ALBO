#include "network/universal/professionalprovider.h"

namespace prototype::network {
    

    MessageData::MessageData(
        uint8_t type, 
        const std::vector<uint8_t>& _data
    ) : message_type(type), data(_data)
        {
        std::string temp(data.begin(), data.end());
        checksum = prototype_functions::sha256_hash(temp);
        explicit_bzero(temp.data(), temp.size());
        time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();   
        //i wont add error checking yet as idk what its gonna be for bounds
    }
    MessageData::~MessageData() {
        if (!data.empty()) {
            explicit_bzero(data.data(), data.size());
        }
    }
    void MessageData::setAsSent() {
        hasSent = true;
    }
    std::array<uint8_t, 32> MessageData::getChecksum() {
        return checksum;
    }
    bool MessageData::getStatus() {
        return hasSent;
    }

    FileReadStream::FileReadStream(const std::string& _fn) : file(_fn, std::ios::binary) {
        if (!file.is_open()) {
            std::cerr << "Error, File failed to open\n";
            return;
        }
        calculateChunkData();
    }
    FileReadStream::~FileReadStream() {
        if (readWorker.joinable()) readWorker.join();
        if (file.is_open()) file.close();
    }
    bool FileReadStream::isDone() {
        std::lock_guard<std::mutex> lock(mtx);
        return fileFullyRead && !dataReadyForPickup;
    }
    void FileReadStream::start_thread() {
        readWorker = std::thread(&FileReadStream::scanChunk, this);
    }
    int FileReadStream::getTransferBuffer(std::vector<char>& outBuffer) {
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
}
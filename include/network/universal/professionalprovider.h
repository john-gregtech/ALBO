#pragma once
#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <math.h>
#include <condition_variable>
#include "cryptowrapper/sha256.h"
#include <format>
#include <thread>
#include <mutex>
#include <memory>
#include <cstring>
#include <cryptowrapper/X25519.h>

namespace prototype::network {
    struct UUID {
        uint64_t high;
        uint64_t low;
    };
    struct SESSION {
        struct UUID server; //always server
        struct UUID client; //always client
        std::vector<uint8_t> session_unique;
    };


    class MessageData {
    private:
        uint8_t message_type; //We will figure out how to use this later, 0 for raw, 1 for file, 2 empty idk
        std::vector<uint8_t> data{}; //Data is stored as btyes
        std::array<uint8_t, 32> checksum{};
        int64_t time = 0; //Time that the message was instantiated
        bool hasSent = false;
    public:
        explicit MessageData(uint8_t type, const std::vector<uint8_t>& _data);
        ~MessageData();
        void setAsSent();
        std::array<uint8_t, 32> getChecksum();
        bool getStatus();
        MessageData(const MessageData&) = delete;
        MessageData& operator=(const MessageData&) = delete;

        MessageData(MessageData&&) = default;
        MessageData& operator=(MessageData&&) = default;
    };

    template <typename PacketType> // can have MessageData and other ones later
    class Packet {
    private:
        PacketType message_data;
        UUID sender_uuid = {};
        UUID target_uuid = {}; //target and server can be the same if the message is for the server;
        UUID server_uuid = {};
    public:
        explicit Packet(PacketType& _message_data, const UUID& my_uuid, const UUID& _target_uuid, const UUID& _server_uuid) :
            message_data(std::move(_message_data)), sender_uuid(my_uuid), target_uuid(_target_uuid), server_uuid(_server_uuid) {}
        
        Packet(Packet&&) = default;
        Packet& operator=(Packet&&) = default;

        Packet(const Packet&) = delete;
        Packet& operator=(const Packet&) = delete;

        //const PacketType& getMessageData() const { return message_data; } not needed atm
        const UUID& getSender() const { return sender_uuid; }
        const UUID& getTarget() const { return target_uuid; }
        const UUID& getServer() const { return server_uuid; }
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
        explicit FileReadStream(const std::string& _fn);
        ~FileReadStream();
        int getTransferBuffer(std::vector<char>& outBuffer);
        void start_thread();
        bool isDone();

        FileReadStream(const FileReadStream&) = delete;
        FileReadStream& operator=(const FileReadStream&) = delete;
    };
}
#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <string>
#include "network/universal/packet.h"

namespace prototype::network {

    class PacketDispatcher {
    public:
        std::queue<RawPacket> response_queue;
        std::queue<std::pair<std::string, std::string>> chat_messages;
        std::mutex mtx;
        std::condition_variable cv;

        void push(RawPacket p) { 
            std::lock_guard<std::mutex> lock(mtx); 
            response_queue.push(p); 
            cv.notify_one(); 
        }
        
        void push_chat(const std::string& sender, const std::string& msg) {
            std::lock_guard<std::mutex> lock(mtx);
            chat_messages.push({sender, msg});
        }

        std::optional<RawPacket> wait_for_response(int timeout_sec = 5) {
            std::unique_lock<std::mutex> lock(mtx);
            if (cv.wait_for(lock, std::chrono::seconds(timeout_sec), [this] { return !response_queue.empty(); })) {
                RawPacket p = response_queue.front();
                response_queue.pop();
                return p;
            }
            return std::nullopt;
        }
        
        bool pop_chat(std::string& out_sender, std::string& out_msg) {
            std::lock_guard<std::mutex> lock(mtx);
            if (chat_messages.empty()) return false;
            out_sender = chat_messages.front().first;
            out_msg = chat_messages.front().second;
            chat_messages.pop();
            return true;
        }
    };

}

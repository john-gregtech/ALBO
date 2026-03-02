#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>

namespace prototype::network {

    class RateLimiter {
    private:
        struct ConnectionInfo {
            int count;
            std::chrono::steady_clock::time_point last_reset;
        };

        std::unordered_map<std::string, ConnectionInfo> client_rates;
        std::mutex mtx;
        
        int max_requests;
        int window_seconds;

    public:
        RateLimiter(int max_req, int window_sec);

        /**
         * @brief Checks if an IP is allowed to perform an action.
         * @return true if allowed, false if rate limited.
         */
        bool check_and_increment(const std::string& ip_address);
    };

}

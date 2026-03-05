#include "universal/network/rate_limiter.h"

namespace prototype::network {

    RateLimiter::RateLimiter(int max_req, int window_sec) 
        : max_requests(max_req), window_seconds(window_sec) {}

    bool RateLimiter::check_and_increment(const std::string& ip_address) {
        std::lock_guard<std::mutex> lock(mtx);
        auto now = std::chrono::steady_clock::now();
        auto& info = client_rates[ip_address];
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - info.last_reset).count();
        if (duration >= window_seconds) {
            info.count = 1;
            info.last_reset = now;
            return true;
        }
        if (info.count >= max_requests) return false;
        info.count++;
        return true;
    }

}

#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>

namespace prototype::network {
    inline std::string to_lowercase(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        return s;
    }

    inline std::string to_hex(const uint8_t* data, size_t len) {
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        for (size_t i = 0; i < len; ++i) {
            ss << std::setw(2) << (int)data[i];
        }
        return ss.str();
    }

    inline std::string to_hex(const std::vector<uint8_t>& data) {
        return to_hex(data.data(), data.size());
    }

    inline std::vector<uint8_t> from_hex(std::string hex) {
        if (hex.size() >= 2 && hex.substr(0, 2) == "0x") hex = hex.substr(2);
        std::vector<uint8_t> bytes;
        for (size_t i = 0; i < hex.length(); i += 2) {
            std::string byteString = hex.substr(i, 2);
            uint8_t byte = (uint8_t)strtol(byteString.c_str(), nullptr, 16);
            bytes.push_back(byte);
        }
        return bytes;
    }

    inline void ss_uuid_format(std::stringstream& ss, uint64_t high, uint64_t low) {
        ss << std::hex << std::setfill('0');
        ss << std::setw(8) << (uint32_t)(high >> 32) << "-";
        ss << std::setw(4) << (uint16_t)(high >> 16) << "-";
        ss << std::setw(4) << (uint16_t)high << "-";
        ss << std::setw(4) << (uint16_t)(low >> 48) << "-";
        ss << std::setw(12) << (low & 0xFFFFFFFFFFFFULL);
    }

    inline std::string uuid_to_string(uint64_t high, uint64_t low) {
        std::stringstream ss;
        ss_uuid_format(ss, high, low);
        return ss.str();
    }

    inline void string_to_uuid_parts(const std::string& uuid_str, uint64_t& high, uint64_t& low) {
        std::string clean = uuid_str;
        clean.erase(std::remove(clean.begin(), clean.end(), '-'), clean.end());
        if (clean.length() != 32) return;
        try {
            high = std::stoull(clean.substr(0, 16), nullptr, 16);
            low = std::stoull(clean.substr(16), nullptr, 16);
        } catch (...) {
            high = 0; low = 0;
        }
    }
}

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
}

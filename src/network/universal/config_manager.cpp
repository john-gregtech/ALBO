#include "network/universal/config_manager.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

namespace prototype::network {

    ConfigManager::ConfigManager(const std::string& path) : config_path(path) {}

    bool ConfigManager::load() {
        if (!std::filesystem::exists(config_path)) return false;

        std::ifstream file(config_path);
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            size_t sep = line.find('=');
            if (sep != std::string::npos) {
                std::string key = line.substr(0, sep);
                std::string val = line.substr(sep + 1);
                config_data[key] = val;
            }
        }
        return true;
    }

    bool ConfigManager::save() {
        std::filesystem::path p(config_path);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }

        std::ofstream file(config_path);
        if (!file.is_open()) return false;
        
        for (const auto& [key, val] : config_data) {
            file << key << "=" << val << "\n";
        }
        return true;
    }

    std::string ConfigManager::get(const std::string& key, const std::string& default_val) const {
        auto it = config_data.find(key);
        if (it != config_data.end()) return it->second;
        return default_val;
    }

    void ConfigManager::set(const std::string& key, const std::string& val) {
        config_data[key] = val;
    }

    int ConfigManager::get_int(const std::string& key, int default_val) const {
        auto it = config_data.find(key);
        if (it != config_data.end()) {
            try {
                return std::stoi(it->second);
            } catch (...) {
                return default_val;
            }
        }
        return default_val;
    }

}

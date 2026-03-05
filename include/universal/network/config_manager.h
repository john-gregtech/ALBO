#pragma once
#include <string>
#include <map>

namespace prototype::network {

    class ConfigManager {
    private:
        std::map<std::string, std::string> config_data;
        std::string config_path;

    public:
        explicit ConfigManager(const std::string& path);
        
        bool load();
        bool save();

        std::string get(const std::string& key, const std::string& default_val = "") const;
        void set(const std::string& key, const std::string& val);

        int get_int(const std::string& key, int default_val = 0) const;
    };

}

#pragma once

#include <nlohmann/json.hpp>
#include <string>

class ConfigManager {
private:
    static ConfigManager* instance;
    nlohmann::json json_data;
    std::string config_path;

    ConfigManager(std::string configFilePath);

    void write_default_config(const std::string& filePath);

public:

    ConfigManager(const ConfigManager&) = delete;
    void operator=(const ConfigManager&) = delete;

    static ConfigManager* get_instance();
    static ConfigManager* get_instance(std::string config_path);

    const nlohmann::json& get_json_data() const;

    void update_json_data(const std::string& key, const nlohmann::json& value);

    void save_config(const std::string& filePath);
};
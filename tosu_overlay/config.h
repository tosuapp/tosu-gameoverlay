#pragma once

#include <nlohmann/json.hpp>
#include <string>

class ConfigManager {
private:
    static ConfigManager* instance;
    nlohmann::json jsonData;
    std::string config_path;

    ConfigManager(std::string configFilePath);

    void writeDefaultConfig(const std::string& filePath);

public:

    ConfigManager(const ConfigManager&) = delete;
    void operator=(const ConfigManager&) = delete;

    static ConfigManager* getInstance();
    static ConfigManager* getInstance(std::string config_path);

    const nlohmann::json& getJsonData() const;

    void updateJsonData(const std::string& key, const nlohmann::json& value);

    void saveConfig(const std::string& filePath);
};
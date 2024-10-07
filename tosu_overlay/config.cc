#include "tosu_overlay/config.h"
#include <iostream>
#include <fstream>
#include <filesystem>

ConfigManager* ConfigManager::instance = nullptr;

ConfigManager::ConfigManager(std::string configFilePath) {
    if (!std::filesystem::exists(configFilePath)) {
        std::cout << "Configuration file not found, creating default config.json" << std::endl;
        writeDefaultConfig(configFilePath);
    }

    std::ifstream configFile(configFilePath);
    if (configFile.is_open()) {
        configFile >> jsonData;
        configFile.close();
    } else {
        std::cerr << "Could not open config.json" << std::endl;
        jsonData = {};
    }
}

ConfigManager* ConfigManager::getInstance() {
    if (!instance) {
        instance = new ConfigManager("config.json");
    }
    return instance;
}

ConfigManager* ConfigManager::getInstance(std::string config_path) {
    if (!instance) {
        instance = new ConfigManager(config_path);
    }
    return instance;
}

const nlohmann::json& ConfigManager::getJsonData() const {
    return jsonData;
}

void ConfigManager::updateJsonData(const std::string& key, const nlohmann::json& value) {
    jsonData[key] = value;
}

void ConfigManager::saveConfig(const std::string& filePath) {
    std::ofstream configFile(filePath);
    if (configFile.is_open()) {
        configFile << jsonData.dump(4);
        configFile.close();
    } else {
        std::cerr << "Unable to open file for writing: " << filePath << std::endl;
    }
}

void ConfigManager::writeDefaultConfig(const std::string& filePath) {
    nlohmann::json defaultConfig = {
        {"cef_debugging_enabled", false},
        {"cef_fps", 60}
    };

    std::ofstream configFile(filePath);
    if (configFile.is_open()) {
        configFile << defaultConfig.dump(4);
        configFile.close();
    } else {
        std::cerr << "Unable to create default config file: " << filePath << std::endl;
    }
}
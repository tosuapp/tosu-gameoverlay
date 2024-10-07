#include "tosu_overlay/config.h"
#include <iostream>
#include <fstream>
#include <filesystem>

ConfigManager* ConfigManager::instance = nullptr;

ConfigManager::ConfigManager(std::string configFilePath) {
    if (!std::filesystem::exists(configFilePath)) {
        std::cout << "Configuration file not found, creating default config.json" << std::endl;
        write_default_config(configFilePath);
    }

    std::ifstream configFile(configFilePath);
    if (configFile.is_open()) {
        configFile >> json_data;
        configFile.close();
    } else {
        std::cerr << "Could not open config.json" << std::endl;
        json_data = {};
    }
}

ConfigManager* ConfigManager::get_instance() {
    if (!instance) {
        instance = new ConfigManager("config.json");
    }
    return instance;
}

ConfigManager* ConfigManager::get_instance(std::string config_path) {
    if (!instance) {
        instance = new ConfigManager(config_path);
    }
    return instance;
}

const nlohmann::json& ConfigManager::get_json_data() const {
    return json_data;
}

void ConfigManager::update_json_data(const std::string& key, const nlohmann::json& value) {
    json_data[key] = value;
}

void ConfigManager::save_config(const std::string& filePath) {
    std::ofstream configFile(filePath);
    if (configFile.is_open()) {
        configFile << json_data.dump(4);
        configFile.close();
    } else {
        std::cerr << "Unable to open file for writing: " << filePath << std::endl;
    }
}

void ConfigManager::write_default_config(const std::string& filePath) {
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
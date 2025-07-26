#include "config_manager.hpp"
#include "constants.hpp"
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <random>
#include <glog/logging.h>

namespace picoradar::common {

using json = nlohmann::json;

// 静态实例
ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

ConfigResult<void> ConfigManager::loadFromFile(const std::string& filename) {
    try {
        std::ifstream file(filename);
        if (!file.is_open()) {
            return tl::make_unexpected(ConfigError{"Failed to open config file: " + filename});
        }
        
        json json_config;
        file >> json_config;
        
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = std::move(json_config);
        
        LOG(INFO) << "Loaded config from: " << filename;
        loadEnvironmentVariables();
        
        return {};
    } catch (const std::exception& e) {
        return tl::make_unexpected(ConfigError{"Failed to parse config file " + filename + ": " + e.what()});
    }
}

ConfigResult<void> ConfigManager::loadFromJson(const nlohmann::json& json) {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = json;
        loadEnvironmentVariables();
        return {};
    } catch (const std::exception& e) {
        return tl::make_unexpected(ConfigError{"Failed to load JSON config: " + std::string(e.what())});
    }
}

ConfigResult<std::string> ConfigManager::getString(const std::string& key) const {
    auto result = getJsonValue(key);
    if (!result) {
        return tl::make_unexpected(result.error());
    }
    
    try {
        return result->get<std::string>();
    } catch (const std::exception& e) {
        return tl::make_unexpected(ConfigError{"Value at key '" + key + "' is not a string: " + e.what()});
    }
}

ConfigResult<int> ConfigManager::getInt(const std::string& key) const {
    auto result = getJsonValue(key);
    if (!result) {
        return tl::make_unexpected(result.error());
    }
    
    try {
        return result->get<int>();
    } catch (const std::exception& e) {
        return tl::make_unexpected(ConfigError{"Value at key '" + key + "' is not an integer: " + e.what()});
    }
}

ConfigResult<bool> ConfigManager::getBool(const std::string& key) const {
    auto result = getJsonValue(key);
    if (!result) {
        return tl::make_unexpected(result.error());
    }
    
    try {
        return result->get<bool>();
    } catch (const std::exception& e) {
        return tl::make_unexpected(ConfigError{"Value at key '" + key + "' is not a boolean: " + e.what()});
    }
}

ConfigResult<double> ConfigManager::getDouble(const std::string& key) const {
    auto result = getJsonValue(key);
    if (!result) {
        return tl::make_unexpected(result.error());
    }
    
    try {
        return result->get<double>();
    } catch (const std::exception& e) {
        return tl::make_unexpected(ConfigError{"Value at key '" + key + "' is not a double: " + e.what()});
    }
}

bool ConfigManager::hasKey(const std::string& key) const {
    auto result = getJsonValue(key);
    return result.has_value();
}

ConfigResult<void> ConfigManager::saveToFile(const std::string& filename) const {
    try {
        std::ofstream file(filename);
        if (!file.is_open()) {
            return tl::make_unexpected(ConfigError{"Failed to open file for writing: " + filename});
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        file << config_.dump(4);
        
        return {};
    } catch (const std::exception& e) {
        return tl::make_unexpected(ConfigError{"Failed to save config to file: " + std::string(e.what())});
    }
}

nlohmann::json ConfigManager::getConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

ConfigResult<nlohmann::json> ConfigManager::getJsonValue(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 处理点分割的键路径
    std::vector<std::string> keys;
    std::stringstream ss(key);
    std::string item;
    
    while (std::getline(ss, item, '.')) {
        if (!item.empty()) {
            keys.push_back(item);
        }
    }
    
    if (keys.empty()) {
        keys.push_back(key);
    }
    
    json current = config_;
    
    for (const auto& k : keys) {
        if (!current.is_object() || !current.contains(k)) {
            return tl::make_unexpected(ConfigError{"Key not found: " + key});
        }
        current = current[k];
    }
    
    return current;
}

void ConfigManager::loadEnvironmentVariables() {
    // 加载环境变量，例如：
    if (const char* port = std::getenv("PICORADAR_PORT")) {
        try {
            set("server.port", std::stoi(port));
        } catch (...) {
            LOG(WARNING) << "Invalid PICORADAR_PORT value: " << port;
        }
    }
    
    if (const char* auth = std::getenv("PICORADAR_AUTH_ENABLED")) {
        set("server.auth.enabled", std::string(auth) == "true");
    }
    
    if (const char* token = std::getenv("PICORADAR_AUTH_TOKEN")) {
        set("server.auth.token", std::string(token));
    }
}

std::string ConfigManager::generateSecureToken() const {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 61);
    
    const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    std::string token;
    token.reserve(32);
    
    for (int i = 0; i < 32; ++i) {
        token += chars[dis(gen)];
    }
    
    return token;
}

// 模板特化实现
template<>
void ConfigManager::set<std::string>(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> keys;
    std::stringstream ss(key);
    std::string item;
    
    while (std::getline(ss, item, '.')) {
        if (!item.empty()) {
            keys.push_back(item);
        }
    }
    
    if (keys.empty()) {
        keys.push_back(key);
    }
    
    json* current = &config_;
    
    for (size_t i = 0; i < keys.size() - 1; ++i) {
        if (!current->is_object()) {
            *current = json::object();
        }
        current = &(*current)[keys[i]];
    }
    
    (*current)[keys.back()] = value;
}

template<>
void ConfigManager::set<int>(const std::string& key, const int& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> keys;
    std::stringstream ss(key);
    std::string item;
    
    while (std::getline(ss, item, '.')) {
        if (!item.empty()) {
            keys.push_back(item);
        }
    }
    
    if (keys.empty()) {
        keys.push_back(key);
    }
    
    json* current = &config_;
    
    for (size_t i = 0; i < keys.size() - 1; ++i) {
        if (!current->is_object()) {
            *current = json::object();
        }
        current = &(*current)[keys[i]];
    }
    
    (*current)[keys.back()] = value;
}

template<>
void ConfigManager::set<bool>(const std::string& key, const bool& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> keys;
    std::stringstream ss(key);
    std::string item;
    
    while (std::getline(ss, item, '.')) {
        if (!item.empty()) {
            keys.push_back(item);
        }
    }
    
    if (keys.empty()) {
        keys.push_back(key);
    }
    
    json* current = &config_;
    
    for (size_t i = 0; i < keys.size() - 1; ++i) {
        if (!current->is_object()) {
            *current = json::object();
        }
        current = &(*current)[keys[i]];
    }
    
    (*current)[keys.back()] = value;
}

template<>
void ConfigManager::set<double>(const std::string& key, const double& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> keys;
    std::stringstream ss(key);
    std::string item;
    
    while (std::getline(ss, item, '.')) {
        if (!item.empty()) {
            keys.push_back(item);
        }
    }
    
    if (keys.empty()) {
        keys.push_back(key);
    }
    
    json* current = &config_;
    
    for (size_t i = 0; i < keys.size() - 1; ++i) {
        if (!current->is_object()) {
            *current = json::object();
        }
        current = &(*current)[keys[i]];
    }
    
    (*current)[keys.back()] = value;
}

// 模板特化实现 - getWithDefault
template<>
std::string ConfigManager::getWithDefault<std::string>(const std::string& key, const std::string& default_value) const {
    auto result = getString(key);
    return result.has_value() ? result.value() : default_value;
}

template<>
int ConfigManager::getWithDefault<int>(const std::string& key, const int& default_value) const {
    auto result = getInt(key);
    return result.has_value() ? result.value() : default_value;
}

template<>
bool ConfigManager::getWithDefault<bool>(const std::string& key, const bool& default_value) const {
    auto result = getBool(key);
    return result.has_value() ? result.value() : default_value;
}

template<>
double ConfigManager::getWithDefault<double>(const std::string& key, const double& default_value) const {
    auto result = getDouble(key);
    return result.has_value() ? result.value() : default_value;
}

uint16_t ConfigManager::getServicePort() const {
    return static_cast<uint16_t>(getWithDefault("server.port", static_cast<int>(picoradar::config::kDefaultServicePort)));
}

uint16_t ConfigManager::getDiscoveryPort() const {
    return static_cast<uint16_t>(getWithDefault("discovery.udp_port", static_cast<int>(picoradar::config::kDefaultDiscoveryPort)));
}

} // namespace picoradar::common

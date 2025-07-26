#include "config_manager.hpp"
#include "common/logging.hpp"

#include <fstream>
#include <random>
#include <sstream>
#include <chrono>
#include <cstdlib>
#include <algorithm>
#include <cctype>

namespace picoradar::config {

ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

bool ConfigManager::loadFromFile(const std::string& configPath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ifstream file(configPath);
    if (!file.is_open()) {
        LOG_WARNING << "Failed to open config file: " << configPath;
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // 跳过空行和注释
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // 解析 key=value 格式
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            
            // 去除首尾空格
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            // 去除引号
            if (value.length() >= 2 && value[0] == '"' && value.back() == '"') {
                value = value.substr(1, value.length() - 2);
            }
            
            config_[key] = value;
        }
    }
    
    LOG_INFO << "Loaded configuration from: " << configPath;
    return true;
}

void ConfigManager::loadFromEnvironment() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 定义需要从环境变量读取的配置项
    const std::vector<std::string> envVars = {
        "PICORADAR_AUTH_TOKEN",
        "PICORADAR_SERVER_PORT",
        "PICORADAR_DISCOVERY_PORT",
        "PICORADAR_LOG_LEVEL"
    };
    
    for (const auto& envVar : envVars) {
        const char* value = std::getenv(envVar.c_str());
        if (value != nullptr) {
            // 转换环境变量名为配置键（移除前缀，转换为小写）
            std::string key = envVar;
            if (key.size() >= 10 && key.substr(0, 10) == "PICORADAR_") {
                key = key.substr(10); // 移除 "PICORADAR_" 前缀
            }
            
            // 转换为小写并替换下划线为点
            for (char& character : key) {
                if (character == '_') {
                    character = '.';
                } else {
                    character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
                }
            }
            
            config_[key] = value;
            LOG_DEBUG << "Loaded from environment: " << key << " = " << value;
        }
    }
}

std::string ConfigManager::getString(const std::string& key, const std::string& defaultValue) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = config_.find(key);
    return (it != config_.end()) ? it->second : defaultValue;
}

int ConfigManager::getInt(const std::string& key, int defaultValue) const {
    std::string strValue = getString(key);
    if (strValue.empty()) {
        return defaultValue;
    }
    
    try {
        return std::stoi(strValue);
    } catch (const std::exception& e) {
        LOG_WARNING << "Failed to parse int config '" << key << "': " << e.what();
        return defaultValue;
    }
}

bool ConfigManager::getBool(const std::string& key, bool defaultValue) const {
    std::string strValue = getString(key);
    if (strValue.empty()) {
        return defaultValue;
    }
    
    // 支持多种布尔值表示
    std::transform(strValue.begin(), strValue.end(), strValue.begin(), 
                   [](unsigned char c) { return std::tolower(c); });
    return (strValue == "true" || strValue == "1" || strValue == "yes" || strValue == "on");
}

void ConfigManager::set(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_[key] = value;
}

std::string ConfigManager::getAuthToken() const {
    std::string token = getString("auth.token");
    if (token.empty()) {
        // 如果没有配置认证令牌，返回默认的开发用令牌
        LOG_WARNING << "No auth token configured, using default development token";
        return "pico_radar_secret_token";
    }
    return token;
}

std::string ConfigManager::generateAuthToken() {
    std::string token = generateRandomToken();
    set("auth.token", token);
    LOG_INFO << "Generated new auth token";
    return token;
}

std::string ConfigManager::generateRandomToken() const {
    // 使用当前时间作为种子
    auto now = std::chrono::high_resolution_clock::now();
    auto timestamp = now.time_since_epoch().count();
    
    std::mt19937_64 gen(timestamp);
    std::uniform_int_distribution<uint64_t> dis;
    
    // 生成两个64位随机数并转换为十六进制字符串
    uint64_t part1 = dis(gen);
    uint64_t part2 = dis(gen);
    
    std::stringstream ss;
    ss << std::hex << part1 << part2;
    return ss.str();
}

} // namespace picoradar::config

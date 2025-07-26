#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>

namespace picoradar::config {

/**
 * @brief 配置管理类
 * 
 * 提供运行时配置管理，支持从文件和环境变量读取配置
 */
class ConfigManager {
public:
    static ConfigManager& getInstance();
    
    // 禁止拷贝和移动
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    ConfigManager(ConfigManager&&) = delete;
    ConfigManager& operator=(ConfigManager&&) = delete;
    
    /**
     * @brief 从文件加载配置
     * @param configPath 配置文件路径
     * @return 是否成功加载
     */
    bool loadFromFile(const std::string& configPath);
    
    /**
     * @brief 从环境变量加载配置
     */
    void loadFromEnvironment();
    
    /**
     * @brief 获取字符串配置项
     * @param key 配置键
     * @param defaultValue 默认值
     * @return 配置值
     */
    std::string getString(const std::string& key, const std::string& defaultValue = "") const;
    
    /**
     * @brief 获取整数配置项
     * @param key 配置键
     * @param defaultValue 默认值
     * @return 配置值
     */
    int getInt(const std::string& key, int defaultValue = 0) const;
    
    /**
     * @brief 获取布尔配置项
     * @param key 配置键
     * @param defaultValue 默认值
     * @return 配置值
     */
    bool getBool(const std::string& key, bool defaultValue = false) const;
    
    /**
     * @brief 设置配置项
     * @param key 配置键
     * @param value 配置值
     */
    void set(const std::string& key, const std::string& value);
    
    /**
     * @brief 获取认证令牌
     * @return 认证令牌
     */
    std::string getAuthToken() const;
    
    /**
     * @brief 生成新的认证令牌
     * @return 新的认证令牌
     */
    std::string generateAuthToken();

private:
    ConfigManager() = default;
    ~ConfigManager() = default;
    
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::string> config_;
    
    std::string generateRandomToken() const;
};

// 便利函数
inline ConfigManager& getConfig() {
    return ConfigManager::getInstance();
}

} // namespace picoradar::config

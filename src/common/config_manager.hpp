#pragma once

#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <shared_mutex>
#include <string>
#include <tl/expected.hpp>
#include <unordered_map>

namespace picoradar::common {

/**
 * @brief 配置错误类型
 */
struct ConfigError {
  std::string message;

  explicit ConfigError(std::string msg) : message(std::move(msg)) {}
};

/**
 * @brief 配置结果类型
 */
template <typename T>
using ConfigResult = tl::expected<T, ConfigError>;

/**
 * @brief 现代化的配置管理器
 *
 * 使用 nlohmann/json 和 tl::expected 提供类型安全的配置管理
 */
class ConfigManager {
 public:
  static ConfigManager& getInstance();

  // 加载配置
  ConfigResult<void> loadFromFile(const std::string& filename);
  ConfigResult<void> loadFromJson(const nlohmann::json& json);

  // 类型安全的获取方法
  template <typename T>
  ConfigResult<T> get(const std::string& key) const;

  // 便利方法
  ConfigResult<std::string> getString(const std::string& key) const;
  ConfigResult<int> getInt(const std::string& key) const;
  ConfigResult<bool> getBool(const std::string& key) const;
  ConfigResult<double> getDouble(const std::string& key) const;

  // 获取配置值，如果不存在则使用提供的默认值
  template <typename T>
  T getWithDefault(const std::string& key, const T& default_value) const;

  // 专门用于端口号的方法，自动使用常量作为默认值
  uint16_t getServicePort() const;
  uint16_t getDiscoveryPort() const;

  // 设置值
  template <typename T>
  void set(const std::string& key, const T& value);

  // 检查键是否存在
  bool hasKey(const std::string& key) const;

  // 保存配置
  ConfigResult<void> saveToFile(const std::string& filename) const;

  // 获取整个配置的副本
  nlohmann::json getConfig() const;

  // 验证配置完整性
  bool validateConfig() const;

 private:
  ConfigManager() = default;
  ~ConfigManager() = default;
  ConfigManager(const ConfigManager&) = delete;
  ConfigManager& operator=(const ConfigManager&) = delete;

  mutable std::shared_mutex mutex_;
  nlohmann::json config_;
  mutable std::unordered_map<std::string, nlohmann::json> cache_;  // 新增缓存

  // 私有辅助方法
  ConfigResult<nlohmann::json> getJsonValue(const std::string& key) const;
  void loadEnvironmentVariables();
  void validateCriticalConfigs();
  static std::string generateSecureToken();

  // 内部方法，假设调用者已持有锁
  template <typename T>
  void setNoLock(const std::string& key, const T& value);
  ConfigResult<nlohmann::json> getJsonValueNoLock(const std::string& key) const;
  bool hasKeyNoLock(const std::string& key) const;
};

// 模板特化声明
template <>
void ConfigManager::set<std::string>(const std::string& key,
                                     const std::string& value);

template <>
void ConfigManager::set<int>(const std::string& key, const int& value);

template <>
void ConfigManager::set<bool>(const std::string& key, const bool& value);

template <>
void ConfigManager::set<double>(const std::string& key, const double& value);

// getWithDefault 模板特化声明
template <>
std::string ConfigManager::getWithDefault<std::string>(
    const std::string& key, const std::string& default_value) const;

template <>
int ConfigManager::getWithDefault<int>(const std::string& key,
                                       const int& default_value) const;

template <>
bool ConfigManager::getWithDefault<bool>(const std::string& key,
                                         const bool& default_value) const;

template <>
double ConfigManager::getWithDefault<double>(const std::string& key,
                                             const double& default_value) const;

}  // namespace picoradar::common
#include "config_manager.hpp"

#include <glog/logging.h>

#include <cstdlib>
#include <fstream>
#include <random>
#include <sstream>

#include "constants.hpp"

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
      return tl::make_unexpected(
          ConfigError{"Failed to open config file: " + filename});
    }

    json json_config;
    file >> json_config;

    std::unique_lock<std::shared_mutex> lock(mutex_);
    config_ = std::move(json_config);
    cache_.clear();  // 清空缓存

    LOG(INFO) << "Loaded config from: " << filename;
    loadEnvironmentVariables();

    return {};
  } catch (const std::exception& e) {
    return tl::make_unexpected(ConfigError{"Failed to parse config file " +
                                           filename + ": " + e.what()});
  }
}

ConfigResult<void> ConfigManager::loadFromJson(const nlohmann::json& json) {
  try {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    config_ = json;
    cache_.clear();  // 清空缓存
    loadEnvironmentVariables();
    return {};
  } catch (const std::exception& e) {
    return tl::make_unexpected(
        ConfigError{"Failed to load JSON config: " + std::string(e.what())});
  }
}

ConfigResult<std::string> ConfigManager::getString(
    const std::string& key) const {
  auto result = getJsonValue(key);
  if (!result) {
    return tl::make_unexpected(result.error());
  }

  try {
    return result->get<std::string>();
  } catch (const std::exception& e) {
    return tl::make_unexpected(
        ConfigError{"Value at key '" + key + "' is not a string: " + e.what()});
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
    return tl::make_unexpected(ConfigError{"Value at key '" + key +
                                           "' is not an integer: " + e.what()});
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
    return tl::make_unexpected(ConfigError{"Value at key '" + key +
                                           "' is not a boolean: " + e.what()});
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
    return tl::make_unexpected(
        ConfigError{"Value at key '" + key + "' is not a double: " + e.what()});
  }
}

bool ConfigManager::hasKey(const std::string& key) const {
  auto result = getJsonValue(key);
  return result.has_value();
}

ConfigResult<void> ConfigManager::saveToFile(
    const std::string& filename) const {
  try {
    std::ofstream file(filename);
    if (!file.is_open()) {
      return tl::make_unexpected(
          ConfigError{"Failed to open file for writing: " + filename});
    }

    std::shared_lock<std::shared_mutex> lock(mutex_);
    file << config_.dump(4);

    return {};
  } catch (const std::exception& e) {
    return tl::make_unexpected(
        ConfigError{"Failed to save config to file: " + std::string(e.what())});
  }
}

nlohmann::json ConfigManager::getConfig() const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  return config_;
}

ConfigResult<nlohmann::json> ConfigManager::getJsonValue(
    const std::string& key) const {
  // 首先尝试用共享锁读取缓存
  {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = cache_.find(key);
    if (it != cache_.end()) {
      return it->second;
    }
  }

  // 如果缓存中没有，需要独占锁来更新缓存
  std::unique_lock<std::shared_mutex> lock(mutex_);

  // 双重检查：可能在获取独占锁期间其他线程已经更新了缓存
  auto it = cache_.find(key);
  if (it != cache_.end()) {
    return it->second;
  }

  json current = config_;

  // 优化：如果键中没有点，直接处理简单键
  if (key.find('.') == std::string::npos) {
    if (!current.is_object() || !current.contains(key)) {
      return tl::make_unexpected(ConfigError{"Key not found: " + key});
    }
    current = current[key];
  } else {
    // 处理点分割的键路径 - 优化版本
    size_t start = 0;
    size_t end = 0;

    while (end != std::string::npos) {
      end = key.find('.', start);
      std::string k = key.substr(
          start, (end == std::string::npos) ? std::string::npos : end - start);

      if (!k.empty()) {
        if (!current.is_object() || !current.contains(k)) {
          return tl::make_unexpected(ConfigError{"Key not found: " + key});
        }
        current = current[k];
      }

      start = ((end > (std::string::npos - 1)) ? std::string::npos : end + 1);
    }
  }

  // 存入缓存
  cache_[key] = current;

  return current;
}

void ConfigManager::loadEnvironmentVariables() {
  // 注意：此方法假设调用者已经持有mutex_锁

  // 加载环境变量，例如：
  if (const char* port = std::getenv("PICORADAR_PORT")) {
    try {
      setNoLock("server.port", std::stoi(port));
    } catch (...) {
      LOG(WARNING) << "Invalid PICORADAR_PORT value: " << port;
    }
  }

  if (const char* auth = std::getenv("PICORADAR_AUTH_ENABLED")) {
    setNoLock("server.auth.enabled", std::string(auth) == "true");
  }

  if (const char* token = std::getenv("PICORADAR_AUTH_TOKEN")) {
    setNoLock("server.auth.token", std::string(token));
  }
}

std::string ConfigManager::generateSecureToken() const {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 61);

  const std::string chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  std::string token;
  token.reserve(32);

  for (int i = 0; i < 32; ++i) {
    token += chars[dis(gen)];
  }

  return token;
}

// 模板特化实现
template <>
void ConfigManager::set<std::string>(const std::string& key,
                                     const std::string& value) {
  std::unique_lock<std::shared_mutex> lock(mutex_);
  setNoLock(key, value);
  cache_.clear();  // 写入操作后清空缓存
}

template <>
void ConfigManager::set<int>(const std::string& key, const int& value) {
  std::unique_lock<std::shared_mutex> lock(mutex_);
  setNoLock(key, value);
  cache_.clear();  // 写入操作后清空缓存
}

template <>
void ConfigManager::set<bool>(const std::string& key, const bool& value) {
  std::unique_lock<std::shared_mutex> lock(mutex_);
  setNoLock(key, value);
  cache_.clear();  // 写入操作后清空缓存
}

template <>
void ConfigManager::set<double>(const std::string& key, const double& value) {
  std::unique_lock<std::shared_mutex> lock(mutex_);
  setNoLock(key, value);
  cache_.clear();  // 写入操作后清空缓存
}

// 模板特化实现 - getWithDefault
template <>
std::string ConfigManager::getWithDefault<std::string>(
    const std::string& key, const std::string& default_value) const {
  auto result = getString(key);
  return result.has_value() ? result.value() : default_value;
}

template <>
int ConfigManager::getWithDefault<int>(const std::string& key,
                                       const int& default_value) const {
  auto result = getInt(key);
  return result.has_value() ? result.value() : default_value;
}

template <>
bool ConfigManager::getWithDefault<bool>(const std::string& key,
                                         const bool& default_value) const {
  auto result = getBool(key);
  return result.has_value() ? result.value() : default_value;
}

template <>
double ConfigManager::getWithDefault<double>(
    const std::string& key, const double& default_value) const {
  auto result = getDouble(key);
  return result.has_value() ? result.value() : default_value;
}

uint16_t ConfigManager::getServicePort() const {
  return static_cast<uint16_t>(getWithDefault(
      "server.port", static_cast<int>(picoradar::config::kDefaultServicePort)));
}

uint16_t ConfigManager::getDiscoveryPort() const {
  return static_cast<uint16_t>(getWithDefault(
      "discovery.udp_port",
      static_cast<int>(picoradar::config::kDefaultDiscoveryPort)));
}

// 内部设置方法实现 - 不获取锁（假设调用者已持有）
template <typename T>
void ConfigManager::setNoLock(const std::string& key, const T& value) {
  json* current = &config_;

  // 优化：如果键中没有点，直接处理简单键
  if (key.find('.') == std::string::npos) {
    if (!current->is_object()) {
      *current = json::object();
    }
    (*current)[key] = value;
  } else {
    // 处理点分割的键路径 - 优化版本
    size_t start = 0;
    size_t end = 0;
    std::string last_key;

    while (end != std::string::npos) {
      end = key.find('.', start);
      std::string k = key.substr(
          start, (end == std::string::npos) ? std::string::npos : end - start);

      if (!k.empty()) {
        if (end == std::string::npos) {
          // 这是最后一个键
          last_key = k;
          break;
        } else {
          // 不是最后一个键，继续向下
          if (!current->is_object()) {
            *current = json::object();
          }
          current = &(*current)[k];
        }
      }

      start = ((end > (std::string::npos - 1)) ? std::string::npos : end + 1);
    }

    if (!last_key.empty()) {
      if (!current->is_object()) {
        *current = json::object();
      }
      (*current)[last_key] = value;
    }
  }
}

}  // namespace picoradar::common

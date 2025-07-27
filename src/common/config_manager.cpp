#include "config_manager.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <random>

#include "common/logging.hpp"
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

    std::unique_lock lock(mutex_);
    config_ = std::move(json_config);
    cache_.clear();  // 清空缓存

    LOG_INFO << "Loaded config from: " << filename;
    loadEnvironmentVariables();

    // 验证关键配置项
    validateCriticalConfigs();
    return {};
  } catch (const std::exception& e) {
    return tl::make_unexpected(ConfigError{"Failed to parse config file " +
                                           filename + ": " + e.what()});
  }
}

ConfigResult<void> ConfigManager::loadFromJson(const nlohmann::json& json) {
  try {
    std::unique_lock lock(mutex_);
    config_ = json;
    cache_.clear();  // 清空缓存
    loadEnvironmentVariables();

    // 验证关键配置项
    validateCriticalConfigs();

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
    LOG_WARNING << "Config key not found: " << key << " - "
                << result.error().message;
    return tl::make_unexpected(result.error());
  }

  try {
    return result->get<std::string>();
  } catch (const std::exception& e) {
    LOG_WARNING << "Config value type mismatch for key '" << key
                << "': expected string, got " << result->type_name() << " - "
                << e.what();
    return tl::make_unexpected(
        ConfigError{"Value at key '" + key + "' is not a string: " + e.what()});
  }
}

ConfigResult<int> ConfigManager::getInt(const std::string& key) const {
  auto result = getJsonValue(key);
  if (!result) {
    LOG_WARNING << "Config key not found: " << key << " - "
                << result.error().message;
    return tl::make_unexpected(result.error());
  }

  try {
    return result->get<int>();
  } catch (const std::exception& e) {
    LOG_WARNING << "Config value type mismatch for key '" << key
                << "': expected integer, got " << result->type_name() << " - "
                << e.what();
    return tl::make_unexpected(ConfigError{"Value at key '" + key +
                                           "' is not an integer: " + e.what()});
  }
}

ConfigResult<bool> ConfigManager::getBool(const std::string& key) const {
  auto result = getJsonValue(key);
  if (!result) {
    LOG_WARNING << "Config key not found: " << key << " - "
                << result.error().message;
    return tl::make_unexpected(result.error());
  }

  try {
    return result->get<bool>();
  } catch (const std::exception& e) {
    LOG_WARNING << "Config value type mismatch for key '" << key
                << "': expected boolean, got " << result->type_name() << " - "
                << e.what();
    return tl::make_unexpected(ConfigError{"Value at key '" + key +
                                           "' is not a boolean: " + e.what()});
  }
}

ConfigResult<double> ConfigManager::getDouble(const std::string& key) const {
  auto result = getJsonValue(key);
  if (!result) {
    LOG_WARNING << "Config key not found: " << key << " - "
                << result.error().message;
    return tl::make_unexpected(result.error());
  }

  try {
    return result->get<double>();
  } catch (const std::exception& e) {
    LOG_WARNING << "Config value type mismatch for key '" << key
                << "': expected double, got " << result->type_name() << " - "
                << e.what();
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

    std::shared_lock lock(mutex_);
    file << config_.dump(4);

    return {};
  } catch (const std::exception& e) {
    return tl::make_unexpected(
        ConfigError{"Failed to save config to file: " + std::string(e.what())});
  }
}

nlohmann::json ConfigManager::getConfig() const {
  std::shared_lock lock(mutex_);
  return config_;
}

bool ConfigManager::validateConfig() const {
  std::shared_lock lock(mutex_);

  bool is_valid = true;

  // 检查关键配置项
  const std::vector<std::pair<std::string, std::string>> required_configs = {
      {"server.port", "Server port"},
      {"server.host", "Server host"},
      {"discovery.udp_port", "Discovery UDP port"},
      {"auth.token", "Authentication token"}};

  for (const auto& [key, description] : required_configs) {
    auto result = getJsonValue(key);
    if (!result.has_value()) {
      LOG_ERROR << "Missing critical config: " << description
                << " (key: " << key << ")";
      is_valid = false;
    }
  }

  // 验证端口值
  auto validate_port_value = [&](const std::string& key,
                                 const std::string& name) {
    auto result = getJsonValue(key);
    if (result.has_value()) {
      if (!result->is_number_integer()) {
        LOG_ERROR << "Invalid " << name << " port type in key '" << key
                  << "': expected integer, got " << result->type_name();
        is_valid = false;
        return;
      }
      int port = result->get<int>();
      if (port < 1 || port > 65535) {
        LOG_ERROR << "Invalid " << name << " port value: " << port
                  << " (must be between 1-65535)";
        is_valid = false;
      }
    }
  };

  validate_port_value("server.port", "service");
  validate_port_value("discovery.udp_port", "discovery");

  if (is_valid) {
    LOG_INFO << "Configuration validation passed";
  } else {
    LOG_ERROR << "Configuration validation failed";
  }

  return is_valid;
}

ConfigResult<nlohmann::json> ConfigManager::getJsonValue(
    const std::string& key) const {
  // 首先尝试用共享锁读取缓存
  {
    std::shared_lock lock(mutex_);
    auto it = cache_.find(key);
    if (it != cache_.end()) {
      return it->second;
    }
  }

  // 如果缓存中没有，需要独占锁来更新缓存
  std::unique_lock lock(mutex_);

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
          start, end == std::string::npos ? std::string::npos : end - start);

      if (!k.empty()) {
        if (!current.is_object() || !current.contains(k)) {
          return tl::make_unexpected(ConfigError{"Key not found: " + key});
        }
        current = current[k];
      }

      start = end > std::string::npos - 1 ? std::string::npos : end + 1;
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
      LOG_WARNING << "Invalid PICORADAR_PORT value: " << port;
    }
  }

  if (const char* auth = std::getenv("PICORADAR_AUTH_ENABLED")) {
    setNoLock("server.auth.enabled", std::string(auth) == "true");
  }

  if (const char* token = std::getenv("PICORADAR_AUTH_TOKEN")) {
    setNoLock("server.auth.token", std::string(token));
  }
}

auto ConfigManager::generateSecureToken() -> std::string {
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
  std::unique_lock lock(mutex_);
  setNoLock(key, value);
  cache_.clear();  // 写入操作后清空缓存
}

template <>
void ConfigManager::set<int>(const std::string& key, const int& value) {
  std::unique_lock lock(mutex_);
  setNoLock(key, value);
  cache_.clear();  // 写入操作后清空缓存
}

template <>
void ConfigManager::set<bool>(const std::string& key, const bool& value) {
  std::unique_lock lock(mutex_);
  setNoLock(key, value);
  cache_.clear();  // 写入操作后清空缓存
}

template <>
void ConfigManager::set<double>(const std::string& key, const double& value) {
  std::unique_lock lock(mutex_);
  setNoLock(key, value);
  cache_.clear();  // 写入操作后清空缓存
}

// 模板特化实现 - getWithDefault
template <>
std::string ConfigManager::getWithDefault<std::string>(
    const std::string& key, const std::string& default_value) const {
  auto result = getString(key);
  if (!result.has_value()) {
    LOG_WARNING << "Using default value for config key '" << key << "': '"
                << default_value << "' (reason: " << result.error().message
                << ")";
    return default_value;
  }
  return result.value();
}

template <>
int ConfigManager::getWithDefault<int>(const std::string& key,
                                       const int& default_value) const {
  auto result = getInt(key);
  if (!result.has_value()) {
    LOG_WARNING << "Using default value for config key '" << key
                << "': " << default_value
                << " (reason: " << result.error().message << ")";
    return default_value;
  }
  return result.value();
}

template <>
bool ConfigManager::getWithDefault<bool>(const std::string& key,
                                         const bool& default_value) const {
  auto result = getBool(key);
  if (!result.has_value()) {
    LOG_WARNING << "Using default value for config key '" << key
                << "': " << (default_value ? "true" : "false")
                << " (reason: " << result.error().message << ")";
    return default_value;
  }
  return result.value();
}

template <>
double ConfigManager::getWithDefault<double>(
    const std::string& key, const double& default_value) const {
  auto result = getDouble(key);
  if (!result.has_value()) {
    LOG_WARNING << "Using default value for config key '" << key
                << "': " << default_value
                << " (reason: " << result.error().message << ")";
    return default_value;
  }
  return result.value();
}

uint16_t ConfigManager::getServicePort() const {
  const std::string key = "server.port";
  const int default_port =
      static_cast<int>(picoradar::constants::kDefaultServicePort);

  auto result = getInt(key);
  if (!result.has_value()) {
    LOG_WARNING << "Failed to get service port from config key '" << key
                << "', using default port " << default_port
                << " (reason: " << result.error().message << ")";
    return static_cast<uint16_t>(default_port);
  }

  int port_value = result.value();
  if (port_value < 1 || port_value > 65535) {
    LOG_WARNING << "Invalid service port value " << port_value
                << " in config key '" << key << "', using default port "
                << default_port;
    return static_cast<uint16_t>(default_port);
  }

  return static_cast<uint16_t>(port_value);
}

uint16_t ConfigManager::getDiscoveryPort() const {
  const std::string key = "discovery.udp_port";
  const int default_port =
      static_cast<int>(picoradar::constants::kDefaultDiscoveryPort);

  auto result = getInt(key);
  if (!result.has_value()) {
    LOG_WARNING << "Failed to get discovery port from config key '" << key
                << "', using default port " << default_port
                << " (reason: " << result.error().message << ")";
    return static_cast<uint16_t>(default_port);
  }

  int port_value = result.value();
  if (port_value < 1 || port_value > 65535) {
    LOG_WARNING << "Invalid discovery port value " << port_value
                << " in config key '" << key << "', using default port "
                << default_port;
    return static_cast<uint16_t>(default_port);
  }

  return static_cast<uint16_t>(port_value);
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
          start, end == std::string::npos ? std::string::npos : end - start);

      if (!k.empty()) {
        if (end == std::string::npos) {
          // 这是最后一个键
          last_key = k;
          break;
        }
        // 不是最后一个键，继续向下
        if (!current->is_object()) {
          *current = json::object();
        }
        current = &(*current)[k];
      }

      start = end > std::string::npos - 1 ? std::string::npos : end + 1;
    }

    if (!last_key.empty()) {
      if (!current->is_object()) {
        *current = json::object();
      }
      (*current)[last_key] = value;
    }
  }
}

void ConfigManager::validateCriticalConfigs() {
  // 注意：此方法假设调用者已经持有mutex_锁

  try {
    // 验证关键配置项是否存在且有效
    const std::vector<std::pair<std::string, std::string>> required_configs = {
        {"server.port", "Server port"},
        {"server.host", "Server host"},
        {"discovery.udp_port", "Discovery UDP port"},
        {"discovery.request_message", "Discovery request message"},
        {"discovery.response_prefix", "Discovery response prefix"},
        {"auth.token", "Authentication token"},
        {"logging.level", "Logging level"}};

    for (const auto& [key, description] : required_configs) {
      try {
        // 直接检查配置而不调用会获取锁的方法
        if (!hasKeyNoLock(key)) {
          LOG_WARNING << "Critical config missing: " << description
                      << " (key: " << key << ") - will use default value";
        }
      } catch (const std::exception& e) {
        LOG_WARNING << "Error checking config key '" << key
                    << "': " << e.what();
      }
    }

    // 验证端口范围
    auto validate_port = [this](const std::string& key,
                                const std::string& name) {
      try {
        auto result = getJsonValueNoLock(key);
        if (result.has_value() && result->is_number_integer()) {
          int port = result->get<int>();
          if (port < 1 || port > 65535) {
            LOG_WARNING << "Invalid " << name << " port value: " << port
                        << " (must be between 1-65535) - will use default";
          }
        }
      } catch (const std::exception& e) {
        LOG_WARNING << "Error validating port '" << key << "': " << e.what();
      }
    };

    validate_port("server.port", "service");
    validate_port("discovery.udp_port", "discovery");

    // 验证日志级别
    try {
      auto log_level_result = getJsonValueNoLock("logging.level");
      if (log_level_result.has_value() && log_level_result->is_string()) {
        std::string level = log_level_result->get<std::string>();
        const std::vector<std::string> valid_levels = {
            "DEBUG", "INFO", "WARNING", "ERROR", "FATAL"};
        if (std::find(valid_levels.begin(), valid_levels.end(), level) ==
            valid_levels.end()) {
          LOG_WARNING << "Invalid logging level: " << level
                      << " (valid values: DEBUG, INFO, WARNING, ERROR, FATAL) "
                         "- will use default";
        }
      }
    } catch (const std::exception& e) {
      LOG_WARNING << "Error validating logging level: " << e.what();
    }

    LOG_INFO << "Configuration validation completed";
  } catch (const std::exception& e) {
    LOG_WARNING << "Error during configuration validation: " << e.what();
  }
}

ConfigResult<nlohmann::json> ConfigManager::getJsonValueNoLock(
    const std::string& key) const {
  // 注意：此方法假设调用者已经持有mutex_锁

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
          start, end == std::string::npos ? std::string::npos : end - start);

      if (!k.empty()) {
        if (!current.is_object() || !current.contains(k)) {
          return tl::make_unexpected(ConfigError{"Key not found: " + key});
        }
        current = current[k];
      }

      start = end > std::string::npos - 1 ? std::string::npos : end + 1;
    }
  }

  return current;
}

bool ConfigManager::hasKeyNoLock(const std::string& key) const {
  // 注意：此方法假设调用者已经持有mutex_锁
  auto result = getJsonValueNoLock(key);
  return result.has_value();
}

}  // namespace picoradar::common

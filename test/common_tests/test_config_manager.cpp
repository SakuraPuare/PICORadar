#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>

#include "common/config_manager.hpp"
#include "common/logging.hpp"

using namespace picoradar::common;

class ConfigManagerTest : public testing::Test {
 protected:
  static void SetUpTestSuite() {
    // 初始化日志系统
    logger::LogConfig config = logger::LogConfig::loadFromConfigManager();
    config.log_directory = "./logs";
    config.global_level = logger::LogLevel::INFO;
    config.file_enabled = true;
    config.console_enabled = false;
    config.max_files = 10;
    logger::Logger::Init("config_manager_test", config);
  }

  static void TearDownTestSuite() {
    // glog 会自动清理
  }

  void SetUp() override {
    // 创建临时配置文件目录
    temp_dir_ = std::filesystem::temp_directory_path() / "picoradar_test";
    std::filesystem::create_directories(temp_dir_);

    // 创建测试配置文件路径
    test_config_path_ = temp_dir_ / "test_config.json";
  }

  void TearDown() override {
    // 清理临时文件
    if (std::filesystem::exists(temp_dir_)) {
      std::filesystem::remove_all(temp_dir_);
    }
  }

  void createTestConfigFile(const std::string& content) const {
    std::ofstream file(test_config_path_);
    file << content;
    file.close();
  }

  std::filesystem::path temp_dir_;
  std::filesystem::path test_config_path_;
};

/**
 * @brief 测试ConfigManager的单例模式
 */
TEST_F(ConfigManagerTest, SingletonPattern) {
  ConfigManager& instance1 = ConfigManager::getInstance();
  ConfigManager& instance2 = ConfigManager::getInstance();

  // 验证是同一个实例
  EXPECT_EQ(&instance1, &instance2);
}

/**
 * @brief 测试从文件加载配置
 */
TEST_F(ConfigManagerTest, LoadFromFile) {
  // 创建测试配置文件（JSON格式）
  createTestConfigFile(R"({
        "server_port": 8080,
        "server_host": "localhost",
        "debug": true,
        "timeout": 1,
        "auth_token": "test_token_123"
    })");

  ConfigManager& config = ConfigManager::getInstance();

  // 测试文件加载
  auto result = config.loadFromFile(test_config_path_.string());
  EXPECT_TRUE(result.has_value()) << "Failed to load config file";

  // 验证配置值
  auto host = config.getString("server_host");
  EXPECT_TRUE(host.has_value());
  EXPECT_EQ(host.value(), "localhost");

  auto port = config.getInt("server_port");
  EXPECT_TRUE(port.has_value());
  EXPECT_EQ(port.value(), 8080);

  auto debug = config.getBool("debug");
  EXPECT_TRUE(debug.has_value());
  EXPECT_TRUE(debug.value());

  auto timeout = config.getInt("timeout");
  EXPECT_TRUE(timeout.has_value());
  EXPECT_EQ(timeout.value(), 1);

  auto token = config.getString("auth_token");
  EXPECT_TRUE(token.has_value());
  EXPECT_EQ(token.value(), "test_token_123");
}

/**
 * @brief 测试加载不存在的文件
 */
TEST_F(ConfigManagerTest, LoadNonExistentFile) {
  ConfigManager& config = ConfigManager::getInstance();

  // 测试不存在的文件
  auto result = config.loadFromFile("/non/existent/path/config.json");
  EXPECT_FALSE(result.has_value());
}

/**
 * @brief 测试无效的JSON配置文件格式
 */
TEST_F(ConfigManagerTest, InvalidJsonFormat) {
  // 创建包含无效JSON格式的配置文件
  createTestConfigFile(R"({
        "valid_key": "valid_value",
        "another_valid_key": "another_value"
        // 缺少闭合括号，故意制造无效JSON
    )");

  ConfigManager& config = ConfigManager::getInstance();

  // 应该加载失败
  auto result = config.loadFromFile(test_config_path_.string());
  EXPECT_FALSE(result.has_value());
}

/**
 * @brief 测试获取不存在的配置项
 */
TEST_F(ConfigManagerTest, GetNonExistentKeys) {
  const ConfigManager& config = ConfigManager::getInstance();

  // 测试不存在的键应该返回错误
  auto result = config.getString("non_existent_key");
  EXPECT_FALSE(result.has_value());

  auto int_result = config.getInt("non_existent_int");
  EXPECT_FALSE(int_result.has_value());

  auto bool_result = config.getBool("non_existent_bool");
  EXPECT_FALSE(bool_result.has_value());

  auto double_result = config.getDouble("non_existent_double");
  EXPECT_FALSE(double_result.has_value());
}

/**
 * @brief 测试布尔值解析
 */
TEST_F(ConfigManagerTest, BooleanParsing) {
  createTestConfigFile(R"({
        "bool_true": true,
        "bool_false": false
    })");

  ConfigManager& config = ConfigManager::getInstance();
  auto result = config.loadFromFile(test_config_path_.string());
  EXPECT_TRUE(result.has_value());

  // 测试布尔值
  auto true_val = config.getBool("bool_true");
  EXPECT_TRUE(true_val.has_value());
  EXPECT_TRUE(true_val.value());

  auto false_val = config.getBool("bool_false");
  EXPECT_TRUE(false_val.has_value());
  EXPECT_FALSE(false_val.value());
}

/**
 * @brief 测试数值解析
 */
TEST_F(ConfigManagerTest, NumberParsing) {
  createTestConfigFile(R"({
        "int_positive": 123,
        "int_negative": -456,
        "int_zero": 0,
        "double_positive": 3.14159,
        "double_negative": -2.718,
        "double_zero": 0.0
    })");

  ConfigManager& config = ConfigManager::getInstance();
  auto result = config.loadFromFile(test_config_path_.string());
  EXPECT_TRUE(result.has_value());

  // 测试整数解析
  auto pos_int = config.getInt("int_positive");
  EXPECT_TRUE(pos_int.has_value());
  EXPECT_EQ(pos_int.value(), 123);

  auto neg_int = config.getInt("int_negative");
  EXPECT_TRUE(neg_int.has_value());
  EXPECT_EQ(neg_int.value(), -456);

  auto zero_int = config.getInt("int_zero");
  EXPECT_TRUE(zero_int.has_value());
  EXPECT_EQ(zero_int.value(), 0);

  // 测试浮点数解析
  auto pos_double = config.getDouble("double_positive");
  EXPECT_TRUE(pos_double.has_value());
  EXPECT_DOUBLE_EQ(pos_double.value(), 3.14159);

  auto neg_double = config.getDouble("double_negative");
  EXPECT_TRUE(neg_double.has_value());
  EXPECT_DOUBLE_EQ(neg_double.value(), -2.718);

  auto zero_double = config.getDouble("double_zero");
  EXPECT_TRUE(zero_double.has_value());
  EXPECT_DOUBLE_EQ(zero_double.value(), 0.0);
}

/**
 * @brief 测试配置键检查
 */
TEST_F(ConfigManagerTest, HasKeyFunction) {
  createTestConfigFile(R"({
        "existing_key": "value",
        "another_key": 42
    })");

  ConfigManager& config = ConfigManager::getInstance();
  auto result = config.loadFromFile(test_config_path_.string());
  EXPECT_TRUE(result.has_value());

  // 测试存在的键
  EXPECT_TRUE(config.hasKey("existing_key"));
  EXPECT_TRUE(config.hasKey("another_key"));

  // 测试不存在的键
  EXPECT_FALSE(config.hasKey("non_existent_key"));
}

/**
 * @brief 测试线程安全性
 */
TEST_F(ConfigManagerTest, ThreadSafety) {
  // 创建配置文件
  createTestConfigFile(R"({
        "shared_value": "initial",
        "counter": 0
    })");

  ConfigManager& config = ConfigManager::getInstance();
  auto result = config.loadFromFile(test_config_path_.string());
  EXPECT_TRUE(result.has_value());

  constexpr int num_threads = 10;
  constexpr int operations_per_thread = 100;
  std::vector<std::thread> threads;
  std::atomic<int> success_count{0};

  // 启动多个线程同时读取配置
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&config, &success_count, operations_per_thread] {
      for (int j = 0; j < operations_per_thread; ++j) {
        // 同时读取不同的配置项
        auto value = config.getString("shared_value");
        auto counter = config.getInt("counter");
        bool has_key = config.hasKey("shared_value");

        if (value.has_value() && counter.has_value() && has_key) {
          success_count.fetch_add(1);
        }
      }
    });
  }

  // 等待所有线程完成
  for (auto& thread : threads) {
    thread.join();
  }

  // 验证所有操作都成功
  EXPECT_EQ(success_count.load(), num_threads * operations_per_thread);
}

/**
 * @brief 测试环境变量加载
 */
TEST_F(ConfigManagerTest, EnvironmentVariablesLoading) {
  // 设置环境变量
  setenv("PICORADAR_PORT", "8080", 1);
  setenv("PICORADAR_AUTH_ENABLED", "true", 1);
  setenv("PICORADAR_AUTH_TOKEN", "test_token_123", 1);

  createTestConfigFile(R"({
        "server": {
            "port": 9000
        }
    })");

  ConfigManager& config = ConfigManager::getInstance();
  auto result = config.loadFromFile(test_config_path_.string());
  EXPECT_TRUE(result.has_value());

  // 环境变量应该覆盖配置文件的值
  auto port = config.getInt("server.port");
  EXPECT_TRUE(port.has_value());
  EXPECT_EQ(port.value(), 8080);

  auto auth_enabled = config.getBool("server.auth.enabled");
  EXPECT_TRUE(auth_enabled.has_value());
  EXPECT_TRUE(auth_enabled.value());

  auto auth_token = config.getString("server.auth.token");
  EXPECT_TRUE(auth_token.has_value());
  EXPECT_EQ(auth_token.value(), "test_token_123");

  // 清理环境变量
  unsetenv("PICORADAR_PORT");
  unsetenv("PICORADAR_AUTH_ENABLED");
  unsetenv("PICORADAR_AUTH_TOKEN");
}

/**
 * @brief 测试嵌套键的设置和获取
 */
TEST_F(ConfigManagerTest, NestedKeyOperations) {
  ConfigManager& config = ConfigManager::getInstance();

  // 设置嵌套键
  config.set("level1.level2.level3", std::string("deep_value"));
  config.set("level1.level2.number", 42);
  config.set("level1.another_branch.flag", true);

  // 验证可以获取嵌套键
  auto deep_value = config.getString("level1.level2.level3");
  EXPECT_TRUE(deep_value.has_value());
  EXPECT_EQ(deep_value.value(), "deep_value");

  auto number = config.getInt("level1.level2.number");
  EXPECT_TRUE(number.has_value());
  EXPECT_EQ(number.value(), 42);

  auto flag = config.getBool("level1.another_branch.flag");
  EXPECT_TRUE(flag.has_value());
  EXPECT_TRUE(flag.value());

  // 验证hasKey对嵌套键的支持
  EXPECT_TRUE(config.hasKey("level1.level2.level3"));
  EXPECT_TRUE(config.hasKey("level1.level2.number"));
  EXPECT_TRUE(config.hasKey("level1.another_branch.flag"));
  EXPECT_FALSE(config.hasKey("level1.level2.nonexistent"));
}

/**
 * @brief 测试配置保存功能
 */
TEST_F(ConfigManagerTest, SaveToFileFunction) {
  ConfigManager& config = ConfigManager::getInstance();

  // 设置一些配置值
  config.set("test_string", std::string("save_test"));
  config.set("test_int", 123);
  config.set("test_bool", true);
  config.set("test_double", 3.14159);
  config.set("nested.value", std::string("nested_save"));

  // 保存到文件
  auto save_result = config.saveToFile(test_config_path_.string());
  EXPECT_TRUE(save_result.has_value());

  // 创建新的实例并加载
  auto& new_config = ConfigManager::getInstance();
  auto load_result = new_config.loadFromFile(test_config_path_.string());
  EXPECT_TRUE(load_result.has_value());

  // 验证所有值都被正确保存和加载
  auto string_val = new_config.getString("test_string");
  EXPECT_TRUE(string_val.has_value());
  EXPECT_EQ(string_val.value(), "save_test");

  auto int_val = new_config.getInt("test_int");
  EXPECT_TRUE(int_val.has_value());
  EXPECT_EQ(int_val.value(), 123);

  auto bool_val = new_config.getBool("test_bool");
  EXPECT_TRUE(bool_val.has_value());
  EXPECT_TRUE(bool_val.value());

  auto double_val = new_config.getDouble("test_double");
  EXPECT_TRUE(double_val.has_value());
  EXPECT_DOUBLE_EQ(double_val.value(), 3.14159);

  auto nested_val = new_config.getString("nested.value");
  EXPECT_TRUE(nested_val.has_value());
  EXPECT_EQ(nested_val.value(), "nested_save");
}

/**
 * @brief 测试getWithDefault函数
 */
TEST_F(ConfigManagerTest, GetWithDefaultFunction) {
  createTestConfigFile(R"({
        "existing_string": "test_value",
        "existing_int": 100
    })");

  ConfigManager& config = ConfigManager::getInstance();
  auto result = config.loadFromFile(test_config_path_.string());
  EXPECT_TRUE(result.has_value());

  // 测试存在的键，应该返回实际值
  EXPECT_EQ(config.getWithDefault("existing_string", std::string("default")),
            "test_value");
  EXPECT_EQ(config.getWithDefault("existing_int", -1), 100);

  // 测试不存在的键，应该返回默认值
  EXPECT_EQ(config.getWithDefault("nonexistent_string", std::string("default")),
            "default");
  EXPECT_EQ(config.getWithDefault("nonexistent_int", -1), -1);
  EXPECT_EQ(config.getWithDefault("nonexistent_bool", true), true);
  EXPECT_DOUBLE_EQ(config.getWithDefault("nonexistent_double", 2.71), 2.71);
}

/**
 * @brief 测试端口号获取方法
 */
TEST_F(ConfigManagerTest, PortNumberMethods) {
  ConfigManager& config = ConfigManager::getInstance();

  // 测试默认端口号
  const uint16_t service_port = config.getServicePort();
  const uint16_t discovery_port = config.getDiscoveryPort();

  // 验证返回的是合理的端口号
  EXPECT_GT(service_port, 0);
  EXPECT_GT(discovery_port, 0);
  EXPECT_LE(service_port, 65535);
  EXPECT_LE(discovery_port, 65535);

  // 设置自定义端口并测试
  config.set("server.port", 8080);
  config.set("discovery.udp_port", 9090);

  EXPECT_EQ(config.getServicePort(), 8080);
  EXPECT_EQ(config.getDiscoveryPort(), 9090);
}

/**
 * @brief 测试JSON加载功能
 */
TEST_F(ConfigManagerTest, LoadFromJsonFunction) {
  ConfigManager& config = ConfigManager::getInstance();

  nlohmann::json test_json = {
      {"string_key", "json_value"},
      {"int_key", 456},
      {"bool_key", false},
      {"nested", {{"sub_key", "sub_value"}, {"sub_number", 789}}}};

  auto result = config.loadFromJson(test_json);
  EXPECT_TRUE(result.has_value());

  // 验证JSON数据被正确加载
  auto string_val = config.getString("string_key");
  EXPECT_TRUE(string_val.has_value());
  EXPECT_EQ(string_val.value(), "json_value");

  auto int_val = config.getInt("int_key");
  EXPECT_TRUE(int_val.has_value());
  EXPECT_EQ(int_val.value(), 456);

  auto bool_val = config.getBool("bool_key");
  EXPECT_TRUE(bool_val.has_value());
  EXPECT_FALSE(bool_val.value());

  auto nested_val = config.getString("nested.sub_key");
  EXPECT_TRUE(nested_val.has_value());
  EXPECT_EQ(nested_val.value(), "sub_value");

  auto nested_num = config.getInt("nested.sub_number");
  EXPECT_TRUE(nested_num.has_value());
  EXPECT_EQ(nested_num.value(), 789);
}

/**
 * @brief 测试错误处理
 */
TEST_F(ConfigManagerTest, ErrorHandling) {
  ConfigManager& config = ConfigManager::getInstance();

  // 测试加载不存在的文件
  auto load_result = config.loadFromFile("/nonexistent/path/config.json");
  EXPECT_FALSE(load_result.has_value());

  // 测试加载无效JSON文件
  createTestConfigFile("{ invalid json }");
  auto invalid_result = config.loadFromFile(test_config_path_.string());
  EXPECT_FALSE(invalid_result.has_value());

  // 加载正确的配置用于后续测试
  createTestConfigFile(R"({
        "string_value": "test",
        "number_value": 42
    })");
  auto valid_result = config.loadFromFile(test_config_path_.string());
  EXPECT_TRUE(valid_result.has_value());

  // 测试类型不匹配的错误
  auto string_as_int = config.getInt("string_value");
  EXPECT_FALSE(string_as_int.has_value());

  auto number_as_bool = config.getBool("number_value");
  EXPECT_FALSE(number_as_bool.has_value());

  // 测试不存在的键
  auto nonexistent = config.getString("nonexistent_key");
  EXPECT_FALSE(nonexistent.has_value());

  // 测试保存到无效路径
  auto save_result = config.saveToFile("/invalid/path/config.json");
  EXPECT_FALSE(save_result.has_value());
}

/**
 * @brief 测试getConfig方法
 */
TEST_F(ConfigManagerTest, GetConfigMethod) {
  ConfigManager& config = ConfigManager::getInstance();

  createTestConfigFile(R"({
        "test_key": "test_value",
        "nested": {
            "inner_key": "inner_value"
        }
    })");

  auto result = config.loadFromFile(test_config_path_.string());
  EXPECT_TRUE(result.has_value());

  nlohmann::json full_config = config.getConfig();

  // 验证返回的JSON包含所有配置
  EXPECT_TRUE(full_config.contains("test_key"));
  EXPECT_EQ(full_config["test_key"], "test_value");
  EXPECT_TRUE(full_config.contains("nested"));
  EXPECT_TRUE(full_config["nested"].contains("inner_key"));
  EXPECT_EQ(full_config["nested"]["inner_key"], "inner_value");
}

/**
 * @brief 测试配置文件的边界条件
 */
TEST_F(ConfigManagerTest, ConfigFileBoundaryConditions) {
  ConfigManager& config = ConfigManager::getInstance();

  // 测试空JSON文件
  createTestConfigFile("{}");
  auto empty_result = config.loadFromFile(test_config_path_.string());
  EXPECT_TRUE(empty_result.has_value());
  EXPECT_EQ(config.getConfig().size(), 0);

  // 测试只有null值的JSON
  createTestConfigFile(R"({"null_value": null})");
  auto null_result = config.loadFromFile(test_config_path_.string());
  EXPECT_TRUE(null_result.has_value());
  EXPECT_FALSE(config.getString("null_value").has_value());

  // 测试包含数组的JSON
  createTestConfigFile(R"({
        "array_value": [1, 2, 3, "string", true],
        "object_array": [{"name": "obj1"}, {"name": "obj2"}]
    })");
  auto array_result = config.loadFromFile(test_config_path_.string());
  EXPECT_TRUE(array_result.has_value());

  // 数组应该存在于配置中，但不能直接获取为基本类型
  EXPECT_TRUE(config.hasKey("array_value"));
  EXPECT_FALSE(config.getString("array_value").has_value());
}

/**
 * @brief 测试极大和极小的数值
 */
TEST_F(ConfigManagerTest, ExtremeNumericValues) {
  ConfigManager& config = ConfigManager::getInstance();

  nlohmann::json extreme_json = {
      {"max_int", std::numeric_limits<int>::max()},
      {"min_int", std::numeric_limits<int>::min()},
      {"max_double", std::numeric_limits<double>::max()},
      {"min_double", std::numeric_limits<double>::lowest()},
      {"infinity", std::numeric_limits<double>::infinity()},
      {"neg_infinity", -std::numeric_limits<double>::infinity()}};

  auto result = config.loadFromJson(extreme_json);
  EXPECT_TRUE(result.has_value());

  // 测试极值整数
  auto max_int = config.getInt("max_int");
  EXPECT_TRUE(max_int.has_value());
  EXPECT_EQ(max_int.value(), std::numeric_limits<int>::max());

  auto min_int = config.getInt("min_int");
  EXPECT_TRUE(min_int.has_value());
  EXPECT_EQ(min_int.value(), std::numeric_limits<int>::min());

  // 测试极值浮点数
  auto max_double = config.getDouble("max_double");
  EXPECT_TRUE(max_double.has_value());
  EXPECT_DOUBLE_EQ(max_double.value(), std::numeric_limits<double>::max());

  auto min_double = config.getDouble("min_double");
  EXPECT_TRUE(min_double.has_value());
  EXPECT_DOUBLE_EQ(min_double.value(), std::numeric_limits<double>::lowest());

  // 测试无穷大值
  auto inf_val = config.getDouble("infinity");
  EXPECT_TRUE(inf_val.has_value());
  EXPECT_TRUE(std::isinf(inf_val.value()));
  EXPECT_GT(inf_val.value(), 0);

  auto neg_inf_val = config.getDouble("neg_infinity");
  EXPECT_TRUE(neg_inf_val.has_value());
  EXPECT_TRUE(std::isinf(neg_inf_val.value()));
  EXPECT_LT(neg_inf_val.value(), 0);
}

/**
 * @brief 测试超长字符串和键名
 */
TEST_F(ConfigManagerTest, VeryLongStringsAndKeys) {
  ConfigManager& config = ConfigManager::getInstance();

  // 创建超长字符串
  std::string very_long_value(10000, 'A');
  std::string very_long_key(1000, 'K');

  config.set("very_long_value", very_long_value);
  config.set(very_long_key, std::string("short_value"));

  // 验证超长值能正确存储和获取
  auto retrieved_long = config.getString("very_long_value");
  EXPECT_TRUE(retrieved_long.has_value());
  EXPECT_EQ(retrieved_long.value(), very_long_value);
  EXPECT_EQ(retrieved_long.value().length(), 10000);

  // 验证超长键名能正确存储和获取
  auto retrieved_by_long_key = config.getString(very_long_key);
  EXPECT_TRUE(retrieved_by_long_key.has_value());
  EXPECT_EQ(retrieved_by_long_key.value(), "short_value");
}

/**
 * @brief 测试特殊字符在键名和值中的处理
 */
TEST_F(ConfigManagerTest, SpecialCharactersHandling) {
  ConfigManager& config = ConfigManager::getInstance();

  // 测试包含特殊字符的键名和值
  std::string special_key = "key.with.dots[and]brackets{and}braces";
  std::string special_value = "Value with newlines\nand tabs\tand quotes\"'";
  std::string unicode_value = "Unicode: 中文 🌟 ñoël café";

  config.set(special_key, special_value);
  config.set("unicode_test", unicode_value);
  config.set("empty_string", std::string(""));

  // 验证特殊字符正确处理
  auto retrieved_special = config.getString(special_key);
  EXPECT_TRUE(retrieved_special.has_value());
  EXPECT_EQ(retrieved_special.value(), special_value);

  auto retrieved_unicode = config.getString("unicode_test");
  EXPECT_TRUE(retrieved_unicode.has_value());
  EXPECT_EQ(retrieved_unicode.value(), unicode_value);

  auto retrieved_empty = config.getString("empty_string");
  EXPECT_TRUE(retrieved_empty.has_value());
  EXPECT_EQ(retrieved_empty.value(), "");
}

/**
 * @brief 测试配置重载和覆盖行为
 */
TEST_F(ConfigManagerTest, ConfigReloadAndOverride) {
  ConfigManager& config = ConfigManager::getInstance();

  // 首次加载配置
  createTestConfigFile(R"({
        "shared_key": "original_value",
        "only_in_first": "first_value"
    })");
  auto first_load = config.loadFromFile(test_config_path_.string());
  EXPECT_TRUE(first_load.has_value());

  auto original_value = config.getString("shared_key");
  EXPECT_TRUE(original_value.has_value());
  EXPECT_EQ(original_value.value(), "original_value");

  auto first_only = config.getString("only_in_first");
  EXPECT_TRUE(first_only.has_value());
  EXPECT_EQ(first_only.value(), "first_value");

  // 第二次加载不同的配置（覆盖）
  createTestConfigFile(R"({
        "shared_key": "overridden_value",
        "only_in_second": "second_value"
    })");
  auto second_load = config.loadFromFile(test_config_path_.string());
  EXPECT_TRUE(second_load.has_value());

  // 验证覆盖行为
  auto overridden_value = config.getString("shared_key");
  EXPECT_TRUE(overridden_value.has_value());
  EXPECT_EQ(overridden_value.value(), "overridden_value");

  auto second_only = config.getString("only_in_second");
  EXPECT_TRUE(second_only.has_value());
  EXPECT_EQ(second_only.value(), "second_value");

  // 原来的键应该不再存在
  auto missing_first = config.getString("only_in_first");
  EXPECT_FALSE(missing_first.has_value());
}

/**
 * @brief 测试配置缓存机制
 */
TEST_F(ConfigManagerTest, ConfigCachingMechanism) {
  ConfigManager& config = ConfigManager::getInstance();

  createTestConfigFile(R"({
        "cached_string": "test_value",
        "cached_int": 42
    })");
  auto load_result = config.loadFromFile(test_config_path_.string());
  EXPECT_TRUE(load_result.has_value());

  // 首次访问，应该填充缓存
  auto start_time = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < 1000; ++i) {
    auto value = config.getString("cached_string");
    EXPECT_TRUE(value.has_value());
  }
  auto first_duration = std::chrono::high_resolution_clock::now() - start_time;

  // 第二次访问，应该使用缓存，速度更快
  start_time = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < 1000; ++i) {
    auto value = config.getString("cached_string");
    EXPECT_TRUE(value.has_value());
  }
  auto second_duration = std::chrono::high_resolution_clock::now() - start_time;

  // 缓存应该使访问速度更快（至少不会更慢）
  EXPECT_LE(second_duration, first_duration * 2);  // 允许一些性能波动
}

/**
 * @brief 测试高频率并发访问
 */
TEST_F(ConfigManagerTest, HighFrequencyConcurrentAccess) {
  ConfigManager& config = ConfigManager::getInstance();

  createTestConfigFile(R"({
        "concurrent_string": "concurrent_value",
        "concurrent_int": 100,
        "concurrent_bool": true
    })");
  auto load_result = config.loadFromFile(test_config_path_.string());
  EXPECT_TRUE(load_result.has_value());

  constexpr int num_threads = 20;
  constexpr int operations_per_thread = 1000;
  std::vector<std::thread> threads;
  std::atomic<int> successful_operations{0};
  std::atomic<int> failed_operations{0};

  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&] {
      for (int j = 0; j < operations_per_thread; ++j) {
        try {
          auto str_val = config.getString("concurrent_string");
          auto int_val = config.getInt("concurrent_int");
          auto bool_val = config.getBool("concurrent_bool");
          bool has_key = config.hasKey("concurrent_string");

          if (str_val.has_value() && int_val.has_value() &&
              bool_val.has_value() && has_key) {
            successful_operations.fetch_add(1);
          } else {
            failed_operations.fetch_add(1);
          }
        } catch (...) {
          failed_operations.fetch_add(1);
        }
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  // 验证所有操作都成功，没有崩溃或数据竞争
  EXPECT_EQ(successful_operations.load(), num_threads * operations_per_thread);
  EXPECT_EQ(failed_operations.load(), 0);
}

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <thread>
#include <atomic>

#include "common/config_manager.hpp"

using namespace picoradar::common;

class ConfigManagerTest : public ::testing::Test {
protected:
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
    
    void createTestConfigFile(const std::string& content) {
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
        "timeout": 30,
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
    EXPECT_EQ(timeout.value(), 30);
    
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
    ConfigManager& config = ConfigManager::getInstance();
    
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
    
    const int num_threads = 10;
    const int operations_per_thread = 100;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    // 启动多个线程同时读取配置
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&config, &success_count, operations_per_thread]() {
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

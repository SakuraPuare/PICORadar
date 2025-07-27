#include <gtest/gtest.h>

#include "common/string_utils.hpp"

using namespace picoradar::common;

class StringUtilsTest : public testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

/**
 * @brief 测试基本的十六进制转换
 */
TEST_F(StringUtilsTest, BasicHexConversion) {
  // 测试空字符串
  EXPECT_EQ(to_hex(""), "");

  // 测试单个字符
  EXPECT_EQ(to_hex("A"), "41");  // ASCII 'A' = 65 = 0x41
  EXPECT_EQ(to_hex("a"), "61");  // ASCII 'a' = 97 = 0x61
  EXPECT_EQ(to_hex("0"), "30");  // ASCII '0' = 48 = 0x30

  // 测试数字
  EXPECT_EQ(to_hex("123"), "313233");

  // 测试简单字符串
  EXPECT_EQ(to_hex("hello"), "68656c6c6f");
  EXPECT_EQ(to_hex("world"), "776f726c64");
}

/**
 * @brief 测试特殊字符的十六进制转换
 */
TEST_F(StringUtilsTest, SpecialCharactersHex) {
  // 测试空格
  EXPECT_EQ(to_hex(" "), "20");

  // 测试换行符
  EXPECT_EQ(to_hex("\n"), "0a");

  // 测试制表符
  EXPECT_EQ(to_hex("\t"), "09");

  // 测试回车符
  EXPECT_EQ(to_hex("\r"), "0d");

  // 测试空字符
  EXPECT_EQ(to_hex(std::string(1, '\0')), "00");

  // 测试混合特殊字符
  EXPECT_EQ(to_hex("a\nb"), "610a62");
}

/**
 * @brief 测试二进制数据的十六进制转换
 */
TEST_F(StringUtilsTest, BinaryDataHex) {
  // 测试二进制数据
  std::string binary_data;
  for (int i = 0; i < 256; ++i) {
    binary_data.push_back(static_cast<char>(i));
  }

  std::string hex_result = to_hex(binary_data);

  // 验证长度（每个字节对应2个十六进制字符）
  EXPECT_EQ(hex_result.length(), 512);

  // 验证前几个字节
  EXPECT_EQ(hex_result.substr(0, 2), "00");  // 0x00
  EXPECT_EQ(hex_result.substr(2, 2), "01");  // 0x01
  EXPECT_EQ(hex_result.substr(4, 2), "02");  // 0x02

  // 验证最后几个字节
  EXPECT_EQ(hex_result.substr(508, 2), "fe");  // 0xFE
  EXPECT_EQ(hex_result.substr(510, 2), "ff");  // 0xFF
}

/**
 * @brief 测试Unicode和多字节字符的十六进制转换
 */
TEST_F(StringUtilsTest, UnicodeCharactersHex) {
  // 测试UTF-8编码的中文字符
  std::string chinese = "中";  // UTF-8: E4 B8 AD
  EXPECT_EQ(to_hex(chinese), "e4b8ad");

  // 测试表情符号
  std::string emoji = "😀";  // UTF-8: F0 9F 98 80
  EXPECT_EQ(to_hex(emoji), "f09f9880");

  // 测试拉丁扩展字符
  std::string latin = "café";  // 'é' in UTF-8: C3 A9
  EXPECT_EQ(to_hex(latin), "636166c3a9");
}

/**
 * @brief 测试大数据的十六进制转换性能
 */
TEST_F(StringUtilsTest, LargeDataHex) {
  // 创建大字符串
  std::string large_string(10000, 'X');

  // 测试转换不会崩溃
  EXPECT_NO_THROW({
    std::string hex_result = to_hex(large_string);
    EXPECT_EQ(hex_result.length(), 20000);  // 每个字符变成2个十六进制字符

    // 验证所有字符都是'X' (0x58)
    for (size_t i = 0; i < hex_result.length(); i += 2) {
      EXPECT_EQ(hex_result.substr(i, 2), "58");
    }
  });
}

/**
 * @brief 测试边界条件
 */
TEST_F(StringUtilsTest, EdgeCases) {
  // 测试只包含null字符的字符串
  std::string null_string(5, '\0');
  EXPECT_EQ(to_hex(null_string), "0000000000");

  // 测试最大值字符
  std::string max_char(1, '\xFF');
  EXPECT_EQ(to_hex(max_char), "ff");

  // 测试混合null和其他字符
  std::string mixed = std::string("abc") + '\0' + "def";
  EXPECT_EQ(to_hex(mixed), "61626300646566");
}

/**
 * @brief 测试十六进制输出格式的一致性
 */
TEST_F(StringUtilsTest, HexFormatConsistency) {
  // 测试所有单字节值的格式一致性
  for (int i = 0; i < 256; ++i) {
    std::string single_char(1, static_cast<char>(i));
    std::string hex_result = to_hex(single_char);

    // 每个字节都应该产生恰好2个十六进制字符
    EXPECT_EQ(hex_result.length(), 2) << "Failed for byte value: " << i;

    // 验证字符都是有效的十六进制字符
    for (char c : hex_result) {
      EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))
          << "Invalid hex character: " << c << " for byte value: " << i;
    }
  }
}

/**
 * @brief 测试十六进制转换的往返一致性（如果有逆向函数的话）
 */
TEST_F(StringUtilsTest, RoundTripConsistency) {
  std::vector<std::string> test_strings = {
      "",
      "a",
      "hello world",
      "The quick brown fox jumps over the lazy dog",
      std::string(1, '\0'),
      std::string(1, '\xFF'),
      "Mixed\0Content",
      "🌟✨🚀",   // Unicode emojis
      "数据测试"  // Chinese characters
  };

  for (const auto& original : test_strings) {
    std::string hex = to_hex(original);

    // 验证十六进制字符串的基本属性
    EXPECT_EQ(hex.length(), original.length() * 2);

    // 验证只包含有效的十六进制字符
    for (char c : hex) {
      EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))
          << "Invalid hex character in: " << hex;
    }
  }
}

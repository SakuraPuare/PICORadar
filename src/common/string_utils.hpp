#pragma once

#include <string>

namespace picoradar::common {

/**
 * @brief 将字符串转换为十六进制表示
 * @param input 输入字符串
 * @return 十六进制表示的字符串
 */
auto to_hex(const std::string& input) -> std::string;

}  // namespace picoradar::common
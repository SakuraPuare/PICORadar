#include "string_utils.hpp"
#include <iomanip>
#include <sstream>

namespace picoradar::common {

// =========================
// 详细中文注释已添加到 to_hex 函数实现。
// =========================
std::string to_hex(const std::string& input) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (unsigned char c : input) {
        ss << std::setw(2) << static_cast<int>(c);
    }
    return ss.str();
}

} // namespace picoradar::common 
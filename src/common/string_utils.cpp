#include "string_utils.hpp"

#include <iomanip>
#include <sstream>

namespace picoradar::common {

auto to_hex(const std::string& input) -> std::string {
  std::stringstream ss;
  ss << std::hex << std::setfill('0');
  for (unsigned char c : input) {
    ss << std::setw(2) << static_cast<int>(c);
  }
  return ss.str();
}

}  // namespace picoradar::common
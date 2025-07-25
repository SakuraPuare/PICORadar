#include "logging.hpp"
#include <glog/logging.h>
#include <filesystem>

namespace picoradar::common {

// =========================
// 详细中文注释已添加到 setup_logging 函数实现。
// =========================
void setup_logging(std::string_view app_name, bool log_to_file, const std::string& log_dir) {
    google::InitGoogleLogging(app_name.data());

    if (log_to_file) {
        FLAGS_logtostderr = false;
        FLAGS_alsologtostderr = true;
        std::filesystem::path log_path = log_dir;
        FLAGS_log_dir = log_dir;
        if (!std::filesystem::exists(log_path)) {
            std::filesystem::create_directories(log_path);
        }
    } else {
        FLAGS_logtostderr = true;
    }
}

} // namespace picoradar::common 
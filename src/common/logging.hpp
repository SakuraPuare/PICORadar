#pragma once

#include <string>
#include <string_view>

namespace picoradar::common {

/**
 * @brief 初始化应用程序的日志系统。
 *
 * 这个函数封装了 glog 的初始化和配置逻辑，确保了日志系统
 * 在整个应用程序中的行为一致性。它会根据指定的程序名称设置
 * 日志，并可以选择性地将日志输出到文件和/或标准错误流。
 *
 * @param app_name 应用程序的名称，通常是 argv[0]。
 *                 用于glog内部识别和日志文件命名。
 * @param log_to_file 是否将日志写入文件。默认为 true。
 * @param log_dir 日志文件存放的目录。默认为 "./logs"。
 *                如果目录不存在，该函数会自动创建。
 */
void setup_logging(std::string_view app_name, bool log_to_file = true,
                   const std::string& log_dir = "./logs");

} // namespace picoradar::common 
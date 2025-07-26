#include "cli_log_adapter.hpp"
#include "cli_interface.hpp"
#include "common/logging.hpp"
#include <sstream>

namespace picoradar::server {

std::shared_ptr<CLIInterface> CLILogAdapter::cli_interface_;
std::function<void(const std::string&, const std::string&)> CLILogAdapter::additional_handler_;

void CLILogAdapter::initialize(std::shared_ptr<CLIInterface> cli_interface) {
    cli_interface_ = cli_interface;
}

void CLILogAdapter::shutdown() {
    cli_interface_.reset();
    additional_handler_ = nullptr;
}

void CLILogAdapter::setAdditionalHandler(
    std::function<void(const std::string&, const std::string&)> handler) {
    additional_handler_ = handler;
}

void CLILogAdapter::addLogEntry(const std::string& level, const std::string& message) {
    // 发送到CLI界面
    if (cli_interface_) {
        cli_interface_->addLogEntry(level, message);
    }
    
    // 发送到额外的处理器（例如文件日志）
    if (additional_handler_) {
        additional_handler_(level, message);
    }
    
    // 同时保持原有的日志系统
    if (level == "INFO") {
        LOG_INFO << message;
    } else if (level == "WARNING") {
        LOG_WARNING << message;
    } else if (level == "ERROR") {
        LOG_ERROR << message;
    } else if (level == "DEBUG") {
        // 只有在调试模式下才输出DEBUG日志到原系统
        LOG_INFO << "[DEBUG] " << message;
    }
}

} // namespace picoradar::server

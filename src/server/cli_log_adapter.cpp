#include "cli_log_adapter.hpp"

#include "cli_interface.hpp"
#include "common/config_manager.hpp"
#include "common/logging.hpp"

namespace picoradar::server {

bool CLILogAdapter::enabled_ = false;

void CLILogAdapter::initialize(
    const std::shared_ptr<CLIInterface>& cli_interface) {
  // 检查配置是否启用CLI输出
  auto& config_manager = ::picoradar::common::ConfigManager::getInstance();
  bool cli_enabled = config_manager.template getWithDefault<bool>(
      "logging.cli.enabled", false);

  if (cli_enabled && cli_interface) {
    // 将CLIInterface转换为CLIOutput接口
    std::shared_ptr<logger::CLIOutput> cli_output =
        std::static_pointer_cast<logger::CLIOutput>(cli_interface);

    // 启用CLI输出流
    logger::Logger::enableCLIOutput(cli_output);
    enabled_ = true;
  }
}

void CLILogAdapter::shutdown() {
  if (enabled_) {
    logger::Logger::disableCLIOutput();
    enabled_ = false;
  }
}

bool CLILogAdapter::isEnabled() { return enabled_; }

}  // namespace picoradar::server

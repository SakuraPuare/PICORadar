#pragma once

#include <cstdint>
#include <string>

namespace picoradar::config {

// 网络配置
constexpr uint16_t kDiscoveryPort = 9001;
const std::string kDiscoveryRequest = "PICO_RADAR_DISCOVERY_REQUEST";
const std::string kDiscoveryResponsePrefix = "PICO_RADAR_SERVER:";

// 其他未来可能用到的配置...

} // namespace picoradar::config 
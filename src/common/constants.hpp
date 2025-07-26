#pragma once

#include <cstdint>
#include <string>

namespace picoradar::config {

// Websocket and network settings
constexpr uint16_t kDefaultServicePort = 11451;

// UDP Discovery
constexpr uint16_t kDefaultDiscoveryPort = 11452;
const std::string kDiscoveryRequest = "PICO_RADAR_DISCOVERY_REQUEST";
const std::string kDiscoveryResponsePrefix = "PICORADAR_SERVER_AT_";

// Client-side interpolation
constexpr double kInterpolationPeriodS = 0.1;  // 100ms

// Network timeouts
constexpr int kDefaultConnectionTimeoutSeconds = 30;
constexpr int kDefaultReadTimeoutSeconds = 10;

// Performance settings
constexpr size_t kDefaultMaxMessageSize = 1024 * 1024;  // 1MB
constexpr size_t kDefaultMaxConnections = 1000;

} // namespace picoradar::config  // namespace picoradar::config
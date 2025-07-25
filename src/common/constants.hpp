#pragma once

#include <cstdint>
#include <string>

namespace picoradar::config {

// Websocket and network settings
constexpr uint16_t kDefaultServicePort = 11451;
const std::string kAuthToken =
    "pico_radar_secret_token";  // Warning: For internal/dev use ONLY.

// UDP Discovery
constexpr uint16_t kDefaultDiscoveryPort = 11452;
const std::string kDiscoveryRequest = "PICO_RADAR_DISCOVERY_REQUEST";
const std::string kDiscoveryResponsePrefix = "PICORADAR_SERVER_AT_";

// Client-side interpolation
constexpr double kInterpolationPeriodS = 0.1;  // 100ms

}  // namespace picoradar::config
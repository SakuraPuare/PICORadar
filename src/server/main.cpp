#include <csignal>
#include <iostream>
#include <atomic>

#include "common/logging.hpp"
#include "common/single_instance_guard.hpp"
#include "common/constants.hpp"
#include "server.hpp"

static std::atomic<bool> g_stop_signal(false);

void signal_handler(int signal) {
    if (signal == SIGINT) {
        LOG_INFO << "Caught SIGINT, shutting down...";
        g_stop_signal = true;
    }
}

auto main(int argc, char* argv[]) -> int {
    logger::Logger::Init(argv[0], "./logs", logger::LogLevel::INFO);

    std::unique_ptr<picoradar::common::SingleInstanceGuard> guard;
    try {
        guard = std::make_unique<picoradar::common::SingleInstanceGuard>("PicoRadar.pid");
    } catch (const std::runtime_error& e) {
        LOG_ERROR << "Failed to start: " << e.what();
        return 1;
    }

    std::signal(SIGINT, signal_handler);

    LOG_INFO << "PICO Radar Server Starting...";
    
    picoradar::server::Server server;
    uint16_t port = picoradar::config::kDefaultServicePort;
    if (argc > 1) {
        try {
            port = std::stoi(argv[1]);
        } catch (const std::exception& e) {
            LOG_ERROR << "Invalid port number provided. Using default " << port;
        }
    }
    
    server.start(port, 4);

    LOG_INFO << "Server started successfully. Press Ctrl+C to exit.";

    while (!g_stop_signal) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    LOG_INFO << "Shutdown complete.";
    return 0;
}
#include "common/single_instance_guard.hpp"
#include <glog/logging.h>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

using picoradar::common::SingleInstanceGuard;

const char* LOCK_FILE_NAME = "pico_radar_test.pid";

// 模式1: 尝试获取锁并永久持有
void run_lock_mode() {
    try {
        SingleInstanceGuard guard(LOCK_FILE_NAME);
        LOG(INFO) << "LOCK_ACQUIRED";
        // 模拟一个正在运行的服务器
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } catch (const std::runtime_error& e) {
        LOG(ERROR) << "LOCK_FAILED: " << e.what();
        exit(1);
    }
}

// 模式2: 检查是否能获取锁（预期失败）
void run_check_mode() {
    try {
        SingleInstanceGuard guard(LOCK_FILE_NAME);
        // 如果我们能获取锁，说明测试失败了
        LOG(ERROR) << "CHECK_FAILED: Successfully acquired lock when it should have failed.";
        exit(1);
    } catch (const std::runtime_error& e) {
        // 成功捕获到异常，说明测试通过
        LOG(INFO) << "CHECK_SUCCESS: Correctly failed to acquire lock.";
        exit(0);
    }
}


int main(int argc, char** argv) {
    google::InitGoogleLogging(argv[0]);

    if (argc != 2) {
        LOG(ERROR) << "Invalid usage. Expected one argument: --lock or --check";
        return 1;
    }

    std::string mode = argv[1];
    if (mode == "--lock") {
        run_lock_mode();
    } else if (mode == "--check") {
        run_check_mode();
    } else {
        LOG(ERROR) << "Unknown mode: " << mode;
        return 1;
    }

    return 0;
}

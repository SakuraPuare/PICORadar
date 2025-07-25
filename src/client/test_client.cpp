#include "client/client.hpp"
#include "common/logging.hpp"
#include <glog/logging.h>
#include <thread>
#include <chrono>

// =========================
// 详细中文注释已添加到 main 函数和关键流程。
// =========================
int main(int argc, char* argv[]) {
    // 初始化日志系统
    picoradar::common::setup_logging(argv[0]);
    
    LOG(INFO) << "Starting PICO Radar test client";
    
    // 创建客户端实例
    picoradar::client::Client client;
    
    // 设置认证信息
    client.set_auth_token("test_token");
    client.set_player_id("test_player");
    
    // 尝试发现服务器
    std::string server_address = client.discover_server();
    
    if (!server_address.empty()) {
        LOG(INFO) << "发现服务器: " << server_address;
        
        // 分离主机和端口
        size_t pos = server_address.find(':');
        if (pos != std::string::npos) {
            std::string host = server_address.substr(0, pos);
            std::string port = server_address.substr(pos + 1);
            
            // 尝试连接
            client.connect(host, port);
            LOG(INFO) << "连接请求已发送，等待2秒...";
            std::this_thread::sleep_for(std::chrono::seconds(2));

            if (client.is_connected()) {
                LOG(INFO) << "成功连接到服务器";
                
                // 等待一段时间以接收数据
                LOG(INFO) << "保持连接2秒...";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                
                // 断开连接
                client.disconnect();
                LOG(INFO) << "已断开连接";
            } else {
                LOG(ERROR) << "连接失败";
            }
        }
    } else {
        LOG(WARNING) << "未发现服务器";
    }
    
    LOG(INFO) << "Test client finished";
    google::ShutdownGoogleLogging();
    return 0;
}
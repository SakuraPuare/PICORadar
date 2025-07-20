#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <cstdlib>
#include <glog/logging.h>
#include <iostream>
#include <string>
#include "player_data.pb.h"

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

// 报告错误并退出
void fail(beast::error_code ec, char const* what) {
    LOG(ERROR) << what << ": " << ec.message();
}

// 一个简单的同步WebSocket客户端
class SyncClient {
public:
    SyncClient(std::string host, std::string port)
        : host_(std::move(host)), port_(std::move(port)), resolver_(ioc_), ws_(ioc_) {}

    // 返回0表示成功，非0表示失败
    int run(const std::string& token, const std::string& mode) {
        try {
            auto const results = resolver_.resolve(host_, port_);
            net::connect(ws_.next_layer(), results.begin(), results.end());
            
            websocket::stream_base::timeout opt{
                std::chrono::seconds(5), 
                std::chrono::seconds(5),
                true
            };
            ws_.set_option(opt);

            ws_.handshake(host_ + ":" + port_, "/");
            LOG(INFO) << "Successfully connected to " << host_ << ":" << port_;

            send_auth(token);
            return listen(mode);

        } catch (std::exception const& e) {
            LOG(ERROR) << "Exception: " << e.what();
            // 在测试失败模式下，连接被服务器关闭是预期行为
            if (mode == "--test-auth-fail" && std::string(e.what()).find("closed") != std::string::npos) {
                LOG(INFO) << "Connection correctly closed by server. Test PASSED.";
                return 0;
            }
            return 1;
        }
    }

private:
    void send_auth(const std::string& token) {
        picoradar::ClientToServer message;
        message.mutable_auth_request()->set_token(token);
        
        std::string serialized_message;
        message.SerializeToString(&serialized_message);

        LOG(INFO) << "Sending auth request with token: " << token.substr(0, 8) << "...";
        ws_.binary(true);
        ws_.write(net::buffer(serialized_message));
    }

    int listen(const std::string& mode) {
        beast::flat_buffer buffer;
        ws_.read(buffer);

        picoradar::ServerToClient response;
        if (!response.ParseFromArray(buffer.data().data(), buffer.size())) {
            LOG(ERROR) << "Failed to parse server response.";
            return 1;
        }

        if (!response.has_auth_response()) {
            LOG(ERROR) << "Received unexpected message type from server.";
            return 1;
        }

        const auto& auth_response = response.auth_response();
        LOG(INFO) << "Received AuthResponse: success=" << auth_response.success() 
                  << ", message='" << auth_response.message() << "'";

        if (mode == "--test-auth-success") {
            return auth_response.success() ? 0 : 1;
        }
        if (mode == "--test-auth-fail") {
            // 对于失败测试，我们期望 success() 是 false
            // 但由于服务器会直接关闭连接，我们可能根本走不到这里
            // 主要的成功判定在 run() 的 catch 块中
            return !auth_response.success() ? 0 : 1;
        }
        
        // 交互模式
        if (mode == "--interactive") {
            if (!auth_response.success()) return 1;
            for (;;) {
                buffer.clear();
                ws_.read(buffer);
                LOG(INFO) << "Received message: " << beast::make_printable(buffer.data());
            }
        }
        return 1; // 未知模式
    }

    net::io_context ioc_;
    std::string host_;
    std::string port_;
    tcp::resolver resolver_;
    websocket::stream<tcp::socket> ws_;
};


int main(int argc, char** argv) {
    google::InitGoogleLogging(argv[0]);
    FLAGS_logtostderr = true;

    if (argc < 4) {
        LOG(ERROR) << "Usage: mock_client <host> <port> <token> [--test-auth-success | --test-auth-fail | --interactive]";
        return 1;
    }

    const std::string host = argv[1];
    const std::string port = argv[2];
    const std::string token = argv[3];
    const std::string mode = (argc > 4) ? argv[4] : "--interactive";

    SyncClient client(host, port);
    return client.run(token, mode);
}

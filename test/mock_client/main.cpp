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
    int run(const std::string& token, const std::string& mode, const std::string& player_id) {
        try {
            auto const results = resolver_.resolve(host_, port_);
            net::connect(ws_.next_layer(), results.begin(), results.end());
            
            websocket::stream_base::timeout opt{
                std::chrono::seconds(10), 
                std::chrono::seconds(10),
                true
            };
            ws_.set_option(opt);

            ws_.handshake(host_ + ":" + port_, "/");
            LOG(INFO) << "Successfully connected to " << host_ << ":" << port_;

            if (!authenticate(token, player_id)) {
                LOG(ERROR) << "Authentication failed.";
                return (mode == "--test-auth-fail") ? 0 : 1;
            }

            if (mode == "--test-auth-success") {
                return 0; // 认证成功即测试通过
            }
            if (mode == "--seed-data") {
                return seed_data();
            }
            if (mode == "--test-broadcast") {
                return test_broadcast();
            }
            if (mode == "--interactive") {
                return interactive_listen();
            }

            LOG(ERROR) << "Unknown client mode: " << mode;
            return 1;

        } catch (std::exception const& e) {
            LOG(ERROR) << "Exception: " << e.what();
            if (mode == "--test-auth-fail" && std::string(e.what()).find("closed") != std::string::npos) {
                LOG(INFO) << "Connection correctly closed by server. Test PASSED.";
                return 0;
            }
            return 1;
        }
    }

private:
    bool authenticate(const std::string& token, const std::string& player_id) {
        picoradar::ClientToServer auth_message;
        auto* auth_request = auth_message.mutable_auth_request();
        auth_request->set_token(token);
        auth_request->set_player_id(player_id);

        std::string serialized_auth;
        auth_message.SerializeToString(&serialized_auth);
        LOG(INFO) << "Sending auth request for player " << player_id << "...";
        ws_.binary(true);
        ws_.write(net::buffer(serialized_auth));

        beast::flat_buffer buffer;
        ws_.read(buffer);
        picoradar::ServerToClient response;
        if (!response.ParseFromArray(buffer.data().data(), buffer.size()) || !response.has_auth_response()) {
            LOG(ERROR) << "Failed to parse auth response.";
            return false;
        }
        LOG(INFO) << "Received AuthResponse: success=" << response.auth_response().success();
        return response.auth_response().success();
    }

    int seed_data() {
        LOG(INFO) << "Seeding data...";
        picoradar::ClientToServer seed_message;
        auto* player_data = seed_message.mutable_player_data();
        player_data->set_player_id("seeder_client"); // This ID is what gets stored
        player_data->mutable_position()->set_x(1.23f);

        std::string serialized_seed;
        seed_message.SerializeToString(&serialized_seed);
        ws_.write(net::buffer(serialized_seed));
        LOG(INFO) << "Seed data sent.";
        
        beast::error_code ec;
        ws_.next_layer().shutdown(tcp::socket::shutdown_both, ec);
        ws_.next_layer().close(ec);

        return 0;
    }

    int test_broadcast() {
        LOG(INFO) << "Waiting for broadcast...";
        beast::flat_buffer buffer;
        ws_.read(buffer);

        picoradar::ServerToClient response;
        if (!response.ParseFromArray(buffer.data().data(), buffer.size()) || !response.has_player_list()) {
            LOG(ERROR) << "Did not receive a valid player list broadcast.";
            return 1;
        }

        LOG(INFO) << "Received player list with " << response.player_list().players_size() << " players.";
        if (response.player_list().players_size() > 0) {
            LOG(INFO) << "Broadcast test PASSED.";
            return 0;
        } else {
            LOG(ERROR) << "Broadcast test FAILED: player list was empty.";
            return 1;
        }
    }

    int interactive_listen() {
        LOG(INFO) << "Entering interactive listen mode...";
        for (;;) {
            beast::flat_buffer buffer;
            ws_.read(buffer);
            picoradar::ServerToClient response;
            if (response.ParseFromArray(buffer.data().data(), buffer.size())) {
                if (response.has_player_list()) {
                    LOG(INFO) << "Received player list with " << response.player_list().players_size() << " players.";
                } else {
                    LOG(INFO) << "Received an unknown protobuf message type.";
                }
            } else {
                 LOG(INFO) << "Received raw message: " << beast::make_printable(buffer.data());
            }
        }
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

    if (argc < 5) {
        LOG(ERROR) << "Usage: mock_client <host> <port> <token> <player_id> [--test-auth-success | ...]";
        return 1;
    }

    const std::string host = argv[1];
    const std::string port = argv[2];
    const std::string token = argv[3];
    const std::string player_id = argv[4];
    const std::string mode = (argc > 5) ? argv[5] : "--interactive";

    SyncClient client(host, port);
    return client.run(token, mode, player_id);
}

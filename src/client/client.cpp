#include "client.hpp"
// #include "common/string_utils.hpp" // No longer needed
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ip/udp.hpp>
#include <glog/logging.h>
#include <iostream>
#include <thread>
#include "common/constants.hpp"

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;
using udp = net::ip::udp;

namespace picoradar::client {

Client::Client()
    : ws_(ioc_),
      is_connected_(false),
      is_authenticated_(false),
      work_(new net::executor_work_guard<net::io_context::executor_type>(ioc_.get_executor()))
{}

Client::~Client() {
    disconnect();
}

void Client::connect(const std::string& host, const std::string& port) {
    try {
        tcp::resolver resolver(ioc_);
        auto const results = resolver.resolve(host, port);
        net::connect(ws_.next_layer(), results.begin(), results.end());

        websocket::stream_base::timeout opt{
            std::chrono::seconds(10),
            std::chrono::seconds(10),
            true
        };
        ws_.set_option(opt);

        ws_.handshake(host + ":" + port, "/");

        is_connected_ = true;
        LOG(INFO) << "Successfully connected to server at " << host << ":" << port;

        send_authentication_request();
        start_read();
        
        io_thread_ = std::thread([this]() { ioc_.run(); });
    } catch (const std::exception& e) {
        LOG(ERROR) << "Connection failed: " << e.what();
        is_connected_ = false;
    }
}

std::string Client::discover_server(uint16_t discovery_port) {
    try {
        LOG(INFO) << "Attempting to discover server via UDP broadcast...";
        
        // 创建UDP socket
        udp::socket socket(ioc_);
        socket.open(udp::v4());
        socket.set_option(net::socket_base::broadcast(true));

        // 发送发现请求
        udp::endpoint broadcast_endpoint(net::ip::address_v4::broadcast(), discovery_port);
        socket.send_to(net::buffer(config::kDiscoveryRequest), broadcast_endpoint);

        // 接收服务器响应
        udp::endpoint server_endpoint;
        std::array<char, 128> recv_buf;
        size_t len = socket.receive_from(net::buffer(recv_buf), server_endpoint);

        // 解析响应
        std::string response(recv_buf.data(), len);
        if (response.rfind(config::kDiscoveryResponsePrefix, 0) != 0) {
            LOG(ERROR) << "Received invalid discovery response: " << response;
            return "";
        }

        std::string server_address = response.substr(config::kDiscoveryResponsePrefix.length());
        LOG(INFO) << "Server discovered at " << server_address;
        return server_address;
    } catch (const std::exception& e) {
        LOG(ERROR) << "Discovery failed: " << e.what();
        return "";
    }
}

void Client::disconnect() {
    if (is_connected_.exchange(false)) {
        net::post(ioc_, [this]() {
            beast::error_code ec;
            ws_.close(websocket::close_code::normal, ec);
            if (ec) {
                LOG(WARNING) << "Error closing websocket: " << ec.message();
            }
        });
    }

    work_.reset(); // Allow io_context to exit
    if (io_thread_.joinable()) {
        io_thread_.join();
    }
}

bool Client::send_player_data(const picoradar::PlayerData& player_data) {
    if (!is_connected_) {
        LOG(ERROR) << "Not connected to server";
        return false;
    }

    try {
        // 创建消息
        picoradar::ClientToServer message;
        auto* data = message.mutable_player_data();
        data->CopyFrom(player_data);

        // 序列化消息
        std::string serialized_message;
        message.SerializeToString(&serialized_message);

        // LOG(INFO) << "Client sending hex: " << common::to_hex(serialized_message); // Removed for cleanup

        // 发送消息
        ws_.binary(true);
        ws_.write(net::buffer(serialized_message));
        
        return true;
    } catch (const std::exception& e) {
        LOG(ERROR) << "Failed to send player data: " << e.what();
        return false;
    }
}

void Client::set_player_id(const std::string& player_id) {
    player_id_ = player_id;
}

const std::string& Client::get_player_id() const {
    return player_id_;
}

void Client::set_auth_token(const std::string& token) {
    auth_token_ = token;
}

const std::string& Client::get_auth_token() const {
    return auth_token_;
}

const picoradar::PlayerList& Client::get_player_list() const {
    std::lock_guard<std::mutex> lock(player_list_mutex_);
    return player_list_;
}

bool Client::is_connected() const {
    return is_connected_;
}

void Client::send_authentication_request() {
    if (auth_token_.empty() || player_id_.empty()) {
        LOG(ERROR) << "Authentication token or Player ID is empty";
        return;
    }

    try {
        picoradar::ClientToServer auth_message;
        auto* auth_request = auth_message.mutable_auth_request();
        auth_request->set_token(auth_token_);
        auth_request->set_player_id(player_id_);

        std::string serialized_auth;
        auth_message.SerializeToString(&serialized_auth);
        
        ws_.binary(true);
        ws_.write(net::buffer(serialized_auth));
    } catch (const std::exception& e) {
        LOG(ERROR) << "Failed to send authentication request: " << e.what();
        disconnect();
    }
}

void Client::start_read() {
    ws_.async_read(
        read_buffer_,
        [this](beast::error_code ec, std::size_t bytes_transferred) {
            on_read(ec, bytes_transferred);
        });
}

void Client::on_read(beast::error_code ec, std::size_t bytes_transferred) {
    if (ec == websocket::error::closed) {
        LOG(INFO) << "Server closed the connection";
        is_connected_ = false;
        return;
    }

    if (ec) {
        LOG(ERROR) << "Read error: " << ec.message();
        is_connected_ = false;
        return;
    }

    // 处理收到的消息
    picoradar::ServerToClient response;
    if (response.ParseFromArray(read_buffer_.data().data(), bytes_transferred)) {
        if (response.has_player_list()) {
            std::lock_guard<std::mutex> lock(player_list_mutex_);
            player_list_ = response.player_list();
            LOG(INFO) << "Received player list with " << player_list_.players_size() << " players";
        } else if (response.has_auth_response()) {
            const auto& auth_response = response.auth_response();
            if (auth_response.success()) {
                is_authenticated_ = true;
                LOG(INFO) << "Authentication successful";
            } else {
                LOG(ERROR) << "Authentication failed: " << auth_response.message();
                is_authenticated_ = false;
                disconnect();
            }
        }
    } else {
        LOG(WARNING) << "Failed to parse server message";
    }

    // 清空缓冲区
    read_buffer_.consume(read_buffer_.size());

    // 如果已连接，继续读取下一个消息
    if (is_connected_) {
        start_read();
    }
}

} // namespace picoradar::client
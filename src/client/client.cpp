#include "client.hpp"

#include "common/constants.hpp"
// #include "common/string_utils.hpp" // No longer needed
#include <glog/logging.h>

#include <algorithm>
#include <boost/asio.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <chrono>
#include <iostream>
#include <thread>

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
      work_(new net::executor_work_guard<net::io_context::executor_type>(
          ioc_.get_executor())) {}

Client::~Client() { disconnect(); }

void Client::connect(const std::string& host, const std::string& port) {
  try {
    tcp::resolver resolver(ioc_);
    auto const results = resolver.resolve(host, port);
    net::connect(ws_.next_layer(), results.begin(), results.end());

    websocket::stream_base::timeout opt{std::chrono::seconds(10),
                                        std::chrono::seconds(10), true};
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

auto Client::discover_server(uint16_t discovery_port) -> std::string {
  try {
    LOG(INFO) << "Attempting to discover server via UDP broadcast...";

    // 创建临时 io_context
    boost::asio::io_context temp_ioc;
    udp::socket socket(temp_ioc);
    socket.open(udp::v4());
    socket.set_option(net::socket_base::broadcast(true));

    udp::endpoint broadcast_endpoint(net::ip::address_v4::broadcast(),
                                     discovery_port);
    socket.send_to(net::buffer(config::kDiscoveryRequest), broadcast_endpoint);

    udp::endpoint server_endpoint;
    std::array<char, 128> recv_buf;
    size_t len = socket.receive_from(net::buffer(recv_buf), server_endpoint);

    std::string response(recv_buf.data(), len);
    if (response.rfind(config::kDiscoveryResponsePrefix, 0) != 0) {
      LOG(ERROR) << "Received invalid discovery response: " << response;
      return "";
    }

    std::string server_address =
        response.substr(config::kDiscoveryResponsePrefix.length());
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

  work_.reset();  // Allow io_context to exit
  if (io_thread_.joinable()) {
    io_thread_.join();
  }
}

auto Client::send_player_data(const picoradar::PlayerData& player_data)
    -> bool {
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

    // LOG(INFO) << "Client sending hex: " <<
    // common::to_hex(serialized_message); // Removed for cleanup

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

auto Client::get_player_id() const -> const std::string& { return player_id_; }

void Client::set_auth_token(const std::string& token) { auth_token_ = token; }

auto Client::get_auth_token() const -> const std::string& {
  return auth_token_;
}

auto Client::get_player_list() const -> const picoradar::PlayerList& {
  std::lock_guard<std::mutex> lock(player_list_mutex_);
  return player_list_;
}

void Client::update_visual_state(float delta_time_s) {
  std::lock_guard<std::mutex> lock(visual_players_mutex_);
  if (visual_players_.empty()) {
    return;
  }

  const double now = std::chrono::duration_cast<std::chrono::duration<double>>(
                         std::chrono::steady_clock::now().time_since_epoch())
                         .count();

  for (auto& pair : visual_players_) {
    VisualPlayerState& state = pair.second;

    // Update interpolation alpha
    if (state.interpolation_alpha < 1.0F) {
      double time_since_update = now - state.last_update_time_s;
      state.interpolation_alpha =
          static_cast<float>(time_since_update / config::kInterpolationPeriodS);
      state.interpolation_alpha = std::min(state.interpolation_alpha, 1.0F);
    }
  }
}

auto Client::get_visual_players() const
    -> const std::map<std::string, VisualPlayerState>& {
  std::lock_guard<std::mutex> lock(visual_players_mutex_);
  return visual_players_;
}

auto Client::is_connected() const -> bool { return is_connected_; }

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
  ws_.async_read(read_buffer_,
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
      const auto& new_player_list = response.player_list();

      // --- Begin interpolation logic ---
      const double now =
          std::chrono::duration_cast<std::chrono::duration<double>>(
              std::chrono::steady_clock::now().time_since_epoch())
              .count();

      std::lock_guard<std::mutex> lock(visual_players_mutex_);

      // Mark all current players as potentially disconnected
      std::map<std::string, bool> players_in_new_list;

      for (const auto& new_player_data : new_player_list.players()) {
        if (new_player_data.player_id() == player_id_) {
          continue;  // Skip self
        }

        players_in_new_list[new_player_data.player_id()] = true;
        auto it = visual_players_.find(new_player_data.player_id());

        if (it != visual_players_.end()) {
          // Existing player: update state
          VisualPlayerState& state = it->second;
          state.from_pos = state.to_pos;
          state.from_rot = state.to_rot;
          state.to_pos = new_player_data.position();
          state.to_rot = new_player_data.rotation();
          state.last_update_time_s = now;
          state.interpolation_alpha = 0.0F;
        } else {
          // New player: add to visual list
          VisualPlayerState new_state;
          new_state.player_id = new_player_data.player_id();
          new_state.from_pos = new_player_data.position();
          new_state.to_pos = new_player_data.position();
          new_state.from_rot = new_player_data.rotation();
          new_state.to_rot = new_player_data.rotation();
          new_state.last_update_time_s = now;
          new_state.interpolation_alpha = 1.0F;  // Start fully interpolated
          visual_players_[new_player_data.player_id()] = new_state;
        }
      }

      // Remove players that are no longer in the list
      for (auto it = visual_players_.begin(); it != visual_players_.end();) {
        if (players_in_new_list.find(it->first) == players_in_new_list.end()) {
          it = visual_players_.erase(it);
        } else {
          ++it;
        }
      }
      // --- End interpolation logic ---

      {  // Original logic for raw player list
        std::lock_guard<std::mutex> list_lock(player_list_mutex_);
        player_list_ = new_player_list;
        LOG(INFO) << "Received and processed player list with "
                  << player_list_.players_size() << " players";
      }

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

}  // namespace picoradar::client
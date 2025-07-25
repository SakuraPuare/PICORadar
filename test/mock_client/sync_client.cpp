#include "sync_client.hpp"

#include <glog/logging.h>

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "common/constants.hpp"
#include "common/logging.hpp"
#include "player_data.pb.h"

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;
using udp = net::ip::udp;
using namespace std::chrono_literals;

namespace picoradar::mock_client {

SyncClient::SyncClient() : resolver_(ioc_) {
  ws_ = std::make_shared<websocket::stream<tcp::socket>>(ioc_);
}

void SyncClient::run(const std::string& host, const std::string& port,
                     const std::string& mode, const std::string& player_id) {
  connect_and_run(host, port, mode, player_id);
}

auto SyncClient::discover_and_run(const std::string& player_id,
                                  uint16_t discovery_port) -> int {
  try {
    std::string server = discover_server(discovery_port);
    if (server.empty()) {
      LOG(ERROR) << "Failed to discover server.";
      return 1;
    }
    // 解析 host:port
    size_t pos = server.find(':');
    if (pos == std::string::npos) {
      LOG(ERROR) << "Discovered server address format invalid: " << server;
      return 1;
    }
    std::string host = server.substr(0, pos);
    std::string port = server.substr(pos + 1);

    auto const results = resolver_.resolve(host, port);
    net::connect(ws_->next_layer(), results.begin(), results.end());
    ws_->handshake(host, "/");

    LOG(INFO) << "WebSocket connected to " << host << ":" << port;

    picoradar::PlayerData auth_msg;
    auth_msg.set_player_id(player_id);
    std::string serialized_auth;
    auth_msg.SerializeToString(&serialized_auth);
    ws_->write(net::buffer(serialized_auth));
    LOG(INFO) << "Auth message sent.";

    ws_->close(websocket::close_code::normal);

  } catch (const std::exception& e) {
    LOG(ERROR) << "Exception in discover_and_run: " << e.what();
    return 1;
  }
  return 0;
}

void SyncClient::connect_and_run(const std::string& host,
                                 const std::string& port,
                                 const std::string& mode,
                                 const std::string& player_id) {
  try {
    host_ = host;
    port_ = port;

    auto const results = resolver_.resolve(host, port);
    net::connect(ws_->next_layer(), results.begin(), results.end());
    ws_->handshake(host, "/");

    LOG(INFO) << "Client connected to " << host << ":" << port;

    picoradar::PlayerData auth_msg;
    auth_msg.set_player_id(player_id);
    std::string serialized_auth;
    auth_msg.SerializeToString(&serialized_auth);
    ws_->write(net::buffer(serialized_auth));
    LOG(INFO) << "Auth message sent for player " << player_id;

    if (mode == "test") {
      send_test_data(player_id);
    } else if (mode == "seed") {
      seed_data_and_exit();
    } else if (mode == "broadcast") {
      test_broadcast(player_id);
    } else if (mode == "stress") {
      stress_test(player_id);
    }

    if (ws_ && ws_->is_open()) {
      ws_->close(websocket::close_code::normal);
    }
  } catch (std::exception const& e) {
    LOG(ERROR) << "Error in connect_and_run: " << e.what();
  }
}

void SyncClient::send_test_data(const std::string& player_id) {
  picoradar::PlayerData player_data;
  player_data.set_player_id(player_id);

  auto* position = player_data.mutable_position();
  position->set_x(static_cast<float>((rand() % 200) - 100));
  position->set_y(static_cast<float>((rand() % 200) - 100));
  position->set_z(0.0F);

  auto* rotation = player_data.mutable_rotation();
  rotation->set_x(0.0F);
  rotation->set_y(0.0F);
  rotation->set_z(0.0F);
  rotation->set_w(1.0F);

  player_data.set_timestamp(
      std::chrono::system_clock::now().time_since_epoch().count());

  std::string output;
  if (player_data.SerializeToString(&output)) {
    try {
      if (ws_ && ws_->is_open()) {
        ws_->write(net::buffer(output));
      }
    } catch (const std::exception& e) {
      LOG(ERROR) << "Failed to send data for " << player_id << ": " << e.what();
    }
  }
}

void SyncClient::seed_data_and_exit() {
  LOG(INFO) << "Seeder client sending data and exiting...";
  send_test_data("seeder_for_broadcast");
  std::this_thread::sleep_for(100ms);  // 确保数据有时间发送
  LOG(INFO) << "Seeder client finished.";
}

void SyncClient::test_broadcast(const std::string& player_id) {
  LOG(INFO) << "Testing broadcast for player " << player_id;
  beast::flat_buffer buffer;
  try {
    if (ws_ && ws_->is_open()) {
      ws_->read(buffer);
      LOG(INFO) << "Broadcast message received: "
                << beast::make_printable(buffer.data());
      buffer.consume(buffer.size());
    }
  } catch (std::exception const& e) {
    LOG(ERROR) << "Error in test_broadcast: " << e.what();
  }
}

auto SyncClient::discover_server(uint16_t discovery_port) -> std::string {
  net::io_context discovery_ioc;
  udp::socket socket(discovery_ioc);

  try {
    socket.open(udp::v4());
    socket.set_option(udp::socket::reuse_address(true));
    // 修正：客户端绑定 0 端口，让系统自动分配
    socket.bind(udp::endpoint(udp::v4(), 0));

    LOG(INFO) << "Discovery client listening on port " << discovery_port;

    // 主动发送发现请求到 255.255.255.255 广播地址
    udp::endpoint broadcast_endpoint(boost::asio::ip::address_v4::broadcast(),
                                     discovery_port);
    socket.set_option(boost::asio::socket_base::broadcast(true));
    socket.send_to(net::buffer(picoradar::config::kDiscoveryRequest),
                   broadcast_endpoint);

    char data[1024];
    udp::endpoint sender_endpoint;

    boost::system::error_code ec;
    std::string message;
    bool received = false;

    net::steady_timer timer(discovery_ioc, std::chrono::seconds(2));
    timer.async_wait([&](const boost::system::error_code& error) {
      if (!error) {
        socket.cancel();
      }
    });

    socket.async_receive_from(
        net::buffer(data), sender_endpoint,
        [&](const boost::system::error_code& error, size_t length) {
          if (!error) {
            message.assign(data, length);
            received = true;
          } else if (error != net::error::operation_aborted) {
            LOG(ERROR) << "async_receive_from failed: " << error.message();
          }
        });

    discovery_ioc.run();

    if (received) {
      LOG(INFO) << "Discovered server at " << message << " from "
                << sender_endpoint;
      // 去除前缀
      const std::string prefix = picoradar::config::kDiscoveryResponsePrefix;
      if (message.rfind(prefix, 0) == 0) {
        return message.substr(prefix.size());
      }
      return message;
    }

  } catch (const std::exception& e) {
    LOG(ERROR) << "Discovery failed: " << e.what();
  }

  return "";
}

void SyncClient::stress_test(const std::string& player_id) {
  LOG(INFO) << "Starting stress test for player " << player_id;
  beast::flat_buffer buffer;
  beast::error_code ec;

  std::thread read_thread([this, &buffer, &ec]() {
    while (ws_ && ws_->is_open()) {
      ws_->next_layer().non_blocking(true);
      size_t bytes_read = ws_->read_some(buffer.prepare(1024), ec);
      if (bytes_read > 0) {
        buffer.commit(bytes_read);
        // Process received data if necessary
        LOG(INFO) << "Stress test received data: "
                  << beast::make_printable(buffer.data());
        buffer.consume(buffer.size());
      }
      ws_->next_layer().non_blocking(false);

      if (ec == websocket::error::closed) {
        LOG(INFO) << "WebSocket connection closed by peer.";
        break;
      }
      if (ec && ec != net::error::would_block) {
        LOG(ERROR) << "Stress test read error: " << ec.message();
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  });

  std::thread write_thread([this, &player_id, &ec]() {
    while (ws_ && ws_->is_open()) {
      try {
        send_test_data(player_id);
        std::this_thread::sleep_for(100ms);
      } catch (const std::exception& e) {
        LOG(ERROR) << "Exception in write_thread for " << player_id << ": "
                   << e.what();
        break;
      }
    }
  });

  read_thread.join();
  write_thread.join();
  LOG(INFO) << "Stress test client " << player_id << " finished.";
}

}  // namespace picoradar::mock_client

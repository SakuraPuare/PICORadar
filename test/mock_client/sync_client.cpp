#include "mock_client/sync_client.hpp"
#include <glog/logging.h>
#include <boost/asio/ip/udp.hpp>
#include <boost/beast/core.hpp>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include "player_data.pb.h"
#include "common/constants.hpp"

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;
using udp = net::ip::udp;

namespace picoradar::mock_client {

void fail(beast::error_code ec, char const* what) {
  LOG(ERROR) << what << ": " << ec.message();
}

auto split_address(const std::string& s)
    -> std::pair<std::string, std::string> {
  size_t pos = s.find(':');
  if (pos == std::string::npos) {
    return {"", ""};
  }
  return {s.substr(0, pos), s.substr(pos + 1)};
}

SyncClient::SyncClient() : resolver_(ioc_), ws_(ioc_) {}

auto SyncClient::discover_and_run(const std::string& player_id) -> int {
  try {
    LOG(INFO) << "Attempting to discover server via UDP broadcast...";
    udp::socket socket(ioc_);
    socket.open(udp::v4());
    socket.set_option(net::socket_base::broadcast(true));

    udp::endpoint broadcast_endpoint(net::ip::address_v4::broadcast(),
                                     config::kDiscoveryPort);
    socket.send_to(net::buffer(config::kDiscoveryRequest), broadcast_endpoint);

    udp::endpoint server_endpoint;
    std::array<char, 128> recv_buf;
    size_t len = socket.receive_from(net::buffer(recv_buf), server_endpoint);

    std::string response(recv_buf.data(), len);
    if (response.rfind(config::kDiscoveryResponsePrefix, 0) != 0) {
      LOG(ERROR) << "Received invalid discovery response: " << response;
      return 1;
    }

    std::string server_address_str =
        response.substr(config::kDiscoveryResponsePrefix.length());
    auto [host, port] = split_address(server_address_str);

    if (host.empty() || port.empty()) {
      LOG(ERROR) << "Failed to parse host and port from discovery response: "
                 << server_address_str;
      return 1;
    }

    host_ = host;
    port_ = port;

    LOG(INFO) << "Server discovered at " << host_ << ":" << port_;

    // Connect, register, and disconnect. Do not enter interactive mode.
    auto const results = resolver_.resolve(host_, port_);
    net::connect(ws_.next_layer(), results.begin(), results.end());

    websocket::stream_base::timeout opt{std::chrono::seconds(10),
                                        std::chrono::seconds(10), true};
    ws_.set_option(opt);

    ws_.handshake(host_ + ":" + port_, "/");
    LOG(INFO) << "Successfully connected to " << host_ << ":" << port_;

    LOG(INFO) << "Client [" << player_id << "] sending initial player ID.";
    picoradar::ClientToServer initial_message;
    initial_message.mutable_player_data()->set_player_id(player_id);
    std::string serialized_initial_message;
    initial_message.SerializeToString(&serialized_initial_message);
    ws_.binary(true);
    ws_.write(net::buffer(serialized_initial_message));
    LOG(INFO) << "Player ID " << player_id << " sent to server.";

    beast::error_code ec;
    ws_.close(websocket::close_code::normal, ec);
    if(ec) {
        fail(ec, "close");
        // Don't return 1 here, as the main goal was connection.
    }
    LOG(INFO) << "Client disconnected after discovery test.";
    return 0;

  } catch (std::exception const& e) {
    LOG(ERROR) << "Discovery failed: " << e.what();
    return 1;
  }
}

auto SyncClient::run(const std::string& host, const std::string& port,
         const std::string& mode, const std::string& player_id) -> int {
  host_ = host;
  port_ = port;
  return run_internal(mode, player_id);
}

auto SyncClient::run_internal(const std::string& mode,
                  const std::string& player_id) -> int {
  try {
    auto const results = resolver_.resolve(host_, port_);
    net::connect(ws_.next_layer(), results.begin(), results.end());

    websocket::stream_base::timeout opt{std::chrono::seconds(10),
                                        std::chrono::seconds(10), true};
    ws_.set_option(opt);

    ws_.handshake(host_ + ":" + port_, "/");
    LOG(INFO) << "Successfully connected to " << host_ << ":" << port_;

    // Since auth is removed, we send player_id directly upon connection.
    LOG(INFO) << "Client [" << player_id << "] sending initial player ID.";
    picoradar::ClientToServer initial_message;
    initial_message.mutable_player_data()->set_player_id(player_id);
    std::string serialized_initial_message;
    initial_message.SerializeToString(&serialized_initial_message);
    ws_.binary(true); // Tell the stream to send binary data
    ws_.write(net::buffer(serialized_initial_message));
    LOG(INFO) << "Player ID " << player_id << " sent to server.";


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
    return 1;
  }
}

auto SyncClient::seed_data() -> int {
  LOG(INFO) << "Seeding data...";
  picoradar::ClientToServer seed_message;
  auto* player_data = seed_message.mutable_player_data();
  // Note: The player_id here is different from the one in the test assertion.
  // This is okay, the test is checking for ANY other player.
  player_data->set_player_id("seeder");
  player_data->mutable_position()->set_x(1.23F);

  std::string serialized_seed;
  seed_message.SerializeToString(&serialized_seed);
  LOG(INFO) << "[Seeder] Writing seed data to websocket.";
  ws_.binary(true); // Tell the stream to send binary data
  ws_.write(net::buffer(serialized_seed));
  LOG(INFO) << "Seed data sent. Waiting a moment before disconnecting...";

  // Give the listener a chance to connect and receive a broadcast.
  std::this_thread::sleep_for(std::chrono::seconds(1));

  beast::error_code ec;
  ws_.close(websocket::close_code::normal, ec);
  if (ec) {
    fail(ec, "close");
  }

  return 0;
}

auto SyncClient::test_broadcast() -> int {
  LOG(INFO) << "Waiting for broadcast containing the seeder...";

  // Set a timeout for the read operation to avoid blocking forever.
  websocket::stream_base::timeout opt{
      std::chrono::seconds(10), // handshake timeout
      std::chrono::seconds(5), // stream timeout
      true // ping/pong messages to keep alive
  };
  ws_.set_option(opt);

  // Try to read a few times.
  for (int i = 0; i < 10; ++i) {
    beast::flat_buffer buffer;
    beast::error_code ec;
    LOG(INFO) << "[Listener] Attempting to read from websocket (attempt " << i + 1 << "/10)...";
    ws_.read(buffer, ec);

    if (ec == websocket::error::closed || ec == beast::error::timeout) {
      LOG(ERROR) << "Connection closed or timed out while waiting for broadcast. " << ec.message();
      return 1;
    }
    if (ec) {
      fail(ec, "read");
      return 1;
    }

    LOG(INFO) << "[Listener] Successfully read " << buffer.size() << " bytes from websocket.";
    picoradar::ServerToClient response;
    if (response.ParseFromArray(buffer.data().data(), buffer.size()) &&
        response.has_player_list()) {
      LOG(INFO) << "[Listener] Parsed ServerToClient message with player list.";
      
      // The list should contain the listener itself AND the seeder.
      if (response.player_list().players_size() > 1) {
        LOG(INFO) << "Received broadcast with " << response.player_list().players_size()
                  << " players. Test PASSED.";
        return 0;
      }
      LOG(INFO) << "Received broadcast with only " << response.player_list().players_size()
                << " player(s). Waiting for the seeder...";
    } else {
      LOG(WARNING) << "Received a message that was not a valid player list.";
    }
    // Wait a bit before retrying
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  LOG(ERROR) << "Broadcast test FAILED: Did not receive broadcast with seeder after several attempts.";
  return 1;
}

auto SyncClient::interactive_listen() -> int {
  LOG(INFO) << "Entering interactive listen mode...";
  for (;;) {
    beast::flat_buffer buffer;
    ws_.read(buffer);
    picoradar::ServerToClient response;
    if (response.ParseFromArray(buffer.data().data(), buffer.size())) {
      if (response.has_player_list()) {
        LOG(INFO) << "Received player list with "
                  << response.player_list().players_size() << " players.";
      } else {
        LOG(INFO) << "Received an unknown protobuf message type.";
      }
    } else {
      LOG(INFO) << "Received raw message: "
                << beast::make_printable(buffer.data());
    }
  }
}

} // namespace picoradar::mock_client

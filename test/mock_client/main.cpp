#include <glog/logging.h>

#include <boost/asio/ip/udp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "player_data.pb.h"

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;
using udp = net::ip::udp;

const std::string DISCOVERY_REQUEST = "PICO_RADAR_DISCOVERY_REQUEST";
const std::string DISCOVERY_RESPONSE_PREFIX = "PICO_RADAR_SERVER:";

void fail(beast::error_code ec, char const* what) {
  LOG(ERROR) << what << ": " << ec.message();
}

std::pair<std::string, std::string> split_address(const std::string& s) {
  size_t pos = s.find(':');
  if (pos == std::string::npos) {
    return {"", ""};
  }
  return {s.substr(0, pos), s.substr(pos + 1)};
}

class SyncClient {
 public:
  SyncClient() : resolver_(ioc_), ws_(ioc_) {}

  int discover_and_run(const std::string& token, const std::string& player_id) {
    try {
      LOG(INFO) << "Attempting to discover server via UDP broadcast...";
      udp::socket socket(ioc_);
      socket.open(udp::v4());
      socket.set_option(net::socket_base::broadcast(true));

      udp::endpoint broadcast_endpoint(net::ip::address_v4::broadcast(), 9001);
      socket.send_to(net::buffer(DISCOVERY_REQUEST), broadcast_endpoint);

      udp::endpoint server_endpoint;
      std::array<char, 128> recv_buf;
      size_t len = socket.receive_from(net::buffer(recv_buf), server_endpoint);

      std::string response(recv_buf.data(), len);
      if (response.rfind(DISCOVERY_RESPONSE_PREFIX, 0) != 0) {
        LOG(ERROR) << "Received invalid discovery response: " << response;
        return 1;
      }

      std::string server_address_str =
          response.substr(DISCOVERY_RESPONSE_PREFIX.length());
      auto [host, port] = split_address(server_address_str);

      host_ = server_endpoint.address().to_string();
      port_ = port;

      LOG(INFO) << "Server discovered at " << host_ << ":" << port_;
      return run_internal(token, "--test-auth-success", player_id);

    } catch (std::exception const& e) {
      LOG(ERROR) << "Discovery failed: " << e.what();
      return 1;
    }
  }

  int run(const std::string& host, const std::string& port,
          const std::string& token, const std::string& mode,
          const std::string& player_id) {
    host_ = host;
    port_ = port;
    return run_internal(token, mode, player_id);
  }

 private:
  int run_internal(const std::string& token, const std::string& mode,
                   const std::string& player_id) {
    try {
      auto const results = resolver_.resolve(host_, port_);
      net::connect(ws_.next_layer(), results.begin(), results.end());

      websocket::stream_base::timeout opt{std::chrono::seconds(10),
                                          std::chrono::seconds(10), true};
      ws_.set_option(opt);

      ws_.handshake(host_ + ":" + port_, "/");
      LOG(INFO) << "Successfully connected to " << host_ << ":" << port_;

      if (!authenticate(token, player_id)) {
        LOG(ERROR) << "Authentication failed.";
        return (mode == "--test-auth-fail") ? 0 : 1;
      }

      if (mode == "--test-auth-success") return 0;
      if (mode == "--seed-data") return seed_data();
      if (mode == "--test-broadcast") return test_broadcast();
      if (mode == "--interactive") return interactive_listen();

      LOG(ERROR) << "Unknown client mode: " << mode;
      return 1;

    } catch (std::exception const& e) {
      LOG(ERROR) << "Exception: " << e.what();
      if (mode == "--test-auth-fail" &&
          std::string(e.what()).find("closed") != std::string::npos) {
        LOG(INFO) << "Connection correctly closed by server. Test PASSED.";
        return 0;
      }
      return 1;
    }
  }

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
    if (!response.ParseFromArray(buffer.data().data(), buffer.size()) ||
        !response.has_auth_response()) {
      LOG(ERROR) << "Failed to parse auth response.";
      return false;
    }
    LOG(INFO) << "Received AuthResponse: success="
              << response.auth_response().success();
    return response.auth_response().success();
  }

  int seed_data() {
    LOG(INFO) << "Seeding data...";
    picoradar::ClientToServer seed_message;
    auto* player_data = seed_message.mutable_player_data();
    player_data->set_player_id("seeder_client");
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
    if (!response.ParseFromArray(buffer.data().data(), buffer.size()) ||
        !response.has_player_list()) {
      LOG(ERROR) << "Did not receive a valid player list broadcast.";
      return 1;
    }

    LOG(INFO) << "Received player list with "
              << response.player_list().players_size() << " players.";
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

  net::io_context ioc_;
  std::string host_;
  std::string port_;
  tcp::resolver resolver_;
  websocket::stream<tcp::socket> ws_;
};

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  FLAGS_logtostderr = true;

  if (argc < 2) {
    LOG(ERROR) << "Usage: \n"
               << "  " << argv[0]
               << " <host> <port> <token> <player_id> [mode]\n"
               << "  " << argv[0] << " --discover <token> <player_id>";
    return 1;
  }

  SyncClient client;
  std::string mode_str = argv[1];

  if (mode_str == "--discover") {
    if (argc != 4) {
      LOG(ERROR) << "Usage: " << argv[0] << " --discover <token> <player_id>";
      return 1;
    }
    const std::string token = argv[2];
    const std::string player_id = argv[3];
    return client.discover_and_run(token, player_id);
  } else {
    if (argc < 5) {
      LOG(ERROR) << "Usage: " << argv[0]
                 << " <host> <port> <token> <player_id> [mode]";
      return 1;
    }
    const std::string host = argv[1];
    const std::string port = argv[2];
    const std::string token = argv[3];
    const std::string player_id = argv[4];
    const std::string mode = (argc > 5) ? argv[5] : "--interactive";
    return client.run(host, port, token, mode, player_id);
  }

  return 0;
}

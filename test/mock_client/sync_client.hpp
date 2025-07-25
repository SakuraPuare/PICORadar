#pragma once

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/websocket.hpp>
#include <string>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

namespace picoradar::mock_client {

class SyncClient {
 public:
  SyncClient();

  auto discover_and_run(const std::string& player_id,
                        uint16_t discovery_port) -> int;

  void run(const std::string& host, const std::string& port,
           const std::string& mode, const std::string& player_id);

 private:
  void connect_and_run(const std::string& host, const std::string& port,
                       const std::string& mode, const std::string& player_id);

  void send_test_data(const std::string& player_id);
  void seed_data_and_exit();
  void test_broadcast(const std::string& player_id);
  static auto discover_server(uint16_t discovery_port) -> std::string;
  void stress_test(const std::string& player_id);

  net::io_context ioc_;
  std::string host_;
  std::string port_;
  tcp::resolver resolver_;
  std::shared_ptr<websocket::stream<tcp::socket>> ws_;
};

}  // namespace picoradar::mock_client
#pragma once

#include <atomic>
#include <boost/asio.hpp>
#include <string>
#include <thread>

namespace net = boost::asio;
using udp = net::ip::udp;

namespace picoradar::network {

class UdpDiscoveryServer {
 public:
  UdpDiscoveryServer(net::io_context& ioc, uint16_t discovery_port,
                     uint16_t service_port,
                     std::string service_host = "0.0.0.0");
  ~UdpDiscoveryServer();

  UdpDiscoveryServer(const UdpDiscoveryServer&) = delete;
  auto operator=(const UdpDiscoveryServer&) -> UdpDiscoveryServer& = delete;

  void start();
  void stop();

 private:
  void do_receive();

  net::io_context& ioc_;
  udp::socket socket_;
  udp::endpoint remote_endpoint_;
  std::array<char, 128> recv_buffer_{};
  uint16_t service_port_;
  std::string service_host_;
  std::atomic<bool> is_running_{false};
};

}  // namespace picoradar::network

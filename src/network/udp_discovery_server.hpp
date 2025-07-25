#ifndef PICORADAR_NETWORK_UDP_DISCOVERY_SERVER_HPP
#define PICORADAR_NETWORK_UDP_DISCOVERY_SERVER_HPP

#include <atomic>
#include <boost/asio.hpp>
#include <string>
#include <thread>

namespace picoradar {
namespace network {

namespace net = boost::asio;
using udp = net::ip::udp;

class UdpDiscoveryServer {
 public:
  UdpDiscoveryServer(net::io_context& ioc, uint16_t discovery_port,
                     uint16_t service_port, const std::string& host_ip);
  ~UdpDiscoveryServer();

  UdpDiscoveryServer(const UdpDiscoveryServer&) = delete;
  UdpDiscoveryServer& operator=(const UdpDiscoveryServer&) = delete;

  void start();
  void stop();

 private:
  void do_receive();
  void handle_receive(const boost::system::error_code& error,
                      std::size_t bytes_transferred);
  void do_send(const std::string& message, udp::endpoint target_endpoint);

  net::io_context& ioc_;
  udp::socket socket_;
  udp::endpoint remote_endpoint_;
  std::array<char, 128> recv_buffer_;
  uint16_t service_port_;
  std::string server_address_response_;
  std::atomic<bool> stop_flag_{false};
};

}  // namespace network
}  // namespace picoradar

#endif  // PICORADAR_NETWORK_UDP_DISCOVERY_SERVER_HPP

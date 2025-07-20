#pragma once

#include <boost/asio.hpp>
#include <atomic>
#include <string>
#include <thread>

namespace net = boost::asio;
using udp = net::ip::udp;

namespace picoradar {
namespace network {

class UdpDiscoveryServer {
public:
    UdpDiscoveryServer(net::io_context& ioc, uint16_t discovery_port, uint16_t service_port);
    ~UdpDiscoveryServer();

    UdpDiscoveryServer(const UdpDiscoveryServer&) = delete;
    UdpDiscoveryServer& operator=(const UdpDiscoveryServer&) = delete;

    void start();
    void stop();

private:
    void do_receive();

    net::io_context& ioc_;
    udp::socket socket_;
    udp::endpoint remote_endpoint_;
    std::array<char, 128> recv_buffer_;
    
    uint16_t service_port_;
    std::atomic<bool> is_running_{false};
};

} // namespace network
} // namespace picoradar

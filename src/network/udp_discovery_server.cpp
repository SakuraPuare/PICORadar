#include "udp_discovery_server.hpp"
#include <glog/logging.h>

namespace picoradar {
namespace network {

const std::string DISCOVERY_REQUEST = "PICO_RADAR_DISCOVERY_REQUEST";
const std::string DISCOVERY_RESPONSE_PREFIX = "PICO_RADAR_SERVER:";

UdpDiscoveryServer::UdpDiscoveryServer(net::io_context& ioc, uint16_t discovery_port, uint16_t service_port)
    : ioc_(ioc),
      socket_(ioc, udp::endpoint(udp::v4(), discovery_port)),
      service_port_(service_port) {}

UdpDiscoveryServer::~UdpDiscoveryServer() {
    stop();
}

void UdpDiscoveryServer::start() {
    LOG(INFO) << "UDP Discovery Server started on port " << socket_.local_endpoint().port();
    is_running_ = true;
    do_receive();
}

void UdpDiscoveryServer::stop() {
    if (is_running_.exchange(false)) {
        net::post(ioc_, [this]() { socket_.cancel(); });
    }
}

void UdpDiscoveryServer::do_receive() {
    socket_.async_receive_from(
        net::buffer(recv_buffer_), remote_endpoint_,
        [this](const boost::system::error_code& ec, std::size_t bytes_recvd) {
            if (!ec && bytes_recvd > 0) {
                std::string received_data(recv_buffer_.data(), bytes_recvd);
                if (received_data == DISCOVERY_REQUEST) {
                    LOG(INFO) << "Received discovery request from " << remote_endpoint_;
                    
                    std::string response_message = DISCOVERY_RESPONSE_PREFIX +
                                                   // We don't know our own public IP, so we respond on the interface we received the request on.
                                                   // The client will use the source of this packet as the server IP.
                                                   "0.0.0.0:" + 
                                                   std::to_string(service_port_);

                    socket_.async_send_to(
                        net::buffer(response_message), remote_endpoint_,
                        [this](const boost::system::error_code& /*ec*/, std::size_t /*bytes_sent*/) {
                            // Continue listening after sending
                            if (is_running_) {
                                do_receive();
                            }
                        });
                    return; // Don't call do_receive here, it's called after send
                }
            }
            
            if (is_running_) {
                do_receive();
            }
        });
}

} // namespace network
} // namespace picoradar

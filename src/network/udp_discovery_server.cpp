#include "network/udp_discovery_server.hpp"

#include "common/config_manager.hpp"
#include "common/constants.hpp"
#include "common/logging.hpp"
#include "common/platform_fixes.hpp"

namespace picoradar::network {

UdpDiscoveryServer::UdpDiscoveryServer(net::io_context& ioc,
                                       const uint16_t discovery_port,
                                       const uint16_t service_port,
                                       const std::string& host_ip)
    : ioc_(ioc), socket_(ioc), recv_buffer_(), service_port_(service_port) {
  const auto& config = common::ConfigManager::getInstance();

  // Get response prefix from config, fallback to constant
  std::string response_prefix = config.getString("discovery.response_prefix")
                                    .value_or(config::kDiscoveryResponsePrefix);

  server_address_response_ =
      response_prefix + host_ip + ":" + std::to_string(service_port_);

  const udp::endpoint listen_endpoint(udp::v4(), discovery_port);
  socket_.open(listen_endpoint.protocol());
  socket_.set_option(net::socket_base::reuse_address(true));
  socket_.set_option(net::socket_base::broadcast(true));
  socket_.bind(listen_endpoint);
}

UdpDiscoveryServer::~UdpDiscoveryServer() { stop(); }

void UdpDiscoveryServer::start() {
  LOG_INFO << "Starting UDP discovery server on port "
           << socket_.local_endpoint().port();
  do_receive();
}

void UdpDiscoveryServer::stop() {
  stop_flag_ = true;
  net::post(ioc_, [this] {
    if (socket_.is_open()) {
      socket_.close();
    }
  });
}

void UdpDiscoveryServer::do_receive() {
  if (stop_flag_) return;

  socket_.async_receive_from(net::buffer(recv_buffer_), remote_endpoint_,
                             [this](const boost::system::error_code& ec,
                                    const std::size_t bytes_transferred) {
                               handle_receive(ec, bytes_transferred);
                             });
}

void UdpDiscoveryServer::handle_receive(const boost::system::error_code& error,
                                        const std::size_t bytes_transferred) {
  if (!error) {
    std::string received_message(recv_buffer_.data(), bytes_transferred);
    LOG_DEBUG << "Discovery server received: '" << received_message << "' from "
              << remote_endpoint_.address().to_string() << ":"
              << remote_endpoint_.port();

    const auto& config = common::ConfigManager::getInstance();
    const std::string expected_request =
        config.getString("discovery.request_message")
            .value_or(config::kDiscoveryRequest);

    if (received_message == expected_request) {
      LOG_INFO << "Received valid discovery request from "
               << remote_endpoint_.address().to_string() << ":"
               << remote_endpoint_.port() << ". Responding with "
               << server_address_response_;
      do_send(server_address_response_, remote_endpoint_);
    } else {
      LOG_WARNING << "Received invalid discovery request: " << received_message;
    }

    do_receive();
  } else if (error != net::error::operation_aborted) {
    LOG_ERROR << "Discovery server receive error: " << error.message();
  }
}

void UdpDiscoveryServer::do_send(const std::string& message,
                                 const udp::endpoint& target_endpoint) {
  socket_.async_send_to(net::buffer(message), target_endpoint,
                        [this](const boost::system::error_code& ec,
                               std::size_t /*bytes_transferred*/) {
                          if (ec) {
                            LOG_ERROR << "Discovery server send error: "
                                      << ec.message();
                          }
                        });
}

}  // namespace picoradar::network

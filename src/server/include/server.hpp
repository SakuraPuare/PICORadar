#pragma once

#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <boost/asio/io_context.hpp>

namespace net = boost::asio;

namespace picoradar {
namespace core { class PlayerRegistry; }
namespace network { class WebsocketServer; }

namespace server {

class Server {
public:
    Server();
    ~Server();

    void start(uint16_t port, int thread_count);
    void stop();
    
    // Method to get player count for testing
    size_t getPlayerCount();

private:
    std::unique_ptr<net::io_context> ioc_;
    std::shared_ptr<core::PlayerRegistry> registry_;
    std::shared_ptr<network::WebsocketServer> ws_server_;
    std::vector<std::thread> server_threads_;
};

} // namespace server
} // namespace picoradar

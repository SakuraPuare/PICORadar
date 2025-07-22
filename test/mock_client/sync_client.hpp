#pragma once

#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <string>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

namespace picoradar::mock_client {

class SyncClient {
public:
    SyncClient();

    auto discover_and_run(const std::string& player_id) -> int;
    
    auto run(const std::string& host, const std::string& port,
             const std::string& mode,
             const std::string& player_id) -> int;

private:
    auto run_internal(const std::string& mode,
                      const std::string& player_id) -> int;
    
    auto seed_data() -> int;
    auto test_broadcast() -> int;
    auto interactive_listen() -> int;

    net::io_context ioc_;
    std::string host_;
    std::string port_;
    tcp::resolver resolver_;
    websocket::stream<tcp::socket> ws_;
};

} // namespace picoradar::mock_client 
#pragma once

#include "core/player_registry.hpp"
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include "network/udp_discovery_server.hpp"
#include "player_data.pb.h"

namespace beast = boost::beast;
namespace net = boost::asio;
namespace websocket = beast::websocket;
using tcp = net::ip::tcp;

namespace picoradar::network {

class WebsocketServer; // Forward declaration

// Handles a single WebSocket connection
class Session : public std::enable_shared_from_this<Session> {
    websocket::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;
    WebsocketServer& server_;
    std::string player_id_;

public:
    Session(tcp::socket&& socket, WebsocketServer& server)
        : ws_(std::move(socket)), server_(server) {}

    void run() {
        // We need to be executing within a strand to perform async operations
        // on the I/O objects in this session.
        net::dispatch(ws_.get_executor(),
                      beast::bind_front_handler(&Session::do_accept, shared_from_this()));
    }

    void close() {
        ws_.async_close(websocket::close_code::normal,
                        beast::bind_front_handler(&Session::on_close, shared_from_this()));
    }

private:
    void do_accept() {
        ws_.async_accept(
            beast::bind_front_handler(&Session::on_accept, shared_from_this()));
    }
    
    void on_accept(beast::error_code ec);
    void do_read();
    void on_read(beast::error_code ec, std::size_t bytes_transferred);
    void on_close(beast::error_code ec);
};

// Accepts incoming connections and launches the sessions
class Listener : public std::enable_shared_from_this<Listener> {
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    WebsocketServer& server_;

public:
    Listener(net::io_context& ioc, const tcp::endpoint& endpoint, WebsocketServer& server)
        : ioc_(ioc), acceptor_(ioc), server_(server) {
        beast::error_code ec;
        acceptor_.open(endpoint.protocol(), ec);
        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        acceptor_.bind(endpoint, ec);
        acceptor_.listen(net::socket_base::max_listen_connections, ec);
    }

    void run() {
        do_accept();
    }

    void stop() {
        acceptor_.close();
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            net::make_strand(ioc_),
            beast::bind_front_handler(&Listener::on_accept, shared_from_this()));
    }

    void on_accept(beast::error_code ec, tcp::socket socket);
};

class WebsocketServer {
public:
    WebsocketServer(net::io_context& ioc, core::PlayerRegistry& registry);
    ~WebsocketServer();

    WebsocketServer(const WebsocketServer&) = delete;
    WebsocketServer& operator=(const WebsocketServer&) = delete;

    void start(const std::string& address, uint16_t port, int thread_count);
    void stop();
    
    // Called by Listener
    void onSessionOpened(std::shared_ptr<Session> session);
    // Called by Session
    void onSessionClosed(std::shared_ptr<Session> session);
    void processMessage(const std::string& player_id, const std::string& message);
    
    core::PlayerRegistry& getRegistry() { return registry_; }

private:
    net::io_context& ioc_;
    core::PlayerRegistry& registry_;
    std::shared_ptr<Listener> listener_;
    std::unique_ptr<UdpDiscoveryServer> discovery_server_;
    std::vector<std::thread> threads_;
    std::set<std::shared_ptr<Session>> sessions_;
    std::atomic<bool> is_running_{false};
};

} // namespace picoradar::network

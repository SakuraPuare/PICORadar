#include "network/websocket_server.hpp"
#include <glog/logging.h>
#include "common/constants.hpp"

namespace picoradar::network {

//------------------------------------------------------------------------------
// Listener implementation

void Listener::on_accept(beast::error_code ec, tcp::socket socket) {
    if (ec) {
        LOG(ERROR) << "Listener accept error: " << ec.message();
        return; // To avoid infinite loop
    }

    // Create the session and run it
    auto session = std::make_shared<Session>(std::move(socket), server_);
    server_.onSessionOpened(session);
    session->run();
    
    // Accept another connection
    do_accept();
}

//------------------------------------------------------------------------------
// Session implementation

void Session::on_accept(beast::error_code ec) {
    if (ec) {
        LOG(ERROR) << "Session accept error: " << ec.message();
        server_.onSessionClosed(shared_from_this());
        return;
    }

    // Read a message
    do_read();
}

void Session::do_read() {
    // Read a message into our buffer
    ws_.async_read(
        buffer_,
        beast::bind_front_handler(&Session::on_read, shared_from_this()));
}

void Session::on_read(beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    // This indicates that the session was closed
    if (ec == websocket::error::closed) {
        server_.onSessionClosed(shared_from_this());
        return;
    }

    if (ec) {
        LOG(ERROR) << "Session read error: " << ec.message();
        server_.onSessionClosed(shared_from_this());
        return;
    }
    
    const auto* msg_data = static_cast<const char*>(buffer_.data().data());
    const std::string message(msg_data, buffer_.size());
    
    if (player_id_.empty()) {
        // First message is the player_id
        player_id_ = message;
        LOG(INFO) << "Player " << player_id_ << " connected.";
    } else {
        server_.processMessage(player_id_, message);
    }

    // Clear the buffer
    buffer_.consume(buffer_.size());

    // Do another read
    do_read();
}

void Session::on_close(beast::error_code ec) {
    if (ec) {
        LOG(ERROR) << "Session close error: " << ec.message();
    }
}

//------------------------------------------------------------------------------
// WebsocketServer implementation

WebsocketServer::WebsocketServer(net::io_context& ioc, core::PlayerRegistry& registry)
    : ioc_{ioc}, registry_{registry} {}

WebsocketServer::~WebsocketServer() {
    if (is_running_) {
        stop();
    }
}

void WebsocketServer::start(const std::string& address, uint16_t port, int thread_count) {
    if (is_running_) {
        return;
    }

    auto const server_address = net::ip::make_address(address);

    listener_ = std::make_shared<Listener>(ioc_, tcp::endpoint{server_address, port}, *this);
    discovery_server_ = std::make_unique<UdpDiscoveryServer>(ioc_, config::kDiscoveryPort, port);

    listener_->run();
    discovery_server_->start();

    threads_.reserve(thread_count);
    for (int i = 0; i < thread_count; ++i) {
        threads_.emplace_back([this] { ioc_.run(); });
    }
    is_running_ = true;
    LOG(INFO) << "WebsocketServer started on " << address << ":" << port;
}

void WebsocketServer::stop() {
    if (!is_running_) {
        return;
    }
    
    // Use post to avoid deadlocks if stop() is called from within an ioc handler
    net::post(ioc_, [this]() {
        if (discovery_server_) {
            discovery_server_->stop();
        }
        if (listener_) {
            listener_->stop();
        }
        auto sessions_copy = sessions_;
        for (const auto& session : sessions_copy) {
            if (session) {
                session->close();
            }
        }
        sessions_.clear();
    });

    ioc_.stop();

    for (auto& t : threads_) {
        if (t.joinable()) {
            t.join();
        }
    }
    threads_.clear();
    
    is_running_ = false;
    LOG(INFO) << "WebsocketServer stopped.";
}

void WebsocketServer::onSessionOpened(std::shared_ptr<Session> session) {
    sessions_.insert(session);
    LOG(INFO) << "Session opened. Total sessions: " << sessions_.size();
}

void WebsocketServer::onSessionClosed(std::shared_ptr<Session> session) {
    if (sessions_.erase(session)) {
        LOG(INFO) << "Session closed. Total sessions: " << sessions_.size();
    }
}

void WebsocketServer::processMessage(const std::string& player_id, const std::string& message) {
    // For now, we just log the message.
    // Later, this will parse the protobuf message and update the player registry.
    LOG(INFO) << "Received message from " << player_id << ": " << message;
}

} // namespace picoradar::network

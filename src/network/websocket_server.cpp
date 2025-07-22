#include "network/websocket_server.hpp"
#include <glog/logging.h>
#include "common/constants.hpp"
#include "player_data.pb.h"

namespace picoradar::network {

//------------------------------------------------------------------------------
// Listener implementation

void Listener::on_accept(beast::error_code ec) {
    if (ec) {
        LOG(ERROR) << "Listener accept error: " << ec.message();
        return; // To avoid infinite loop
    }

    // Create the session and run it
    auto session = std::make_shared<Session>(std::move(socket_), server_);
    server_.onSessionOpened(session);
    session->run();
    
    // Accept another connection
    do_accept();
}

//------------------------------------------------------------------------------
// Session implementation

Session::Session(tcp::socket&& socket, WebsocketServer& server)
    : ws_(std::move(socket)),
      server_(server),
      strand_(ws_.get_executor()) {}

void Session::run() {
    // We need to be executing within a strand to perform async operations
    // on the I/O objects in this session.
    net::dispatch(
        strand_,
        beast::bind_front_handler(&Session::do_accept, shared_from_this()));
}

void Session::do_accept() {
    ws_.async_accept(
        beast::bind_front_handler(&Session::on_accept, shared_from_this()));
}


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
    ws_.binary(true);
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
    
    // It's a binary message
    const auto* msg_data = static_cast<const char*>(buffer_.data().data());
    const std::string message(msg_data, buffer_.size());

    server_.processMessage(shared_from_this(), message);

    // Clear the buffer
    buffer_.consume(buffer_.size());

    // Do another read
    do_read();
}

void Session::send(const std::string& message) {
    net::post(strand_, [self = shared_from_this(), message] {
        self->write_queue_.push(message);
        if (self->write_queue_.size() == 1) {
            self->do_write();
        }
    });
}

void Session::do_write() {
    ws_.binary(true);
    ws_.async_write(
        net::buffer(write_queue_.front()),
        beast::bind_front_handler(
            &Session::on_write,
            shared_from_this()));
}

void Session::on_write(beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    if (ec) {
        LOG(ERROR) << "Session write error: " << ec.message();
        server_.onSessionClosed(shared_from_this());
        return;
    }
    
    write_queue_.pop();
    if (!write_queue_.empty()) {
        do_write();
    }
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
    discovery_server_ = std::make_unique<UdpDiscoveryServer>(ioc_, config::kDiscoveryPort, port, address);

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
    if (!session->getPlayerId().empty()) {
        registry_.removePlayer(session->getPlayerId());
    }
    if (sessions_.erase(session)) {
        LOG(INFO) << "Session closed. Total sessions: " << sessions_.size();
        broadcastPlayerList();
    }
}

void WebsocketServer::processMessage(std::shared_ptr<Session> session, const std::string& message) {
    ClientToServer request;
    if (!request.ParseFromString(message)) {
        LOG(WARNING) << "Failed to parse ClientToServer message.";
        return;
    }

    if (request.has_player_data()) {
        const auto& player_data = request.player_data();
        const std::string& player_id = player_data.player_id();

        if (session->getPlayerId().empty()) {
             session->setPlayerId(player_id);
             LOG(INFO) << "Player " << player_id << " connected and registered.";
        }
        
        registry_.updatePlayer(player_id, player_data);
        broadcastPlayerList();
    }
}

void WebsocketServer::broadcastPlayerList() {
    ServerToClient response;
    auto* player_list = response.mutable_player_list();
    
    for(const auto& player : registry_.getAllPlayers()) {
        auto* player_data = player_list->add_players();
        player_data->CopyFrom(player.second);
    }
    
    std::string serialized_response;
    response.SerializeToString(&serialized_response);

    for (const auto& session : sessions_) {
        session->send(serialized_response);
    }
}

} // namespace picoradar::network

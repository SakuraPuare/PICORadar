#include "network/websocket_server.hpp"
#include "common/logging.hpp"

#include "client.pb.h"
#include "common/config_manager.hpp"
#include "common/constants.hpp"
#include "server.pb.h"

namespace picoradar::network {

//------------------------------------------------------------------------------
// Listener implementation

void Listener::on_accept(beast::error_code ec) {
  if (ec) {
    LOG_ERROR << "Listener accept error: " << ec.message();
    return;  // To avoid infinite loop
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
    : ws_(std::move(socket)), server_(server), strand_(ws_.get_executor()) {}

void Session::run() {
  // We need to be executing within a strand to perform async operations
  // on the I/O objects in this session.
  net::dispatch(strand_, beast::bind_front_handler(&Session::do_accept,
                                                   shared_from_this()));
}

void Session::do_accept() {
  ws_.async_accept(
      beast::bind_front_handler(&Session::on_accept, shared_from_this()));
}

void Session::on_accept(beast::error_code ec) {
  if (ec) {
    LOG_ERROR << "Session accept error: " << ec.message();
    server_.onSessionClosed(shared_from_this());
    return;
  }

  // Read a message
  do_read();
}

void Session::do_read() {
  // Read a message into our buffer
  ws_.binary(true);
  ws_.async_read(buffer_, beast::bind_front_handler(&Session::on_read,
                                                    shared_from_this()));
}

void Session::on_read(beast::error_code ec, std::size_t bytes_transferred) {
  boost::ignore_unused(bytes_transferred);

  // This indicates that the session was closed
  if (ec == websocket::error::closed) {
    server_.onSessionClosed(shared_from_this());
    return;
  }

  if (ec) {
    LOG_ERROR << "Session read error: " << ec.message();
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
      beast::bind_front_handler(&Session::on_write, shared_from_this()));
}

void Session::on_write(beast::error_code ec, std::size_t bytes_transferred) {
  boost::ignore_unused(bytes_transferred);

  if (ec) {
    LOG_ERROR << "Session write error: " << ec.message();
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
    LOG_ERROR << "Session close error: " << ec.message();
  }
}

//------------------------------------------------------------------------------
// WebsocketServer implementation

WebsocketServer::WebsocketServer(net::io_context& ioc,
                                 core::PlayerRegistry& registry)
    : ioc_{ioc}, registry_{registry} {}

WebsocketServer::~WebsocketServer() {
  if (is_running_) {
    stop();
  }
}

void WebsocketServer::start(const std::string& address, uint16_t port,
                            int thread_count) {
  if (is_running_) {
    return;
  }

  auto const server_address = net::ip::make_address(address);

  // Start the I/O context threads first
  threads_.reserve(thread_count);
  for (int i = 0; i < thread_count; ++i) {
    threads_.emplace_back([this] { ioc_.run(); });
  }

  listener_ = std::make_shared<Listener>(
      ioc_, tcp::endpoint{server_address, port}, *this);
  discovery_server_ = std::make_unique<UdpDiscoveryServer>(
      ioc_, config::kDefaultDiscoveryPort, port, address);

  listener_->run();
  discovery_server_->start();

  is_running_ = true;
  LOG_INFO << "WebsocketServer started on " << address << ":" << port;
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
  LOG_INFO << "WebsocketServer stopped.";
}

void WebsocketServer::onSessionOpened(const std::shared_ptr<Session>& session) {
  sessions_.insert(session);
  LOG_INFO << "Session opened. Total sessions: " << sessions_.size();
}

void WebsocketServer::onSessionClosed(const std::shared_ptr<Session>& session) {
  if (!session->getPlayerId().empty()) {
    registry_.removePlayer(session->getPlayerId());
  }
  if (sessions_.erase(session) != 0u) {
    LOG_INFO << "Session closed. Total sessions: " << sessions_.size();
    broadcastPlayerList();
  }
}

void WebsocketServer::processMessage(const std::shared_ptr<Session>& session,
                                     const std::string& message) {
  ClientToServer request;
  if (!request.ParseFromString(message)) {
    LOG_WARNING << "Failed to parse ClientToServer message.";
    return;
  }

  if (request.has_auth_request()) {
    const auto& auth_request = request.auth_request();
    const std::string& player_id = auth_request.player_id();
    const std::string& token = auth_request.token();

    LOG_DEBUG << "Processing auth request for player_id: '" << player_id << "' with token: '" << token << "'";

    // *** ACTUAL AUTHENTICATION LOGIC ***
    const std::string expectedToken = config::getConfig().getAuthToken();
    const bool is_token_valid = (token == expectedToken);
    LOG_DEBUG << "Server-side token check for '" << player_id << "': " << (is_token_valid ? "Valid" : "Invalid");

    if (is_token_valid && !player_id.empty()) {
      // Set the player ID in the session
      session->setPlayerId(player_id);

      // Create a minimal player data for registry
      picoradar::PlayerData player_data;
      player_data.set_player_id(player_id);
      player_data.set_timestamp(0);

      // Add player to registry (using move semantics for better performance)
      registry_.updatePlayer(player_id, std::move(player_data));

      LOG_INFO << "Player " << player_id << " authenticated successfully.";

      // Send auth response
      ServerToClient response;
      auto* auth_response = response.mutable_auth_response();
      auth_response->set_success(true);
      auth_response->set_message("Authentication successful");

      std::string serialized_response;
      response.SerializeToString(&serialized_response);
      session->send(serialized_response);

      // Broadcast updated player list
      broadcastPlayerList();
    } else {
      LOG_WARNING << "Authentication failed for player: '" << player_id << "'. Token Valid: " << is_token_valid << ", PlayerID Empty: " << player_id.empty();

      // Send auth response with failure
      ServerToClient response;
      auto* auth_response = response.mutable_auth_response();
      auth_response->set_success(false);
      auth_response->set_message("Invalid token or player ID");

      std::string serialized_response;
      response.SerializeToString(&serialized_response);
      session->send(serialized_response);

      // Close the session
      session->close();
    }
  } else if (request.has_player_data()) {
    const auto& player_data = request.player_data();
    const std::string& player_id = player_data.player_id();

    if (session->getPlayerId().empty()) {
      session->setPlayerId(player_id);
      LOG_INFO << "Player " << player_id << " connected and registered.";
    }

    registry_.updatePlayer(player_id, player_data);
    broadcastPlayerList();
  }
}

void WebsocketServer::broadcastPlayerList() {
  ServerToClient response;
  auto* player_list = response.mutable_player_list();

  const auto players = registry_.getAllPlayers(); // 现在返回副本，去掉引用
  for (const auto& player : players) {
    auto* player_data = player_list->add_players();
    player_data->CopyFrom(player.second);
  }

  LOG_DEBUG << "Broadcasting player list to " << sessions_.size() << " clients. Total players: " << players.size();

  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  for (const auto& session : sessions_) {
    session->send(serialized_response);
  }
}

}  // namespace picoradar::network

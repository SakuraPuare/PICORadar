#include "network/websocket_server.hpp"
#include "network/error_context.hpp"
#include "common/constants.hpp"
#include "common/logging.hpp"
#include "common/config_manager.hpp"
#include "client.pb.h"
#include "server.pb.h"
#include "player.pb.h"
#include <fmt/format.h>

namespace picoradar::network {

//------------------------------------------------------------------------------
// Listener implementation

void Listener::on_accept(beast::error_code ec) {
  if (ec) {
    NetworkContext ctx("accept", "listener");
    ErrorLogger::logNetworkError(ctx, ec, "Failed to accept new connection");
    return;
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
    : ws_{std::move(socket)}, server_{server}, strand_{ws_.get_executor()} {}

void Session::run() {
  net::dispatch(strand_, beast::bind_front_handler(&Session::do_accept,
                                                   shared_from_this()));
}

void Session::do_accept() {
  ws_.async_accept(
      beast::bind_front_handler(&Session::on_accept, shared_from_this()));
}

void Session::on_accept(beast::error_code ec) {
  auto endpoint = ws_.next_layer().socket().remote_endpoint().address().to_string() + 
                  ":" + std::to_string(ws_.next_layer().socket().remote_endpoint().port());
  NetworkContext ctx("accept", endpoint);
  ctx.player_id = player_id_;

  if (ec) {
    ErrorLogger::logNetworkError(ctx, ec, "WebSocket handshake failed");
    server_.onSessionClosed(shared_from_this());
    return;
  }
  
  ErrorLogger::logOperationSuccess(ctx);
  do_read();
}

void Session::do_read() {
  ws_.binary(true);
  ws_.async_read(buffer_, beast::bind_front_handler(&Session::on_read,
                                                    shared_from_this()));
}

void Session::on_read(beast::error_code ec, std::size_t bytes_transferred) {
  auto endpoint = ws_.next_layer().socket().remote_endpoint().address().to_string() + 
                  ":" + std::to_string(ws_.next_layer().socket().remote_endpoint().port());
  NetworkContext ctx("read", endpoint);
  ctx.player_id = player_id_;
  ctx.bytes_transferred = bytes_transferred;

  if (ec == websocket::error::closed || ec) {
    if (ErrorHelper::isClientDisconnect(ec)) {
      LOG_INFO << "Client disconnected: " << endpoint 
               << (player_id_.empty() ? "" : " (Player: " + player_id_ + ")");
    } else {
      ErrorLogger::logNetworkError(ctx, ec, "Read operation failed");
    }
    server_.onSessionClosed(shared_from_this());
    return;
  }

  if (bytes_transferred > 0) {
    ErrorLogger::logOperationSuccess(ctx);
  }

  const auto* msg_data = static_cast<const char*>(buffer_.data().data());
  std::string message(msg_data, buffer_.size());

  server_.processMessage(shared_from_this(), message);

  buffer_.consume(buffer_.size());
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
  auto endpoint = ws_.next_layer().socket().remote_endpoint().address().to_string() + 
                  ":" + std::to_string(ws_.next_layer().socket().remote_endpoint().port());
  NetworkContext ctx("write", endpoint);
  ctx.player_id = player_id_;
  ctx.bytes_transferred = bytes_transferred;

  if (ec) {
    ErrorLogger::logNetworkError(ctx, ec, "Write operation failed");
    server_.onSessionClosed(shared_from_this());
    return;
  }

  ErrorLogger::logOperationSuccess(ctx);

  write_queue_.pop();
  if (!write_queue_.empty()) {
    do_write();
  }
}

void Session::close() {
  net::post(strand_, [self = shared_from_this()]() {
    beast::get_lowest_layer(self->ws_).close();
  });
}

void Session::on_close(beast::error_code ec) {
  boost::ignore_unused(ec);
  LOG_DEBUG << "WebSocket connection closed";
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
    LOG_WARNING << "WebSocket server is already running";
    return;
  }

  auto server_address = net::ip::make_address(address);

  threads_.reserve(thread_count);
  for (int i = 0; i < thread_count; ++i) {
    threads_.emplace_back([this] { ioc_.run(); });
  }

  listener_ = std::make_shared<Listener>(
      ioc_, tcp::endpoint{server_address, port}, *this);

  listener_->run();

  is_running_ = true;
  LOG_INFO << fmt::format("WebSocket server started on {}:{}", address, port);
}

void WebsocketServer::stop() {
  if (!is_running_) {
    return;
  }

  LOG_INFO << "Stopping WebSocket server...";
  net::post(ioc_, [this]() {
    if (listener_) {
      listener_->stop();
    }
    auto sessions_copy = sessions_;
    for (auto& session : sessions_copy) {
      session->close();
    }
    sessions_.clear();
    ioc_.stop();
  });

  for (auto& t : threads_) {
    if (t.joinable()) {
      t.join();
    }
  }
  threads_.clear();

  is_running_ = false;
  LOG_INFO << "WebSocket server stopped";
}

void WebsocketServer::onSessionOpened(const std::shared_ptr<Session>& session) {
  sessions_.insert(session);
  LOG_DEBUG << "Client connected. Total connections: " << sessions_.size();
}

void WebsocketServer::onSessionClosed(const std::shared_ptr<Session>& session) {
  if (!session->getPlayerId().empty()) {
    registry_.removePlayer(session->getPlayerId());
  }
  if (sessions_.erase(session) != 0u) {
    LOG_DEBUG << "Client disconnected. Total connections: " << sessions_.size();
    broadcastPlayerList();
  }
}

void WebsocketServer::processMessage(const std::shared_ptr<Session>& session,
                                    const std::string& raw_message) {
  try {
    ::picoradar::ClientToServer client_msg;
    if (!client_msg.ParseFromString(raw_message)) {
      LOG_WARNING << "Failed to parse client message";
      return;
    }

    if (client_msg.has_auth_request()) {
      const auto& auth_req = client_msg.auth_request();
      const std::string& token = auth_req.token();
      const std::string& player_id = auth_req.player_id();

      auto& config = picoradar::common::ConfigManager::getInstance();
      auto expectedToken = config.getString("auth.token");
      
      if (!expectedToken || token != expectedToken.value()) {
        LOG_WARNING << "Authentication failed for token: " << token;
        
        ::picoradar::ServerToClient response;
        auto* auth_response = response.mutable_auth_response();
        auth_response->set_success(false);
        auth_response->set_message("Invalid authentication token");
        
        std::string serialized_response;
        response.SerializeToString(&serialized_response);
        session->send(serialized_response);
        return;
      }

      if (!player_id.empty()) {
        LOG_INFO << fmt::format("Player {} authenticated successfully", player_id);
        
        session->setPlayerId(player_id);

        ::picoradar::PlayerData player_data;
        player_data.set_player_id(player_id);
        auto* position = player_data.mutable_position();
        position->set_x(0.0);
        position->set_y(0.0);
        position->set_z(0.0);
        player_data.set_timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

        registry_.updatePlayer(player_id, std::move(player_data));

        ::picoradar::ServerToClient response;
        auto* auth_response = response.mutable_auth_response();
        auth_response->set_success(true);
        auth_response->set_message("Authentication successful");

        std::string serialized_response;
        response.SerializeToString(&serialized_response);
        session->send(serialized_response);

        broadcastPlayerList();
      } else {
        LOG_WARNING << "Empty player ID in auth request";
        
        ::picoradar::ServerToClient response;
        auto* auth_response = response.mutable_auth_response();
        auth_response->set_success(false);
        auth_response->set_message("Player ID cannot be empty");

        std::string serialized_response;
        response.SerializeToString(&serialized_response);
        session->send(serialized_response);
        session->close();
      }
    } else if (client_msg.has_player_data()) {
      const auto& player_update = client_msg.player_data();
      const std::string& player_id = player_update.player_id();

      if (session->getPlayerId().empty()) {
        session->setPlayerId(player_id);
      }

      registry_.updatePlayer(player_id, player_update);
      broadcastPlayerList();
    }
  } catch (const std::exception& e) {
    LOG_ERROR << "Error processing message: " << e.what();
  }
}

void WebsocketServer::broadcastPlayerList() {
  ::picoradar::ServerToClient response;
  auto* player_list = response.mutable_player_list();

  const auto players = registry_.getAllPlayers();
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

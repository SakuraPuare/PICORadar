#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <memory>
#include <thread>
#include <string>
#include <vector>

#include "core/player_registry.hpp"

// 定义命名空间别名以简化代码
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

namespace picoradar {
namespace network {

// 前向声明 Listener 类
class Listener;

class WebsocketServer : public std::enable_shared_from_this<WebsocketServer> {
public:
    WebsocketServer(core::PlayerRegistry& registry);
    ~WebsocketServer();

    // 禁止拷贝和赋值
    WebsocketServer(const WebsocketServer&) = delete;
    WebsocketServer& operator=(const WebsocketServer&) = delete;

    /**
     * @brief 在一个新线程中启动服务器。
     * @param address 要监听的IP地址
     * @param port 要监听的端口
     * @param threads 用于处理IO的线程数
     */
    void run(const std::string& address, uint16_t port, int threads);

    /**
     * @brief 停止服务器并等待其线程结束。
     */
    void stop();

private:
    friend class Listener; // 允许 Listener 访问私有成员

    core::PlayerRegistry& registry_;
    net::io_context ioc_;
    std::shared_ptr<Listener> listener_;
    std::vector<std::thread> threads_;
};

} // namespace network
} // namespace picoradar
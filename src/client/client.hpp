#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio.hpp>
#include <string>
#include <memory>
#include "player_data.pb.h"
#include <atomic> // Added for std::atomic
#include <mutex> // Added for std::mutex
#include <thread>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

namespace picoradar::client {

class Client {
public:
    Client();
    ~Client();

    // 禁止拷贝和赋值
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    /**
     * @brief 初始化客户端并连接到服务器
     * @param host 服务器主机地址
     * @param port 服务器端口
     * @return 连接是否成功
     */
    void connect(const std::string& host, const std::string& port);

    /**
     * @brief 通过UDP广播发现服务器
     * @param discovery_port 发现端口，默认为9001
     * @return 发现的服务器地址，格式为"host:port"
     */
    std::string discover_server(uint16_t discovery_port = 9001);

    /**
     * @brief 断开与服务器的连接
     */
    void disconnect();

    /**
     * @brief 发送玩家数据到服务器
     * @param player_data 玩家数据
     * @return 发送是否成功
     */
    bool send_player_data(const picoradar::PlayerData& player_data);

    /**
     * @brief 设置玩家ID
     * @param player_id 玩家ID
     */
    void set_player_id(const std::string& player_id);

    /**
     * @brief 获取玩家ID
     * @return 玩家ID
     */
    const std::string& get_player_id() const;

    /**
     * @brief 设置认证令牌
     * @param token 认证令牌
     */
    void set_auth_token(const std::string& token);

    /**
     * @brief 获取认证令牌
     * @return 认证令牌
     */
    const std::string& get_auth_token() const;

    /**
     * @brief 获取所有玩家数据
     * @return 玩家数据列表
     */
    const picoradar::PlayerList& get_player_list() const;

    /**
     * @brief 检查客户端是否已连接
     * @return 是否已连接
     */
    bool is_connected() const;

private:
    /**
     * @brief 开始读取来自服务器的消息
     */
    void start_read();

    /**
     * @brief 处理从服务器读取到的消息
     * @param ec 错误代码
     * @param bytes_transferred 传输的字节数
     */
    void on_read(beast::error_code ec, std::size_t bytes_transferred);
    
    /**
     * @brief 发送认证请求
     */
    void send_authentication_request();

    // 网络相关成员
    net::io_context ioc_;
    websocket::stream<tcp::socket> ws_;
    
    // 客户端状态
    std::string player_id_;
    std::string auth_token_;
    picoradar::PlayerList player_list_;
    std::atomic<bool> is_connected_;
    std::atomic<bool> is_authenticated_;

    // 线程安全
    mutable std::mutex player_list_mutex_;

    // 异步处理
    std::thread io_thread_;
    std::unique_ptr<net::executor_work_guard<net::io_context::executor_type>> work_;

    // 读取缓冲区
    beast::flat_buffer read_buffer_;
};

} // namespace picoradar::client
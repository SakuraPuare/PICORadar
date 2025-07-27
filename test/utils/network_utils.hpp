#pragma once

#include <boost/asio.hpp>

namespace picoradar::test {

/**
 * @brief 获取一个可用的随机TCP端口。
 *
 * 该函数通过绑定到端口0来请求操作系统分配一个临时端口。
 * 这对于并行运行测试以避免端口冲突非常有用。
 *
 * @return 一个可用的端口号，如果失败则可能返回0.
 */
inline auto get_available_port() -> uint16_t {
  try {
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::acceptor acceptor(io_context);
    boost::system::error_code ec;
    acceptor.open(boost::asio::ip::tcp::v4(), ec);
    if (ec) {
      return 0;
    }
    acceptor.bind(boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 0),
                  ec);
    if (ec) {
      return 0;
    }
    const uint16_t port = acceptor.local_endpoint().port();
    acceptor.close();
    return port;
  } catch (const std::exception&) {
    return 0;
  }
}

}  // namespace picoradar::test

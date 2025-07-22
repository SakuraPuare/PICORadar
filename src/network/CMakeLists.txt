# src/network/CMakeLists.txt

add_library(network_lib STATIC)

target_sources(network_lib
    PRIVATE
        udp_discovery_server.cpp
        websocket_server.cpp
    PUBLIC
        udp_discovery_server.hpp
        websocket_server.hpp
)

target_include_directories(network_lib PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(network_lib PUBLIC
    common_lib
    core_logic
    Boost::beast
    project_includes # 链接到中央包含目录
)
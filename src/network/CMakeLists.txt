# src/network/CMakeLists.txt

add_library(network_lib
    websocket_server.cpp
    udp_discovery_server.cpp
)

target_include_directories(network_lib
    PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(network_lib
    PUBLIC
    core_logic
    glog::glog
    PRIVATE
    Boost::system
    Boost::thread
    Threads::Threads
)
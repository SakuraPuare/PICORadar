# src/network/CMakeLists.txt

add_library(network_lib STATIC)

target_sources(network_lib
    PRIVATE
    udp_discovery_server.cpp
    websocket_server.cpp
)

target_include_directories(network_lib
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(network_lib
    PUBLIC
    project_includes
    common_lib
    core_lib
    proto_gen
    Boost::beast
)

# test/mock_client/CMakeLists.txt

# 1. 创建可被其他目标（如测试）重用的静态库
add_library(
    mock_client_lib
    sync_client.cpp
    sync_client.hpp
)

target_include_directories(
    mock_client_lib 
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}" # For sync_client.hpp
        ${CMAKE_SOURCE_DIR}/src # For common headers
        ${CMAKE_BINARY_DIR} # For protobuf generated headers
)

target_link_libraries(
    mock_client_lib
    PRIVATE
        Boost::asio
        Boost::beast
        glog::glog
        common_lib
        proto_gen
)

# Main executable for the mock client
add_executable(
    mock_client_app
    main.cpp
)

target_link_libraries(
    mock_client_app
    PRIVATE
        mock_client_lib
        glog::glog
)

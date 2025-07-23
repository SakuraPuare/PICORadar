# src/client/CMakeLists.txt

add_library(client_lib STATIC)

target_sources(client_lib
    PRIVATE
        client.cpp
    PUBLIC
        client.hpp
)

target_include_directories(client_lib PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(client_lib PUBLIC
    common_lib
    proto_gen
    Boost::beast
    project_includes
)

# 添加一个简单的测试程序（可选）
add_executable(test_client
    test_client.cpp
)

target_link_libraries(test_client
    PRIVATE
    client_lib
    common_lib  # 确保链接日志库
)
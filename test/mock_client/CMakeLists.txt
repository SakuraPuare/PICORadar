# test/mock_client/CMakeLists.txt

# 1. 创建可被其他目标（如测试）重用的静态库
add_library(mock_client_lib STATIC)

target_sources(mock_client_lib
    PRIVATE
        sync_client.cpp
    PUBLIC
        sync_client.hpp
)

target_include_directories(mock_client_lib PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

# 链接 network_lib 会传递所有需要的依赖
target_link_libraries(mock_client_lib PUBLIC
    network_lib
    project_includes # 链接到中央包含目录
)

# 2. 创建一个轻量的可执行文件作为入口点
add_executable(mock_client
    main.cpp
)

target_link_libraries(mock_client
    PRIVATE
    mock_client_lib
)

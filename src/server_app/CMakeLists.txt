# src/server_app/CMakeLists.txt

add_executable(server_app
    main.cpp
)

# 只需要链接 network_lib，它会传递性地链接所有需要的库和包含目录
target_link_libraries(server_app
    PRIVATE
    network_lib
    common_lib # 显式链接 common_lib
)
# src/server_app/CMakeLists.txt

add_executable(server_app main.cpp)

target_link_libraries(server_app
    PRIVATE
    # 添加我们自己项目的库
    core_logic
    
    # 添加第三方库
    protobuf::libprotobuf
    ${WEBSOCKETPP_LIBRARIES}
)

# 包含生成的proto代码
target_link_libraries(server_app PRIVATE proto_gen)

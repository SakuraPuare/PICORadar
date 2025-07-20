# src/server_app/CMakeLists.txt

add_executable(server_app
    main.cpp
)

target_include_directories(server_app
    PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
)

target_link_libraries(server_app
    PRIVATE
    # 我们自己项目的库
    network_lib
    common_lib
    
    # 第三方库
    glog::glog
)
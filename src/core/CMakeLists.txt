# src/core/CMakeLists.txt

add_library(core_logic STATIC)

target_sources(core_logic
    PRIVATE
        player_registry.cpp
    PUBLIC
        player_registry.hpp
)

# 库本身的实现需要包含当前目录的头文件
target_include_directories(core_logic PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

# 链接到 common_lib 和 proto_gen 库
# glog 和 protobuf 的依赖会通过 common_lib 和 proto_gen 传递过来
target_link_libraries(core_logic PUBLIC
    common_lib
    proto_gen
    project_includes # 链接到中央包含目录
)


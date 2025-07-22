# src/common/CMakeLists.txt

add_library(common_lib STATIC)

target_sources(common_lib
    PRIVATE
        process_utils.cpp
        single_instance_guard.cpp
    PUBLIC
        # 公开头文件，其他模块可以引用
        process_utils.hpp
        single_instance_guard.hpp
        constants.hpp
)

# 库本身的实现需要包含当前目录的头文件
target_include_directories(common_lib PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

# 链接依赖
target_link_libraries(common_lib PUBLIC
    glog::glog
    project_includes # 链接到中央包含目录
)

if (UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(common_lib PUBLIC Threads::Threads)
endif()

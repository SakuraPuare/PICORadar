# src/common/CMakeLists.txt

add_library(common_lib
    single_instance_guard.cpp
    single_instance_guard.hpp
    process_utils.cpp
    process_utils.hpp
)

target_include_directories(common_lib
    PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

# 平台特定的库依赖
if(UNIX AND NOT APPLE)
    target_link_libraries(common_lib PUBLIC Threads::Threads)
endif()

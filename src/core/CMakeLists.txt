# src/core/CMakeLists.txt

add_library(core_logic
    player_registry.cpp
    player_registry.hpp
)

# 设置包含目录，仅包含当前目录
target_include_directories(core_logic
    PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

# 链接到 proto_gen 库。
# 使用 PUBLIC 关键字可以确保任何链接到 core_logic 的目标
# 都能自动获取 proto_gen 的包含目录和依赖项。
target_link_libraries(core_logic
    PUBLIC
    proto_gen
)


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
target_link_libraries(core_logic
    PUBLIC
    proto_gen
)


# src/core/CMakeLists.txt

add_library(core_logic
    player_registry.cpp
)

target_include_directories(core_logic
    PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(core_logic
    PRIVATE
    protobuf::libprotobuf
)

# 包含生成的proto代码
target_link_libraries(core_logic PRIVATE proto_gen)

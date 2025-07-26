# src/core/CMakeLists.txt

add_library(core_lib STATIC)

target_sources(core_lib
    PRIVATE
    player_registry.cpp
)

target_include_directories(core_lib
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(core_lib
    PUBLIC
    project_includes
    proto_gen
)
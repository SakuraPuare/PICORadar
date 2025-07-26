# src/server/CMakeLists.txt

# Create the library
add_library(server_lib STATIC)

target_sources(server_lib
    PRIVATE
    server.cpp
)

target_include_directories(server_lib
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(server_lib
    PUBLIC
    project_includes
    network_lib
    core_lib
    common_lib
    proto_gen
)

# Create the executable that uses the library
add_executable(server main.cpp)
target_link_libraries(server PRIVATE server_lib)

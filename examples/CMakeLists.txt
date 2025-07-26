# ==============================================================================
# Examples CMakeLists.txt
# ==============================================================================

# Client library usage example
add_executable(client_example
    client_example.cpp
)

target_link_libraries(client_example
    PRIVATE
        client_lib
        common_lib
        glog::glog
)

target_include_directories(client_example
    PRIVATE
        "${CMAKE_SOURCE_DIR}/src/client/include"
        "${CMAKE_SOURCE_DIR}/src"
)

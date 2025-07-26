# ==============================================================================
# Client Library CMakeLists.txt
# ==============================================================================

# Create the client library
add_library(client_lib STATIC
    client.cpp
    impl/client_impl.cpp
)

# Link against required libraries
target_link_libraries(client_lib
    PUBLIC
        proto_gen
        project_includes
    PRIVATE
        glog::glog
        Boost::system
        Boost::thread
)

# Set public include directories
target_include_directories(client_lib
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/impl"
)

# Set compile features
target_compile_features(client_lib
    PUBLIC
        cxx_std_17
)

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(client_lib PRIVATE
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
    )
endif()

# For Windows MSVC
if(MSVC)
    target_compile_options(client_lib PRIVATE
        /W4
        /wd4100  # Disable unused parameter warning
    )
endif()

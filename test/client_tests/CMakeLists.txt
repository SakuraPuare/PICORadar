# ==============================================================================
# Client Tests CMakeLists.txt
# ==============================================================================

# Include GoogleTest module for gtest_discover_tests
include(GoogleTest)

add_executable(client_tests
    test_client_basic.cpp
    test_client_connection.cpp
    test_client_integration.cpp
)

target_link_libraries(client_tests
    PRIVATE
        client_lib
        server_lib
        GTest::gtest
        GTest::gtest_main
        glog::glog
)

target_include_directories(client_tests
    PRIVATE
        "${CMAKE_SOURCE_DIR}/src/client/include"
)

# Use gtest_discover_tests to automatically discover and add individual tests
gtest_discover_tests(client_tests
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    PROPERTIES
        TIMEOUT 60
)

# test/integration_tests/CMakeLists.txt

add_executable(integration_tests
    test_broadcast.cpp
    test_discovery.cpp
)

target_link_libraries(integration_tests PRIVATE
    network_lib
    mock_client_lib
    GTest::gtest
    GTest::gmock
    GTest::gtest_main
    project_includes
)

include(GoogleTest)
gtest_discover_tests(integration_tests)

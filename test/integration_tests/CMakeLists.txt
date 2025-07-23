# test/integration_tests/CMakeLists.txt

add_executable(integration_tests
    test_broadcast.cpp
    test_discovery.cpp
    test_client_library.cpp
    $<TARGET_OBJECTS:gtest_main_obj>
)

# 链接 GTest 和我们自定义的 main 函数
target_link_libraries(integration_tests
    PRIVATE
    common_lib
    network_lib
    core_logic
    mock_client_lib
    client_lib
    GTest::gtest
    GTest::gmock
)

gtest_discover_tests(integration_tests)

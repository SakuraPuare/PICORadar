# test/core_tests/CMakeLists.txt

add_executable(core_tests
    test_player_registry.cpp
    test_server_stats.cpp
    test_cli_commands.cpp
    test_stats_integration.cpp
    test_stats_boundary.cpp
    test_stats_performance.cpp
    $<TARGET_OBJECTS:gtest_main_obj>
)

target_link_libraries(core_tests
    PRIVATE
    core_lib
    common_lib
    server_lib
    network_lib
    proto_gen
    GTest::gtest
    GTest::gmock
    Boost::system
    Boost::thread
)

gtest_discover_tests(core_tests
    PROPERTIES
        TIMEOUT 10
)


if(ENABLE_COVERAGE)
    target_link_libraries(core_tests PRIVATE --coverage)
endif()


# 添加头文件搜索路径
target_include_directories(core_tests
    PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
)

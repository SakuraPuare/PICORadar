# test/core_tests/CMakeLists.txt

add_executable(core_tests
    test_player_registry.cpp
)

target_link_libraries(core_tests
    PRIVATE
    core_logic
    project_includes # For common headers and correct include paths
    GTest::gtest
    GTest::gmock
    GTest::gtest_main # Provides main function and links gtest
)

# Discover and add tests to CTest automatically
include(GoogleTest)
gtest_discover_tests(core_tests)


if(ENABLE_COVERAGE)
    target_link_libraries(core_tests PRIVATE --coverage)
endif()


# 添加头文件搜索路径
target_include_directories(core_tests
    PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
)

# test/common_tests/CMakeLists.txt

# 1. 编译一个辅助可执行文件，用于在测试中扮演“锁持有者”
add_executable(common_tests_locker
    single_instance_guard_main.cpp
)

target_link_libraries(common_tests_locker PRIVATE
    common_lib
    project_includes
)


# 2. 编译 GTest 测试运行器
add_executable(common_tests_runner
    test_single_instance_guard.cpp
)

target_link_libraries(common_tests_runner PRIVATE
    common_lib
    GTest::gtest
    GTest::gmock
    GTest::gtest_main
    project_includes
)

# 3. 注册 GTest 测试
include(GoogleTest)
gtest_discover_tests(common_tests_runner
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

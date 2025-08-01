# test/common_tests/CMakeLists.txt

# 编译 GTest 测试运行器tests/CMakeLists.txt

# 1. 编译一个辅助可执行文件，用于在测试中扮演“锁持有者”
add_executable(common_tests_locker
    single_instance_guard_main.cpp
)

target_link_libraries(common_tests_locker PRIVATE
    common_lib
    project_includes
)


# 2. 编译 GTest 测试运行器
add_executable(common_tests
    test_single_instance_guard.cpp
    test_config_manager.cpp
    test_process_utils.cpp
    test_string_utils.cpp
    test_logging.cpp
    test_performance.cpp
    test_integration.cpp
    $<TARGET_OBJECTS:gtest_main_obj>
)

# Link against gtest and our common library
target_link_libraries(common_tests
    PRIVATE
    common_lib
    GTest::gtest
    GTest::gmock
    protobuf::libprotobuf
)

gtest_discover_tests(common_tests
    PROPERTIES
        TIMEOUT 10
)

# test/common_tests/CMakeLists.txt

add_executable(common_tests
    test_single_instance_guard.cpp
)

target_include_directories(common_tests
    PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
)

target_link_libraries(common_tests
    PRIVATE
    common_lib
    glog::glog
)

# 注册测试脚本
add_test(
    NAME StaleLockTest
    COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/run_stale_lock_test.sh"
            "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME SingleInstanceGuardTest
    COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/run_instance_test.sh"
            "${CMAKE_CURRENT_BINARY_DIR}"
)

# 确保陈旧锁测试在常规实例测试之前运行
set_tests_properties(
    SingleInstanceGuardTest
    PROPERTIES
    DEPENDS StaleLockTest
)

# test/core_tests/CMakeLists.txt

add_executable(core_tests
    test_player_registry.cpp
)

# 将测试可执行文件注册到CTest
add_test(NAME core_tests COMMAND core_tests)


# 添加头文件搜索路径
target_include_directories(core_tests
    PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
)

target_link_libraries(core_tests
    PRIVATE
    core_logic
    proto_gen
    GTest::gtest
    GTest::gtest_main # 包含main函数，使测试可独立运行
)

# 将此可执行文件添加为测试
include(GoogleTest)
gtest_add_tests(TARGET core_tests)

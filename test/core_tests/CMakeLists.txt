# test/core_tests/CMakeLists.txt

add_executable(core_tests
    test_player_registry.cpp
)

target_link_libraries(core_tests
    PRIVATE
    core_logic
    GTest::gtest
    GTest::gtest_main # 包含main函数，使测试可独立运行
)

# 将此可执行文件添加为测试
include(GoogleTest)
gtest_add_tests(TARGET core_tests)

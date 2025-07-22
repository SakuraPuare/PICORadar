# test/core_tests/CMakeLists.txt

add_executable(core_tests
    test_player_registry.cpp
    $<TARGET_OBJECTS:gtest_main_obj>
)

target_link_libraries(core_tests
    PRIVATE
    core_logic
    common_lib
    GTest::gtest
    GTest::gmock
)

gtest_discover_tests(core_tests)


if(ENABLE_COVERAGE)
    target_link_libraries(core_tests PRIVATE --coverage)
endif()


# 添加头文件搜索路径
target_include_directories(core_tests
    PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
)

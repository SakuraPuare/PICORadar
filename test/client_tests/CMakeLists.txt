# test/client_tests/CMakeLists.txt

# 创建客户端库测试可执行文件
add_executable(client_tests
    test_client.cpp
)

# 链接到gtest_main_obj对象库（它提供了main函数）
target_link_libraries(client_tests
    PRIVATE
    gtest_main_obj
    client_lib
)

# 将测试添加到CTest
gtest_discover_tests(client_tests)
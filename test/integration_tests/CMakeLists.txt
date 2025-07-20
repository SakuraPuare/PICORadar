# test/integration_tests/CMakeLists.txt

# 这个目录包含用于运行集成测试的脚本。
# 这些测试通常涉及启动服务器和客户端。

add_test(
    NAME AuthFailTest
    COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/run_auth_fail_test.sh"
            "${CMAKE_BINARY_DIR}" # 传递构建目录
)

add_test(
    NAME AuthSuccessTest
    COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/run_auth_success_test.sh"
            "${CMAKE_BINARY_DIR}" # 传递构建目录
)

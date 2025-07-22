# test/CMakeLists.txt

# 单元测试
add_subdirectory(core_tests)
add_subdirectory(common_tests)

# 集成测试
add_subdirectory(integration_tests)

# 模拟客户端（不是一个测试，但为了编译而包含）
add_subdirectory(mock_client)

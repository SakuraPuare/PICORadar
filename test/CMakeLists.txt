# test/CMakeLists.txt

# 启用测试
enable_testing()

# 添加gtest (我们稍后会通过vcpkg或FetchContent来获取)
# find_package(GTest REQUIRED)

if(PICORADAR_BUILD_MOCK_CLIENT)
    add_subdirectory(mock_client)
endif()

add_subdirectory(core_tests)

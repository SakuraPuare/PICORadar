# test/CMakeLists.txt

# 添加gtest (我们稍后会通过vcpkg或FetchContent来获取)
# find_package(GTest REQUIRED)

if(PICORAD-AR_BUILD_MOCK_CLIENT)
    # add_subdirectory(mock_client)
endif()

add_subdirectory(core_tests)

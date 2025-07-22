# GTest/GMock
find_package(GTest REQUIRED)
include(GoogleTest)

# Add our custom main entry point for all tests as an OBJECT library
add_library(gtest_main_obj OBJECT main.cpp)
target_link_libraries(gtest_main_obj PRIVATE common_lib GTest::gtest GTest::gmock)

# Add subdirectories for different test suites
add_subdirectory(common_tests)
add_subdirectory(core_tests)
add_subdirectory(integration_tests)
add_subdirectory(mock_client)

# Add individual test runners to CTest
# gtest_discover_tests(common_tests_runner)
# gtest_discover_tests(core_tests_runner)
# gtest_discover_tests(integration_tests)

# Network tests
file(GLOB NETWORK_TEST_SOURCES "*.cpp")

add_executable(network_tests
    ${NETWORK_TEST_SOURCES}
    $<TARGET_OBJECTS:gtest_main_obj>
)

target_include_directories(network_tests PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_BINARY_DIR}"
)

target_link_libraries(network_tests PRIVATE
    network_lib
    common_lib
    core_lib
    GTest::gtest
    GTest::gmock
    proto_gen
    Boost::system
    Boost::thread
)

# Add timeout to all tests
gtest_discover_tests(network_tests
    TIMEOUT 10
)

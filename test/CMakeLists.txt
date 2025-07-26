# GTest/GMock
find_package(GTest REQUIRED)
include(GoogleTest)

# Add our custom main entry point for all tests as an OBJECT library
add_library(gtest_main_obj OBJECT main.cpp)

target_include_directories(gtest_main_obj PUBLIC
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_BINARY_DIR}"
)

target_link_libraries(gtest_main_obj PRIVATE
    common_lib
    GTest::gtest
    GTest::gmock
    proto_gen
)

# Add test subdirectories
add_subdirectory(common_tests)
add_subdirectory(core_tests)
add_subdirectory(client_tests)
add_subdirectory(network_tests)

if(USE_VALGRIND)
    find_program(VALGRIND_EXECUTABLE valgrind)
    if(VALGRIND_EXECUTABLE)
        message(STATUS "Valgrind found at: ${VALGRIND_EXECUTABLE}")
        # Define a memory check for each test
        add_test(NAME common_tests_memcheck COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --show-leak-kinds=all --track-origins=yes -s $<TARGET_FILE:common_tests>)
        set_tests_properties(common_tests_memcheck PROPERTIES 
            WILL_FAIL true
            TIMEOUT 10
        )

        add_test(NAME core_tests_memcheck COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --show-leak-kinds=all --track-origins=yes -s $<TARGET_FILE:core_tests>)
        set_tests_properties(core_tests_memcheck PROPERTIES 
            WILL_FAIL true
            TIMEOUT 5
        )

    else()
        message(WARNING "Valgrind not found. Memory checks will be skipped.")
    endif()
endif()

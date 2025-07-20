# test/CMakeLists.txt

add_subdirectory(core_tests)
add_subdirectory(common_tests)
add_subdirectory(integration_tests)

if(PICORADAR_BUILD_MOCK_CLIENT)
    add_subdirectory(mock_client)
endif()

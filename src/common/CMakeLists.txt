# src/common/CMakeLists.txt

add_library(common_lib STATIC)

# Source files are private to the library
target_sources(common_lib
    PRIVATE
    config_manager.cpp
    process_utils.cpp
    single_instance_guard.cpp
    logging.cpp
    string_utils.cpp
)

# Headers are made public so consumers can find them
target_include_directories(common_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})


target_link_libraries(common_lib
    PUBLIC
    project_includes
    glog::glog
    nlohmann_json::nlohmann_json
    fmt::fmt
    tl::expected
)

if (UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(common_lib PUBLIC Threads::Threads)
endif()

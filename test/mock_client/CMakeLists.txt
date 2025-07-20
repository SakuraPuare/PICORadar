# test/mock_client/CMakeLists.txt

add_executable(mock_client
    main.cpp
)

target_include_directories(mock_client
    PRIVATE
    # For including headers like core/player_registry.hpp, etc.
    "${CMAKE_SOURCE_DIR}/src"
    # For including generated protobuf headers
    "${CMAKE_BINARY_DIR}"
)

target_link_libraries(mock_client
    PRIVATE
    # Our own libraries
    proto_gen

    # Third-party libraries
    glog::glog
    Boost::system
    Boost::thread
    Threads::Threads
)

# 性能测试模块
find_package(benchmark CONFIG REQUIRED)

# 创建性能测试可执行文件
add_executable(performance_tests
    simple_benchmark.cpp
)

# 添加个别基准测试可执行文件
add_executable(benchmark_config_manager benchmark_config_manager_fixed.cpp)
add_executable(benchmark_player_registry benchmark_player_registry.cpp)
add_executable(benchmark_network_simple benchmark_network_simple.cpp)
add_executable(benchmark_advanced benchmark_advanced.cpp)

# 链接依赖
target_link_libraries(performance_tests
    PRIVATE
        benchmark::benchmark
        core_lib
        proto_gen
)

# 配置管理器基准测试链接
target_link_libraries(benchmark_config_manager 
    PRIVATE
        benchmark::benchmark
        proto_gen
)

# 玩家注册基准测试链接  
target_link_libraries(benchmark_player_registry
    PRIVATE
        benchmark::benchmark
        core_lib
        proto_gen
)

# 网络延迟基准测试链接
target_link_libraries(benchmark_network_simple
    PRIVATE
        benchmark::benchmark
        proto_gen
)

# 高级基准测试链接
target_link_libraries(benchmark_advanced
    PRIVATE
        benchmark::benchmark
        core_lib
        proto_gen
)

# 包含头文件
target_include_directories(performance_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/common
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/client/include
    ${CMAKE_SOURCE_DIR}/src/network
    ${CMAKE_BINARY_DIR}
)

# 配置其他基准测试的头文件包含
target_include_directories(benchmark_config_manager PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
)

target_include_directories(benchmark_player_registry PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_BINARY_DIR}
)

target_include_directories(benchmark_network_simple PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
)

target_include_directories(benchmark_advanced PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_BINARY_DIR}
)

# 设置C++标准
set_target_properties(performance_tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# 设置其他基准测试的C++标准
set_target_properties(
    benchmark_config_manager
    benchmark_player_registry  
    benchmark_network_simple
    benchmark_advanced
    PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# 添加性能测试目标
add_custom_target(benchmark
    COMMAND performance_tests --benchmark_out=benchmark_results.json --benchmark_out_format=json
    DEPENDS performance_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running performance benchmarks..."
)

# Simple benchmark executable - core functionality tests
add_executable(simple_benchmark simple_benchmark.cpp)

# Include directories for simple benchmark
target_include_directories(simple_benchmark PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_BINARY_DIR}
)

target_link_libraries(simple_benchmark PRIVATE
    benchmark::benchmark
    core_lib
    proto_gen
)

# 更多基准测试目标（待实现）
# add_executable(benchmark_client_throughput benchmark_client_throughput.cpp)
# add_executable(benchmark_memory_usage benchmark_memory_usage.cpp)  
# add_executable(benchmark_concurrent_clients benchmark_concurrent_clients.cpp)

# Set C++ standard for the simple benchmark
set_target_properties(
    simple_benchmark
    performance_tests
    PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# 添加性能报告生成目标
add_custom_target(benchmark_report
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/generate_performance_report.py
    DEPENDS simple_benchmark
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Generating performance report..."
)

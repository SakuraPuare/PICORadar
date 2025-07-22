cmake_minimum_required(VERSION 3.20)

project(PicoRadar VERSION 1.0.0 LANGUAGES CXX)

option(USE_VALGRIND "Enable Valgrind for memory checking" OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENABLE_COVERAGE "Enable code coverage" OFF)

if(ENABLE_COVERAGE)
    if(CMAKE_COMPILER_IS_GNUCXX)
        message(STATUS "Coverage enabled for GCC/Clang")
        add_compile_options(--coverage)
        add_link_options(--coverage)
    else()
        message(WARNING "Code coverage is only supported for GCC/Clang")
    endif()
endif()

# 生成 compile_commands.json 文件，供 linter 使用
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ==============================================================================
# VCPKG 集成
# ==============================================================================
# 如果vcpkg目录存在，则设置工具链文件
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg")
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

# ==============================================================================
# 项目选项
# ==============================================================================
option(PICORADAR_BUILD_TESTS "构建测试" ON)
option(PICORADAR_BUILD_SERVER "构建服务端应用" ON)
option(PICORADAR_BUILD_CLIENT_LIB "构建客户端库" ON)
option(PICORADAR_BUILD_MOCK_CLIENT "构建模拟客户端" ON)
option(ENABLE_COVERAGE "启用代码覆盖率" OFF)

# ==============================================================================
# 覆盖率配置
# ==============================================================================
if(ENABLE_COVERAGE)
    # 设置覆盖率编译标志
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage")
    
    # 设置覆盖率链接标志
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} --coverage")
    
    # 查找lcov工具
    find_program(LCOV_PATH lcov)
    if(LCOV_PATH)
        message(STATUS "找到 lcov: ${LCOV_PATH}")
    else()
        message(WARNING "未找到 lcov，覆盖率报告生成可能受限")
    endif()
    
    # 查找genhtml工具
    find_program(GENHTML_PATH genhtml)
    if(GENHTML_PATH)
        message(STATUS "找到 genhtml: ${GENHTML_PATH}")
    else()
        message(WARNING "未找到 genhtml，HTML覆盖率报告生成可能受限")
    endif()
    
    message(STATUS "覆盖率功能已启用")
endif()

# ==============================================================================
# 查找依赖 (通过 vcpkg)
# ==============================================================================
find_package(Protobuf CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(glog CONFIG REQUIRED)
find_package(Boost CONFIG REQUIRED COMPONENTS system thread beast) # Beast是头文件库，但依赖system

# ==============================================================================
# Central Include Directories Management
# ==============================================================================
# 创建一个接口库来管理整个项目的包含目录。
# 这样所有目标都可以通过链接到它来获得正确的 #include 路径。
add_library(project_includes INTERFACE)
target_include_directories(project_includes INTERFACE
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/test"
    # Protobuf 的包含目录由 proto_gen 目标自动传递，无需在此添加
)

# ==============================================================================
# Protobuf 代码生成 (Modern CMake)
# ==============================================================================
# 使用现代 CMake 函数 protobuf_generate
# 1. 创建一个库目标，并将 .proto 文件作为其“源”文件
add_library(proto_gen STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/proto/player_data.proto"
)

# 2. 调用 protobuf_generate 将生成的 .pb.cc 和 .pb.h 添加到 proto_gen 目标
protobuf_generate(
    TARGET proto_gen
    # 指定 .proto 文件的根目录，以便 import 正常工作
    IMPORT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/proto"
)

# 3. 将 protobuf 运行时库链接到我们的生成库
target_link_libraries(proto_gen PUBLIC protobuf::libprotobuf)

# 4. 将二进制目录（生成代码的位置）添加到包含路径中
target_include_directories(proto_gen PUBLIC
    "${CMAKE_CURRENT_BINARY_DIR}"
)



# ==============================================================================
# Add Subdirectories
# ==============================================================================
add_subdirectory(src/common)
add_subdirectory(src/core)
add_subdirectory(src/network)
add_subdirectory(src/server_app)

if(PICORADAR_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

# ==============================================================================
# 输出构建信息
# ==============================================================================
message(STATUS "========================================")
message(STATUS "PicoRadar 项目配置")
message(STATUS "  - C++ 标准: ${CMAKE_CXX_STANDARD}")
message(STATUS "  - Protobuf 库: ${Protobuf_LIBRARIES}")
message(STATUS "  - GTest 库: ${GTest_LIBRARIES}")
message(STATUS "  - glog 库: ${glog_LIBRARIES}")
message(STATUS "  - Boost 库: ${Boost_LIBRARIES}")
message(STATUS "  - 构建服务端: ${PICORADAR_BUILD_SERVER}")
message(STATUS "  - 构建客户端库: ${PICORADAR_BUILD_CLIENT_LIB}")
message(STATUS "  - 构建模拟客户端: ${PICORADAR_BUILD_MOCK_CLIENT}")
message(STATUS "  - 构建测试: ${PICORADAR_BUILD_TESTS}")
message(STATUS "  - 覆盖率功能: ${ENABLE_COVERAGE}")
if(ENABLE_COVERAGE)
    message(STATUS "  - lcov: ${LCOV_PATH}")
    message(STATUS "  - genhtml: ${GENHTML_PATH}")
endif()
message(STATUS "========================================")
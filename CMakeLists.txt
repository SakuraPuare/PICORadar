cmake_minimum_required(VERSION 3.16)

project(PicoRadar VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

# ==============================================================================
# 查找依赖 (通过 vcpkg)
# ==============================================================================
find_package(Protobuf CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(glog CONFIG REQUIRED)
find_package(Boost CONFIG REQUIRED COMPONENTS system thread) # Beast是头文件库，但依赖system

# ==============================================================================
# Protobuf 代码生成
# ==============================================================================
set(PROTO_FILES "${CMAKE_CURRENT_SOURCE_DIR}/proto/player_data.proto")
set(PROTO_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

# 确保生成目录存在
file(MAKE_DIRECTORY "${PROTO_GEN_DIR}")

# 创建一个目标来执行protoc编译器
add_custom_command(
    OUTPUT "${PROTO_GEN_DIR}/player_data.pb.cc" "${PROTO_GEN_DIR}/player_data.pb.h"
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
        --cpp_out="${PROTO_GEN_DIR}"
        --proto_path="${CMAKE_CURRENT_SOURCE_DIR}/proto"
        ${PROTO_FILES}
    DEPENDS ${PROTO_FILES}
    COMMENT "Compiling proto files: ${PROTO_FILES}"
)

# 创建一个自定义目标来封装这个命令
add_custom_target(generate_proto ALL DEPENDS "${PROTO_GEN_DIR}/player_data.pb.cc" "${PROTO_GEN_DIR}/player_data.pb.h")

# 创建一个库来表示生成的代码
add_library(proto_gen STATIC "${PROTO_GEN_DIR}/player_data.pb.cc" "${PROTO_GEN_DIR}/player_data.pb.h")
target_include_directories(proto_gen PUBLIC "${PROTO_GEN_DIR}")
target_link_libraries(proto_gen PUBLIC protobuf::libprotobuf)
add_dependencies(proto_gen generate_proto)


# ==============================================================================
# 添加子目录
# ==============================================================================
add_subdirectory(src)

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
message(STATUS "========================================")
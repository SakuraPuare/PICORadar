# src/CMakeLists.txt

# 核心逻辑库 (始终需要)
add_subdirectory(core)

# 网络库
add_subdirectory(network)

# 公共代码库 (待办)
# add_subdirectory(common)

if(PICORADAR_BUILD_SERVER)
    add_subdirectory(server_app)
endif()

if(PICORADAR_BUILD_CLIENT_LIB)
    # add_subdirectory(client_lib)
endif()

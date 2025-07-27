# Custom triplet for cross-compiling to Windows x64 from Linux using MinGW-w64
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

# Specify that this is a cross-compilation to Windows
set(VCPKG_CMAKE_SYSTEM_NAME Windows)

# Use our custom MinGW-w64 toolchain
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE ${CMAKE_CURRENT_LIST_DIR}/../toolchains/mingw-w64.cmake)

# Environment setup for MinGW-w64
set(VCPKG_ENV_PASSTHROUGH_UNTRACKED CC CXX AR RANLIB)

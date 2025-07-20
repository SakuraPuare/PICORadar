#!/bin/bash

# 更新编译数据库脚本
# 用于重新生成 compile_commands.json 文件

set -e

echo "正在更新编译数据库..."

# 确保在项目根目录
if [ ! -f "CMakeLists.txt" ]; then
    echo "错误：请在项目根目录运行此脚本"
    exit 1
fi

# 重新配置CMake
echo "重新配置CMake..."
cd build
cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# 生成protobuf文件
echo "生成protobuf文件..."
make generate_proto

# 更新符号链接
cd ..
if [ -L "compile_commands.json" ]; then
    rm compile_commands.json
fi
ln -sf build/compile_commands.json .

echo "编译数据库更新完成！"
echo "现在linter应该能够正确识别所有头文件了。" 
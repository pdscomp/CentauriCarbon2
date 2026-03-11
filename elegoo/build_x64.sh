###############################################################################
 # @Author       : huangpeiyun
 # @Date         : 2025-06-07 12:17:44
 # @LastEditors  : huangpeiyun
 # @LastEditTime : 2025-06-07 12:18:48
 # @Description  : use x64 toolchain to build elegoo
 # 
 # Copyright (c) 2025 by ELEGOO, All Rights Reserved. 
###############################################################################

#!/bin/bash
# 创建构建目录
BUILD_DIR=build_x64
if [ ! -d "$BUILD_DIR" ]; then
    mkdir -p $BUILD_DIR
fi
cd $BUILD_DIR

# 执行 cmake 配置
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=../install_x64

# 执行 make 进行编译
make -j$(nproc)
# 检查编译是否成功
if [ $? -eq 0 ]; then
    echo "Build succeeded!"
else
    echo "Build failed."
    exit 1
fi
# 执行 make install 进行安装
make install
# 检查安装是否成功
if [ $? -eq 0 ]; then
    echo "Installation succeeded!"
else
    echo "Installation failed."
    exit 1
fi
# 返回上级目录
cd ..
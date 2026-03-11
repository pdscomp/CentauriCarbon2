#!/bin/bash
###############################################################################
 # @Author       : huangpeiyun
 # @Date         : 2025-06-03 15:49:43
 # @LastEditors  : huangpeiyun
 # @LastEditTime : 2025-06-07 12:18:03
 # @Description  : to simple build elegoo for arm platform
 # 
 # Copyright (c) 2025 by ELEGOO, All Rights Reserved. 
###############################################################################

# 创建构建目录
BUILD_DIR=build
if [ ! -d "$BUILD_DIR" ]; then
    mkdir -p $BUILD_DIR
fi
cd $BUILD_DIR

# 执行 cmake 配置
export CC=$PWD/../../scripts/arm/toolchain-sunxi-glibc/toolchain/bin/arm-openwrt-linux-gnueabi-gcc
export CXX=$PWD/../../scripts/arm/toolchain-sunxi-glibc/toolchain/bin/arm-openwrt-linux-gnueabi-g++

cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=$CC \
    -DCMAKE_CXX_COMPILER=$CXX \
    -DCMAKE_INSTALL_PREFIX=../install 

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
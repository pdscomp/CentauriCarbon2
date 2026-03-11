#!/bin/bash
#/*****************************************************************************
# * @Author       : loping
# * @Date         : 2025-02-18 11:38:51
# * @LastEditors  : loping
# * @LastEditTime : 2025-07-29 15:24:04
# * @Description  :
# *
# * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
# *****************************************************************************/


. ./elegoo-build.sh

OUTPUT_DIR=`pwd`/output
SOURCE_DIR=`pwd`/source
CONTROL_DIR=$SOURCE_DIR/control
DATA_DIR=$SOURCE_DIR/data

mkdir -p $OUTPUT_DIR
mkdir -p $SOURCE_DIR
mkdir -p $CONTROL_DIR
mkdir -p $DATA_DIR

#找到对应的文件夹
if [ -z $1 ];then
    echo "please input package name:$1"
    exit 1
else
    TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
    TARGET_IPK=$1_$TIMESTAMP.ipk
    echo "====================================="
    echo "package_name: $TARGET_IPK"
    echo "package_source: $SOURCE_DIR"
    echo "package_output: $OUTPUT_DIR"
    echo "====================================="
fi

# 清空 OUTPUT_DIR目录
rm -rf $OUTPUT_DIR/*

ver=$(date +"%m.%d.%H.%M")
sed -i "s|Version: .*|Version: ${ver}|" $CONTROL_DIR/control

# #压缩CONTROL目录
CONTROL_TAR=control.tar.gz
cd $CONTROL_DIR
tar -zcvf $CONTROL_TAR *
mv $CONTROL_TAR $OUTPUT_DIR
cd -

# #压缩data目录
DATA_TAR=data.tar.gz
cd $DATA_DIR
tar -zcvf $DATA_TAR *
mv $DATA_TAR $OUTPUT_DIR
cd -

echo 2.0 > $OUTPUT_DIR/debian-binary
#压缩全部tar.gz生成tar.gz,并重命名成ipk
cd $OUTPUT_DIR
# arm-openwrt-linux-ar -rv $OUTPUT_DIR/$TARGET_IPK $CONTROL_TAR $DATA_TAR debian-binary
tar -zcvf $OUTPUT_DIR/$TARGET_IPK $CONTROL_TAR $DATA_TAR debian-binary
rm -rf $OUTPUT_DIR/*.tar.gz
rm -rf $OUTPUT_DIR/debian-binary
cp ./../update_ipk.sh $OUTPUT_DIR -rf
# cp ./../install_elegoo.sh $OUTPUT_DIR -rf
chmod +x $OUTPUT_DIR/$TARGET_IPK
chmod +x $OUTPUT_DIR/update_ipk.sh
# chmod +x $OUTPUT_DIR/install_elegoo.sh
cd -


echo "build $OUTPUT_DIR/$TARGET_IPK ok!"

#!/bin/bash
#/*****************************************************************************
# * @Author       : loping
# * @Date         : 2025-02-18 11:38:51
# * @LastEditors  : loping
# * @LastEditTime : 2025-05-22 12:07:55
# * @Description  :
# *
# * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
# *****************************************************************************/
#注意:ipkg-build是在elegoo同级目录下

if [ -n "$2" ];then
    echo "cur param2 is $2!"
    mcu_selection=$2
else
    echo "cur param2 is nul"
    echo "please input mcu selecton: gd st,or gd is the default mcu selecton!"
    mcu_selection=gd
fi



IPKG_TOP_DIR=.

INSTALL_DIR=${IPKG_TOP_DIR}/source/data
rm ${INSTALL_DIR}/* -rf
mkdir -p ${INSTALL_DIR}/bin
mkdir -p ${INSTALL_DIR}/lib
mkdir -p ${INSTALL_DIR}/inst
# mkdir -p ${INSTALL_DIR}/etc
# mkdir -p ${INSTALL_DIR}/config
mkdir -p ${INSTALL_DIR}/inst/firmware_version

#拷贝build目录下文件
#lib
find ${IPKG_TOP_DIR}/../elegoo/build -name "*.so*" | xargs -I {} cp {} ${INSTALL_DIR}/lib -rf
#bin
# cp ${IPKG_TOP_DIR}/install_elegoo.sh ${INSTALL_DIR}/bin -rf
# 增加DSP启动脚本
if [[ "$mcu_selection" == "dsp" ]]; then
    cp ${IPKG_TOP_DIR}/run_printer_dsp.sh ${INSTALL_DIR}/bin/run_printer.sh -rvf
else
    cp ${IPKG_TOP_DIR}/run_printer.sh ${INSTALL_DIR}/bin/run_printer.sh -rvf
fi

cp ${IPKG_TOP_DIR}/update_ipk.sh ${INSTALL_DIR}/bin -rf
cp ${IPKG_TOP_DIR}/../elegoo/build/elegoo_printer ${INSTALL_DIR}/bin -rf
cp ${IPKG_TOP_DIR}/../elegoo/build/debug_window ${INSTALL_DIR}/bin -rf

#config
cp ${IPKG_TOP_DIR}/../debugfile/* ${INSTALL_DIR}/inst -rf
#daemon-000
mkdir -p ${INSTALL_DIR}/inst/daemon-000
cp ${IPKG_TOP_DIR}/../daemon-000/* ${INSTALL_DIR}/inst/daemon-000/ -rvf
if [[ "$mcu_selection" == "dsp" ]]; then
    cp ${IPKG_TOP_DIR}/../daemon-000/daemon-000_C ${INSTALL_DIR}/inst/daemon-000/daemon-000 -rvf
else
    cp ${IPKG_TOP_DIR}/../daemon-000/daemon-000 ${INSTALL_DIR}/inst/daemon-000/daemon-000 -rvf
fi
rm ${INSTALL_DIR}/inst/daemon-000/daemon-000_C -rvf


# aicamera
# elegoo/build/extras/ai_camera/ai_camera    -->  /opt/bin/
# elegoo/lib/libhv/lib/arm/libhv.so   --> /opt/lib/
# elegoo/lib/ffmpeg/ffmpeg --> /opt/bin/
# elegoo/lib/ffmpeg/ffprobe --> /opt/bin/
# elegoo/lib/ffmpeg/lib/libx264.so.165 --> /opt/lib/
cp ${IPKG_TOP_DIR}/../elegoo/build/extras/ai_camera/ai_camera ${INSTALL_DIR}/bin -f
cp ${IPKG_TOP_DIR}/../elegoo/lib/ffmpeg/ffmpeg ${INSTALL_DIR}/bin -f
cp ${IPKG_TOP_DIR}/../elegoo/lib/ffmpeg/ffprobe ${INSTALL_DIR}/bin -f
cp ${IPKG_TOP_DIR}/../elegoo/lib/libhv/lib/arm/libhv.so ${INSTALL_DIR}/lib -f
cp ${IPKG_TOP_DIR}/../elegoo/lib/ffmpeg/lib/* ${INSTALL_DIR}/lib -f
cp ${IPKG_TOP_DIR}/../elegoo/lib/libjpeg/lib/* ${INSTALL_DIR}/lib -f
# 增加可执行权限
cd ${INSTALL_DIR}/bin
chmod +x run_printer.sh
chmod +x update_ipk.sh
chmod +x elegoo_printer
chmod +x debug_window
chmod +x ai_camera
chmod +x ffmpeg
chmod +x ffprobe
cd -
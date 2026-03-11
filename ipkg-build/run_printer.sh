#! /bin/sh

#/*****************************************************************************
# * @Author       : loping
# * @Date         : 2025-02-18 11:38:51
# * @LastEditors  : loping
# * @LastEditTime : 2025-06-11 11:23:15
# * @Description  :
# *
# * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
# *****************************************************************************/

DEF_CFG_DIR=/opt/inst
USR_CFG_DIR=/opt/usr/cfg
CFG_FILE=printer_dsp.cfg
PRODUCT_TEST_CFG_FILE=product_test.cfg
AUTO_SAVE_CFG_FILE=autosave.cfg
UDS_FILE=/tmp/elegoo_uds

mkdir -p $DEF_CFG_DIR
mkdir -p $USR_CFG_DIR

start(){
    # /etc/init.d/ntpd start
    # hwclock -r
    # hwclock -w
    # sleep 1
    # date
    # if [ -e "$USR_CFG_DIR/user_$CFG_FILE" ]; then
    #     echo "执行用户自定义配置"
    #     elegoo_printer $USR_CFG_DIR/user_$CFG_FILE -a $UDS_FILE &
    # else
    #     echo "用户自定义配置不存在，拷贝默认配置为自定义配置"
    #     cp $DEF_CFG_DIR/$CFG_FILE $USR_CFG_DIR/user_$CFG_FILE -rf
    #     elegoo_printer $USR_CFG_DIR/user_$CFG_FILE -a $UDS_FILE &
    # fi
    LD_BIND_NOW=1 elegoo_printer $DEF_CFG_DIR/$CFG_FILE -s $USR_CFG_DIR/$AUTO_SAVE_CFG_FILE -a $UDS_FILE&
}

start_product_test(){
    # /etc/init.d/ntpd start
    # hwclock -r
    # hwclock -w
    # sleep 1
    # date
    # if [ -e "$USR_CFG_DIR/user_$CFG_FILE" ]; then
    #     echo "执行用户自定义配置"
    #     elegoo_printer $USR_CFG_DIR/user_$CFG_FILE -a $UDS_FILE &
    # else
    #     echo "用户自定义配置不存在，拷贝默认配置为自定义配置"
    #     cp $DEF_CFG_DIR/$CFG_FILE $USR_CFG_DIR/user_$CFG_FILE -rf
    #     elegoo_printer $USR_CFG_DIR/user_$CFG_FILE -a $UDS_FILE &
    # fi
    exec elegoo_printer $DEF_CFG_DIR/$PRODUCT_TEST_CFG_FILE -s $USR_CFG_DIR/$AUTO_SAVE_CFG_FILE -a $UDS_FILE
}

stop(){
    killall elegoo_printer
}

if [ "$1" = "start" ]; then
    echo "start elegoo_printer!"
    start
elif [ "$1" = "product_test" ]; then
    echo "start product_test elegoo_printer!"
    start_product_test
elif [ "$1" = "stop" ]; then
    echo "stop elegoo_printer!"
    stop
else
    echo "error! unknown action!"
fi

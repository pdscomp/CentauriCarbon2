#! /bin/sh

#/*****************************************************************************
# * @Author       : loping
# * @Date         : 2025-03-21 11:38:51
# * @LastEditors  : loping
# * @LastEditTime : 2025-07-31 15:17:56
# * @Description  :
# *
# * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
# *****************************************************************************/

# # 需要 root 权限
# su

source /etc/profile

# 检查 /etc/opkg.conf 文件中是否存在 "dest opt /opt"
if grep -q "dest opt /opt" /etc/opkg.conf; then
    echo "The 'dest opt /opt' string already exists in the opkg.conf file"
else
    echo "The 'dest opt /opt' string does not exist in the opkg.conf file. Add the string to the file!"
    echo "dest opt /opt" >> /etc/opkg.conf
fi

echo "#"
echo "#"
echo "#"

# 使用字符串匹配检查$PATH是否包含/opt/bin
if echo "$PATH" | grep -q ":/opt/bin:"; then
    echo "The ':/opt/bin:' string already exists in the $PATH"
else
    echo "The ':/opt/bin:' string does not exist in the $PATH, export the string to PATH"
    echo "export PATH=:/opt/bin:\$PATH" >> /etc/profile
fi

# 使用字符串匹配检查$PATH是否包含/opt/bin
if echo "$LD_LIBRARY_PATH" | grep -q ":/opt/lib:"; then
    echo "The ':/opt/lib:' string already exists in the $LD_LIBRARY_PATH"
else
    echo "The ':/opt/lib:' string does not exist in the $LD_LIBRARY_PATH, export the string to LD_LIBRARY_PATH"
    echo "export LD_LIBRARY_PATH=:/opt/lib:\$LD_LIBRARY_PATH" >> /etc/profile
fi

source /etc/profile

echo "#"
echo $PATH
echo $LD_LIBRARY_PATH

# if [ -e "/mnt/exUDISK/tina-r528-evb1-ab_250410_wifi4.swu" ]; then
#     boot_partition=$(fw_printenv -n boot_partition)
#     echo "boot_partition $boot_partition" >> /dev/ttyS2 2>&1
#     if [ $boot_partition == "bootA" ]; then
#         swupdate -i /mnt/exUDISK/tina-r528-evb1-ab_250410_wifi4.swu -e stable,now_A_next_B
#     elif [ $boot_partition == "bootB" ]; then
#         swupdate -i /mnt/exUDISK/tina-r528-evb1-ab_250410_wifi4.swu -e stable,now_B_next_A
#     else
#         echo "boot"
#     fi

#     rm /mnt/exUDISK/tina-r528-evb1-ab_250410_wifi4.swu
#     sync
#     exit 0
# fi

INST_DIR=/opt/inst
mkdir -p $INST_DIR
mkdir -p $INST_DIR/udisk
mkdir -p $INST_DIR/ota
mkdir -p $INST_DIR/firmware_version

CCPRO_EEB001=ccpro_eeb001
# TINA_R528=tina-r528-evb1-ab
TINA_R528=ec_eeb001
IMAGE_NAME=$TINA_R528
GUI_NAME=eeb001-gui
FIRMWARE_NAME=elegoo
INIT_FIRMWARE_NAME=elegoo_printer
IPK_PKG_NAME=update_pkg
IPK_PKG_FILE=$IPK_PKG_NAME.ipk
FIRMWARE_VERSION_DIR=$INST_DIR/firmware_version
UPAN_UPDATE=/mnt/exUDISK
UPDATE_START=$INST_DIR/update_start

case $1 in
    upan)
        echo "update upan ..."
        IPK_INST_DIR=$INST_DIR/udisk
        UPDATE_IPK=$IPK_INST_DIR/$FIRMWARE_NAME.ipk
        if [ -e "$UPAN_UPDATE/$FIRMWARE_NAME.ipk" ]; then
            cp $UPAN_UPDATE/$FIRMWARE_NAME.ipk $IPK_INST_DIR -f
        fi
        ;;
    ota)
        echo "update ota ..."
        IPK_INST_DIR=$INST_DIR/ota
        UPDATE_IPK=$IPK_INST_DIR/$FIRMWARE_NAME.ipk
        echo "ls $IPK_INST_DIR"
        ls $IPK_INST_DIR
        echo "$UPDATE_IPK"
        if [ -e "$INST_DIR/ota_$FIRMWARE_NAME.ipk" ]; then
            echo "exist $INST_DIR/ota_$FIRMWARE_NAME.ipk"
            mv $INST_DIR/ota_$FIRMWARE_NAME.ipk $IPK_INST_DIR/$FIRMWARE_NAME.ipk -f
            echo "ls $INST_DIR"
            ls $INST_DIR
        fi
        ;;
    *)
        echo "error! param \$1 must is upan or ota!"
        exit 1
        ;;
esac

FORCE_REINSTALL=
if [ -n "$2" ]; then
    echo "param \$2 is $2"
    case "$2" in
        *$INIT_FIRMWARE_NAME*.ipk)
            if [ -e "$2" ]; then
                BASE_NAME=$(basename $2)
                echo "param2 \$2 base file name is $BASE_NAME"
                cp $2 $IPK_INST_DIR/$INIT_FIRMWARE_NAME.ipk -rf
                UPDATE_IPK=$IPK_INST_DIR/$INIT_FIRMWARE_NAME.ipk
                echo "cur is $UPDATE_IPK"
            fi
            ;;
        *$FIRMWARE_NAME*.ipk)
            if [ -e "$2" ]; then
                BASE_NAME=$(basename $2)
                echo "param2 \$2 base file name is $BASE_NAME"
                cp $2 $IPK_INST_DIR/$FIRMWARE_NAME.ipk -rf
                UPDATE_IPK=$IPK_INST_DIR/$FIRMWARE_NAME.ipk
                echo "cur is $UPDATE_IPK"
            fi
            ;;
        *$GUI_NAME*.ipk)
            if [ -e "$2" ]; then
                BASE_NAME=$(basename $2)
                echo "param2 \$2 base file name is $BASE_NAME"
                cp $2 $IPK_INST_DIR/$GUI_NAME.ipk -rf
                UPDATE_IPK=$IPK_INST_DIR/$GUI_NAME.ipk
                echo "cur is $UPDATE_IPK"
            fi
            ;;
        *$IMAGE_NAME*.swu)
            if [ -e "$2" ]; then
                BASE_NAME=$(basename $2)
                echo "param2 \$2 base file name is $BASE_NAME"
                cp $2 $IPK_INST_DIR/$IMAGE_NAME.swu -rf
                echo "cur is $IPK_INST_DIR/$IMAGE_NAME.swu"
                boot_partition=$(fw_printenv -n boot_partition)
                echo "boot_partition is $boot_partition"
                if [ $boot_partition == "bootA" ]; then
                    swupdate -i $IPK_INST_DIR/$IMAGE_NAME.swu -e stable,now_A_next_B
                elif [ $boot_partition == "bootB" ]; then
                    swupdate -i $IPK_INST_DIR/$IMAGE_NAME.swu -e stable,now_B_next_A
                else
                    echo "boot_partition is not normal!"
                fi
                sync
                exit 0
            fi
            ;;
        *$CCPRO_EEB001*.swu)
            if [ -e "$2" ]; then
                BASE_NAME=$(basename $2)
                echo "param2 \$2 base file name is $BASE_NAME"
                cp $2 $IPK_INST_DIR/$CCPRO_EEB001.swu -rf
                echo "cur is $IPK_INST_DIR/$CCPRO_EEB001.swu"
                boot_partition=$(fw_printenv -n boot_partition)
                echo "boot_partition is $boot_partition"
                if [ $boot_partition == "bootA" ]; then
                    swupdate -i $IPK_INST_DIR/$CCPRO_EEB001.swu -e stable,now_A_next_B
                elif [ $boot_partition == "bootB" ]; then
                    swupdate -i $IPK_INST_DIR/$CCPRO_EEB001.swu -e stable,now_B_next_A
                else
                    echo "boot_partition is not normal!"
                fi
                sync
                exit 0
            fi
            ;;
        *$EEB001_NAME*.zip.sig)
            if [ -e "$2" ]; then
                BASE_NAME=$(basename $2)
                echo "param2 \$2 base file name is $BASE_NAME"
                if echo "$BASE_NAME" | grep -q '\.sig$'; then
                    echo "the $BASE_NAME is a sig file"
                    # 执行解签名流程
                else
                    echo "the $BASE_NAME is not a sig file"
                fi
                # RE_NAME="${BASE_NAME%.zip*}"
                # cp $2 $IPK_INST_DIR/$RE_NAME.zip -rf
                # unzip $IPK_INST_DIR/$RE_NAME.zip
                exit 0
            fi
            ;;
        -f)
            FORCE_REINSTALL='--force-reinstall --force-downgrade'

            ;;
        -o)
            FORCE_REINSTALL='--force-overwrite'

            ;;
    esac
fi

if [ -n "$3" ]; then
    echo "param \$3 is $3"
    case "$3" in
        -f)
            FORCE_REINSTALL='--force-reinstall --force-downgrade'

            ;;
        -o)
            FORCE_REINSTALL='--force-overwrite'

            ;;
    esac
fi

if [ -e "$UPDATE_IPK" ]; then
    if [ -e "$UPDATE_START" ]; then
        echo "update failed!recovery..."
        BASE_NAME=$(basename $UPDATE_IPK)
        echo "param2 \$UPDATE_IPK base file name is $BASE_NAME"
        if [ -e "$FIRMWARE_VERSION_DIR/$BASE_NAME" ]; then
            cp $FIRMWARE_VERSION_DIR/$BASE_NAME $IPK_INST_DIR -f
        else
            echo "error!$FIRMWARE_VERSION_DIR/$BASE_NAME is not exist!"
            rm $UPDATE_START
            exit 1
        fi
        opkg install $UPDATE_IPK -d opt
        if [ $? -eq 0 ]; then
            echo "recovery success!"
        else
            echo "recovery failed!opkg install error!"
            rm $UPDATE_START
            exit 1
        fi
        rm $UPDATE_START
    else
        echo "$UPDATE_IPK update ..."
        touch $UPDATE_START
        echo "FORCE_REINSTALL=$FORCE_REINSTALL"
        opkg install $UPDATE_IPK -d opt $FORCE_REINSTALL
        if [ $? -eq 0 ]; then
            echo "update success!"
            if [ -e "/opt/usr/lib/opkg/info/$INIT_FIRMWARE_NAME.control" ];then
                FIRMWARE_VER=$(cat /opt/usr/lib/opkg/info/$INIT_FIRMWARE_NAME.control | grep 'Version' | awk '{print $2}')
                echo "cur $FIRMWARE_NAME version is $FIRMWARE_VER"
            fi
            if [ -e "/usr/lib/opkg/info/$GUI_NAME.control" ];then
                GUI_VER=$(cat /usr/lib/opkg/info/$GUI_NAME.control | grep 'Version' | awk '{print $2}')
                echo "cur $GUI_NAME version is $GUI_VER"
            fi
            cp $UPDATE_IPK $FIRMWARE_VERSION_DIR -rf
            rm $UPDATE_START
            exit 0
        else
            echo "opkg install error!"
            exit 1
        fi
    fi
else
    echo "$UPDATE_IPK is not exist!"
    exit 1
fi

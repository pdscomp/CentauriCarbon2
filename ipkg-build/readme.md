<!--
#/***************************************************************************** 
# * @Author       : loping
# * @Date         : 2025-02-18 11:38:51
# * @LastEditors  : loping
# * @LastEditTime : 2025-02-19 11:38:51
# * @Description  :  
# * 
# * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
# *****************************************************************************/
 -->

#### ipk打包方法

##### SOURCE目录结构体
      
      --control;控制指令目录，最后压缩成control.tar.gz
        -control :记录包的信息
        -preinst :在包安装前，执行脚本
        -postinst：真实安装，在包解压后执行的脚本
        -prerm：预卸载，在包删除前执行的脚本
        -postrm：卸载后，需要执行的脚本
      --data;存放更新的文件目录
      --etc;需要拷贝etc目录下的脚本
      
      
      control目录、data目录、etc目录下的文件全部都要进入目录中使用(tar -zcvf xx.tar.gz *)进行打包
      在ipk目录下生成：control.tar.gz
                      data.tar.gz
                      etc.tar.gz

      echo 2.0 > debian-binary

      最终：
        进入ipk目录(ar -rv 目标.ipk *) 生成 ipk文件


#####开发板上opkg常用指令

    opkg install xxx.ipk 安装包xxx
    opkg install xxx.ipk -o /opt/packages/  把包中的文件安装到目录 /opt/packages/

    opkg list -o /opt/package/ 查看已经安装的包列表
    opkg remove -o /opt/package/ 从指定目录中卸载包
    

    
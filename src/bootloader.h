/*****************************************************************************
 * @Author       : Coconut
 * @Date         : 2025-03-22 11:23:58
 * @LastEditors  : coconut
 * @LastEditTime : 2025-03-22 11:37:56
 * @Description  : 
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/
#ifndef __BOOTLOADER_H
#define __BOOTLOADER_H
#include <stdint.h> // uint8_t
#include "autoconf.h"

#define BOOTLOADER_SIZE CONFIG_BOOTLOADER_SIZE
#define APP_START_ADDRESS (CONFIG_FLASH_BOOT_ADDRESS + BOOTLOADER_SIZE)

#endif // bootloader.h

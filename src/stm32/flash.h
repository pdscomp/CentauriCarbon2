/*****************************************************************************
 * @Author       : Coconut
 * @Date         : 2025-03-22 11:23:58
 * @LastEditors  : coconut
 * @LastEditTime : 2025-03-22 16:52:51
 * @Description  : 实现FLASH编程以及BOOTLOADER跳转操作
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#ifndef __FLASH_H
#define __FLASH_H
#include <stdint.h> // uint8_t
#include "autoconf.h"

void jump_to_app(uint32_t address);

// STM32F401没有页的概念,只有扇区
#if CONFIG_MACH_GD32F303 || CONFIG_MACH_STM32F103
void flash_erase_pages(uint32_t start_address, uint16_t *page_size, uint16_t page_num);
uint32_t flash_get_page_size(uint32_t start_address);
#elif CONFIG_MACH_STM32F401
void flash_erase_sector(uint16_t sector_num);
uint32_t flash_get_sector_size(uint32_t sector);
#endif

void flash_word_program(uint32_t start_address, uint32_t *data, uint32_t count);
void flash_halfword_program(uint32_t start_address, uint16_t *data, uint32_t count);

#endif // flash.h

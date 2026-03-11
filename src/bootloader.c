/*****************************************************************************
 * @Author       : Coconut
 * @Date         : 2025-03-22 11:23:58
 * @LastEditors  : coconut
 * @LastEditTime : 2025-03-22 11:24:34
 * @Description  :
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/
#include "bootloader.h"
#include "bootloader_com.h"

#include "autoconf.h"
#include "sched.h"
#include "board/misc.h"
#include "board/flash.h"
#include "debug.h"

struct timer bootloader_main_timer;
static struct task_wake wake_bootloader_main;
uint8_t main_sta;
static uint_fast8_t
bootloader_main_event(struct timer *t);

enum
{
    BOOTLOADER_MAIN_STA_IDLE,
    BOOTLOADER_MAIN_STA_JUMP_TO_APP,
};

void bootloader_com_handle(uint8_t cmd, uint8_t *payload, uint32_t len)
{
    switch (cmd)
    {
    case BOOTLOADER_COM_CMD_ERASE:
    {
        bootloader_cmd_erase_req_t *req = (bootloader_cmd_erase_req_t *)payload;
        if (req->size > CONFIG_FLASH_SIZE - BOOTLOADER_SIZE)
            req->size = 0;

        if (main_sta == BOOTLOADER_MAIN_STA_JUMP_TO_APP)
        {
            main_sta = BOOTLOADER_MAIN_STA_IDLE;
            sched_del_timer(&bootloader_main_timer);
        }

        // 应用启动地址
        uint32_t app_start_address = CONFIG_FLASH_BOOT_ADDRESS + BOOTLOADER_SIZE;
        uint32_t start_address = app_start_address;
        uint32_t end_address = app_start_address + req->size;
#if CONFIG_MACH_GD32F303 || CONFIG_MACH_STM32F103
        while (start_address < end_address)
        {
            uint16_t page_size = flash_get_page_size(start_address);
            flash_erase_pages(start_address, &page_size, 1);
            start_address += page_size;
        }
#elif CONFIG_MACH_STM32F401
        int start_page = -1;
        int end_page = -1;
        uint32_t addr_start = CONFIG_FLASH_BOOT_ADDRESS;
        uint32_t addr_end = CONFIG_FLASH_BOOT_ADDRESS;
        for (int page = 0; page < 8; page++)
        {
            addr_end = addr_start + flash_get_sector_size(page);
            if (start_page < 0 && addr_start <= start_address && start_address < addr_end)
                start_page = page;
            if (end_page < 0 && addr_start <= end_address && end_address < addr_end)
                end_page = page;
            addr_start = addr_end;
        }
        for (int i = start_page; i <= end_page; i++)
        {
            start_address += flash_get_sector_size(i);
            flash_erase_sector(i);
        }
#endif
        // ACK
        bootloader_cmd_erase_ack_t ack = {.size = start_address - app_start_address};
        uint8_t ack_buf[BOOTLOADER_COM_MIN_PACKAGE_SIZE + sizeof(bootloader_cmd_erase_ack_t)];
        bootloader_package_data(ack_buf, sizeof(ack_buf), BOOTLOADER_COM_CMD_ERASE, (uint8_t *)&ack, sizeof(ack));
        bootloader_send_data(ack_buf, sizeof(ack_buf));
    }
    break;
    case BOOTLOADER_CMD_PROGRAM:
    {
        bootloader_cmd_program_req_t *req = (bootloader_cmd_program_req_t *)payload;
        uint8_t *data = payload + sizeof(bootloader_cmd_program_req_t);

        if (req->offset + req->length > req->size)
            req->length = 0;

        uint32_t app_start_address = CONFIG_FLASH_BOOT_ADDRESS + BOOTLOADER_SIZE;
        uint32_t start_address = app_start_address + req->offset;
        uint32_t end_address = app_start_address + req->offset + req->length;

        // 先按全字编程
        uint16_t word_count = (end_address - start_address) / 4;
        if (word_count > 0)
        {
            if (start_address == app_start_address)
                word_count = (end_address - start_address) / 4;
            flash_word_program(start_address, (uint32_t *)data, word_count);
            start_address += 4 * word_count;
            data += 4 * word_count;
        }
        // 处理非对齐部分
        if (start_address < end_address)
        {
            uint32_t word = *((uint32_t *)start_address);
            uint8_t byte_count = end_address - start_address;
            for (int i = 0; i < byte_count; i++)
            {
                word &= (~(0x000000ff << (3 - i))) | (*data) << 8 * i;
                data++;
            }
            flash_word_program(start_address, &word, 1);
            start_address += 4;
        }

        // ACK
        bootloader_cmd_program_ack_t ack = {.length = start_address - app_start_address - req->offset, .offset = start_address - app_start_address};
        uint8_t ack_buf[BOOTLOADER_COM_MIN_PACKAGE_SIZE + sizeof(bootloader_cmd_program_ack_t)];
        bootloader_package_data(ack_buf, sizeof(ack_buf), BOOTLOADER_CMD_PROGRAM, (uint8_t *)&ack, sizeof(ack));
        bootloader_send_data(ack_buf, sizeof(ack_buf));
    }
    break;
    case BOOTLOADER_CMD_JUMP:
    {
        // 唤醒定时器下次开启任务做跳转
        main_sta = BOOTLOADER_MAIN_STA_JUMP_TO_APP;
        bootloader_set_request(1, 0);
        sched_del_timer(&bootloader_main_timer);
        bootloader_main_timer.func = bootloader_main_event;
        bootloader_main_timer.waketime = timer_read_time() + timer_from_us(1000);
        sched_add_timer(&bootloader_main_timer);
    }
    break;
    case BOOTLOADER_CMD_PING:
    {
        bootloader_cmd_ping_req_t *req = (bootloader_cmd_ping_req_t *)payload;
        bootloader_cmd_ping_ack_t ack = {.pong = req->ping};

        if (main_sta == BOOTLOADER_MAIN_STA_JUMP_TO_APP)
        {
            sched_del_timer(&bootloader_main_timer);
            bootloader_main_timer.func = bootloader_main_event;
            bootloader_main_timer.waketime = timer_read_time() + timer_from_us(50000);
            sched_add_timer(&bootloader_main_timer);
        }

        uint8_t ack_buf[BOOTLOADER_COM_MIN_PACKAGE_SIZE + sizeof(bootloader_cmd_ping_ack_t)];
        bootloader_package_data(ack_buf, sizeof(ack_buf), BOOTLOADER_CMD_PING, (uint8_t *)&ack, sizeof(ack));
        bootloader_send_data(ack_buf, sizeof(ack_buf));
    }
    break;
    }
}

// 定时器仅负责唤醒任务,定时器运行于中断中不合适做跳转操作.
static uint_fast8_t
bootloader_main_event(struct timer *t)
{
    sched_wake_task(&wake_bootloader_main);
    return SF_DONE;
}

void bootloader_main_task(void)
{
    if (!sched_check_wake(&wake_bootloader_main))
        return;

    if (BOOTLOADER_MAIN_STA_JUMP_TO_APP == main_sta)
    {
        jump_to_app(APP_START_ADDRESS);
    }
}
DECL_TASK(bootloader_main_task);

void bootloader_init(void)
{
    // 设置通讯回调
    bootloader_set_handle(bootloader_com_handle);

    // 检查是否存在升级请求
    if (bootloader_get_request(0) == 0x454C)
    {
        main_sta = BOOTLOADER_MAIN_STA_IDLE;
        bootloader_set_request(0, 0);
        // bootloader_set_request(1, 0);
        return;
    }

    // 未存在请求尝试跳转至应用分区并标记跳转次数
    // uint8_t try = bootloader_get_request(1);
    // if (try < 3)
    // {
    //     bootloader_set_request(1, try + 1);
    // }
    // else
    // {
    //     // 多次跳转失败
    //     main_sta = BOOTLOADER_MAIN_STA_IDLE;
    //     return;
    // }

    // 设置跳转应用分区定时器任务
    main_sta = BOOTLOADER_MAIN_STA_JUMP_TO_APP;
    sched_del_timer(&bootloader_main_timer);
    bootloader_main_timer.func = bootloader_main_event;
    bootloader_main_timer.waketime = timer_read_time() + timer_from_us(200000);
    sched_add_timer(&bootloader_main_timer);
}

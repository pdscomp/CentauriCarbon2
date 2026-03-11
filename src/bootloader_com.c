/*****************************************************************************
 * @Author       : Coconut
 * @Date         : 2025-03-22 11:23:58
 * @LastEditors  : coconut
 * @LastEditTime : 2025-03-22 11:24:34
 * @Description  :
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/
#include "bootloader_com.h"
#include "sched.h"
#include "ringbuffer.h"
#include "debug.h"

// 串口通信环形缓冲区
DEFINE_RINGBUFFER(com_rb, 1024);
// 解析缓冲区与状态机
uint8_t com_parse_buf[128];
uint8_t com_parse_cmd;
uint8_t com_parse_sta;
uint16_t com_parse_count, com_parse_len, com_parse_crc;
// 通信载荷
uint8_t com_payload[BOOTLOADER_COM_MAX_PAYLOAD];
static struct task_wake wake_bootloader_com_handler;
// 回调函数
void (*com_handle)(uint8_t cmd, uint8_t *payload, uint32_t len);

uint16_t bootloader_crc16_ccitt(uint8_t *buf, uint_fast16_t len)
{
    uint16_t crc = 0xffff;
    while (len--)
    {
        uint8_t data = *buf++;
        data ^= crc & 0xff;
        data ^= data << 4;
        crc = ((((uint16_t)data << 8) | (crc >> 8)) ^ (uint8_t)(data >> 4) ^ ((uint16_t)data << 3));
    }
    return crc;
}

void bootloader_set_handle(void (*handle)(uint8_t cmd, uint8_t *payload, uint32_t len))
{
    com_handle = handle;
}

void bootloader_commit_data(uint8_t *buf, uint32_t len)
{
    ringbuffer_put(&com_rb, buf, len);
    if (ringbuffer_available_get_length(&com_rb) >= BOOTLOADER_COM_MIN_PACKAGE_SIZE || com_parse_sta != BOOTLOADER_COM_PARSE_STA_A5)
        sched_wake_task(&wake_bootloader_com_handler);
}

void bootloader_package_data(uint8_t *buf, uint32_t size, uint8_t cmd, uint8_t *payload, uint32_t len)
{
    if (size < BOOTLOADER_COM_MIN_PACKAGE_SIZE + len)
        return;
    uint8_t *pbuf = buf;
    uint16_t crc16 = bootloader_crc16_ccitt(payload, len);
    *pbuf++ = 0xa5;
    *pbuf++ = 0x5a;
    *pbuf++ = cmd;
    *pbuf++ = len >> 8;
    *pbuf++ = len & 0xff;
    memcpy(pbuf, payload, len);
    pbuf += len;
    *pbuf++ = crc16 >> 8;
    *pbuf++ = crc16 & 0xff;
}

void bootloader_com_handler_task(void)
{
    if (!sched_check_wake(&wake_bootloader_com_handler))
        return;
    do
    {
        // 拉取数据
        uint32_t len = ringbuffer_get(&com_rb, com_parse_buf, sizeof(com_parse_buf));

        // 解析数据
        uint32_t i = 0;

        while (i < len)
        {
            switch (com_parse_sta)
            {
            case BOOTLOADER_COM_PARSE_STA_A5:
                if (com_parse_buf[i] == 0xa5)
                    com_parse_sta = BOOTLOADER_COM_PARSE_STA_5A;
                else
                    com_parse_sta = BOOTLOADER_COM_PARSE_STA_A5;
                break;
            case BOOTLOADER_COM_PARSE_STA_5A:
                if (com_parse_buf[i] == 0x5a)
                    com_parse_sta = BOOTLOADER_COM_PARSE_STA_CMD;
                else
                    com_parse_sta = BOOTLOADER_COM_PARSE_STA_A5;
                break;
            case BOOTLOADER_COM_PARSE_STA_CMD:
                com_parse_cmd = com_parse_buf[i];
                com_parse_sta = BOOTLOADER_COM_PARSE_STA_LEN1;
                break;
            case BOOTLOADER_COM_PARSE_STA_LEN1:
                com_parse_len = com_parse_buf[i];
                com_parse_sta = BOOTLOADER_COM_PARSE_STA_LEN2;
                break;
            case BOOTLOADER_COM_PARSE_STA_LEN2:
                com_parse_len = (com_parse_len << 8) | com_parse_buf[i];
                DEBUG("com_parse_len %d\n", com_parse_len);
                if (com_parse_len <= ARRAY_SIZE(com_payload))
                {
                    com_parse_count = 0;
                    com_parse_sta = BOOTLOADER_COM_PARSE_STA_DATA;
                }
                else
                {
                    com_parse_sta = BOOTLOADER_COM_PARSE_STA_A5;
                }
                break;
            case BOOTLOADER_COM_PARSE_STA_DATA:
                // 直接拷贝剩余数据
                {
                    uint32_t l = len - i;
                    l = l < com_parse_len - com_parse_count ? l : com_parse_len - com_parse_count;
                    memcpy(com_payload + com_parse_count, com_parse_buf + i, l);
                    com_parse_count += l;
                    i += l;
                    if (com_parse_count == com_parse_len)
                        com_parse_sta = BOOTLOADER_COM_PARSE_STA_CRC1;
                    else
                        com_parse_sta = BOOTLOADER_COM_PARSE_STA_DATA;
                    goto next;
                }
                break;
            case BOOTLOADER_COM_PARSE_STA_CRC1:
                com_parse_crc = com_parse_buf[i];
                com_parse_sta = BOOTLOADER_COM_PARSE_STA_CRC2;
                break;
            case BOOTLOADER_COM_PARSE_STA_CRC2:
                com_parse_crc = (com_parse_crc << 8) | com_parse_buf[i];
                DEBUG("com_parse_crc %x\n", com_parse_crc);
                if (com_parse_cmd == BOOTLOADER_CMD_PROGRAM)
                {
                    com_parse_sta = BOOTLOADER_COM_PARSE_STA_CRC2;
                }
                if (bootloader_crc16_ccitt(com_payload, com_parse_len) == com_parse_crc && com_handle)
                    com_handle(com_parse_cmd, com_payload, com_parse_len);
                com_parse_sta = BOOTLOADER_COM_PARSE_STA_A5;
                break;
            }
            i++;
        next:
            continue;
        }
    } while (ringbuffer_available_get_length(&com_rb) >= BOOTLOADER_COM_MIN_PACKAGE_SIZE);
}
DECL_TASK(bootloader_com_handler_task);

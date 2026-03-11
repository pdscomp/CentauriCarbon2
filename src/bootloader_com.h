/*****************************************************************************
 * @Author       : Coconut
 * @Date         : 2025-03-22 11:23:58
 * @LastEditors  : coconut
 * @LastEditTime : 2025-03-22 11:37:56
 * @Description  :
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/
#ifndef __BOOTLOADER_COM_H
#define __BOOTLOADER_COM_H
#include <stdint.h> // uint8_t

#ifdef __cplusplus
extern "C"
{
#endif

    enum
    {
        BOOTLOADER_COM_PARSE_STA_A5 = 0,
        BOOTLOADER_COM_PARSE_STA_5A,
        BOOTLOADER_COM_PARSE_STA_CMD,
        BOOTLOADER_COM_PARSE_STA_LEN1,
        BOOTLOADER_COM_PARSE_STA_LEN2,
        BOOTLOADER_COM_PARSE_STA_DATA,
        BOOTLOADER_COM_PARSE_STA_CRC1,
        BOOTLOADER_COM_PARSE_STA_CRC2,
    };

#define BOOTALODER_COM_MAGIC_SIZE 2
#define BOOTALODER_COM_LENGTH_SIZE 2
#define BOOTALODER_COM_CMD_SIZE 1
#define BOOTALODER_COM_CRC_SIZE 2

#define BOOTLOADER_COM_MAX_PAYLOAD 4096
#define BOOTLOADER_COM_MIN_PACKAGE_SIZE (BOOTALODER_COM_MAGIC_SIZE + BOOTALODER_COM_LENGTH_SIZE + \
                                         BOOTALODER_COM_CMD_SIZE + BOOTALODER_COM_CRC_SIZE)

    enum
    {
        BOOTLOADER_COM_CMD_ERASE = 0X00,
        BOOTLOADER_CMD_PROGRAM = 0x01,
        BOOTLOADER_CMD_JUMP = 0x02,
        BOOTLOADER_CMD_PING = 0x03,
    };

    typedef struct __attribute__((packed))
    {
        uint32_t size;
    } bootloader_cmd_erase_req_t;

    typedef struct __attribute__((packed))
    {
        uint32_t size;
    } bootloader_cmd_erase_ack_t;

    typedef struct __attribute__((packed))
    {
        uint32_t offset;
        uint32_t length;
        uint32_t size;
    } bootloader_cmd_program_req_t;

    typedef struct __attribute__((packed))
    {
        uint32_t length;
        uint32_t offset;
    } bootloader_cmd_program_ack_t;

    typedef struct __attribute__((packed))
    {
        uint32_t ping;
    } bootloader_cmd_ping_req_t;

    typedef struct __attribute__((packed))
    {
        uint32_t pong;
    } bootloader_cmd_ping_ack_t;

    uint16_t bootloader_crc16_ccitt(uint8_t *buf, uint_fast8_t len);
    void bootloader_commit_data(uint8_t *buf, uint32_t len);
    void bootloader_send_data(uint8_t *buf, uint16_t len);
    void bootloader_set_handle(void (*handle)(uint8_t cmd, uint8_t *payload, uint32_t len));
    void bootloader_package_data(uint8_t *buf, uint32_t size, uint8_t cmd, uint8_t *payload, uint32_t len);
#ifdef __cplusplus
}

#endif

#endif // bootloader_com.h

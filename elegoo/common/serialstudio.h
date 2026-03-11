/*****************************************************************************
 * @Author       : Coconut
 * @Date         : 2025-02-12 11:05:41
 * @LastEditors  : coconut
 * @LastEditTime : 2025-02-12 18:26:41
 * @Description  : SerialStudio Support
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#pragma once
#include <string>

#include <sys/types.h>
#include <sys/socket.h>
#include <linux/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <arpa/inet.h>

namespace elegoo
{
    namespace common
    {
        class SerialStudio
        {
        public:
            struct __attribute__((packed)) ANOFrameHeader
            {
                uint8_t head;
                uint8_t s_addr;
                uint8_t d_addr;
                uint8_t id;
                uint16_t len;
            };
            struct __attribute__((packed)) ANOFrameTail
            {
                uint8_t sum_check;
                uint8_t add_check;
            };

            static SerialStudio &get_instance()
            {
                static SerialStudio ss;
                return ss;
            }

            SerialStudio(const SerialStudio &) = delete;
            SerialStudio &operator=(const SerialStudio &) = delete;

            int connect(const std::string &ipaddr, const std::string &port)
            {
                if (connected)
                {
                    printf("serialstudio already connected\n");
                    return -1;
                }

                fd = socket(AF_INET, SOCK_DGRAM, 0);
                if (fd < 0)
                {
                    printf("serialstudio socket open failed\n");
                    return -1;
                }

                // 设置本地IP与端口号
                memset(&src_addr, 0, sizeof(src_addr));
                src_addr.sin_family = AF_INET;
                src_addr.sin_addr.s_addr = INADDR_ANY;
                src_addr.sin_port = htons(996);
                if (bind(fd, (const struct sockaddr *)&src_addr, sizeof(src_addr)) < 0)
                {
                    printf("bind failed\n");
                    return -1;
                }
                // 设置对端IP与端口号
                memset(&dest_addr, 0, sizeof(dest_addr));
                dest_addr.sin_family = AF_INET;
                dest_addr.sin_port = htons(atol(port.c_str()));
                inet_aton(ipaddr.c_str(), &dest_addr.sin_addr);

                return 0;
            }

            int send(const char *fmt, ...)
            {
                size_t max_length = sizeof(buffer) - sizeof(ANOFrameHeader) - sizeof(ANOFrameTail);
                size_t count = 0;

                ANOFrameHeader *header = (ANOFrameHeader *)buffer;
                size_t len = 0;

                char fmtcopy[1024];
                char typeinfo[512];
                strncpy(fmtcopy, fmt, sizeof(fmtcopy));

                // 解析格式化字符串
                char *token;
                char *saveptr;
                token = strtok_r(fmtcopy, ",", &saveptr);
                while (token != NULL)
                {
                    if (token[0] != '%')
                    {
                        printf("token error! %s %s\n", fmtcopy, token);
                        return -1;
                    }
                    // uint32    'u'
                    // int32_t   'd'
                    // float     'f'
                    if (token[1] == 'u' || token[1] == 'd' || token[1] == 'f')
                    {
                        typeinfo[count] = token[1];
                        count++;
                    }
                    else
                    {
                        printf("token unknown! %s %s\n", fmtcopy, token);
                        return -1;
                    }

                    token = strtok_r(NULL, ",", &saveptr);
                }
                // 分析参数
                va_list args;
                va_start(args, fmt);
                for (int i = 0; i < count; i++)
                {
                    if (typeinfo[i] == 'u')
                    {
                        uint32_t arg = va_arg(args, uint32_t);
                        memcpy(buffer + sizeof(ANOFrameHeader) + len, &arg, 4);
                    }
                    else if (typeinfo[i] == 'd')
                    {
                        int32_t arg = va_arg(args, int32_t);
                        memcpy(buffer + sizeof(ANOFrameHeader) + len, &arg, 4);
                    }
                    else if (typeinfo[i] == 'f')
                    {
                        float arg = va_arg(args, double);
                        memcpy(buffer + sizeof(ANOFrameHeader) + len, &arg, 4);
                    }
                    len += 4;
                    if (len > max_length)
                    {
                        printf("over max_length %d %d\n", len, max_length);
                        return -1;
                    }
                }
                va_end(args);

                // 填充包头
                header->head = 0xab;
                header->s_addr = 0x01;
                header->d_addr = 0xfe;
                header->id = 0xf1;
                header->len = len;
                // 计算校验和
                calc_tail((uint8_t *)buffer);
                int ret = 0;
                ret = sendto(fd, buffer, len + sizeof(ANOFrameHeader) + sizeof(ANOFrameTail), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
                if (ret != len + sizeof(ANOFrameHeader) + sizeof(ANOFrameTail))
                    printf("sendto failed ret %d\n", ret);
                return ret;
            }

        private:
            SerialStudio() = default;
            int fd = -1;
            bool connected = false;
            struct sockaddr_in src_addr;
            struct sockaddr_in dest_addr;
            char buffer[1024];

            void calc_tail(uint8_t *buffer)
            {
                uint8_t sumcheck = 0, addcheck = 0;
                uint16_t flen = buffer[4] + buffer[5] * 256;
                for (int i = 0; i < (flen + 6); i++)
                {
                    sumcheck += buffer[i];
                    addcheck += sumcheck;
                }
                buffer[flen + 6] = sumcheck;
                buffer[flen + 7] = addcheck;
            }
        };

    }
}
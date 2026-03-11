/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2024-12-05 12:31:27
 * @LastEditors  : Jack
 * @LastEditTime : 2024-12-05 14:19:25
 * @Description  : 
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#pragma once

#include <iostream>
#include <vector>

#include "spdlog/spdlog.h"
#include "crc16.h"

namespace znp {

#define FRAME_MAX_SIZE (128)  // 一帧最长
#define FRAME_HEAD_L (0x09)   // 帧头低8位
#define FRAME_HEAD_H (0x23)   // 帧头高8位
struct FrameMessage {
  bool received{false};
  uint16_t length{0};  // 记录帧总长
  uint8_t step{0};     // 记录进度
  uint8_t key{0};      // 偶尔用于计数，一般不用
  uint16_t count{0};   // 记录当前 frame 下标
  uint8_t type{0};     // 用于区分多种帧，只有一种帧是一般不用
  uint8_t frame[FRAME_MAX_SIZE] = {0};  // 保存一帧
};

struct FrameFormat {
  uint16_t head{0};
  uint16_t crc16{0};
  uint16_t length{0};
  uint8_t cmd{0};
  uint8_t payload[FRAME_MAX_SIZE - 7];  // payload
} __attribute__((packed));

class Protocol {
 public:
  Protocol(){};
  ~Protocol(){};

  // 打包函数
  void Packet(const uint8_t cmd, const uint8_t param) {
    frame_.clear();

    // 帧头
    frame_.push_back(0x09);
    frame_.push_back(0x23);
    // 长度
    if ((cmd == 0x00) || (cmd == 0x02) || (cmd == 0x04)) {
      // 根据实际长度计算crc
      uint8_t src[3] = {0x01, 0x00, cmd};
      uint16_t crc16 = crc_16(src, sizeof(src));
      frame_.push_back((crc16 >> 0) & 0xFF);
      frame_.push_back((crc16 >> 8) & 0xFF);
      frame_.push_back(0x01);
      frame_.push_back(0x00);
      frame_.push_back(cmd);
    } else {
      uint8_t src[4] = {0x02, 0x00, cmd, param};
      uint16_t crc16 = crc_16(src, sizeof(src));
      frame_.push_back((crc16 >> 0) & 0xFF);
      frame_.push_back((crc16 >> 8) & 0xFF);
      frame_.push_back(0x02);
      frame_.push_back(0x00);
      frame_.push_back(cmd);
      frame_.push_back(param);
    }
  }

  // 校验包
  bool PacketCheck(uint8_t *buf, int len) {
    if (!buf || len < 0) {
      SPDLOG_ERROR("error prama!");
      return false;
    }

    if (buf[0] != 0x09 || buf[1] != 0x23) {
      SPDLOG_ERROR("Frame head wrong!");
      return false;
    }
    uint16_t uI16Size = buf[4] | (buf[5] << 8);
    uint16_t uI16CheckSum = buf[2] | (buf[3] << 8);

    if (uI16CheckSum != crc_16(&buf[6], uI16Size)) {
      SPDLOG_ERROR("Frame checkSum wrong!");
      return false;
    }

    return true;
  }

  const std::vector<uint8_t> &Getframe() const { return frame_; }

  bool Parse(FrameMessage &message, uint8_t data) {
    switch (message.step) {
      case 0: {  // head_l
        memset(&message, 0, sizeof(FrameMessage));
        if (FRAME_HEAD_L == data) {
          message.frame[0] = data;
          message.step = 1;
        }
        break;
      }
      case 1: {  // head_h
        if (FRAME_HEAD_H == data) {
          message.frame[1] = data;
          message.step = 2;
          message.type = FRAME_HEAD_H;
        } else {
          message.step = 0;  // 进入获取CRC阶段
        }
        break;
      }
      case 2: {  // crc
        if (FRAME_HEAD_H == message.type) {
          if (0 == message.key) {
            message.frame[2] = data;
            message.key = 1;
            // 此时 step 保持为 2
          } else if (1 == message.key) {
            message.frame[3] = data;
            message.key = 0;
            message.step = 3;  // 进入获取长度阶段
          }
        } else {
          message.step = 0;
        }
        break;
      }
      case 3: {  // len
        if (FRAME_HEAD_H == message.type) {
          if (0 == message.key) {
            message.frame[4] = data;
            message.key = 1;
            // 此时 step 保持为 2
          } else if (1 == message.key) {
            message.frame[5] = data;
            message.key = 0;
            message.count = 0;
            // 此时 length 暂时保存长度值
            message.length = (message.frame[5] << 8) | (message.frame[4] << 0);
            message.step = 4;  // 根据获得长度值读取最后数据
          }
        } else {
          message.step = 0;
        }
        break;
      }
      case 4: {
        if (FRAME_HEAD_H == message.type) {
          if (message.count < message.length) {
            message.frame[6 + message.count] = data;
            message.count++;

            // 说明获取到所有数据，开始校验
            if (message.count == message.length) {
              uint16_t len = (message.frame[5] << 8) | (message.frame[4] << 0);
              uint16_t read_crc =
                  (message.frame[2] << 8) | (message.frame[3] << 0);
              if (read_crc == crc_16(&message.frame[4], len + 2)) {
                message.length = 6 + message.length;  // 此时表示完整一帧长度
                message.received = true;              // 结束组帧
              }
              message.step = 0;  // 重新回到第一步获取帧头
            }
          }
        } else {
          message.step = 0;
        }
        break;
      }
      default: {
        message.step = 0;
        break;
      }
    }

    return message.received;
  }

 private:
  std::vector<uint8_t> frame_;
};
}

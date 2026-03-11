/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2024-10-25 17:52:25
 * @LastEditors  : Jack
 * @LastEditTime : 2024-12-05 14:18:50
 * @Description  : 
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#pragma once

#include "serial_info.h"
#include "spdlog/spdlog.h"

#include <functional>
#include <iostream>

#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <iostream>
#include <string>

#include <atomic>
#include <thread>
#include <vector>

namespace znp {

class Uart {
  using ReceiveCallBack = std::function<void(uint8_t*, int)>;

 public:
  Uart(std::string port, int baudrate,
       znp::StopBits stopbits = znp::StopBits::One,
       znp::DataBits databits = znp::DataBits::Eight,
       znp::Parity parity = znp::Parity::None);
  ~Uart();

  void Start();
  void Stop();
  bool IsOpen() const;
  bool Write(const uint8_t* buf, int len);  // 串口写入指令到摄像头模组
  void RegisterReceivedCallBack(ReceiveCallBack callback);

 private:
  bool Open();
  void Close();
  void ConfigParam();
  void ReceiveThread();

 private:
  int fd_;
  struct SerialPortInfo serialInfo_;
  struct termios options_;
  std::atomic<bool> isAlive_;

  std::shared_ptr<std::thread> thread_;
  ReceiveCallBack received_callback_;
};

}  // namespace znp
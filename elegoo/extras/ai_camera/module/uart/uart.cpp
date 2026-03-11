/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2024-11-05 11:48:29
 * @LastEditors  : Jack
 * @LastEditTime : 2024-12-10 15:07:40
 * @Description  : 
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "uart.h"
#include "camera_execption.h"
#include "ai_camera_pthread.h"
#include "spdlog/spdlog.h"
namespace znp {

Uart::Uart(std::string port, int baudrate, znp::StopBits stopbits,
           znp::DataBits databits, znp::Parity parity)
    : received_callback_(nullptr),
      thread_(nullptr),
      isAlive_(false),
      fd_(-1),
      serialInfo_{port, baudrate, stopbits, databits, parity} {
  SPDLOG_INFO("Uart:{}, baudrate:{}", port, baudrate);
}

Uart::~Uart() { this->Stop(); }

void Uart::Start() {
  if (this->Open()) {
    this->isAlive_ = true;
    SPDLOG_INFO("串口打开");
    CreateNewThread([this]() { ReceiveThread();},  1*1024*1024, "CamreaUart");
  }
  else{
    SPDLOG_INFO("串口打开失败!");
  }
}

void Uart::Stop() {
  SPDLOG_INFO("串口关闭");
  this->isAlive_ = false;
  this->Close();
}

bool Uart::IsOpen() const { return this->isAlive_; }

bool Uart::Open() {
  this->fd_ =
      ::open(this->serialInfo_.portName.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
  if (this->fd_ == -1) {
    SPDLOG_ERROR("Unable to open serial port");
    throw CameraException(CameraException::ErrorCode::INITIALIZATION_ERROR,
                          "Uart open failed!");
  }

  this->ConfigParam();
  return true;
}

void Uart::Close() {
  if (this->fd_ > 0) {
    ::close(this->fd_);
    this->fd_ = -1;
  }
}

void Uart::ConfigParam() {
  if (this->fd_ < 0) {
    SPDLOG_ERROR("Error fd!");
    return;
  }

  tcgetattr(this->fd_, &options_);
  speed_t baud = 0;
  switch (this->serialInfo_.baudRate) {
    case 9600:
      baud = B9600;
      break;
    case 115200:
      baud = B115200;
      break;

    default:
      SPDLOG_ERROR("不支持的波特率!\n");
      baud = B115200;
      break;
  }
  cfsetispeed(&this->options_, baud);
  cfsetospeed(&this->options_, baud);

  options_.c_cflag |= (CLOCAL | CREAD);  // 使能接收

  switch (this->serialInfo_.parity)  // 奇偶校验
  {
    case znp::Parity::None:
      options_.c_cflag &= ~PARENB;  // 无奇偶校验
      break;
    case znp::Parity::Even:

      break;
    default:
      options_.c_cflag &= ~PARENB;
      break;
  }

  options_.c_cflag &= ~CSTOPB;  // 1个停止位
  options_.c_cflag &= ~CSIZE;   // 清除数据位掩码
  options_.c_cflag |= CS8;      // 8个数据位

  options_.c_iflag &= ~(IXON | IXOFF | IXANY);          // 禁用软件流控制
  options_.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);  // 原始输入模式
  options_.c_oflag &= ~OPOST;                           // 原始输出模式

  options_.c_cc[VMIN] = 1;   // 最小字符数
  options_.c_cc[VTIME] = 0;  // 超时时间

  tcsetattr(this->fd_, TCSANOW, &this->options_);
}

void Uart::RegisterReceivedCallBack(ReceiveCallBack callback) {
  this->received_callback_ = callback;
}

void Uart::ReceiveThread() {
  if (fd_ < 0) {
    SPDLOG_ERROR("fd error!");
    return;
  }

  int ret = -1;
  int buf_len = -1;
  uint8_t buf[1024] = {0};
  fd_set readfds;
  struct timeval timeout;

  while (this->isAlive_) {
    FD_ZERO(&readfds);
    FD_SET(this->fd_, &readfds);

    timeout.tv_sec = 5;   // 5 秒超时
    timeout.tv_usec = 0;  // 0 微秒
    fd_set tempfds = readfds;
    ret = select(this->fd_ + 1, &tempfds, nullptr, nullptr, &timeout);
    if (ret < 0) {
      SPDLOG_ERROR("select error");
    } else if (ret > 0) {
      if (FD_ISSET(this->fd_, &tempfds)) {
        memset(buf, 0, sizeof(buf));
        buf_len = ::read(this->fd_, buf, sizeof(buf));
        // SPDLOG_INFO("Uart get buf len: {}", buf_len);
        // for (int i=0; i<buf_len; i++) {
        //   printf("0x%02x ", buf[i]);
        // }
        // printf("\n");
        if (received_callback_) {
          received_callback_(buf, buf_len);
        }
      }
    } else {
      SPDLOG_ERROR("select time out! {}", __FUNCTION__);
    }
  }
}

bool Uart::Write(const uint8_t* buf, int len) {
  if (fd_ > 0) {
    if (::write(fd_, buf, len) == len) {
      // for (int i = 0; i < len; i++) {
      //   printf("0x%02x ", buf[i]);
      // }
      // printf("\n");
      return true;
    } else {
      SPDLOG_ERROR("Write uart cmd failed!");
      return false;
    }
  } else {
    SPDLOG_ERROR("Write uart cmd failed fd error!");
    return false;
  }
}

}  // namespace znp

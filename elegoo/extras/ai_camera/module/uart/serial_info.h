#pragma once

#include <string>

namespace znp {
// 定义一个枚举类型来表示停止位
enum class StopBits { One = 1, OnePointFive = 15, Two = 2 };

// 定义一个枚举类型来表示数据位
enum class DataBits { Five = 5, Six = 6, Seven = 7, Eight = 8 };

// 定义一个枚举类型来表示奇偶校验
enum class Parity { None, Odd, Even, Mark, Space };

// 定义一个结构体来包含串口的相关信息
struct SerialPortInfo {
  std::string portName;  // 串口名称，例如 "/dev/ttyS0" 或 "COM1"
  int baudRate;          // 波特率，例如 9600, 115200 等
  StopBits stopBits;     // 停止位
  DataBits dataBits;     // 数据位
  Parity parity;         // 奇偶校验

  // 构造函数
  SerialPortInfo(std::string name, int baud, StopBits stop, DataBits data,
                 Parity par)
      : portName(name),
        baudRate(baud),
        stopBits(stop),
        dataBits(data),
        parity(par) {}

  // 可以添加其他成员函数，例如设置和获取各个属性的方法
};
}

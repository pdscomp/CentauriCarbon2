/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2024-11-21 10:19:07
 * @LastEditors  : loping
 * @LastEditTime : 2025-09-20 15:23:36
 * @Description  : Define exception handling objects.
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#pragma once

#include <exception>
#include <stdexcept>
#include <string>
#include "json.h"

namespace elegoo
{
namespace common
{

class BaseException : public std::exception {
public:
    explicit BaseException(const std::string& message);

    const char* what() const noexcept override;

private:
    std::string message;
};

// 对齐Python中PinsError异常
class PinsError : public BaseException {
    public:
        explicit PinsError(const std::string& message);
    };
    
// 对齐Python中CommandError异常
class CommandError : public BaseException {
public:
    explicit CommandError(const std::string& message);
};

// 对齐Python中error异常
class Error : public BaseException {
public:
    explicit Error(const std::string& message);
};

// 对齐Python中DripModeEndSignal异常
class DripModeEndSignalError : public BaseException {
public:
    explicit DripModeEndSignalError(const std::string& message);
};

// 对齐Python中BedMeshError异常
class BedMeshError : public BaseException {
public:
    explicit BedMeshError(const std::string& message);
};

// 对齐Python中ConfigParser.Error异常
class ConfigParserError : public BaseException {
public:
    explicit ConfigParserError(const std::string& message);
};

// 对齐Python中SerialException异常
class SerialError : public BaseException {
public:
    explicit SerialError(const std::string& message);
};

// 对齐Python中UnicodeDecodeError异常
class UnicodeDecodeError : public BaseException {
public:
    explicit UnicodeDecodeError(const std::string& message);
};

// 对齐Python中ValueError异常
class ValueError : public BaseException {
public:
    explicit ValueError(const std::string& message);
};

// 对齐Python中IOError异常
class IOError : public BaseException {
public:
    explicit IOError(const std::string& message);
};

// 对齐Python中OSError\os.error异常
class OSError : public BaseException {
public:
    explicit OSError(const std::string& message);
};

// 对齐Python中KeyError异常
class KeyError : public BaseException {
public:
    explicit KeyError(const std::string& message);
};

// 对齐Python中ImportError异常
class ImportError : public BaseException {
public:
    explicit ImportError(const std::string& message);
};

// 对齐Python中SyntaxError异常
class SyntaxError : public BaseException {
public:
    explicit SyntaxError(const std::string& message);
};

// 对齐Python中TypeError异常
class TypeError : public BaseException {
public:
    explicit TypeError(const std::string& message);
};

// 对齐Python中IndexError异常
class IndexError : public BaseException {
public:
    explicit IndexError(const std::string& message);
};

// 对齐Python中Empty异常
class EmptyError : public BaseException {
public:
    explicit EmptyError(const std::string& message);
};

// 对齐Python中enumeration_error异常
class EnumerationError : public BaseException {
public:
    explicit EnumerationError(const std::string& enum_name, const std::string& value);

    std::pair<std::string, std::string> get_enum_params() const;

private:
    std::string enum_name;
    std::string value;
};

class WebRequestError : public BaseException {
public:
    explicit WebRequestError(const std::string& message);

    json to_dict() const;
};

class MMUError : public BaseException {
public:
    explicit MMUError(const std::string& message);
};
class ApiRequestError : public BaseException {
public:
    explicit ApiRequestError(const std::string& message);

    json to_dict() const;
};


// 错误码定义：外部维护
namespace ErrorCode {
    constexpr int CODE_OK = 0;
    constexpr int VERIFY_HEATER_HEATED_BED=101;
    constexpr int ADC_TEMPERATURE_HEATED_BED=102;
    constexpr int VERIFY_HEATER_EXTRUDER=103;
    constexpr int ADC_TEMPERATURE_EXTRUDER=104;
    constexpr int OT_BED=108;
    constexpr int HOMING_MOVE_Z=304;
    constexpr int LIS2DW_SENSOR=401;
    constexpr int FAN_BOARD=701;
    constexpr int FAN_THROAT=702;
    constexpr int FAN_MODEL=703;
    constexpr int BED_MESH_FAIL=704;
    constexpr int FAN_FAN1=705;
    constexpr int FAN_BOX_FAN=706;
    constexpr int CANVAS_FAN_OFF = 707;                                    // 喷头前盖风扇脱落异常
    constexpr int EXTRUDER_CONNECT_FAIL=801;
    constexpr int BED_MESH_CONNECT_FAIL=802;
    constexpr int SYSTEM_ERROR=803;
    constexpr int OT_BOX=902;
    // CANVAS
    constexpr int GRILLE_OPEN_FAILED = 1101;                                // 舵机远离，排气格栅开启失败
    constexpr int GRILLE_CLOSE_FAILED = 1103;                               // 舵机归零，排气格栅关闭失败
    constexpr int CANVAS_SERIAL_ERROR = 1210;                               // 通讯断开连接
    constexpr int CANVAS_OUT_OF_FILAMENT = 1211;                               // 耗材用尽
    constexpr int CANVAS_ABNORMAL_FILA = 1220;                              // 检测到异常耗材在挤出机,请手动退出耗材
    constexpr int CANVAS_CUTTING_KNIFET_NOT_TRIGGER = 1231;                 // 切断耗材时切刀未被按下
    constexpr int CANVAS_CUTTING_KNIFET_TRIGGER = 1232;                     // 切断耗材时切刀未正常释放
    constexpr int CANVAS_PLUT_IN_FILA_NOT_TRIGGER = 1241;                   // 进料自检时耗材传感器未触发,可能多色通道的PTFE管没有插上
    constexpr int CANVAS_PLUT_IN_FILA_TIMEOUT = 1242;                       // 进料自检超时,可能通道耗材打滑或者挤出机构堵塞
    constexpr int CANVAS_PLUT_IN_FILA_BLOCKED = 1243;                       // 进料自检异常,可能挤出机构堵塞
    constexpr int CANVAS_PLUT_OUT_FILA_ABNORMAL = 1251;                     // 退料自检异常,可能挤出机存在耗材断裂
    constexpr int CANVAS_PLUT_OUT_FILA_TIMEOUT = 1252;                      // 退料自检超时,可能通道耗材打滑
    //
    constexpr int CANVAS_RUNOUT = 1260;                                     // 断料异常
    constexpr int CANVAS_CUT_KNIFET = 1262;                                 // 切刀异常
    constexpr int CANVAS_WRAP_FILA = 1263;                                  // 缠料异常
    constexpr int EXTERNAL_FILA_ERROR = 1264;                               // 外置耗材堵头异常
};

// namespace ErrorCode {
//     constexpr int VERIFY_HEATER_HEATED_BED=51017;
//     constexpr int ADC_TEMPERATURE_HEATED_BED=37252;
//     constexpr int VERIFY_HEATER_EXTRUDER = 5503;
//     constexpr int ADC_TEMPERATURE_EXTRUDER=14462;
//     constexpr int HOMING_MOVE_Z=77526;
//     constexpr int LIS2DW_SENSOR=64806;
//     constexpr int FAN_BOARD=82810;
//     constexpr int FAN_THROAT=18942;
//     constexpr int FAN_MODEL=52742;
//     constexpr int BED_MESH_FAIL=15309;
// }

enum class ErrorLevel {
    INFO = 0,
    WARNING,
    CRITICAL,
    RESUME
};

} // namespace common
} // namespace elegoo
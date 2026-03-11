/*****************************************************************************
 * @Author       : huangpeiyun
 * @Date         : 2025-06-27 18:32:07
 * @LastEditors  : huangpeiyun
 * @LastEditTime : 2025-09-04 20:47:43
 * @Description  : 
 * 
 * Copyright (c) 2025 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#pragma once

#define PROTOCOL_METHODS(XX) \
    XX(1001, GET_SYSTEM_INFO, get system info)  \
    XX(1002, GET_BASE_INFO, get base info) \
    XX(1003, GET_STATUS, get status) \
    XX(1004, GET_FAN_STATUS, get fan status) \
    XX(1005, GET_PRINTS_INFO, get prints info) \
    XX(1006, GET_HOME_STATUS, get home status) \
    XX(1007, EMERGENCY_STOP, emergency stop) \
    XX(1019, SET_PRINT_CFG,  set print cfg) \
    XX(1020, START_PRINT, start print)  \
    XX(1021, PAUSE_PRINT, stop print) \
    XX(1022, CANCEL_PRINT, cancel print)  \
    XX(1023, RESUME_PRINT, resume print) \
    XX(1024, LOAD_FILAMENT, load filament) \
    XX(1025, UNLOAD_FILAMENT, unload filament) \
    XX(1026, CTRL_HOME, ctrl home) \
    XX(1027, CTRL_MOVE, ctrl move) \
    XX(1028, SET_TEMPTURE, set temperature) \
    XX(1029, CTRL_LED, ctrl led) \
    XX(1030, CTRL_FAN, ctrl fan) \
    XX(1031, CTRL_PRINT_SPEED, ctrl print speed) \
    XX(1032, AUTO_BED_LEVELING, auto bed leveling) \
    XX(1033, RINGING_OPTIMIZE, ringing optimize) \
    XX(1034, PID_CHECK, pid check) \
    XX(1035, AUTO_DIAGNOSTIC, auto diagnostic) \
    XX(1036, GET_HISTORY_TASK, get history task) \
    XX(1038, DEL_HISTORY_TASK_REPORT, del history task report) \
    XX(1039, OTA_UPGRADING, ota upgrading) \
    XX(1042, GET_MONITOR_VIDENO_URL, get monitor videno url)\
    XX(1043, SET_HOSTNAME, set hostname) \
    XX(1044, GET_FILE_LIST, get file list) \
    XX(1045, GET_FILE_THUMBNAIL, get file thumbnail) \
    XX(1046, GET_FILE_INFO, get file info) \
    XX(1047, DEL_FILE, del file) \
    XX(1048, GET_DISK_VALUME, get disk valume) \
    XX(1049, UPDATE_TOKEN, update token) \
    XX(1050, GET_TOKEN, get token)  \
    XX(1051, EXPORT_TIMELAPSE_VIDEO, export timelapse video) \
    XX(1053, SET_TOKEN_SWITCH, set token switch)  \
    XX(1054, CTRL_LIVE_STREAM, ctrl live stream)   \
    XX(1055, SET_MONO_FILAMENT_INFO, set mono filament info) \
    XX(1056, EXTRUDER_FILAMENT, extruder filament)  \
    XX(2001, CANVAS_LOAD_FILAMENT, canvas load filament) \
    XX(2002, CANVAS_UNLOAD_FILAMENT, canvas unload filament) \
    XX(2003, CANVAS_EDIT_FILAMENT_INFO, canvas edit filament) \
    XX(2004, CANVAS_AUTO_IN_FILAMENT, canvas auto in filament) \
    XX(2005, CANVAS_GET_CHANNEL_INFO, canvas get channel info) \
    XX(6000, POST_INFO, post info) \
    XX(7000, SEARCH_BRODCAST, search brodcast)

enum class ProtocolMethod {
#define XX(code, name, desc) name = code,
    PROTOCOL_METHODS(XX)
#undef XX
};

static const char *protocol_method_to_string(ProtocolMethod code) {
    switch (code) {
#define XX(code, name, desc) \
        case ProtocolMethod::name: return #name;
        PROTOCOL_METHODS(XX)
#undef XX
        default: return "Unknown Error";
    }
}

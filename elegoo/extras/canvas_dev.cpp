/*****************************************************************************
 * @Author       : loping
 * @Date         : 2025-05-28 16:25:15
 * @LastEditors  : zhangjxxxx
 * @LastEditTime : 2025-09-12 11:57:20
 * @Description  : 
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include <fstream> 
#include <endian.h>
#include <glob.h>
#include <regex>
#include "canvas_dev.h"
// #include <math.h>
#include "buttons.h"
#include "idle_timeout.h"
#include "print_stats.h"
#include "pause_resume.h"
#include "serial_bootloader.h"

#define RECORD_INFO_INI "/opt/usr/canvas_record_info.json"
// #define NOT_FROM_SLICE 1
#define FROM_GCODE_MCRO 1

constexpr uint8_t CRC8_POLY = 0x39;
constexpr uint8_t CRC8_INIT = 0x66;
constexpr uint16_t CRC16_POLY = 0x1021;
constexpr uint16_t CRC16_INIT = 0x913D;

// CRC-8计算函数
static uint8_t crc8(const std::vector<uint8_t>& data, const std::size_t length)
{
    uint8_t crc = CRC8_INIT;
    
    for (std::size_t i = 0; i < length; i++) {
        crc ^= data[i]; // 先将数据与CRC值进行异或
        for (std::size_t j = 0; j < 8; j++) {  // 循环进行8次移位运算
            if (crc & 0x80) {  // 判断最高位是否为1
                crc = (crc << 1) ^ CRC8_POLY;  // 左移并与多项式异或
            } else {
                crc <<= 1;  // 只进行左移
            }
        }
    }

    return crc;
}

// CRC-16计算函数
static uint16_t crc16(const std::vector<uint8_t>& data, const std::size_t length)
{
    uint16_t crc = CRC16_INIT;

    for (std::size_t i = 0; i < length; i++) {
        crc ^= (data[i] << 8);  // 将数据与CRC值异或，并左移8位
        for (std::size_t j = 0; j < 8; j++) {  // 对每一位进行移位
            if (crc & 0x8000) {  // 判断最高位是否为1
                crc = (crc << 1) ^ CRC16_POLY;  // 左移并与多项式异或
            } else {
                crc <<= 1;  // 否则只进行左移
            }
        }
    }
    
    return crc;
}

static uint64_t swap_uint64(uint64_t val) {
    return ((val >> 56) & 0x00000000000000FFULL) |
           ((val >> 40) & 0x000000000000FF00ULL) |
           ((val >> 24) & 0x0000000000FF0000ULL) |
           ((val >> 8)  & 0x00000000FF000000ULL) |
           ((val << 8)  & 0x000000FF00000000ULL) |
           ((val << 24) & 0x0000FF0000000000ULL) |
           ((val << 40) & 0x00FF000000000000ULL) |
           ((val << 56) & 0xFF00000000000000ULL);
}

namespace elegoo
{
    namespace extras
    {
        #undef SPDLOG_DEBUG
        #define SPDLOG_DEBUG SPDLOG_INFO

        std::string firmware_dir = "/opt/usr/ota";

        std::map<uint32_t,std::string> filament_type = {
            {PLA,"PLA"},
            {PETG,"PETG"},
            {ABS,"ABS"},
            {TPU,"TPU"},
            {PA,"PA"},
            {CPE,"CPE"},
            {PVA,"PVA"},
            {ASA,"ASA"},
            {BVOH,"BVOH"},
            {EVA,"EVA"},
            {HIPS,"HIPS"},
            {PP,"PP"},
            {PPA,"PPA"},
        };

        std::map<uint32_t,std::string> filament_detail_type = {
            // PLA
            {PLA_,"PLA"},
            {PLA_Plus,"Plus"},
            {PLA_Hyper,"Hyper"},
            {PLA_Silk,"Silk"},
            {PLA_CF,"CF"},
            {PLA_Carbon,"Carbon"},
            {PLA_Matte,"Matte"},
            {PLA_Fluo,"Fluo"},
            {PLA_Wood,"Wood"},
            // PETG
            {PETG_,"PETG"},
            {PETG_CF,"CF"},
            {PETG_Hyper,"Hyper"},
            // ABS
            {ABS_,"ABS"},
            {ABS_Hyper,"Hyper"},
            // TPU
            {TPU_,"TPU"},
            {TPU_Hyper,"Hyper"},
            // PA
            {PA_,"PA"},
            {PA_CF,"CF"},
            {PA_Hyper,"Hyper"},
            // CPE
            {CPE_,"CPE"},
            {CPE_Hyper,"Hyper"},
            // PC
            {PC_,"PC"},
            {PC_PCTG,"PCTG"},
            {PC_Hyper,"Hyper"},
            // PVA
            {PVA_,"PVA"},
            {PVA_Hyper,"Hyper"},
            // ASA
            {ASA_,"ASA"},
            {ASA_Hyper,"Hyper"},
            // BVOH
            {BVOH_,"BVOH"},
            // EVA
            {EVA_,"EVA"},
            // HIPS
            {HIPS_,"HIPS"},
            // PP
            {PP_,"PP"},
            {PP_CF,"CF"},
            {PP_GF,"GF"},
            // PPA
            {PPA_,"PPA"},
            {PPA_CF,"CF"},
            {PPA_GF,"GF"},
        };

        // 查找是否存在Canvas固件
        std::string has_canvas_bin(const std::string& dir)
        {
            glob_t glob_result;
            std::string pattern = dir + "/canvas_gd32-*-app.bin";

            glob(pattern.c_str(), 0, nullptr, &glob_result);

            std::string path;
            if (glob_result.gl_pathc > 0) {
                // 取第一个匹配到的完整路径
                path = glob_result.gl_pathv[0];
            }

            globfree(&glob_result);

            return path; // 找不到返回空字符串
        }

        bool update_firmware(std::string firmware_path)
        {
            std::string tty = "/dev/ttyS1";
            int badurate = 115200;

            int retry = 5;
            SerialBootloader app_serial(tty, badurate);
            SerialBootloader bootloader_serial(tty, badurate);

            while (--retry >= 0)
            {
                int ping_retry = 10;

                if (!app_serial.connect()) {
                    SPDLOG_INFO("Failed to connect to app_serial");
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }

                app_serial.jump_to_bootloader();
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                app_serial.disconnect();

                if (!bootloader_serial.connect()) {
                    SPDLOG_INFO("bootloader_serial.connect() failed");
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }

                while (--ping_retry >= 0)
                {
                    if (bootloader_serial.ping())
                    {
                        SPDLOG_INFO("bootloader connected %d", ping_retry);
                        break;
                    }
                    bootloader_serial.disconnect();
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                    bootloader_serial.connect();
                }
                
                if (ping_retry >= 0) // Ping Bootloader成功，跳出外层循环，结束重试
                    break;
            }

            if (retry < 0)
            {
                bootloader_serial.disconnect();
                SPDLOG_INFO("canvas update error connect");
                return -1;
            }

            // 执行升级
            if (!bootloader_serial.update(firmware_path.c_str()))
            {
                bootloader_serial.jump_to_app();
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                bootloader_serial.disconnect();
                SPDLOG_INFO("canvas update error update");
                return -1;
            }

            bootloader_serial.jump_to_app();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            bootloader_serial.disconnect();

            return 0;
        }
    
        std::vector<uint8_t> pack_short_frame(const std::vector<uint8_t>& data)
        {
            std::vector<uint8_t> frame;
            uint8_t frame_length = data.size() + 6;
            // 帧头 + 帧标志 + 长度 + CRC8 + 负载数据 + CRC16
            
            frame.push_back(FRAME_HEAD);
            frame.push_back(FRAME_SHORT);
            frame.push_back(frame_length);
            frame.push_back(crc8(frame, 3));
            frame.insert(frame.end(), data.begin(), data.end());
            uint16_t crc = crc16(frame, frame.size());
            frame.push_back((crc >> 8) & 0xFF);
            frame.push_back((crc >> 0) & 0xFF);

            return frame;
        }

        std::vector<uint8_t> unpack_short_frame(const std::vector<uint8_t>& frame)
        {
            std::vector<uint8_t> data;

            if (frame.size() < 6) {
                // 帧长不足（帧头 + 类型 + 长度 + CRC8 + CRC16）
                return data;
            }

            if (frame[0] != FRAME_HEAD || frame[1] != FRAME_SHORT) {
                // 帧头或帧类型不匹配
                return data;
            }

            uint8_t length = frame[2];
            if (frame.size() != length) {
                // 长度字段和实际长度不一致
                return data;
            }

            uint8_t crc8_val = crc8(frame, 3);
            if (crc8_val != frame[3]) {
                // CRC8 校验失败
                return data;
            }

            uint16_t crc_received = (frame[length - 2] << 8) | frame[length - 1];
            uint16_t crc_calculated = crc16(frame, length - 2);
            if (crc_received != crc_calculated) {
                // CRC16 校验失败
                return data;
            }

            // 负载数据从下标 4 到 length - 3
            data.insert(data.end(), frame.begin() + 4, frame.begin() + (length - 2));

            return data;
        }

        std::vector<uint8_t> pack_long_frame(const uint16_t seq, const std::vector<uint8_t>& data)
        {
            std::vector<uint8_t> frame;
            uint16_t frame_length = data.size() + 9;
            // 帧头 + 长帧标志 + 序列号 + 长度 + CRC8 + 负载数据 + CRC16
            // 注意：data_len + 9 是因为长帧包含序列号（2字节）和长度（2字节），以及CRC8（1字节）和CRC16（2字节）

            frame.push_back(FRAME_HEAD);
            frame.push_back(FRAME_LONG);
            frame.push_back((seq >> 8) & 0xFF);
            frame.push_back((seq >> 0) & 0xFF);
            frame.push_back((frame_length >> 8) & 0xFF);
            frame.push_back((frame_length >> 0) & 0xFF);
            frame.push_back(crc8(frame, 6));
            frame.insert(frame.end(), data.begin(), data.end());
            uint16_t crc = crc16(frame, frame.size());
            frame.push_back((crc >> 8) & 0xFF);
            frame.push_back((crc >> 0) & 0xFF);

            return frame;
        }

        std::vector<uint8_t> unpack_long_frame(const std::vector<uint8_t>& frame)
        {
            std::vector<uint8_t> data;

            // 最小帧长检查：帧头 + 类型 + 序列号(2) + 长度(2) + CRC8 + CRC16 = 9 字节
            if (frame.size() < 9) {
                return data;
            }

            // 帧头和类型检查
            if (frame[0] != FRAME_HEAD || frame[1] != FRAME_LONG) {
                return data;
            }

            // 提取帧长（高字节在前）
            uint16_t length = (frame[4] << 8) | frame[5];
            if (frame.size() != length) {
                return data;
            }

            // CRC8 校验
            uint8_t crc8_val = crc8(frame, 6);
            if (crc8_val != frame[6]) {
                return data;
            }

            // CRC16 校验
            uint16_t crc_received = (frame[length - 2] << 8) | frame[length - 1];
            uint16_t crc_calculated = crc16(frame, length - 2);
            if (crc_received != crc_calculated) {
                return data;
            }

            // 负载数据提取
            data.insert(data.end(), frame.begin() + 7, frame.end() - 2);

            return data;
        }

        bool check_short_frame_respond(const uint8_t id, const uint8_t cmd, const std::vector<uint8_t>& frame)
        {
            if (frame.size() < 9) {
                return false;
            }

            if (frame[INDEXS_CRC8] != crc8(frame, INDEXS_CRC8)) {
                SPDLOG_DEBUG("RS485 CRC-8 Incorrect {}/{}",frame[INDEXS_CRC8],crc8(frame, INDEXS_CRC8));
                return false;
            }

            uint16_t crc = frame[frame.size() - 2] << 8 | frame.back();
            uint16_t crc_calc = crc16(frame, frame.size() - 2);
            if (crc != crc_calc) {
                SPDLOG_DEBUG("RS485 CRC-16 Incorrect {}/{}", crc, crc_calc);
                return false;
            }

            if (id != frame[INDEXS_ID] || cmd != frame[INDEXS_CMD]) {
                SPDLOG_DEBUG("Command Incorrect");
                return false;
            }

            return true;
        }
        
        CanvasDev::CanvasDev(std::shared_ptr<ConfigWrapper> config)
        {
            SPDLOG_INFO("{} #1",__func__);
            this->config = config;
            this->printer = config->get_printer();
            this->reactor = printer->get_reactor();
            this->name = config->get_name();
            //
            canvas_init();
            //
            SPDLOG_INFO("{} #1 canvas_ctrl_pin:{}",__func__,config->get("canvas_ctrl_pin",""));
            std::shared_ptr<PinParams> canvas_ctrl_pin_params = any_cast<std::shared_ptr<PrinterPins>>(config->get_printer()->lookup_object("pins"))->lookup_pin(config->get("canvas_ctrl_pin"), true, false);
            if (*canvas_ctrl_pin_params->chip_name == "host")
            {
                SPDLOG_INFO("set canvas ctrl pin");
                std::shared_ptr<MCU_host_digital_pin> canvas_ctrl_digital_pin = std::static_pointer_cast<MCU_host_digital_pin>(canvas_ctrl_pin_params->chip->setup_pin("host_digital_pin", canvas_ctrl_pin_params));
                // 设置输出
                canvas_ctrl_digital_pin->set_direction(0);
                // 设置高电平
                canvas_ctrl_digital_pin->set_digital(1);
            }
            //
            serial_port = config->get("serial","");
            SPDLOG_DEBUG("serial={}", serial_port);
            if (!(serial_port.rfind("/dev/rpmsg_", 0) == 0 
                || serial_port.rfind("/tmp/elegoo_host_", 0) == 0))
            {
                baudrate = config->getint("baud", 250000, 2400);
                SPDLOG_DEBUG("Baudrate:{}", baudrate);
            }
            
            rs485 = std::make_shared<RS485>(serial_port, baudrate);
            //
            elegoo::common::SignalManager::get_instance().register_signal(
                "elegoo:connect", std::function<void()>([this]() { handle_connect(); }));
            elegoo::common::SignalManager::get_instance().register_signal(
                "elegoo:disconnect", std::function<void()>([this]() { handle_disconnect(); }));
            elegoo::common::SignalManager::get_instance().register_signal(
                "elegoo:shutdown",std::function<void()>([this](){ canvas_shutdown(); }));
            elegoo::common::SignalManager::get_instance().register_signal(
                "por:power_off",
                std::function<void()>([this]() { 
                    canvas_shutdown(); 
                })
            );
            elegoo::common::SignalManager::get_instance().register_signal(
                "elegoo:ready",
                std::function<void()>([this](){
                    canvas_handle_ready();
                })
            );
            //
            buttons = any_cast<std::shared_ptr<PrinterButtons>>(printer->load_object(config, "buttons"));
            std::string filament_det_pin = config->get("filament_det_pin");
            std::string nozzle_fan_off_pin = config->get("nozzle_fan_off_pin");
            std::string cutting_knife_pin = config->get("cutting_knife_pin");
            std::string wrap_filament_pin = config->get("wrap_filament_pin");
            std::string model_det_pin = config->get("model_det_pin");
            SPDLOG_INFO("filament_det_pin:{},nozzle_fan_off_pin:{},cutting_knife_pin:{},wrap_filament_pin:{},model_det_pin:{}",filament_det_pin,nozzle_fan_off_pin,cutting_knife_pin,wrap_filament_pin,model_det_pin);
            buttons->register_buttons({filament_det_pin}
                , [this](double eventtime, bool state) {
                    return this->canvas_filament_det_handler(eventtime, state);
                }
            );
            buttons->register_buttons({nozzle_fan_off_pin}
                , [this](double eventtime, bool state) {
                    return this->canvas_nozzle_fan_off_handler(eventtime, state);
                }
            );
            buttons->register_buttons({cutting_knife_pin}
                , [this](double eventtime, bool state) {
                    return this->canvas_cutting_knife_handler(eventtime, state);
                }
            );
            buttons->register_buttons({wrap_filament_pin}
                , [this](double eventtime, bool state) {
                    return this->canvas_wrap_filament_handler(eventtime, state);
                }
            );
            buttons->register_buttons({model_det_pin}
                , [this](double eventtime, bool state) {
                    return this->canvas_model_det_handler(eventtime, state);
                }
            );
            //
            this->canvas_timer = reactor->register_timer(
                [this](double eventtime){ 
                    return canvas_callback(eventtime); 
                }
                , _NEVER, "canvas timer"
            );
            this->abnormal_det_timer = reactor->register_timer(
                [this](double eventtime){ 
                    return abnormal_process(eventtime); 
                }
                , _NEVER, "abnormal det timer"
            );
            //
            this->gcode = any_cast<std::shared_ptr<GCodeDispatch>>(this->printer->lookup_object("gcode"));
            this->gcode->register_command(
                    "USED_CANVAS_DEV"
                    ,[this](std::shared_ptr<GCodeCommand> gcmd){
                        CMD_used_canvas_dev(gcmd);
                    }
                    ,false
                    ,"USED CANVAS DECV");
            this->gcode->register_command(
                    "CANVAS_PRELOAD_FILAMENT"
                    ,[this](std::shared_ptr<GCodeCommand> gcmd){
                        CMD_canvas_preload_filament(gcmd);
                    }
                    ,false
                    ,"CANVAS PRELOAD FILAMENT");
            this->gcode->register_command(
                    "CANVAS_LOAD_FILAMENT"
                    ,[this](std::shared_ptr<GCodeCommand> gcmd){
                        CMD_canvas_load_filament(gcmd);
                    }
                    ,false
                    ,"CANVAS LOAD FILAMENT");
            this->gcode->register_command(
                    "CANVAS_UNLOAD_FILAMENT"
                    ,[this](std::shared_ptr<GCodeCommand> gcmd){
                        CMD_canvas_unload_filament(gcmd);
                    }
                    ,false
                    ,"CANVAS UNLOAD FILAMENT");
            this->gcode->register_command(
                    "CANVAS_MOVE_TO_WASTE_BOX"
                    ,[this](std::shared_ptr<GCodeCommand> gcmd){
                        CMD_move_to_waste_box(gcmd);
                    }
                    ,false
                    ,"CANVAS MOVE TO WASTE BOX");
            this->gcode->register_command(
                    "CANVAS_CLEAN_WASTE_FILAMENT"
                    ,[this](std::shared_ptr<GCodeCommand> gcmd){
                        CMD_clean_waste_filament(gcmd);
                    }
                    ,false
                    ,"CANVAS CLEAN WASTE FILAMENT");
            this->gcode->register_command(
                    "CANVAS_DETECT_FILAMENT"
                    ,[this](std::shared_ptr<GCodeCommand> gcmd){
                        CMD_canvas_detect_filament(gcmd);
                    }
                    ,false
                    ,"CANVAS DETECT FILAMENT");
            this->gcode->register_command(
                    "CANVAS_CUT_FILAMENT"
                    ,[this](std::shared_ptr<GCodeCommand> gcmd){
                        CMD_canvas_cut_filament(gcmd);
                    }
                    ,false
                    ,"CANVAS CUT FILAMENT");
            this->gcode->register_command(
                    "CANVAS_PLUG_OUT_FILAMENT"
                    ,[this](std::shared_ptr<GCodeCommand> gcmd){
                        CMD_canvas_plug_out_filament(gcmd);
                    }
                    ,false
                    ,"CANVAS PLUG OUT FILAMENT");
            this->gcode->register_command(
                    "CANVAS_PLUG_IN_FILAMENT"
                    ,[this](std::shared_ptr<GCodeCommand> gcmd){
                        CMD_canvas_plug_in_filament(gcmd);
                    }
                    ,false
                    ,"CANVAS PLUG IN FILAMENT");
            this->gcode->register_command(
                    "CANVAS_MESH_GEAR"
                    ,[this](std::shared_ptr<GCodeCommand> gcmd){
                        CMD_canvas_channel_motor_ctrl(gcmd);
                    }
                    ,false
                    ,"CANVAS MESH GEAR");
            this->gcode->register_command(
                    "CANVAS_SET_FILAMENT_INFO"
                    ,[this](std::shared_ptr<GCodeCommand> gcmd){
                        CMD_canvas_set_filament_info(gcmd);
                    }
                    ,false
                    ,"CANVAS SET FILAMENT INFO");
            this->gcode->register_command(
                    "CANVAS_RFID_SELECT"
                    ,[this](std::shared_ptr<GCodeCommand> gcmd){
                        CMD_canvas_rfid_select(gcmd);
                    }
                    ,false
                    ,"CANVAS RFID SELECT");
            this->gcode->register_command(
                    "CANVAS_RFID_SCAN"
                    ,[this](std::shared_ptr<GCodeCommand> gcmd){
                        CMD_canvas_rfid_scan(gcmd);
                    }
                    ,false
                    ,"CANVAS RFID SCAN");
            this->gcode->register_command(
                    "CANVAS_RFID_CANCEL_SCAN"
                    ,[this](std::shared_ptr<GCodeCommand> gcmd){
                        CMD_canvas_rfid_cancel_scan(gcmd);
                    }
                    ,false
                    ,"CANVAS RFID CANCEL SCAN");
            this->gcode->register_command(
                    "CANVAS_SET_LED_STATUS"
                    ,[this](std::shared_ptr<GCodeCommand> gcmd){
                        CMD_canvas_set_led_status(gcmd);
                    }
                    ,false
                    ,"CANVAS SET LED STATUS");
            this->gcode->register_command(
                    "CANVAS_SET_AUTO_PLUG_IN"
                    ,[this](std::shared_ptr<GCodeCommand> gcmd){
                        CMD_canvas_set_auto_plug_in(gcmd);
                    }
                    ,false
                    ,"CANVAS SET AUTO PLUG IN FILAMENT");
            this->gcode->register_command(
                    "CANVAS_SET_COLOR_TABLE"
                    ,[this](std::shared_ptr<GCodeCommand> gcmd){
                        CMD_canvas_set_color_table(gcmd);
                    }
                    ,false
                    ,"CANVAS SET COLOR TABLE");
            this->gcode->register_command(
                    "CANVAS_SWITCH_FILAMENT"
                    // "T"
                    ,[this](std::shared_ptr<GCodeCommand> gcmd){
                        CMD_canvas_switch_filament(gcmd);
                    }
                    ,false
                    ,"CANVAS SWITCH FILAMENT");
            this->gcode->register_command(
                    "CANVAS_ABNORMAL_RETRY"
                    ,[this](std::shared_ptr<GCodeCommand> gcmd){
                        CMD_canvas_abnormal_retry(gcmd);
                    }
                    ,false
                    ,"CANVAS ABNORMAL RETRY");
            this->gcode->register_command(
                    "ABNORMAL_STATE_PROCESS"
                    ,[this](std::shared_ptr<GCodeCommand> gcmd){
                        CMD_abnormal_state_process(gcmd);
                    }
                    ,false
                    ,"CANVAS STATE PROCESS");

            SPDLOG_INFO("{} #1 OK__",__func__);
        }

        CanvasDev::~CanvasDev()
        {
            SPDLOG_INFO("~CanvasDev");
        }

        void CanvasDev::canvas_init()
        {
            this->status = 0;
            this->rfid = {};
            this->feeders.resize(CHANNEL_NUM,{});
            this->feeders_leds = {};
            this->rfid_scan_uid = 0;
            this->is_rfid_trig = false;
            this->lite_connected = false;
            this->updating = false;
            
            this->canvas_status_info = {};
            canvas_status_info.host_status.fila_channel = -1;
            canvas_status_info.host_status.switch_filment_T = -1;
            
            this->canvas_dev = {};
            this->channel_color_table = {};
            this->need_save_record = 0;
            this->is_abnormal_fan_off = false;
            this->is_plug_in_filament_over = false;
            this->is_need_abnormal_process = 0;
            this->is_sent_led_ctrl = false;
            this->printing_used_canvas = false;
            this->comminucation_thread = nullptr;
        }

        std::string CanvasDev::uint32_to_hex_string(uint32_t value,std::string hex_prefix,char ch,int32_t hex_w,bool is_uppercase)
        {
            std::stringstream ss;
            if(!hex_prefix.empty())
            {
                ss << hex_prefix;               // 前缀
            }

            if(is_uppercase)
            {
                ss << std::uppercase;           // 输出大写
            }
            else
            {
                ss << std::nouppercase;           // 输出小写
            }

            ss << std::hex                      // 切换到16进制模式
            << std::setfill(ch)                 // 填充字符设为 ch
            << std::setw(hex_w)                 // 固定输出宽度为 hex_w
            << value;
            
            return ss.str();
        }

        int32_t CanvasDev::get_cavans_rfid_info()
        {
            Filament fila = {};
            {
                std::lock_guard<std::mutex> lock(rfid_mtx);
                if (0 == canvas_status_info.mcu_status.scan_rfid_uid)
                {
                    return -1;
                }
                else
                {
                    memcpy((uint8_t *)&fila, (uint8_t *)&rfid.data[16], 32);
                    rfid.uid = 0; // Clear
                    rfid_scan_uid = 0;
                }
            }

            filament_info.brand = uint32_to_hex_string(be32toh(fila.brand),"0x");
            if("0xEEEEEEEE" == filament_info.brand)
            {
                filament_info.manufacturer = "ELEGOO";
            }
            filament_info.code = uint32_to_hex_string(be16toh(fila.code),"0x",'0',4);
            filament_info.type = uint32_to_hex_string(be32toh(fila.type),"0x");
            SPDLOG_INFO("filament_info.type {} second {}",filament_info.type,filament_type.find(be32toh(fila.type))->second);
            filament_info.type = filament_type.find(be32toh(fila.type))->second;
            filament_info.detailed_type = uint32_to_hex_string(be16toh(fila.name),"0x",'0',4);
            SPDLOG_INFO("filament_info.detailed_type {} second {}",filament_info.detailed_type,filament_detail_type.find(be16toh(fila.name))->second);
            filament_info.detailed_type = filament_detail_type.find(be16toh(fila.name))->second;
            filament_info.color = uint32_to_hex_string(fila.rgb[0],"0x",'0',2) + uint32_to_hex_string(fila.rgb[1],"",'0',2) + uint32_to_hex_string(fila.rgb[2],"",'0',2);
            filament_info.diameter = std::to_string(be16toh(fila.diameter)/100.0);
            filament_info.weight = std::to_string(be16toh(fila.weight));
            filament_info.date = uint32_to_hex_string((be16toh(fila.date) >> 8) & 0xFF,"20",'0',2) + "/" + uint32_to_hex_string(be16toh(fila.date) & 0x00FF,"",'0',2);
            filament_info.nozzle_min_temp = std::to_string(be16toh(fila.low_tmp));
            filament_info.nozzle_max_temp = std::to_string(be16toh(fila.hig_tmp));

            SPDLOG_DEBUG("filament_info.manufacturer:{}",filament_info.manufacturer);
            SPDLOG_DEBUG("filament_info.brand:{}",filament_info.brand);
            SPDLOG_DEBUG("filament_info.code:{}",filament_info.code);
            SPDLOG_DEBUG("filament_info.type:{}",filament_info.type);
            SPDLOG_DEBUG("filament_info.detailed_type:{}",filament_info.detailed_type);
            SPDLOG_DEBUG("filament_info.color:{}",filament_info.color);
            SPDLOG_DEBUG("filament_info.diameter:{}",filament_info.diameter);
            SPDLOG_DEBUG("filament_info.weight:{}",filament_info.weight);
            SPDLOG_DEBUG("filament_info.date:{}",filament_info.date);
            SPDLOG_DEBUG("filament_info.nozzle_min_temp:{}",filament_info.nozzle_min_temp);
            SPDLOG_DEBUG("filament_info.nozzle_max_temp:{}",filament_info.nozzle_max_temp);

            return 0;
        }

        void CanvasDev::get_cavans_mcu_info()
        {
            // static int32_t retry_count = 0;
            // if(retry_count < 2)
            // {
            //     if(lite_connected)
            //     {
            //         retry_count++;
            //         SPDLOG_DEBUG("{} lite_connected:{} retry_count:{}",__func__,lite_connected,retry_count);
            //     }
            //     else
            //     {
            //         retry_count = 0;
            //         // SPDLOG_DEBUG("{} lite_connected:{} retry_count:{}",__func__,lite_connected,retry_count);
            //         canvas_status_info.mcu_status.last_mcu_connect_status = canvas_status_info.mcu_status.cur_mcu_connect_status;
            //         canvas_status_info.mcu_status.cur_mcu_connect_status = lite_connected;
            //     }
            // }
            // else
            // {
            //     if(!lite_connected)
            //     {
            //         SPDLOG_DEBUG("{} lite_connected:{} retry_count:{}",__func__,lite_connected,retry_count);
            //         retry_count = 0;
            //     }
                canvas_status_info.mcu_status.last_mcu_connect_status = canvas_status_info.mcu_status.cur_mcu_connect_status;
                canvas_status_info.mcu_status.cur_mcu_connect_status = lite_connected;
            // }
            canvas_status_info.mcu_status.type = "lite";
            canvas_status_info.mcu_status.did = 0;
            if(canvas_status_info.mcu_status.cur_mcu_connect_status && feeders.size() == CHANNEL_NUM)
            {
                // std::lock_guard<std::mutex> lock(mcu_status_mtx);
                for (std::size_t i = 0; i < CHANNEL_NUM; i++)
                {
                    canvas_status_info.mcu_status.moving_status[i] = feeders[i].dragging;
                    canvas_status_info.mcu_status.motor_stalling_status[i] = static_cast<int32_t>(feeders[i].motor_fault);
                    canvas_status_info.mcu_status.last_ch_fila_status[i] = canvas_status_info.mcu_status.ch_fila_status[i];
                    canvas_status_info.mcu_status.ch_fila_status[i] = static_cast<int32_t>(feeders[i].fila_in);
                    canvas_status_info.mcu_status.ch_position[i] = static_cast<double>(feeders[i].odo_value32) * 9.4 * M_PI / (2. * gear_pole_number);
                    canvas_status_info.mcu_status.scan_rfid_uid = rfid_scan_uid;
                    canvas_status_info.mcu_status.ch_fila_move_dist[i] = static_cast<double>(feeders[i].encoder_value32) * 9.4 * M_PI / (2. * motor_pole_number);

                    if(1 == canvas_status_info.mcu_status.ch_fila_status[i] && canvas_status_info.mcu_status.last_ch_fila_status[i] != canvas_status_info.mcu_status.ch_fila_status[i])
                    {
                        canvas_status_info.host_status.pre_load_ch_status[i] = 1;
                        SPDLOG_DEBUG("{} i:{} pre_load_ch_status:{} ch_fila_status:{} last_ch_fila_status:{}",__func__,i,canvas_status_info.host_status.pre_load_ch_status[i],canvas_status_info.mcu_status.ch_fila_status[i],canvas_status_info.mcu_status.last_ch_fila_status[i]);
                    }
                }
            }
            else
            {
                for (size_t i = 0; i < CHANNEL_NUM; i++)
                {
                    canvas_status_info.host_status.pre_load_ch_status[i] = 3;
                    canvas_status_info.mcu_status.last_ch_fila_status[i] = canvas_status_info.mcu_status.ch_fila_status[i] = 1;
                }
            }

            return;
        }
        
        int32_t CanvasDev::send_with_respone(int32_t serial_cmd,Any param,double wait_time)
        {
            // SPDLOG_INFO("{} #1 serial_cmd:{} wait_time:{}",__func__,serial_cmd,wait_time);
            int32_t ret = 0;
            std::tuple<bool,uint32_t,uint32_t> rdid_cmd_param = {};
            std::vector<int32_t> motor_control_para = {};
            FilamentControl fila_ctrl = {};
            std::array<FilamentControl, CHANNEL_NUM> filament_control = {};
            switch (serial_cmd)
            {
            case SERIAL_CMD_MOTOR_CONTROL:
                motor_control_para = any_cast<std::vector<int32_t>>(param);
                if(5 != motor_control_para.size())
                {
                    SPDLOG_ERROR("motor_control_para.size:{}",motor_control_para.size());
                    return -1;
                }
                SPDLOG_INFO("{} #1 ch:{} mm:{} mm_s:{} mm_ss:{} wait_time:{}",__func__,motor_control_para[3],motor_control_para[0],motor_control_para[1],motor_control_para[2],wait_time);
                if(motor_control_para[3] >= 0
                    && motor_control_para[3] < 4)
                {
                    double cur_feeder_time = get_monotonic();
                    ret = feeder_filament_control(motor_control_para[3] + 1,FilamentControl({(int16_t)motor_control_para[0],(int16_t)motor_control_para[1],(int16_t)motor_control_para[2]}));
                    // SPDLOG_DEBUG("ret:{} moving_status[motor_control_para[3]:{}]:{} wait_time:{}",ret,motor_control_para[3],canvas_status_info.mcu_status.moving_status[motor_control_para[3]],wait_time);
                    // if(ret)
                    {
                        if(0. == wait_time)
                        {
                            return 0;
                        }
                        reactor->pause(get_monotonic() + 0.5);
                        sync_det_info();
                        // SPDLOG_DEBUG("moving_status[motor_control_para[3]:{}]:{} wait_time:{}",motor_control_para[3],canvas_status_info.mcu_status.moving_status[motor_control_para[3]],wait_time);
                        while (canvas_status_info.mcu_status.moving_status[motor_control_para[3]])
                        {
                            if(get_monotonic() >= cur_feeder_time + wait_time)
                            {
                                SPDLOG_WARN("moving_status[motor_control_para[3]:{}]:{} wait_time:{} timeout return -1",motor_control_para[3],canvas_status_info.mcu_status.moving_status[motor_control_para[3]],wait_time);
                                return -1;
                            }
                            reactor->pause(get_monotonic() + 0.5);
                            sync_det_info();
                        }
                        // SPDLOG_DEBUG("moving_status[motor_control_para[3]:{}]:{} wait_time:{} return 0",motor_control_para[3],canvas_status_info.mcu_status.moving_status[motor_control_para[3]],wait_time);
                        return 0;
                    }
                }
                else if(4 != motor_control_para[3])
                {
                    SPDLOG_ERROR("4 != motor_control_para[3]:{}",motor_control_para[3]);
                    return -1;
                }
                else
                {
                    for (size_t i = 0; i < CHANNEL_NUM; i++)
                    {
                        filament_control[i].mm = (int16_t)motor_control_para[0];
                        filament_control[i].mm_s = (int16_t)motor_control_para[1];
                        filament_control[i].mm_ss = (int16_t)motor_control_para[2];
                    }
                    
                    ret = feeders_filament_control(filament_control);
                    SPDLOG_DEBUG("ret:{} motor_control_para[3]:{} moving_status[0]:{}",ret,motor_control_para[3],canvas_status_info.mcu_status.moving_status[0]);
                    // if(ret)
                    {
                        if(0. == wait_time)
                        {
                            return 0;
                        }
                        reactor->pause(get_monotonic() + wait_time);
                        sync_det_info();
                        if(false != canvas_status_info.mcu_status.moving_status[0]
                            && false != canvas_status_info.mcu_status.moving_status[1]
                            && false != canvas_status_info.mcu_status.moving_status[2]
                            && false != canvas_status_info.mcu_status.moving_status[3]
                            )
                        {
                            SPDLOG_WARN("moving_status[0]:{},moving_status[1]:{},moving_status[2]:{},moving_status[3]:{}",canvas_status_info.mcu_status.moving_status[0],canvas_status_info.mcu_status.moving_status[1],canvas_status_info.mcu_status.moving_status[2],canvas_status_info.mcu_status.moving_status[3]);
                            return -1;
                        }
                        return 0;
                    }
                }
                break;
            default:
                break;
            }
            return 0;
        }

        double CanvasDev::canvas_filament_det_handler(double eventtime, bool state)
        {
            this->canvas_status_info.host_status.ex_fila_status = state;
            if(canvas_status_info.host_status.ex_fila_status && canvas_status_info.host_status.is_runout_filament_report)
            {
                canvas_status_info.host_status.is_runout_filament_report = false;
                SPDLOG_INFO("{} state:{} ex_fila_status:{} is_runout_filament_report:{}",__func__,state,canvas_status_info.host_status.ex_fila_status,canvas_status_info.host_status.is_runout_filament_report);
            }
            state_feedback("CANVAS_FILA_DET_FEEDBACK",std::to_string(canvas_status_info.host_status.ex_fila_status));
            if(0 == canvas_status_info.host_status.ex_fila_status)
            {
                reactor->update_timer(abnormal_det_timer,_NOW);
            }
            return 0.0;
        }

        double CanvasDev::canvas_nozzle_fan_off_handler(double eventtime, bool state)
        {
            this->canvas_status_info.host_status.nozzle_fan_off_status = state;
            state_feedback("CANVAS_FAN_OFF_FEEDBACK",std::to_string(canvas_status_info.host_status.nozzle_fan_off_status));
            if(1 == canvas_status_info.host_status.nozzle_fan_off_status)
            {
                reactor->update_timer(abnormal_det_timer,_NOW);
            }
            return 0.0;
        }

        double CanvasDev::canvas_cutting_knife_handler(double eventtime, bool state)
        {
            this->canvas_status_info.host_status.cutting_knife_status = state;
            state_feedback("CANVAS_CUT_KNIFT_FEEDBACK",std::to_string(canvas_status_info.host_status.cutting_knife_status));
            // if(0 == canvas_status_info.host_status.cutting_knife_status)
            // {
            //     reactor->update_timer(abnormal_det_timer,_NOW);
            // }
            return 0.0;
        }

        double CanvasDev::canvas_wrap_filament_handler(double eventtime, bool state)
        {
            this->canvas_status_info.host_status.wrap_filament_status = state;
            state_feedback("CANVAS_WRAP_FILA_FEEDBACK",std::to_string(canvas_status_info.host_status.wrap_filament_status));
            if(1 == canvas_status_info.host_status.wrap_filament_status)
            {
                reactor->update_timer(abnormal_det_timer,_NOW);
            }
            return 0.0;
        }

        double CanvasDev::canvas_model_det_handler(double eventtime, bool state)
        {
            // this->canvas_status_info.host_status.wrap_filament_status = state;
            SPDLOG_INFO("{} state:{} update_timer(abnormal_det_timer,_NOW)",__func__,state);
            // reactor->update_timer(abnormal_det_timer,_NOW);
            return 0.0;
        }

        void CanvasDev::handle_connect()
        {
            SPDLOG_DEBUG("{}()", __func__);
            // Serial Open
        }

        void CanvasDev::handle_disconnect()
        {
            SPDLOG_DEBUG("{}()", __func__);
            // Serial Close
        }

        void CanvasDev::canvas_shutdown()
        {
            SPDLOG_DEBUG("{}()", __func__);
            // if(this->need_save_record > 0)
            {
                save_record_info();
            }
        }

        void CanvasDev::canvas_handle_ready()
        {
            print_stats = any_cast<std::shared_ptr<PrintStats>>(printer->load_object(config, "print_stats"));
            idle_timeout = any_cast<std::shared_ptr<IdleTimeout>>(printer->lookup_object("idle_timeout"));
            pause_on_abnormal = config->getboolean("pause_on_abnormal",BoolValue::BOOL_TRUE);
            if (pause_on_abnormal)
            {
                SPDLOG_INFO("pause_on_abnormal {}",pause_on_abnormal);
                pause_resume = any_cast<std::shared_ptr<PauseResume>>(printer->load_object(config, "pause_resume"));
            }
            //
            std::shared_ptr<PrinterGCodeMacro> gcode_macro = any_cast<std::shared_ptr<PrinterGCodeMacro>>(printer->load_object(config, "gcode_macro"));
            if (pause_on_abnormal || !config->get("runout_filament_gcode","").empty())
            {
                SPDLOG_INFO("obtain runout_filament_gcode");
                runout_filament_gcode = gcode_macro->load_template(config, "runout_filament_gcode", "\n");
            }
            move_to_waste_box_macro = gcode_macro->load_template(config, "move_to_waste_box_macro", "\n");
            clean_waste_filament_macro = gcode_macro->load_template(config, "clean_waste_filament_macro", "\n");
            filament_load_gcode_1 = gcode_macro->load_template(config, "filament_load_gcode_1", "\n");
            filament_load_gcode_2 = gcode_macro->load_template(config, "filament_load_gcode_2", "\n");
            //
            this->gear_pole_number = config->getint("gear_pole_number",5);
            this->motor_pole_number = config->getint("motor_pole_number",2);
            this->motor_speed = config->getint("motor_speed",100,10,1000);
            this->motor_accel = config->getint("motor_accel",100,10,1000);
            this->pre_load_fila_dist = config->getint("pre_load_fila_dist",600,0,1000);
            this->pre_load_fila_time = config->getint("pre_load_fila_time",10,0,60);
            this->load_fila_dist = config->getint("load_fila_dist",50,0,1000);
            this->load_fila_time = config->getint("load_fila_time",3,0,60);
            this->unload_fila_dist = config->getint("unload_fila_dist",80,0,1000);
            this->unload_fila_time = config->getint("unload_fila_time",3,0,60);
            this->fila_det_dist = config->getint("fila_det_dist",30,0,1000);
            this->fila_det_time = config->getint("fila_det_time",3,0,60);
            this->mesh_gear_dist = config->getint("mesh_gear_dist",10,0,1000);
            this->mesh_gear_time = config->getint("mesh_gear_time",3,0,60);
            SPDLOG_INFO("motor_speed,motor_accel {} {} {} {} {} {} {}",motor_speed,motor_accel,pre_load_fila_dist,load_fila_dist,unload_fila_dist,fila_det_dist,mesh_gear_dist);
            //
            reactor->update_timer(canvas_timer,get_monotonic());
            reactor->update_timer(abnormal_det_timer,get_monotonic());
            //
            rs485->start();
            comminucation_thread = std::make_shared<std::thread>(&CanvasDev::canvas_communication, this);
            comminucation_thread->detach();

            update_canvas_state(0,0);
            try
            {
                read_record_info();
            }
            catch(...)
            {
                SPDLOG_WARN("delete canvas_record_info.json");
                system("rm /opt/usr/canvas_record_info.json");
            }
            
            set_fila_origin(0);
            
            SPDLOG_INFO("{}() __OVER", __func__);
        }

        double CanvasDev::abnormal_process(double eventtime)
        {
            bool is_idle_printing = idle_timeout->get_status(eventtime)["state"].get<std::string>() == "Printing";
            bool is_printing = print_stats->get_status(eventtime)["state"].get<std::string>() == "printing";
            // starting_usage = print_stats->get_status(eventtime)["filament_used"].get<double>();

            if(is_idle_printing && is_printing)
            {
                // 断料处理
                if(false == canvas_status_info.host_status.is_runout_filament_report
                    && true == is_plug_in_filament_over
                    && false == canvas_status_info.host_status.is_switch_and_clean_filment
                    && false == canvas_status_info.host_status.ex_fila_status
                    )
                {
                    SPDLOG_INFO("is_plug_in_filament_over:{} is_runout_filament_report:{} is_switch_and_clean_filment:{} fila_origin:{} ch_status:{} {} {} {} ex_fila_status:{}",is_plug_in_filament_over,canvas_status_info.host_status.is_runout_filament_report,canvas_status_info.host_status.is_switch_and_clean_filment,canvas_status_info.host_status.fila_origin,canvas_status_info.host_status.ch_status[0],canvas_status_info.host_status.ch_status[1],canvas_status_info.host_status.ch_status[2],canvas_status_info.host_status.ch_status[3],canvas_status_info.host_status.ex_fila_status);
                    canvas_status_info.host_status.is_runout_filament_report = true;
                    is_need_abnormal_process += 0x01;
                    canvas_error_code = elegoo::common::ErrorCode::CANVAS_RUNOUT;
                    SPDLOG_INFO("{} auto_plug_in_enable:{} is_need_plut_in_filment:{}",__func__,canvas_status_info.host_status.auto_plug_in_enable,canvas_status_info.host_status.is_need_plut_in_filment);
                    if(true == canvas_status_info.host_status.auto_plug_in_enable)
                    {
                        canvas_status_info.host_status.is_need_plut_in_filment = true;
                    }
                    else
                    {
                        canvas_status_info.host_status.is_need_plut_in_filment = false;
                    }
                }
                // 缠料处理
                if(true == canvas_status_info.host_status.wrap_filament_status)
                {
                    canvas_status_info.host_status.is_wrap_filament_report = true;
                    is_need_abnormal_process += 0x02;
                    canvas_error_code = elegoo::common::ErrorCode::CANVAS_WRAP_FILA;
                }
                else
                {
                    is_need_abnormal_process -= 0x02;
                    canvas_status_info.host_status.is_wrap_filament_report = false;
                }
                // 堵料处理
                // if(true == canvas_status_info.host_status.locked_rotor_status)
                // {
                //     canvas_status_info.host_status.is_locked_rotor_report = true;
                //     is_need_abnormal_process += 0x04;
                    // canvas_error_code = elegoo::common::ErrorCode::CANVAS_LOCKED_ROTOR;
                // }
                // else
                // {
                    // is_need_abnormal_process -= 0x04;
                //     canvas_status_info.host_status.is_locked_rotor_report = false;
                // }
                // 切刀处理
                // if(true == canvas_status_info.host_status.cutting_knife_status)
                // {
                //     canvas_status_info.host_status.is_cutting_knife_report = true;
                //     is_need_abnormal_process += 0x08;
                    // canvas_error_code = elegoo::common::ErrorCode::CANVAS_CUTTING_KNIFET;
                // }
                // else
                // {
                    // is_need_abnormal_process -= 0x08;
                //     canvas_status_info.host_status.is_cutting_knife_report = false;
                // }
                // 风扇脱落处理
                if(true == canvas_status_info.host_status.nozzle_fan_off_status)
                {
                    canvas_status_info.host_status.is_nozzle_fan_off_report = true;
                    is_need_abnormal_process += 0x16;
                    canvas_error_code = elegoo::common::ErrorCode::CANVAS_FAN_OFF;
                }
                else
                {
                    is_need_abnormal_process -= 0x16;
                    canvas_status_info.host_status.is_nozzle_fan_off_report = false;
                }
                // 异常状态处理
                if(is_need_abnormal_process)
                {
                    canvas_status_info.host_status.cur_retry_abnormal_det = is_need_abnormal_process;
                    is_need_abnormal_process = 0;
                    SPDLOG_INFO("{} is_need_abnormal_process:{} cur_retry_abnormal_det:{}",__func__,is_need_abnormal_process,canvas_status_info.host_status.cur_retry_abnormal_det);
                    if((true == canvas_status_info.host_status.is_nozzle_fan_off_report
                            || (true == canvas_status_info.host_status.is_runout_filament_report && true == is_plug_in_filament_over)
                            || true == canvas_status_info.host_status.is_wrap_filament_report
                            // || true == canvas_status_info.host_status.is_locked_rotor_report
                            )
                        )
                    {
                        gcode->run_script("ABNORMAL_STATE_PROCESS\nM400");
                        SPDLOG_WARN("{} canvas_error_code:{} canvas_abnormal_state:{},canvas_stage_state:{}",__func__,canvas_error_code,canvas_abnormal_state,canvas_stage_state);
                    }
                }
            }
            return eventtime + 3.;
        }

        double CanvasDev::canvas_callback(double eventtime)
        {
            bool is_idle_printing = idle_timeout->get_status(0.)["state"].get<std::string>() == "Printing";
            bool is_printing = print_stats->get_status(get_monotonic())["state"].get<std::string>() == "printing";
            bool is_paused = pause_resume->get_status(0.)["is_paused"].get<bool>();
            sync_det_info();
            serial_connect_det();
            // if(!is_paused && is_sent_led_ctrl)
            // {
            //     SPDLOG_INFO("{} is_paused:{} is_sent_led_ctrl:{} led_status_control(CHANNEL_NUM)",__func__,is_paused,is_sent_led_ctrl);
            //     led_status_control(CHANNEL_NUM);
            // }
            if(this->canvas_status_info.mcu_status.cur_mcu_connect_status)
            {
                if(0x01 == is_abnormal_error)
                {
                    is_abnormal_error = 0;
                    SPDLOG_DEBUG("{} canvas_abnormal_state:{} cur_retry_action:{} is_abnormal_error:{}",__func__,canvas_abnormal_state,canvas_status_info.host_status.cur_retry_action,is_abnormal_error);
                    gcode->run_script("ABNORMAL_STATE_PROCESS\nM400");
                }
                rfid_trig_det();
                fila_origin_det(eventtime);
                
#if 1
                if(0 != this->need_save_record && !is_idle_printing && !is_printing)
                {
                    ++this->need_save_record;
                    if(this->need_save_record >= 10)
                    {
                        SPDLOG_INFO("{} #1 need_save_record:{}",__func__,this->need_save_record);
                        this->need_save_record = 0;
                        auto self = shared_from_this();
                        std::thread([self]() { if(self) { self->save_record_info(); } }).detach();
                    }
                }
#endif
            }
            else if(true == canvas_status_info.mcu_status.last_mcu_connect_status)
            {
                if(is_idle_printing && is_printing && this->printing_used_canvas)
                {
                    canvas_error_code = elegoo::common::ErrorCode::CANVAS_SERIAL_ERROR;
                    gcode->run_script("ABNORMAL_STATE_PROCESS\nM400");
                }
            }
            state_det_process(eventtime);

            return eventtime + 1.;
        }

        void CanvasDev::save_record_info()
        {
            json record_info;
            record_info["auto_plug_in_enable"] = canvas_status_info.host_status.auto_plug_in_enable;
            for(int32_t ii = 0; ii <= CHANNEL_NUM; ++ii)
            {
                std::string channels_tmp = {};
                filament_channels_t channel_tmp = {};
                if(ii == CHANNEL_NUM)
                {
                    channels_tmp = "extern_channel";
                    channel_tmp = canvas_dev.canvas_lite.extern_channel;
                }
                else
                {
                    channels_tmp = "channels" + std::to_string(ii);
                    channel_tmp = canvas_dev.canvas_lite.channels[ii];
                }
                SPDLOG_DEBUG("{} channels_tmp:{},channel_tmp.filament.color:{}",__func__,channels_tmp,channel_tmp.filament.color);
                // filament
                record_info[channels_tmp]["cid"] = channel_tmp.cid;
                record_info[channels_tmp]["status"] = channel_tmp.status;
                record_info[channels_tmp]["manufacturer"] = channel_tmp.filament.manufacturer;
                record_info[channels_tmp]["brand"] = channel_tmp.filament.brand;
                record_info[channels_tmp]["code"] = channel_tmp.filament.code;
                record_info[channels_tmp]["type"] = channel_tmp.filament.type;
                record_info[channels_tmp]["detailed_type"] = channel_tmp.filament.detailed_type;
                record_info[channels_tmp]["color"] = channel_tmp.filament.color;
                record_info[channels_tmp]["diameter"] = channel_tmp.filament.diameter;
                record_info[channels_tmp]["weight"] = channel_tmp.filament.weight;
                record_info[channels_tmp]["date"] = channel_tmp.filament.date;
                record_info[channels_tmp]["nozzle_min_temp"] = channel_tmp.filament.nozzle_min_temp;
                record_info[channels_tmp]["nozzle_max_temp"] = channel_tmp.filament.nozzle_max_temp;
            }

            record_info["color_table_size"] = channel_color_table.size();
            for(auto ii = 0; ii < channel_color_table.size(); ++ii)
            {
                record_info["color_table" + std::to_string(ii)]["T"] = std::get<0>(channel_color_table[ii]);
                record_info["color_table" + std::to_string(ii)]["color"] = std::get<1>(channel_color_table[ii]);
                record_info["color_table" + std::to_string(ii)]["channel"] = std::get<2>(channel_color_table[ii]);
                SPDLOG_DEBUG("{} T:{} color:{} channel:{}","color_table" + std::to_string(ii),std::get<0>(channel_color_table[ii]),std::get<1>(channel_color_table[ii]),std::get<2>(channel_color_table[ii]));
            }
        
            std::string file_name = RECORD_INFO_INI;
            std::ofstream outfile(file_name);
            if (!outfile.is_open())
            {
                SPDLOG_WARN("Unable to open file {} for writing",RECORD_INFO_INI);
                return;
            }
            outfile << record_info.dump(4);
            outfile.flush();
            outfile.close();
            SPDLOG_INFO("{}() __OVER", __func__);
        }

        void CanvasDev::read_record_info()
        {
            SPDLOG_INFO("{} #1", __func__);
            std::string file_name = RECORD_INFO_INI;
            std::ifstream infile(file_name);
            if (!infile.is_open())
            {
                SPDLOG_WARN("Unable to open file {}",RECORD_INFO_INI);
                return;
            }

            json jsonData;
            infile >> jsonData;
            infile.close();

            SPDLOG_INFO("jsonData:{}",jsonData.dump());

            if(jsonData.contains("auto_plug_in_enable"))
            {
                canvas_status_info.host_status.auto_plug_in_enable = jsonData["auto_plug_in_enable"].get<uint8_t>();
                canvas_dev.auto_plug_in_enable = canvas_status_info.host_status.auto_plug_in_enable;
                // SPDLOG_DEBUG("jsonData is contains 'auto_plug_in_enable' {}",canvas_status_info.host_status.auto_plug_in_enable);
            }
            
            for(int32_t ii = 0; ii < CHANNEL_NUM; ++ii)
            {
                if(jsonData.contains("channels" + std::to_string(ii)))
                {
                    canvas_dev.canvas_lite.channels[ii].cid = jsonData["channels" + std::to_string(ii)]["cid"].get<int32_t>();
                    canvas_dev.canvas_lite.channels[ii].status = jsonData["channels" + std::to_string(ii)]["status"].get<int32_t>();
                    canvas_dev.canvas_lite.channels[ii].filament.manufacturer = jsonData["channels" + std::to_string(ii)]["manufacturer"].get<std::string>();
                    canvas_dev.canvas_lite.channels[ii].filament.brand = jsonData["channels" + std::to_string(ii)]["brand"].get<std::string>();
                    canvas_dev.canvas_lite.channels[ii].filament.code = jsonData["channels" + std::to_string(ii)]["code"].get<std::string>();
                    canvas_dev.canvas_lite.channels[ii].filament.type = jsonData["channels" + std::to_string(ii)]["type"].get<std::string>();
                    canvas_dev.canvas_lite.channels[ii].filament.detailed_type = jsonData["channels" + std::to_string(ii)]["detailed_type"].get<std::string>();
                    canvas_dev.canvas_lite.channels[ii].filament.color = jsonData["channels" + std::to_string(ii)]["color"].get<std::string>();
                    canvas_dev.canvas_lite.channels[ii].filament.diameter = jsonData["channels" + std::to_string(ii)]["diameter"].get<std::string>();
                    canvas_dev.canvas_lite.channels[ii].filament.weight = jsonData["channels" + std::to_string(ii)]["weight"].get<std::string>();
                    canvas_dev.canvas_lite.channels[ii].filament.date = jsonData["channels" + std::to_string(ii)]["date"].get<std::string>();
                    canvas_dev.canvas_lite.channels[ii].filament.nozzle_min_temp = jsonData["channels" + std::to_string(ii)]["nozzle_min_temp"].get<std::string>();
                    canvas_dev.canvas_lite.channels[ii].filament.nozzle_max_temp = jsonData["channels" + std::to_string(ii)]["nozzle_max_temp"].get<std::string>();
                    // SPDLOG_WARN("jsonData is contains '{}'","channels" + std::to_string(ii));
                }
            }
            if(jsonData.contains("extern_channel"))
            {
                canvas_dev.canvas_lite.extern_channel.filament.manufacturer = jsonData["extern_channel"]["manufacturer"].get<std::string>();
                canvas_dev.canvas_lite.extern_channel.filament.brand = jsonData["extern_channel"]["brand"].get<std::string>();
                canvas_dev.canvas_lite.extern_channel.filament.code = jsonData["extern_channel"]["code"].get<std::string>();
                canvas_dev.canvas_lite.extern_channel.filament.type = jsonData["extern_channel"]["type"].get<std::string>();
                canvas_dev.canvas_lite.extern_channel.filament.detailed_type = jsonData["extern_channel"]["detailed_type"].get<std::string>();
                canvas_dev.canvas_lite.extern_channel.filament.color = jsonData["extern_channel"]["color"].get<std::string>();
                canvas_dev.canvas_lite.extern_channel.filament.diameter = jsonData["extern_channel"]["diameter"].get<std::string>();
                canvas_dev.canvas_lite.extern_channel.filament.weight = jsonData["extern_channel"]["weight"].get<std::string>();
                canvas_dev.canvas_lite.extern_channel.filament.date = jsonData["extern_channel"]["date"].get<std::string>();
                canvas_dev.canvas_lite.extern_channel.filament.nozzle_min_temp = jsonData["extern_channel"]["nozzle_min_temp"].get<std::string>();
                canvas_dev.canvas_lite.extern_channel.filament.nozzle_max_temp = jsonData["extern_channel"]["nozzle_max_temp"].get<std::string>();
                // SPDLOG_WARN("jsonData is contains '{}'","extern_channel");
            }

            channel_color_table.clear();
            if(jsonData.contains("color_table_size"))
            {
                size_t color_table_size = jsonData["color_table_size"].get<size_t>();
                for(size_t ii = 0; ii < color_table_size; ++ii)
                {
                    if(jsonData.contains("color_table" + std::to_string(ii)))
                    {
                        std::tuple<std::string,std::string,std::string> tup(
                                jsonData["color_table" + std::to_string(ii)]["T"].get<std::string>(),
                                jsonData["color_table" + std::to_string(ii)]["color"].get<std::string>(),
                                jsonData["color_table" + std::to_string(ii)]["channel"].get<std::string>());
                        channel_color_table.emplace_back(tup);
                        // SPDLOG_WARN("jsonData is contains '{}'","color_table" + std::to_string(ii));
                        // SPDLOG_DEBUG("{} T:{} color:{} channel:{}","color_table" + std::to_string(ii),std::get<0>(channel_color_table[ii]),std::get<1>(channel_color_table[ii]),std::get<2>(channel_color_table[ii]));
                    }
                }
            }

            SPDLOG_INFO("{}() __OVER", __func__);
        }

        void CanvasDev::serial_connect_det()
        {
            // SPDLOG_DEBUG("{} #1",__func__);
            // 串口未连接
            if(false == this->canvas_status_info.mcu_status.cur_mcu_connect_status)
            {
                //暂不支持 pro
                if("lite" != this->canvas_status_info.mcu_status.type)
                {
                    this->canvas_dev.type = "pro";
                }
                else
                {
                    this->canvas_dev.type = "lite";
                    this->canvas_dev.canvas_lite.connected = 0;
                    this->canvas_dev.canvas_lite.did = 0;
                }
                // SPDLOG_ERROR("canvas serial is not connected");
            }
            else
            {
                //暂不支持 pro
                if("lite" != this->canvas_status_info.mcu_status.type)
                {
                    this->canvas_dev.type = "pro";
                }
                else //if(1 != this->canvas_dev.canvas_lite.connected)
                {
                    // SPDLOG_DEBUG("{} #1",__func__);
                    this->canvas_dev.type = "lite";
                    this->canvas_dev.canvas_lite.connected = 1;
                    this->canvas_dev.canvas_lite.did = 0;
                }
            }
        }

        void CanvasDev::led_status_control(int32_t channel,feeder_led_state_e red,feeder_led_state_e blue,int32_t id)
        {
            for (std::size_t i = 0; i < CHANNEL_NUM; i++)
            {
                if(i == channel)
                {
                    feeders_leds[i].enable_control = true;
                    feeders_leds[i].red = red;
                    feeders_leds[i].blue = blue;
                    is_sent_led_ctrl = true;
                }
                else
                {
                    feeders_leds[i].enable_control = false;
                }
                SPDLOG_DEBUG("{} channel:{} i:{} enable_control:{} red:{} blue:{}",__func__,channel,i,feeders_leds[i].enable_control,(int32_t)feeders_leds[i].red,(int32_t)feeders_leds[i].blue);
            }

            if(CHANNEL_NUM == channel)
            {
                is_sent_led_ctrl = false;
            }
        
            feeders_leds_control(feeders_leds);
            SPDLOG_INFO("Feeders LEDs control sent");
        }

        void CanvasDev::set_fila_origin(int32_t fila_origin)
        {
            if(canvas_status_info.host_status.fila_origin == fila_origin)
            {
                return;
            }
            //
            if(0 == fila_origin)
            {
                if(canvas_status_info.host_status.fila_origin >= 1 
                    && canvas_status_info.host_status.fila_origin <= CHANNEL_NUM)
                {
                    SPDLOG_DEBUG("{} fila_origin:{} new {}",__func__,canvas_status_info.host_status.fila_origin,fila_origin);
                    this->canvas_status_info.host_status.ch_status[this->canvas_status_info.host_status.fila_origin - 1] = 1;
                    this->canvas_dev.canvas_lite.channels[this->canvas_status_info.host_status.fila_origin - 1].status = 1;
                }
                this->canvas_dev.active_cid = 0;
                this->canvas_dev.active_did = 0;
                this->need_save_record = 1;
            }
            else if(fila_origin >= 1 && fila_origin <= CHANNEL_NUM)
            {
                SPDLOG_DEBUG("{} fila_origin {} new {}",__func__,canvas_status_info.host_status.fila_origin,fila_origin);
                this->canvas_status_info.host_status.ch_status[fila_origin - 1] = 2;
                this->canvas_dev.canvas_lite.channels[fila_origin - 1].status = 2;
                for (size_t i = 0; i < CHANNEL_NUM; i++)
                {
                    if(2 == canvas_status_info.host_status.ch_status[i] && i != fila_origin - 1)
                    {
                        SPDLOG_DEBUG("[before]{} i {} ch_status[i] {}",__func__,i,canvas_status_info.host_status.ch_status[i]);
                        this->canvas_status_info.host_status.ch_status[i] = 1;
                        this->canvas_dev.canvas_lite.channels[i].status = 1;
                    }
                }
                this->canvas_dev.active_cid = fila_origin;
                this->canvas_dev.active_did = 0;
                this->need_save_record = 1;
            }
            else
            {
                return;
            }
            canvas_status_info.host_status.fila_channel = fila_origin - 1;
            this->canvas_status_info.host_status.fila_origin = fila_origin;
        }
        
        int32_t CanvasDev::det_ch_fila_status(double eventtime)
        {
            bool is_printing = print_stats->get_status(eventtime)["state"].get<std::string>() == "printing";
            int32_t ch_num = 0;
            for (size_t i = 0; i < CHANNEL_NUM; i++)
            {
                if(1 == this->canvas_status_info.mcu_status.ch_fila_status[i])
                {
                    ++ch_num;
                    if(0 == canvas_status_info.host_status.ch_status[i])
                    {
                        this->canvas_status_info.host_status.ch_status[i] = 1;
                        SPDLOG_DEBUG("{} i:{} pre_load_ch_status:{} ch_status:{} ch_fila_status:{} last_ch_fila_status:{}",__func__,i,canvas_status_info.host_status.pre_load_ch_status[i],canvas_status_info.host_status.ch_status[i],canvas_status_info.mcu_status.ch_fila_status[i],canvas_status_info.mcu_status.last_ch_fila_status[i]);
                    }
                    if(1 == canvas_status_info.host_status.pre_load_ch_status[i]
                        && 0x00 == canvas_action_state
                        && true != is_printing
                        && false == canvas_status_info.host_status.is_pre_loading)
                    {
                        canvas_status_info.host_status.pre_load_ch_status[i] = 2;
                        gcode->run_script("CANVAS_PRELOAD_FILAMENT");
                    }
                }
                else
                {
                    this->canvas_status_info.host_status.ch_status[i] = 0;
                    if(0 != canvas_status_info.host_status.pre_load_ch_status[i])
                    {
                        canvas_status_info.host_status.pre_load_ch_status[i] = 0;
                        SPDLOG_DEBUG("{} i:{} pre_load_ch_status:{} ch_status:{} ch_fila_status:{} last_ch_fila_status:{}",__func__,i,canvas_status_info.host_status.pre_load_ch_status[i],canvas_status_info.host_status.ch_status[i],canvas_status_info.mcu_status.ch_fila_status[i],canvas_status_info.mcu_status.last_ch_fila_status[i]);
                    }
                }
                //
                if("lite" == this->canvas_status_info.mcu_status.type)
                {
                    if(1 == this->canvas_status_info.host_status.ch_status[i])
                    {
                        this->canvas_dev.canvas_lite.channels[i].status = 1;
                    }
                    else if(2 == this->canvas_status_info.host_status.ch_status[i])
                    {
                        this->canvas_dev.canvas_lite.channels[i].status = 2;
                    }
                    else
                    {
                        this->canvas_dev.canvas_lite.channels[i].status = 0;
                    }
                    this->canvas_dev.canvas_lite.channels[i].cid = i;
                }
            }
            return ch_num;
        }

        std::vector<int32_t> CanvasDev::det_change_cid(int32_t ex_move_dist,int32_t change_dist_min,int32_t change_dist_max,bool from_cmd)
        {
            for(auto ii = 0; ii < CHANNEL_NUM; ++ii)
            {
                canvas_status_info.host_status.before_load_ch_pos[ii] = canvas_status_info.mcu_status.ch_position[ii];
                SPDLOG_DEBUG("{} #1 ii:{} before_load_ch_pos:{} ch_position:{} ch_fila_move_dist:{}",__func__,ii,canvas_status_info.host_status.before_load_ch_pos[ii],canvas_status_info.mcu_status.ch_position[ii],canvas_status_info.mcu_status.ch_fila_move_dist[ii]);
            }
            //E轴挤出30mm
            SPDLOG_DEBUG("{} #1 G1 E{} F300 ex_fila_status:{} {} {} from_cmd:{}",__func__,ex_move_dist,canvas_status_info.host_status.ex_fila_status,change_dist_min,change_dist_max,from_cmd);
            if(from_cmd)
                gcode->run_script_from_command("M83\nG1 E" + std::to_string(ex_move_dist) + "F300\nM400");
            else
                gcode->run_script("M83\nG1 E" + std::to_string(ex_move_dist) + "F300\nM400");
            //3.比较各通道里程数是否变化
            sync_det_info();
            std::vector<int32_t> change_cid;
            for(int32_t ii = 0; ii < CHANNEL_NUM; ++ii)
            {
                if(fabs(canvas_status_info.mcu_status.ch_position[ii] - canvas_status_info.host_status.before_load_ch_pos[ii]) >= change_dist_min && fabs(canvas_status_info.mcu_status.ch_position[ii] - canvas_status_info.host_status.before_load_ch_pos[ii]) <= change_dist_max)
                {
                    change_cid.emplace_back(ii);
                    SPDLOG_DEBUG("{} #1 change_cid.emplace_back({}) before_load_ch_pos:{} ch_position:{} ch_fila_move_dist:{}",__func__,ii,canvas_status_info.host_status.before_load_ch_pos[ii],canvas_status_info.mcu_status.ch_position[ii],canvas_status_info.mcu_status.ch_fila_move_dist[ii]);
                }
                else
                {
                    SPDLOG_DEBUG("{} #1 change_cid.emplace_back({}) before_load_ch_pos:{} ch_position:{} ch_fila_move_dist:{}",__func__,ii,canvas_status_info.host_status.before_load_ch_pos[ii],canvas_status_info.mcu_status.ch_position[ii],canvas_status_info.mcu_status.ch_fila_move_dist[ii]);
                }
            }
            //
            SPDLOG_DEBUG("{} change_cid.size:{}",__func__,change_cid.size());
            return change_cid;
        }

        void CanvasDev::fila_origin_det(double eventtime)
        {
            if(0 == this->canvas_status_info.host_status.ex_fila_status)
            {
                det_ch_fila_status(eventtime);
                set_fila_origin(0);
            }
            else
            {
                int32_t ch_num = det_ch_fila_status(eventtime);
                if(0 == ch_num)
                {
                    // SPDLOG_WARN("set_fila_origin(CHANNEL_NUM + 1) ch_num:{}",ch_num);
                    // set_fila_origin(CHANNEL_NUM + 1);
                    return;
                }
            }
        }

        void CanvasDev::state_det_process(double eventtime)
        {
            std::vector<int32_t> para = {};
            bool is_idle_printing = idle_timeout->get_status(0.)["state"].get<std::string>() == "Printing";
            bool is_printing = print_stats->get_status(get_monotonic())["state"].get<std::string>() == "printing";
            switch (canvas_action_state)
            {
            case 0x0f:
            case 0x04:
                if(0x01 == canvas_stage_state)
                {
                    // 开始扫描
                    if(!is_idle_printing && get_monotonic() < rfid_scan_time + 60.)
                    {
                        if(canvas_status_info.mcu_status.scan_rfid_uid > 0)
                        {
                            update_canvas_state(canvas_action_state,0x03);
                        }
                    }
                    else
                    {
                        // 扫描超时
                        SPDLOG_WARN("{} rfid scan timeout or is idle printing",__func__);
                        update_canvas_state(canvas_action_state,0xfe);
                        std::lock_guard<std::mutex> lock(rfid_mtx);
                        is_rfid_trig = false;
                    }
                }
                else if(0x02 == canvas_stage_state)
                {
                    //取消上报
                    SPDLOG_WARN("{} rfid scan cancel",__func__);
                    update_canvas_state(canvas_action_state,0xff);
                }
                else if(0x03 == canvas_stage_state)
                {
                    //完成上报
                    int32_t ret = get_cavans_rfid_info();
                    if(0 == ret)
                    {
                        SPDLOG_INFO("{} rfid scan success",__func__);
                        if(0x0f == canvas_action_state)
                        {
                            state_feedback("CANVAS_RFID_TRIG",filament_info.type + "," + filament_info.color);
                            update_canvas_state(canvas_action_state,0x0e);
                        }
                        else
                        {
                            canvas_status_info.host_status.edit_filament_status[scan_channel] = 0;
                            sync_rfid_info(scan_id,scan_channel,filament_info);
                            update_canvas_state(canvas_action_state,0xff);
                            std::lock_guard<std::mutex> lock(rfid_mtx);
                            is_rfid_trig = false;
                        }
                    }
                    else
                    {
                        SPDLOG_WARN("{} rfid scan failed",__func__);
                        update_canvas_state(canvas_action_state,0xfe);
                        std::lock_guard<std::mutex> lock(rfid_mtx);
                        is_rfid_trig = false;
                    }
                }
                else if(0x0f == canvas_action_state && 0x0e == canvas_stage_state && get_monotonic() >= rfid_scan_time + 60.)
                {
                    SPDLOG_WARN("{} rfid scan timeout",__func__);
                    update_canvas_state(canvas_action_state,0xff);
                    std::lock_guard<std::mutex> lock(rfid_mtx);
                    is_rfid_trig = false;
                }
                break;
            default:
                break;
            }
            //结束或失败上报
            if(0x00 != canvas_action_state 
                && (0xff == canvas_stage_state || 0xfe == canvas_stage_state))
            {
                update_canvas_state(0x00,canvas_stage_state);
            }
        }

        bool CanvasDev::clean_waste_and_start_switch_filament()
        {
            if(0 != canvas_status_info.host_status.fila_origin)
            {
                SPDLOG_DEBUG("{} is_plug_in_filament_over:{} {} {} {}",__func__,is_plug_in_filament_over,canvas_status_info.host_status.ex_fila_status,canvas_status_info.host_status.fila_origin,canvas_status_info.host_status.ch_status[canvas_status_info.host_status.fila_origin - 1]);
            }
            else
            {
                SPDLOG_DEBUG("{} fila_origin is nul",__func__);
            }
            if(
                true == is_plug_in_filament_over
                && true == canvas_status_info.host_status.ex_fila_status
                && canvas_status_info.host_status.fila_origin >= 1
                && canvas_status_info.host_status.fila_origin <= CHANNEL_NUM
                && 0 == canvas_status_info.host_status.ch_status[canvas_status_info.host_status.fila_origin - 1]
                )
            {
                canvas_status_info.host_status.is_switch_and_clean_filment = true;
                SPDLOG_DEBUG("{} is_switch_and_clean_filment:{}",__func__,canvas_status_info.host_status.is_switch_and_clean_filment);
                // 冲刷废料动作
                SPDLOG_INFO("clean waste and start switch filament ...");
                filament_load_gcode_1->run_gcode_from_command();
                SPDLOG_DEBUG("{} ex_fila_status:{}",__func__,canvas_status_info.host_status.ex_fila_status);
                double cur_plut_in_time = get_monotonic();
                while(true == canvas_status_info.host_status.ex_fila_status)
                {
                    filament_load_gcode_2->run_gcode_from_command();
                    SPDLOG_DEBUG("{} ex_fila_status:{}",__func__,canvas_status_info.host_status.ex_fila_status);
                    if(get_monotonic() - cur_plut_in_time > 6. * 60.)
                    {
                        SPDLOG_WARN("{} ex_fila_status:{} timeout",__func__,canvas_status_info.host_status.ex_fila_status);
                        break;
                    }
                    gcode->run_script_from_command("M400");
                }
                if(true == canvas_status_info.host_status.ex_fila_status)
                {
                    SPDLOG_WARN("{} failed",__func__);
                    return false;
                }
                else
                {
                    SPDLOG_INFO("{} success",__func__);
                }
            }
            SPDLOG_INFO("{} __OVER",__func__);
            return true;
        }
        
        void CanvasDev::after_abnormal_pause_action()
        {
            // led 控制
            if(canvas_status_info.host_status.fila_origin >= 1 && canvas_status_info.host_status.fila_origin <= CHANNEL_NUM)
            {
                led_status_control(canvas_status_info.host_status.fila_origin - 1);
                SPDLOG_DEBUG("{} led control! fila_origin:{}",__func__,canvas_status_info.host_status.fila_origin);
            }
            else if(true == canvas_status_info.host_status.is_runout_filament_report)
            {
                int32_t ch = -1;
                for(auto ii = 0; ii < channel_color_table.size(); ++ii)
                {
                    if(canvas_status_info.host_status.switch_filment_T == std::stoi(std::get<0>(channel_color_table[ii])))
                    {
                        ch = std::stoi(std::get<2>(channel_color_table[ii]));
                        SPDLOG_INFO("{} ch:{} switch_filment_T:{} switch_filment_type:{} switch_filment_color:{}",__func__,ch,canvas_status_info.host_status.switch_filment_T,canvas_status_info.host_status.switch_filment_type,canvas_status_info.host_status.switch_filment_color);
                        break;
                    }
                }
                if(-1 != ch && ch >= 0 && ch < CHANNEL_NUM)
                {
                    led_status_control(ch);
                    SPDLOG_DEBUG("{} led control! ch:{}",__func__,ch);
                }
            }
            // 断料
            if(true == canvas_status_info.host_status.is_runout_filament_report)
            {
                bool is_need_switch_fila = false;
                // 断料后自动续料
                if(true == canvas_status_info.host_status.is_need_plut_in_filment)
                {
                    SPDLOG_INFO("auto plug in filament ...");
                    SPDLOG_INFO("{} switch_filment_T:{} switch_filment_type:{} switch_filment_color:{}",__func__,canvas_status_info.host_status.switch_filment_T,canvas_status_info.host_status.switch_filment_type,canvas_status_info.host_status.switch_filment_color);
                    std::string color = "";
                    std::string type = "";
                    int32_t ch = -1;
                    if(canvas_status_info.host_status.switch_filment_color.empty())
                    {
                        SPDLOG_ERROR("{} switch filment color is empty! auto plug in filament error!",__func__);
                        return;
                    }
                    for(auto ii = 0; ii < channel_color_table.size(); ++ii)
                    {
                        if(canvas_status_info.host_status.switch_filment_T == std::stoi(std::get<0>(channel_color_table[ii])))
                        {
                            ch = std::stoi(std::get<2>(channel_color_table[ii]));
                            SPDLOG_INFO("{} ch:{}",__func__,ch);
                            for (int32_t i = 0; i < CHANNEL_NUM; i++)
                            {
                                try
                                {
                                    if(canvas_dev.canvas_lite.channels[i].filament.color.empty())
                                    {
                                        SPDLOG_DEBUG("{} channels {} color is empty! continue",__func__,i);
                                        continue;
                                    }
                                    SPDLOG_DEBUG("{} filament[{}].color:{} fila_type:{}",__func__,i,canvas_dev.canvas_lite.channels[i].filament.color,canvas_dev.canvas_lite.channels[i].filament.type);
                                    int32_t T_color = std::stoll(canvas_status_info.host_status.switch_filment_color,nullptr,16);
                                    SPDLOG_DEBUG("{} T_color:{:02x}",__func__,T_color);
                                    int32_t fila_color = std::stoll(canvas_dev.canvas_lite.channels[i].filament.color,nullptr,16);
                                    SPDLOG_DEBUG("{} fila_color:{:02x}",__func__,fila_color);
                                    std::string fila_type = canvas_dev.canvas_lite.channels[i].filament.type;
                                    SPDLOG_DEBUG("{} fila_color:{:02x} fila_type:{} T_color:{:02x} switch_filment_color:{}",__func__,fila_color,fila_type,T_color,canvas_status_info.host_status.switch_filment_color);
                                    if(ch != i && canvas_status_info.host_status.switch_filment_type == fila_type && fila_color <= T_color + 100 && fila_color >= T_color - 100)
                                    {
                                        std::get<2>(channel_color_table[ii]) = std::to_string(i);
                                        is_need_switch_fila = true;
                                        SPDLOG_INFO("{} T:{} ch:{} switch_filment_type:{} fila_color:{:02x} is_need_switch_fila:{}",__func__,std::get<0>(channel_color_table[ii]),std::get<2>(channel_color_table[ii]),canvas_status_info.host_status.switch_filment_type,fila_color,is_need_switch_fila);
                                        need_save_record = 1;
                                        break;
                                    }
                                }
                                catch (const std::invalid_argument& e) {
                                    std::cerr << "error:invalid argument" << std::endl;
                                    SPDLOG_ERROR("auto plug in filament error! err_msg:{}",std::string(e.what()));
                                    return;
                                }
                                catch (const std::out_of_range& e) {
                                    std::cerr << "error:out of range" << std::endl;
                                    SPDLOG_ERROR("auto plug in filament error! err_msg:{}",std::string(e.what()));
                                    return;
                                }
                                catch(const std::exception& e)
                                {
                                    std::cerr << e.what() << '\n';
                                    SPDLOG_ERROR("auto plug in filament error! err_msg:{}",std::string(e.what()));
                                    return;
                                }
                                
                            }
                            break;
                        }
                    }
                }
                //
                if(is_need_switch_fila)
                {
                    gcode->run_script_from_command("CANVAS_SWITCH_FILAMENT T=" + std::to_string(canvas_status_info.host_status.switch_filment_T) + " SELECT=0\nM400");
                    if(0 == canvas_abnormal_state)
                    {
                        SPDLOG_INFO("auto plug in filament success!printer RESUME ...");
                        gcode->run_script_from_command("RESUME\nM400");
                    }
                    else if(runout_filament_gcode)
                    {
                        runout_filament_gcode->run_gcode_from_command();
                        gcode->run_script_from_command("\nM400");
                    }
                }
                else if(runout_filament_gcode)
                {
                    runout_filament_gcode->run_gcode_from_command();
                    gcode->run_script_from_command("\nM400");
                }
                canvas_status_info.host_status.is_need_plut_in_filment = false;
            }
        }

        bool CanvasDev::is_canvas_dev_connect()
        {
            return canvas_status_info.mcu_status.cur_mcu_connect_status;
        }

        bool CanvasDev::is_printing_used_canvas()
        {
            return this->printing_used_canvas;
        }

        void CanvasDev::set_printing_used_canvas(bool state)
        {
            this->printing_used_canvas = state;
            SPDLOG_INFO("{} printing_used_canvas:{}",__func__,printing_used_canvas);
        }

        bool CanvasDev::get_ex_fila_status()
        {
            return canvas_status_info.host_status.ex_fila_status;
        }

        int32_t CanvasDev::get_T_channel(int32_t T)
        {
            int32_t channel = -1;
            try
            {
                for(auto ii = 0; ii < channel_color_table.size(); ++ii)
                {
                    int32_t color_table_T = std::stoi(std::get<0>(channel_color_table[ii]));
                    if(color_table_T == T)
                    {
                        channel = std::stoi(std::get<2>(channel_color_table[ii]));
                        SPDLOG_DEBUG("{} #1 table_T {} table_color {} table_channel {} color_table_T {} T {} channel:{}",__func__,std::get<0>(channel_color_table[ii]),std::get<1>(channel_color_table[ii]),std::get<2>(channel_color_table[ii]),color_table_T,T,channel);
                        break;
                    }
                }
            }
            catch(...)
            {
                SPDLOG_WARN("channel color table abnormal");
            }
            
            SPDLOG_INFO("{} channel {}",__func__,channel);
            return channel + 1;
        }

        int32_t CanvasDev::get_cur_channel()
        {
            return canvas_status_info.host_status.fila_origin;
        }

        int32_t CanvasDev::is_abnormal_state()
        {
            return 0xfe == canvas_abnormal_state;
        }

        void CanvasDev::must_pause_work(bool from_cmd)
        {
            // 断料状态重置
            canvas_status_info.host_status.is_runout_filament_report = false;
            // 释放led控制
            if(from_cmd)
                gcode->run_script_from_command("CANVAS_SET_LED_STATUS CHANNEL=" + std::to_string(CHANNEL_NUM));
            else
                gcode->run_script("CANVAS_SET_LED_STATUS CHANNEL=" + std::to_string(CHANNEL_NUM));
        }

        RFID CanvasDev::get_rfid_raw()
        {
            return rfid;
        }

        std::vector<FeederStatus> CanvasDev::get_feeder_status()
        {
            return feeders;
        }

        bool CanvasDev::is_canvas_lite_connected()
        {
            return lite_connected;
        }

        void CanvasDev::state_feedback(std::string command,std::string result)
        {
            json res;
            res["command"] = command;   // "CANVAS_STATE_FEEDBACK"
            res["result"] = result;     // 例:进料开始 canvas_action_state:0x03 + canvas_stage_state:0x01 = 0x0301
            SPDLOG_INFO("__func__:{} #1 res:{}",__func__,res.dump());
            gcode->respond_feedback(res);
        }

        int32_t CanvasDev::update_canvas_state(int32_t action_state,int32_t stage_state)
        {
            canvas_action_state = action_state;
            canvas_stage_state = stage_state;
            state_feedback("CANVAS_STATE_FEEDBACK",uint32_to_hex_string(canvas_action_state,"",'0',2,false) + uint32_to_hex_string(canvas_stage_state,"",'0',2,false));
            return 0;
        }
        
        void CanvasDev::CMD_canvas_detect_filament(std::shared_ptr<GCodeCommand> gcmd)
        {
            SPDLOG_INFO("{} #1",__func__);
            std::vector<int32_t> change_cid = {};
            if(false == canvas_status_info.host_status.ex_fila_status)  // 没有耗材插入挤出机
            {
                SPDLOG_WARN("{} the fila origin is nul",__func__);
                set_fila_origin(0);
                goto fila_det_success;
            }
            SPDLOG_INFO("{} #1 ex_fila_status:{}",__func__,canvas_status_info.host_status.ex_fila_status);
            change_cid = det_change_cid(50,10,60);
            SPDLOG_DEBUG("{} change_cid.size:{}",__func__,change_cid.size());
            if(0 == change_cid.size())
            {
                for (int32_t i = 0; i < CHANNEL_NUM; i++)
                {
                    std::vector<int32_t> para = {-30,motor_speed,motor_accel,i,canvas_status_info.mcu_status.did};
                    send_with_respone(SERIAL_CMD_MOTOR_CONTROL,para,1);
                    if(!canvas_status_info.host_status.ex_fila_status)
                    {
                        para = {-120,motor_speed,motor_accel,i,canvas_status_info.mcu_status.did};
                        send_with_respone(SERIAL_CMD_MOTOR_CONTROL,para,10);
                        set_fila_origin(0);
                        goto fila_det_success;
                    }
                }
                if(canvas_status_info.host_status.ex_fila_status)
                {
                    SPDLOG_WARN("{} error! Failed to detect the fila origin",__func__);
                    set_fila_origin(0);
                    canvas_error_code = elegoo::common::ErrorCode::CANVAS_ABNORMAL_FILA;
                    goto fila_det_error;
                }
            }
            if(change_cid.size() == 1)
            {
                SPDLOG_INFO("{} #1",__func__);
                set_fila_origin(change_cid.back() + 1);
            }
            else
            {
                std::vector<int32_t> change_cid = det_change_cid(20,5,30);
                if(change_cid.size() > 0)
                {
                    if(change_cid.size() == 1)
                    {
                        SPDLOG_INFO("{} #1",__func__);
                        set_fila_origin(change_cid.back() + 1);
                    }
                    else
                    {
                        SPDLOG_WARN("{} error! Failed to detect the fila origin",__func__);
                        set_fila_origin(0);
                        canvas_error_code = elegoo::common::ErrorCode::CANVAS_ABNORMAL_FILA;
                        goto fila_det_error;
                    }
                }
                else
                {
                    for (int32_t i = 0; i < CHANNEL_NUM; i++)
                    {
                        std::vector<int32_t> para = {-30,motor_speed,motor_accel,i,canvas_status_info.mcu_status.did};
                        send_with_respone(SERIAL_CMD_MOTOR_CONTROL,para,1);
                        if(!canvas_status_info.host_status.ex_fila_status)
                        {
                            para = {-120,motor_speed,motor_accel,i,canvas_status_info.mcu_status.did};
                            send_with_respone(SERIAL_CMD_MOTOR_CONTROL,para,10);
                            set_fila_origin(0);
                            break;
                        }
                    }
                    if(canvas_status_info.host_status.ex_fila_status)
                    {
                        SPDLOG_WARN("{} error! Failed to detect the fila origin",__func__);
                        set_fila_origin(0);
                        canvas_error_code = elegoo::common::ErrorCode::CANVAS_ABNORMAL_FILA;
                        goto fila_det_error;
                    }
                }
            }
            for(auto ii = 0; ii < CHANNEL_NUM; ++ii)
            {
                SPDLOG_DEBUG("{} #1 ii:{} before_load_ch_pos:{} ch_position:{} ch_fila_move_dist:{}",__func__,ii,canvas_status_info.host_status.before_load_ch_pos[ii],canvas_status_info.mcu_status.ch_position[ii],canvas_status_info.mcu_status.ch_fila_move_dist[ii]);
            }

            fila_det_success:
                SPDLOG_INFO("{} #1 __OVER",__func__);
                return;
            fila_det_error:
                update_canvas_state(0x09,0xfe);
                SPDLOG_INFO("{} #1 __OVER",__func__);
        }

        /*****************************************************************************
         * @brief        : 检测耗材位置
         * @return        {*}
         *****************************************************************************/
        void CanvasDev::filament_det()
        {
            //
            if(canvas_status_info.host_status.fila_origin >= 1 && canvas_status_info.host_status.fila_origin <= CHANNEL_NUM + 1)
            {
                SPDLOG_INFO("{} #1 fila origin is {}",__func__,canvas_status_info.host_status.fila_origin);
                return;
            }
            gcode->run_script_from_command("CANVAS_DETECT_FILAMENT");
            if(canvas_status_info.host_status.fila_origin >= 1 && canvas_status_info.host_status.fila_origin <= CHANNEL_NUM)
            {
                // led呼吸
                gcode->run_script_from_command("CANVAS_SET_LED_STATUS CHANNEL=" + std::to_string(canvas_status_info.host_status.fila_origin - 1) + " ABNORMAL=" + std::to_string(2));
            }
            return ;
        }
        
        void CanvasDev::load_det()
        {

        }
        void CanvasDev::unload_det()
        {

        }
        /*****************************************************************************
         * @brief        : 切刀切料逻辑
         * @return        {*}
         *****************************************************************************/
        void CanvasDev::CMD_canvas_cut_filament(std::shared_ptr<GCodeCommand> gcmd)
        {
            SPDLOG_INFO("{} #1 fila_origin:{} fila_channel:{}",__func__,canvas_status_info.host_status.fila_origin,canvas_status_info.host_status.fila_channel);
            if(false == canvas_status_info.host_status.ex_fila_status)  // 没有耗材插入挤出机
            {
                SPDLOG_WARN("{} the fila origin is nul",__func__);
                set_fila_origin(0);
                return;
            }
            //1.移动到切刀位置前,坐标待定
            //2.切断耗材前E轴回抽15mm
            //3.通过机械结构移动X或Y撞击切刀达到切断耗材动作
            SPDLOG_DEBUG("{} #1 G28 X Y TRY",__func__);
            bool is_retry_G28 = gcmd->get_int("RETRY",0,0,1);
            if(is_retry_G28)
            {
                gcode->run_script_from_command("G28 X Y");
            }
            else
            {
                gcode->run_script_from_command("G28 X Y TRY");
            }
            gcode->run_script_from_command("M204 S10000\nG1 X245 Y25 F15000\nG1 X254.5 F3000\nG1 Y2.5 F600\nM400\n");
            SPDLOG_DEBUG("cutting_knife_status:{}",canvas_status_info.host_status.cutting_knife_status);
            //4.检测切刀传感器是否触发，若未触发则报错
            if(0 == canvas_status_info.host_status.cutting_knife_status)
            {
                SPDLOG_WARN("{} cutting_knife_status:{} error! retry",__func__,canvas_status_info.host_status.cutting_knife_status);
                gcode->run_script_from_command("G1 Y25 F15000\nG28 X Y\nM204 S10000\nG1 X245 Y25 F15000\nG1 X254.5 F3000\nG1 Y2.5 F600\nM400\n");
                if(0 == canvas_status_info.host_status.cutting_knife_status)
                {
                    SPDLOG_ERROR("error! the cutting knife is not triggered");
                    canvas_error_code = elegoo::common::ErrorCode::CANVAS_CUTTING_KNIFET_NOT_TRIGGER;
                    goto cut_fila_error;
                }
            }
            //5.执行完切断动作后E轴回抽3mm，释放切刀压力
            //6.前往垃圾桶位置,坐标待定
            SPDLOG_DEBUG("{} #1 G1 E-3 F600",__func__);
            gcode->run_script_from_command("M83\nG1 E-1 F120\nM400\nG1 X204 Y250 F10000\nG1 Y263 F3000\nM400");
            SPDLOG_DEBUG("cutting_knife_status:{}",canvas_status_info.host_status.cutting_knife_status);
            //7.检测切刀传感器是否未触发。若触发则报错
            if(1 == canvas_status_info.host_status.cutting_knife_status)
            {
                SPDLOG_WARN("{} cutting_knife_status:{} error! retry",__func__,canvas_status_info.host_status.cutting_knife_status);
                gcode->run_script_from_command("G1 E1.5 F120\nG1 E-2 F120\nM400");
                if(1 == canvas_status_info.host_status.cutting_knife_status)
                {
                    SPDLOG_ERROR("{} error! the cutting knife is triggered",__func__);
                    canvas_error_code = elegoo::common::ErrorCode::CANVAS_CUTTING_KNIFET_TRIGGER;
                    goto cut_fila_error;
                }
            }
            SPDLOG_INFO("{} #1 __OVER",__func__);
            return ;
            cut_fila_error:
                update_canvas_state(0x0a,0xfe);
        }
        /*****************************************************************************
         * @brief        : 抽回旧耗材逻辑
         * @return        {*}
         *****************************************************************************/
        void CanvasDev::CMD_canvas_plug_out_filament(std::shared_ptr<GCodeCommand> gcmd)
        {
            if(0 == canvas_status_info.host_status.ex_fila_status)
            {
                return;
            }
            //
            if(false == clean_waste_and_start_switch_filament())
            {
                SPDLOG_WARN("{} #1 clean_waste_and_start_switch_filament failed!",__func__);
            }
            //
            if(canvas_status_info.host_status.fila_origin >= 1 && canvas_status_info.host_status.fila_origin <= CHANNEL_NUM)
            {
                // led呼吸
                gcode->run_script_from_command("CANVAS_SET_LED_STATUS CHANNEL=" + std::to_string(canvas_status_info.host_status.fila_origin - 1) + " ABNORMAL=" + std::to_string(2));
            }
            //E轴回退36mm
            gcode->run_script_from_command("CANVAS_MESH_GEAR");
            SPDLOG_DEBUG("{} #1 G1 E-36 F900",__func__);
            gcode->run_script_from_command("M83\nG1 E-36 F900\nM400");
            SPDLOG_DEBUG("{} #1 fila_origin:{} fila_channel:{} ex_fila_status:{}",__func__,canvas_status_info.host_status.fila_origin,canvas_status_info.host_status.fila_channel,canvas_status_info.host_status.ex_fila_status);
            double cur_time = get_monotonic();
            double last_time = cur_time;
            double cur_channel_pos = 0.;
            double factor = 1.0;
             int32_t channel = -1;
            std::vector<int32_t> para = {};
            is_plug_in_filament_over = false;
            canvas_status_info.host_status.is_switch_and_clean_filment = false;
            SPDLOG_DEBUG("{} is_plug_in_filament_over:{} is_switch_and_clean_filment:{}",__func__,is_plug_in_filament_over,canvas_status_info.host_status.is_switch_and_clean_filment);
            if(-1 != canvas_status_info.host_status.fila_channel)
            {
                channel = canvas_status_info.host_status.fila_channel;
                cur_channel_pos = canvas_status_info.mcu_status.ch_position[channel];
                cur_time = get_monotonic();
                last_time = cur_time;
                factor = 1.0;
                para = {-1 * unload_fila_dist,(int32_t)(factor * motor_speed),(int32_t)(factor * motor_accel),channel,canvas_status_info.mcu_status.did};
                send_with_respone(SERIAL_CMD_MOTOR_CONTROL,para,0.);
                SPDLOG_DEBUG("{} #1 ex_fila_status:{} channel:{} {}",__func__,canvas_status_info.host_status.ex_fila_status,channel,canvas_status_info.host_status.ch_status[channel]);
                while (cur_time < last_time + unload_fila_time)
                {
                    reactor->pause(get_monotonic() + 0.1);
                    sync_det_info();
                    if(0 == canvas_status_info.host_status.ex_fila_status)
                    {
                        if(canvas_status_info.mcu_status.ch_position[channel] - cur_channel_pos > 120.)
                        {
                            SPDLOG_WARN("{} #1 ex_fila_status:{} {}",__func__,canvas_status_info.host_status.ex_fila_status,canvas_status_info.mcu_status.ch_position[channel] - cur_channel_pos);
                            feeders_filament_stop(channel);
                            set_fila_origin(0);
                            goto plug_put_over;
                        }
                    }
                    else if(canvas_status_info.mcu_status.ch_position[channel] - cur_channel_pos > 30.)//cur_time - last_time > 5. && 
                    {
                        SPDLOG_INFO("{} #1 ex_fila_status:{} {}",__func__,canvas_status_info.host_status.ex_fila_status,canvas_status_info.mcu_status.ch_position[channel] - cur_channel_pos);
                        feeders_filament_stop(channel);
                        break;
                    }
                    cur_time = get_monotonic();
                }
                
                if(cur_time - last_time > unload_fila_time)
                {
                    SPDLOG_ERROR("error! plut out filament timeout");
                    canvas_error_code = elegoo::common::ErrorCode::CANVAS_PLUT_OUT_FILA_TIMEOUT;
                    goto plug_put_error;
                }
                // else if(1 == canvas_status_info.host_status.ex_fila_status)
                // {
                //     SPDLOG_ERROR("error! the ex fila sersor is triggered");
                //     canvas_error_code = elegoo::common::ErrorCode::CANVAS_PLUT_OUT_FILA_ABNORMAL;
                //     goto plug_put_error;
                // }
            }
            
            //4.检测挤出机耗材传感器是否触发，若触发则报错
            SPDLOG_INFO("{} #1 ex_fila_status:{}",__func__,canvas_status_info.host_status.ex_fila_status);
            if(1 == canvas_status_info.host_status.ex_fila_status)
            {
                for (int32_t i = 0; i < CHANNEL_NUM; i++)
                {
                    if(i != channel && 1 == canvas_status_info.host_status.ch_status[i])
                    {
                        SPDLOG_DEBUG("{} #1 ex_fila_status:{} channel:{} i:{} {}",__func__,canvas_status_info.host_status.ex_fila_status,channel,i,canvas_status_info.host_status.ch_status[i]);
                        factor = 1.0;
                        cur_time = get_monotonic();
                        last_time = cur_time;
                        cur_channel_pos = canvas_status_info.mcu_status.ch_position[i];
                        para = {-1 * unload_fila_dist,(int32_t)(factor * motor_speed),(int32_t)(factor * motor_accel),i,canvas_status_info.mcu_status.did};
                        send_with_respone(SERIAL_CMD_MOTOR_CONTROL,para,0.);
                        while (cur_time < last_time + unload_fila_time)
                        {
                            reactor->pause(get_monotonic() + 0.1);
                            sync_det_info();
                            if(0 == canvas_status_info.host_status.ex_fila_status)
                            {
                                if(canvas_status_info.mcu_status.ch_position[i] - cur_channel_pos > 120.)
                                {
                                    SPDLOG_INFO("{} #1 ex_fila_status:{} {}",__func__,canvas_status_info.host_status.ex_fila_status,canvas_status_info.mcu_status.ch_position[i] - cur_channel_pos);
                                    feeders_filament_stop(i);
                                    set_fila_origin(0);
                                    goto plug_put_over;
                                }
                            }
                            else if(canvas_status_info.mcu_status.ch_position[i] - cur_channel_pos > 30.)
                            {
                                SPDLOG_INFO("{} #1 ex_fila_status:{} {}",__func__,canvas_status_info.host_status.ex_fila_status,canvas_status_info.mcu_status.ch_position[i] - cur_channel_pos);
                                feeders_filament_stop(i);
                                break;
                            }
                            cur_time = get_monotonic();
                        }
                    }
                }

                if(cur_time - last_time > unload_fila_time)
                {
                    SPDLOG_ERROR("error! plut out filament timeout");
                    canvas_error_code = elegoo::common::ErrorCode::CANVAS_PLUT_OUT_FILA_TIMEOUT;
                    goto plug_put_error;
                }
                else if(1 == canvas_status_info.host_status.ex_fila_status)
                {
                    SPDLOG_ERROR("error! the ex fila sersor is triggered");
                    canvas_error_code = elegoo::common::ErrorCode::CANVAS_PLUT_OUT_FILA_ABNORMAL;
                    goto plug_put_error;
                }
            }

            plug_put_over:
                // 释放led控制
                gcode->run_script_from_command("CANVAS_SET_LED_STATUS CHANNEL=" + std::to_string(CHANNEL_NUM));
                gcode->run_script_from_command("CANVAS_MESH_GEAR");
                SPDLOG_INFO("{} #1 __OVER",__func__);
                return ;

            plug_put_error:
                update_canvas_state(0x0b,0xfe);
                gcode->run_script_from_command("CANVAS_MESH_GEAR");
                SPDLOG_INFO("{} #1 __OVER",__func__);
        }
        /*****************************************************************************
         * @brief        : 送入新耗材逻辑
         * @return        {*}
         *****************************************************************************/
        void CanvasDev::CMD_canvas_plug_in_filament(std::shared_ptr<GCodeCommand> gcmd)
        {
            if(true == canvas_status_info.host_status.ex_fila_status)
            {
                return;
            }
            SPDLOG_INFO("{} #1",__func__);
            int32_t id = gcmd->get_int("ID",0,0,3);
            int32_t channel = gcmd->get_int("CHANNEL",0,0,3);
            if(-1 == canvas_status_info.host_status.fila_channel)
            {
                return;
            }
            channel = canvas_status_info.host_status.fila_channel;
            SPDLOG_INFO("{} channel {} id {}",__func__,channel,id);
            // led呼吸
            gcode->run_script_from_command("CANVAS_SET_LED_STATUS CHANNEL=" + std::to_string(channel) + " ABNORMAL=" + std::to_string(2) + "\nM400");
            //1.通道电机往送料方向移动,移动距离待定
            gcode->run_script_from_command("CANVAS_MESH_GEAR DIST=" + std::to_string(mesh_gear_dist));
            double cur_channel_pos = canvas_status_info.mcu_status.ch_position[channel];
            double cur_time = get_monotonic();
            double last_time = cur_time;
            double factor = 1.0;//0.6;//1.0;
            std::vector<int32_t> para = {load_fila_dist,(int32_t)(factor * motor_speed),(int32_t)(factor * motor_accel),channel,id};
            send_with_respone(SERIAL_CMD_MOTOR_CONTROL,para,0);
            while (cur_time < last_time + load_fila_time)
            {
                reactor->pause(get_monotonic() + 0.1);
                sync_det_info();
                if(1 == canvas_status_info.host_status.ex_fila_status)
                {
                    reactor->pause(get_monotonic() + 20./motor_speed);
                    SPDLOG_INFO("{} #1 ex_fila_status:{}",__func__,canvas_status_info.host_status.ex_fila_status);
                    feeders_filament_stop(channel);
                    gcode->run_script_from_command("M83\nG1 E10 F1200\nM400");
                    break;
                }
                if(canvas_status_info.mcu_status.ch_position[channel] - cur_channel_pos > load_fila_dist)
                {
                    feeders_filament_stop(channel);
                    SPDLOG_ERROR("{} #1 load filament PTFE not plut in",__func__,canvas_status_info.host_status.ex_fila_status);
                    canvas_error_code = elegoo::common::ErrorCode::CANVAS_PLUT_IN_FILA_NOT_TRIGGER;
                    goto plug_in_error;
                }
                cur_time = get_monotonic();
            }
            //2.检测挤出机耗材传感器是否触发,未触发则送料超时报错
            SPDLOG_DEBUG("{} #1 ex_fila_status:{}",__func__,canvas_status_info.host_status.ex_fila_status);
            if(0 == canvas_status_info.host_status.ex_fila_status)
            {
                feeders_filament_stop(channel);
                SPDLOG_ERROR("{} #1 ex_fila_status:{} plut in filament timeout!",__func__,canvas_status_info.host_status.ex_fila_status);
                canvas_error_code = elegoo::common::ErrorCode::CANVAS_PLUT_IN_FILA_TIMEOUT;
                goto plug_in_error;
            }

            SPDLOG_DEBUG("set_fila_origin({})",channel + 1);
            set_fila_origin(channel + 1);
            is_plug_in_filament_over = true;
            if(canvas_status_info.host_status.is_runout_filament_report)
            {
                canvas_status_info.host_status.is_runout_filament_report = false;
                SPDLOG_INFO("{} is_runout_filament_report:{}",__func__,canvas_status_info.host_status.is_runout_filament_report);
            }
            gcode->run_script_from_command("CANVAS_MESH_GEAR DIST=" + std::to_string(mesh_gear_dist));
            SPDLOG_INFO("{} #1 __OVER",__func__);
            return ;

            plug_in_error:
                update_canvas_state(0x0c,0xfe);
                gcode->run_script_from_command("CANVAS_MESH_GEAR DIST=" + std::to_string(mesh_gear_dist));
                SPDLOG_INFO("{} #1 __OVER",__func__);
        }
        /*****************************************************************************
         * @brief        : 加热并移动至废料桶
         * @return        {*}
         *****************************************************************************/
        void CanvasDev::CMD_move_to_waste_box(std::shared_ptr<GCodeCommand> gcmd)
        {
            SPDLOG_INFO("{} #1",__func__);
            SPDLOG_DEBUG("{} #1 move_to_waste_box_macro",__func__);
            move_to_waste_box_macro->run_gcode_from_command();
            SPDLOG_INFO("{} #1 __OVER",__func__);
        }
        /*****************************************************************************
         * @brief        : 清理喷嘴废弃耗材
         * @return        {*}
         *****************************************************************************/
        void CanvasDev::CMD_clean_waste_filament(std::shared_ptr<GCodeCommand> gcmd)
        {
            SPDLOG_INFO("{} #1",__func__);
            //切片实现？
            SPDLOG_DEBUG("{} #1 clean_waste_filament_macro",__func__);
            clean_waste_filament_macro->run_gcode_from_command();
            SPDLOG_INFO("{} #1 __OVER",__func__);
        }
        /*****************************************************************************
         * @brief        : 预进料
         * @param         {shared_ptr<GCodeCommand>} gcmd:
         * @return        {*}
         *****************************************************************************/        
        void CanvasDev::CMD_canvas_preload_filament(std::shared_ptr<GCodeCommand> gcmd)
        {
            SPDLOG_INFO("{} #1",__func__);
            canvas_status_info.host_status.is_pre_loading = true;
            std::vector<int32_t> v = {};
            for(int32_t ii = 0; ii < CHANNEL_NUM; ++ii)
            {
                if(2 == canvas_status_info.host_status.pre_load_ch_status[ii])
                {
                    v.emplace_back(ii);
                }
            }
            for(auto it = v.begin(); it < v.end(); ++it)
            {
                // led呼吸
                std::string cmd_str = "CANVAS_SET_LED_STATUS CHANNEL=" + std::to_string(*it) + " ABNORMAL=" + std::to_string(2);
                SPDLOG_DEBUG("{} *it:{} cmd_str:{}",__func__,*it,cmd_str);
                gcode->run_script_from_command(cmd_str);
                std::vector<int32_t> para = {pre_load_fila_dist,motor_speed,motor_accel,*it,canvas_status_info.mcu_status.did};
                send_with_respone(SERIAL_CMD_MOTOR_CONTROL,para,0);
                double cur_time = get_monotonic();
                double last_time = cur_time;
                while (cur_time < last_time + pre_load_fila_time)
                {
                    reactor->pause(get_monotonic() + 0.1);
                    if(false == canvas_status_info.mcu_status.moving_status[*it])
                    {
                        canvas_status_info.host_status.pre_load_ch_status[*it] = 3;
                        SPDLOG_DEBUG("{} pre_load_ch_status[{}] {}",__func__,*it,canvas_status_info.host_status.pre_load_ch_status[*it]);
                        v.erase(it);
                        break;
                    }
                    cur_time = get_monotonic();
                }
                if(true == canvas_status_info.mcu_status.moving_status[*it])
                {
                    feeders_filament_stop(*it);
                }
            }
            if(0 == v.size())
            {
                // 释放led控制
                gcode->run_script_from_command("CANVAS_SET_LED_STATUS CHANNEL=" + std::to_string(CHANNEL_NUM));
            }
            canvas_status_info.host_status.is_pre_loading = false;
            SPDLOG_INFO("{} #1 __OVER",__func__);
        }
        void CanvasDev::CMD_used_canvas_dev(std::shared_ptr<GCodeCommand> gcmd)
        {
            printing_used_canvas = gcmd->get_int("USED_CANVAS",0,0,1);
            SPDLOG_INFO("{} #1 printing_used_canvas:{}",__func__,printing_used_canvas);
            SPDLOG_INFO("{} #1 __OVER",__func__);
        }
        /*****************************************************************************
         * @brief        : 进料
         * @return        {*}
         *****************************************************************************/    
        void CanvasDev::CMD_canvas_load_filament(std::shared_ptr<GCodeCommand> gcmd)
        {
            SPDLOG_INFO("{} #1",__func__);
            state_feedback("M2202","1150");
            
            int32_t id = gcmd->get_int("ID",0,0,3);
            int32_t channel = gcmd->get_int("CHANNEL",0,0,3);
            SPDLOG_INFO("{} #1 channel {} id {}",__func__,channel,id);
            canvas_status_info.host_status.cur_retry_action = 1;
            canvas_status_info.host_status.cur_retry_param = channel;
            if(canvas_status_info.host_status.fila_origin >= 1 && canvas_status_info.host_status.fila_origin <= CHANNEL_NUM)
            {
                // led呼吸
                gcode->run_script_from_command("CANVAS_SET_LED_STATUS CHANNEL=" + std::to_string(canvas_status_info.host_status.fila_origin - 1) + " ABNORMAL=" + std::to_string(2));
            }
            //1.加热并移动至废料桶
            #ifdef FROM_GCODE_MCRO
            state_feedback("M2202","1151");
            gcode->run_script_from_command("CANVAS_MOVE_TO_WASTE_BOX\nM400");
            #endif
            //2.
            state_feedback("M2202","1152");
            filament_det();
            if(0xfe == canvas_stage_state)
            {
                goto load_fila_error;
            }
            //3.
            state_feedback("M2202","1153");
            gcode->run_script_from_command("CANVAS_CUT_FILAMENT\nM400");
            if(0xfe == canvas_stage_state)
            {
                goto load_fila_error;
            }
            //4.
            state_feedback("M2202","1154");
            if(true == canvas_status_info.host_status.ex_fila_status)
            {
                gcode->run_script_from_command("CANVAS_PLUG_OUT_FILAMENT\nM400");
                if(0xfe == canvas_stage_state)
                {
                    goto load_fila_error;
                }
            }
            //5.
            state_feedback("M2202","1155");
            canvas_status_info.host_status.fila_channel = channel;
            SPDLOG_INFO("{} fila_channel:{}",__func__,canvas_status_info.host_status.fila_channel);
            gcode->run_script_from_command("CANVAS_PLUG_IN_FILAMENT CHANNEL=" + std::to_string(channel) + " ID=" + std::to_string(id) + "\nM400");
            if(0xfe == canvas_stage_state)
            {
                goto load_fila_error;
            }
            //6.冲刷
            #ifdef FROM_GCODE_MCRO
            state_feedback("M2202","1156");
            gcode->run_script_from_command("CANVAS_CLEAN_WASTE_FILAMENT\nM400");
            #endif
            // 释放led控制
            gcode->run_script_from_command("CANVAS_SET_LED_STATUS CHANNEL=" + std::to_string(CHANNEL_NUM));

            load_fila_success:
                update_canvas_state(0x02,0xff);
                canvas_abnormal_state = 0;
                canvas_status_info.host_status.cur_retry_action = 0;
                state_feedback("M2202","1157");
                SPDLOG_INFO("{} #1 ___OVER",__func__);
                return;
            load_fila_error:
                if(0xfe == canvas_stage_state)
                {
                    update_canvas_state(canvas_action_state,0xff);
                }
                canvas_abnormal_state = 0xfe;
                SPDLOG_WARN("{} #1 failed! canvas_abnormal_state {} canvas_stage_state {}",__func__,canvas_abnormal_state,canvas_stage_state);
                gcode->run_script_from_command("ABNORMAL_STATE_PROCESS NEED_PAUSE=0\nM400");
                state_feedback("M2202","1158");
                SPDLOG_INFO("{} #1 ___OVER",__func__);
        }
        /*****************************************************************************
         * @brief        : 退料
         * @return        {*}
         *****************************************************************************/    
        void CanvasDev::CMD_canvas_unload_filament(std::shared_ptr<GCodeCommand> gcmd)
        {
            SPDLOG_INFO("{} #1",__func__);
            state_feedback("M2202","1160");
            if(0 == this->canvas_status_info.host_status.ex_fila_status)
            {
                SPDLOG_WARN("{} #1 ex_fila_status {}",__func__,canvas_status_info.host_status.ex_fila_status);
                goto unload_fila_success;
            }
            canvas_status_info.host_status.cur_retry_action = 2;
            if(canvas_status_info.host_status.fila_origin >= 1 && canvas_status_info.host_status.fila_origin <= CHANNEL_NUM)
            {
                // led呼吸
                gcode->run_script_from_command("CANVAS_SET_LED_STATUS CHANNEL=" + std::to_string(canvas_status_info.host_status.fila_origin - 1) + " ABNORMAL=" + std::to_string(2));
            }
            //1.加热并移动至废料桶
            #ifdef FROM_GCODE_MCRO
            state_feedback("M2202","1161");
            gcode->run_script_from_command("CANVAS_MOVE_TO_WASTE_BOX\nM400");
            #endif
            //2.
            state_feedback("M2202","1162");
            filament_det();
            if(0xfe == canvas_stage_state)
            {
                goto unload_fila_error;
            }
            //3.
            state_feedback("M2202","1163");
            gcode->run_script_from_command("CANVAS_CUT_FILAMENT\nM400");
            if(0xfe == canvas_stage_state)
            {
                goto unload_fila_error;
            }
            //4.
            state_feedback("M2202","1164");
            if(1 == canvas_status_info.host_status.ex_fila_status)
            {
                gcode->run_script_from_command("CANVAS_PLUG_OUT_FILAMENT\nM400");
                if(0xfe == canvas_stage_state)
                {
                    goto unload_fila_error;
                }
            }
            // 释放led控制
            gcode->run_script_from_command("CANVAS_SET_LED_STATUS CHANNEL=" + std::to_string(CHANNEL_NUM));
            //
            unload_fila_success:
                update_canvas_state(0x03,0xff);
                canvas_abnormal_state = 0;
                canvas_status_info.host_status.cur_retry_action = 0;
                state_feedback("M2202","1165");
                SPDLOG_INFO("{} #1 ___OVER",__func__);
                return;
            unload_fila_error:
                if(0xfe == canvas_stage_state)
                {
                    update_canvas_state(canvas_action_state,0xff);
                }
                canvas_abnormal_state = 0xfe;
                SPDLOG_WARN("{} #1 failed! canvas_abnormal_state {} canvas_stage_state {}",__func__,canvas_abnormal_state,canvas_stage_state);
                gcode->run_script_from_command("ABNORMAL_STATE_PROCESS NEED_PAUSE=0\nM400");
                state_feedback("M2202","1166");
                SPDLOG_INFO("{} #1 ___OVER",__func__);
        }
        void CanvasDev::CMD_canvas_channel_motor_ctrl(std::shared_ptr<GCodeCommand> gcmd)
        {
            SPDLOG_INFO("{} #1",__func__);
            if(!canvas_status_info.mcu_status.cur_mcu_connect_status
                // || !printing_used_canvas
                )
            {
                SPDLOG_WARN("{} printing_used_canvas:{} cur_mcu_connect_status:{}",__func__,printing_used_canvas,canvas_status_info.mcu_status.cur_mcu_connect_status);
                return;
            }
            int32_t dist = gcmd->get_int("DIST",-1 * mesh_gear_dist);
            if(dist < 0)
            {
                is_plug_in_filament_over = false;
                SPDLOG_DEBUG("{} #1 is_plug_in_filament_over:{}",__func__,is_plug_in_filament_over);
            }
            int32_t speed = gcmd->get_int("SPEED",motor_speed);
            int32_t accel = gcmd->get_int("ACCEL",motor_accel);
            int32_t time = gcmd->get_int("TIME",mesh_gear_time);
            int32_t channel = gcmd->get_int("CHANNEL",canvas_status_info.host_status.fila_origin - 1);
            if(channel >= 0 && channel < CHANNEL_NUM)
            {
                SPDLOG_DEBUG("{} #1 channel:{} fila_origin:{}",__func__,channel,canvas_status_info.host_status.fila_origin);
                std::vector<int32_t> para = {dist,speed,accel,channel,canvas_status_info.mcu_status.did};
                send_with_respone(SERIAL_CMD_MOTOR_CONTROL,para,time);
            }
            else
            {
                SPDLOG_DEBUG("{} #1 channel:{} fila_origin:{}",__func__,channel,canvas_status_info.host_status.fila_origin);
            }
            SPDLOG_INFO("{} #1 __OVER",__func__);
        }
        /*****************************************************************************
         * @brief        : 设置耗材信息
         * @return        {*}
         *****************************************************************************/    
        void CanvasDev::CMD_canvas_set_filament_info(std::shared_ptr<GCodeCommand> gcmd)
        {
            SPDLOG_INFO("{} #1",__func__);
            int32_t id = gcmd->get_int("ID",0,0,3);
            int32_t channel = gcmd->get_int("CHANNEL",0,0,3);
            bool extern_fila = gcmd->get_int("EXTERN_FILA",0,0,1);
            // 1 标识耗材编辑cid
            this->canvas_status_info.host_status.edit_filament_status[id] = 1;
            //2 刷新通道耗材信息
            filament_info_t set_filament_info;
            set_filament_info.manufacturer = gcmd->get("MANUFACTURER","ELEGOO");
            set_filament_info.brand = gcmd->get("BRAND","0xEEEEEEEE");
            set_filament_info.code = gcmd->get("CODE","0x0001");
            set_filament_info.type = gcmd->get("TYPE","PLA");
            set_filament_info.detailed_type = gcmd->get("DETAILED_TYPE","CF");
            set_filament_info.color = gcmd->get("COLOR","0xFF3700");
            set_filament_info.diameter = gcmd->get("DIAMETER","1.75");
            set_filament_info.weight = gcmd->get("WEIGHT","1000");
            set_filament_info.date = gcmd->get("DATE","2502");
            set_filament_info.nozzle_min_temp = gcmd->get("NOZZLE_MIN_TEMP","170.0");
            set_filament_info.nozzle_max_temp = gcmd->get("NOZZLE_MAX_TEMP","250.0");
            SPDLOG_DEBUG("{} id:{},channel:{},extern_fila:{} manufacturer:{} brand:{} code:{} type:{} detailed_type:{} color:{} diameter:{} weight:{} date:{} nozzle_min_temp:{} nozzle_max_temp:{}",__func__,id,channel,extern_fila,set_filament_info.manufacturer,set_filament_info.brand,set_filament_info.code,set_filament_info.type,set_filament_info.detailed_type,set_filament_info.color,set_filament_info.diameter,set_filament_info.weight,set_filament_info.date,set_filament_info.nozzle_min_temp,set_filament_info.nozzle_max_temp);
            // 3 耗材信息本地持久化
            sync_rfid_info(id,channel,set_filament_info,extern_fila);
            // 结束
            SPDLOG_INFO("{} #1 __OVER",__func__);
        }
        
        void CanvasDev::CMD_canvas_rfid_select(std::shared_ptr<GCodeCommand> gcmd)
        {
            SPDLOG_INFO("{} #1",__func__);
            if(0x0f == canvas_action_state && 0x0e == canvas_stage_state)
            {
                int32_t select_id = gcmd->get_int("ID",0,0,3);
                select_channel = gcmd->get_int("CHANNEL",0,0,3);
                bool cancel_state = gcmd->get_int("CANCEL",0,0,1);
                SPDLOG_INFO("{} select_id:{},select_channel:{},cancel_state:{}",__func__,select_id,select_channel,cancel_state);
                if(!cancel_state)
                {
                    sync_rfid_info(select_id,select_channel,filament_info);
                }
                update_canvas_state(0x0f,0xff);
                std::lock_guard<std::mutex> lock(rfid_mtx);
                is_rfid_trig = false;
            }
            else
            {
                SPDLOG_WARN("{} #1 error!",__func__);
            }
            SPDLOG_INFO("{} #1 __OVER",__func__);
        }

        /*****************************************************************************
         * @brief        : 发起rfid扫描
         * @return        {*}
         *****************************************************************************/    
        void CanvasDev::CMD_canvas_rfid_scan(std::shared_ptr<GCodeCommand> gcmd)
        {
            SPDLOG_INFO("{} #1",__func__);
            //1 启动扫描
            scan_id = gcmd->get_int("ID",0,0,3);
            scan_channel = gcmd->get_int("CHANNEL",0,0,3);
            rfid_scan_time = get_monotonic();
            update_canvas_state(0x04,0x01);
            SPDLOG_INFO("{} #1 __OVER",__func__);
        }
        /*****************************************************************************
         * @brief        : 取消rfid扫描
         * @return        {*}
         *****************************************************************************/    
        void CanvasDev::CMD_canvas_rfid_cancel_scan(std::shared_ptr<GCodeCommand> gcmd)
        {
            SPDLOG_INFO("{} #1",__func__);
            //取消扫描
            int32_t scan_id = gcmd->get_int("ID",0,0,3);
            int32_t scan_channel = gcmd->get_int("CHANNEL",0,0,3);
            update_canvas_state(0x04,0x02);
            SPDLOG_INFO("{} #1 __OVER",__func__);
        }
        void CanvasDev::CMD_canvas_set_led_status(std::shared_ptr<GCodeCommand> gcmd)
        {
            if(!is_canvas_dev_connect())
            {
                SPDLOG_WARN("{} canvas is not connect",__func__);
                return;
            }
            int32_t red = 0,blue = 0;
            int32_t channel = gcmd->get_int("CHANNEL",CHANNEL_NUM,0,CHANNEL_NUM);
            int32_t abnormal = gcmd->get_int("ABNORMAL",0);
            if(1 == abnormal)
            {
                // 异常时红灯闪烁，500ms每次
                red = gcmd->get_int("RED",(int32_t)LED_BLINK_2Hz,(int32_t)LED_OFF,(int32_t)(LED_MAX - 1));  // 0-4
                blue = gcmd->get_int("BLUE",(int32_t)LED_OFF,(int32_t)LED_OFF,(int32_t)(LED_MAX - 1));
            }
            else if(2 == abnormal)
            {
                // 送料时白灯呼吸，4s每次
                red = gcmd->get_int("RED",(int32_t)LED_OFF,(int32_t)LED_OFF,(int32_t)(LED_MAX - 1));  // 0-4
                blue = gcmd->get_int("BLUE",(int32_t)LED_BREATHE,(int32_t)LED_OFF,(int32_t)(LED_MAX - 1));
            }
            SPDLOG_INFO("{} channel:{} abnormal:{} red:{} blue:{}",__func__,channel,abnormal,red,blue);
            led_status_control(channel,(feeder_led_state_e)red,(feeder_led_state_e)blue);
        }
        /*****************************************************************************
         * @brief        : 设置自动续料开关使能
         * @return        {*}
         *****************************************************************************/    
        void CanvasDev::CMD_canvas_set_auto_plug_in(std::shared_ptr<GCodeCommand> gcmd)
        {
            SPDLOG_INFO("{} #1",__func__);
            canvas_status_info.host_status.auto_plug_in_enable = gcmd->get_int("ENABLE",1,0,1);
            canvas_dev.auto_plug_in_enable = canvas_status_info.host_status.auto_plug_in_enable;
            SPDLOG_INFO("{} #1 auto_plug_in_enable {}",__func__,canvas_status_info.host_status.auto_plug_in_enable);
            need_save_record = 1;
            SPDLOG_INFO("{} #1 __OVER",__func__);
        }
        /*****************************************************************************
         * @brief        : 设置颜色映射表
         * @param         {shared_ptr<GCodeCommand>} gcmd:
         * @return        {*}
         *****************************************************************************/        
        void CanvasDev::CMD_canvas_set_color_table(std::shared_ptr<GCodeCommand> gcmd)
        {
            SPDLOG_INFO("{} #1",__func__);
            std::string T = gcmd->get("T","0,1,2,3");
            std::string channel_table = gcmd->get("CHANNEL","0,1,2,3");
            std::string color_table = gcmd->get("COLOR","0xFF3700,0x735DF9,0x0080FF,0xFFC800"); // 红色 紫色 蓝色 黄色 最大32色
            auto T_vec = elegoo::common::split(T,",");
            auto channel_table_vec = elegoo::common::split(channel_table,",");
            auto color_table_vec = elegoo::common::split(color_table,",");
            if(T_vec.size())
            {
                SPDLOG_DEBUG("channel_color_table.size:{} T_vec.size:{} color_table_vec.size:{}",channel_color_table.size(),T_vec.size(),color_table_vec.size());
                channel_color_table.clear();
                if(channel_table_vec.size() < T_vec.size())
                {
                    channel_table_vec.resize(T_vec.size(),"0");
                }
                if(color_table_vec.size() < T_vec.size())
                {
                    color_table_vec.resize(T_vec.size(),"0xFF3700");
                }
                SPDLOG_DEBUG("channel_color_table.size:{} T_vec.size:{} color_table_vec.size:{}",channel_color_table.size(),T_vec.size(),color_table_vec.size());
            }
            for (size_t i = 0; i < T_vec.size(); i++)
            {
                std::tuple<std::string,std::string,std::string> tup(T_vec[i],color_table_vec[i],channel_table_vec[i]);
                channel_color_table.emplace_back(tup);
                need_save_record = 1;
                SPDLOG_DEBUG("{} #1 table_T {} table_color {} table_channel {} need_save_record {}",__func__,std::get<0>(channel_color_table[i]),std::get<1>(channel_color_table[i]),std::get<2>(channel_color_table[i]),need_save_record);
            }
            SPDLOG_INFO("channel_color_table.size:{} T:{} channel_table:{}",channel_color_table.size(),T,channel_table);
            SPDLOG_INFO("{} #1 __OVER",__func__);
        }
        /*****************************************************************************
         * @brief        : 切换耗材
         * @return        {*}
         *****************************************************************************/
        void CanvasDev::CMD_canvas_switch_filament(std::shared_ptr<GCodeCommand> gcmd)
        {
            SPDLOG_INFO("{} #1",__func__);
            int32_t channel = -1;
            int32_t T = gcmd->get_int("T",0,0,255);
            // SELECT 0x00 执行全流程 0x01 不执行切刀 0x02 不执行退料 0x04 不执行送料
            int32_t select = gcmd->get_int("SELECT",0);//7);//
            if(0xfe == canvas_stage_state && select)
            {
                reactor->pause(get_monotonic() + 1.5);
                SPDLOG_WARN("{} canvas_action_state:{} canvas_stage_state:{}",__func__,canvas_action_state,canvas_stage_state);
            }
            //
            if(!canvas_status_info.mcu_status.cur_mcu_connect_status)
            {
                SPDLOG_WARN("{} printing_used_canvas:{} cur_mcu_connect_status:{}",__func__,printing_used_canvas,canvas_status_info.mcu_status.cur_mcu_connect_status);
                goto switch_fila_success;
            }
            //
            canvas_status_info.host_status.switch_filment_T = T;
            canvas_status_info.host_status.cur_retry_action = 3;
            canvas_status_info.host_status.cur_retry_param = T;
            SPDLOG_INFO("{} T:{} switch_filment_T:{} select:{}",__func__,T,canvas_status_info.host_status.switch_filment_T,select);
            channel = get_T_channel(T) - 1;
            if(channel >= CHANNEL_NUM || channel < 0)
            {
                SPDLOG_ERROR("can not find zhe filament T{} channel:{}",T,channel);
                goto switch_fila_error;
            }
            //
            filament_det();
            if(0xfe == canvas_stage_state)
            {
                goto switch_fila_error;
            }
            //
            if(!(select & 0x01))
            {
                gcode->run_script_from_command("CANVAS_CUT_FILAMENT\nM400");
                if(0xfe == canvas_stage_state)
                {
                    goto switch_fila_error;
                }
            }
            //
            if(!(select & 0x02) && true == canvas_status_info.host_status.ex_fila_status)
            {
                gcode->run_script_from_command("CANVAS_PLUG_OUT_FILAMENT\nM400");
                if(0xfe == canvas_stage_state)
                {
                    goto switch_fila_error;
                }
            }
            //
            canvas_status_info.host_status.fila_channel = channel;
            canvas_status_info.host_status.switch_filment_color = canvas_dev.canvas_lite.channels[channel].filament.color;
            canvas_status_info.host_status.switch_filment_type = canvas_dev.canvas_lite.channels[channel].filament.type;
            SPDLOG_INFO("{} T:{} fila_channel:{}",__func__,T,canvas_status_info.host_status.fila_channel);
            if(!(select & 0x04))
            {
                gcode->run_script_from_command("CANVAS_PLUG_IN_FILAMENT CHANNEL=" + std::to_string(channel) + "\nM400");
                if(0xfe == canvas_stage_state)
                {
                    goto switch_fila_error;
                }
            }
            //
            switch_fila_success:
                update_canvas_state(0x06,0xff);
                canvas_abnormal_state = 0;
                is_abnormal_error = 0;
                canvas_status_info.host_status.cur_retry_action = 0;
                SPDLOG_INFO("{} #1 __OVER",__func__);
                return;
            switch_fila_error:
                if(0xfe == canvas_stage_state)
                {
                    update_canvas_state(canvas_action_state,0xff);
                }
                canvas_abnormal_state = 0xfe;
                is_abnormal_error = 0x01;
                SPDLOG_WARN("{} #1 failed! canvas_abnormal_state {} canvas_stage_state {} is_abnormal_error:{}",__func__,canvas_abnormal_state,canvas_stage_state,is_abnormal_error);
                reactor->update_timer(canvas_timer,_NOW);
                SPDLOG_INFO("{} #1 __OVER",__func__);
        }
        
        void CanvasDev::CMD_canvas_abnormal_retry(std::shared_ptr<GCodeCommand> gcmd)
        {
            bool is_paused = pause_resume->get_status(0.)["is_paused"].get<bool>();
            bool is_resume = gcmd->get_int("RESUME",0,0,1);
            if(!is_canvas_dev_connect())
            {
                SPDLOG_WARN("{} canvas is not connect",__func__);
                goto abnormal_retry_success;
            }
            SPDLOG_INFO("{} #1 cur_retry_abnormal_det:{} cur_retry_action:{} is_paused:{}",__func__,canvas_status_info.host_status.cur_retry_abnormal_det,canvas_status_info.host_status.cur_retry_action,is_paused);
            if(3 == canvas_status_info.host_status.cur_retry_action && !is_paused)
            {
                SPDLOG_WARN("{} cur not paused",__func__);
                goto abnormal_retry_success;
            }
            //
            switch (canvas_status_info.host_status.cur_retry_action)
            {
            case 1:
                gcode->run_script_from_command("CANVAS_LOAD_FILAMENT CHANNEL=" + std::to_string(canvas_status_info.host_status.cur_retry_param) + "\nM400");
                if(0xff == canvas_stage_state)
                {
                    SPDLOG_INFO("{} load filament retry success,resume ...",__func__);
                }
                else
                {
                    goto abnormal_retry_failed;
                }
                break;
            case 2:
                gcode->run_script_from_command("CANVAS_UNLOAD_FILAMENT\nM400");
                if(0xff == canvas_stage_state)
                {
                    SPDLOG_INFO("{} unload filament retry success,resume ...",__func__);
                }
                else
                {
                    goto abnormal_retry_failed;
                }
                break;
            case 3:
                gcode->run_script_from_command("CANVAS_SWITCH_FILAMENT T=" + std::to_string(canvas_status_info.host_status.cur_retry_param) + " SELECT=0\nM400");
                if(0 == canvas_abnormal_state)
                {
                    SPDLOG_INFO("{} switch filament retry success,resume ...",__func__);
                    if(!is_resume)
                    {
                        gcode->run_script_from_command("RESUME\nM400");
                    }
                }
                else
                {
                    goto abnormal_retry_failed;
                }
                break;
            default:
                break;
            }

            abnormal_retry_success:
                canvas_error_code = 0;
                SPDLOG_INFO("{} #1 abnormal_retry_success __OVER",__func__);
                return;
            abnormal_retry_failed:
                SPDLOG_WARN("{} #1 abnormal_retry_failed __OVER",__func__);
        }

        void CanvasDev::CMD_abnormal_state_process(std::shared_ptr<GCodeCommand> gcmd)
        {
            // bool is_paused = pause_resume->get_status(0.)["is_paused"].get<bool>();
            // if(is_paused)
            // {
            //     return eventtime + 3.;
            // }
            bool need_pause = gcmd->get_int("NEED_PAUSE",1,0,1);
            SPDLOG_WARN("abnormal has occurred 0x{:02x} canvas_error_code:{} need_pause:{}",canvas_action_state,canvas_error_code,need_pause);
            if(need_pause)
            {
                SPDLOG_WARN("abnormal has occurred and needs to be pause");
                if(true == canvas_status_info.host_status.is_nozzle_fan_off_report)
                {
                    gcode->run_script_from_command("PAUSE GOTO_WASTE_BOX=0\nM400");
                }
                else
                {
                    gcode->run_script_from_command("PAUSE\nM400");
                }
            }
            //
            after_abnormal_pause_action();
            //
            if(canvas_error_code)
            {
                gcode->respond_ecode("", canvas_error_code, elegoo::common::ErrorLevel::WARNING);
                canvas_error_code = 0;
            }
        }

        /*****************************************************************************
         * @brief        : 扫描请求
         * @return        {*}
         *****************************************************************************/
        void CanvasDev::request_rfid(uint32_t id,uint32_t channel,bool is_scan)
        {
            std::tuple<bool,uint32_t,uint32_t> param = {is_scan,channel,id};
            send_with_respone(SERIAL_CMD_RFID_REQUEST,param,1.);
        }
        /*****************************************************************************
         * @brief        : 同步扫描结果
         * @param         {filament_info_t} filament:
         * @return        {*}
         *****************************************************************************/
        void CanvasDev::sync_rfid_info(uint32_t id,uint32_t channel,filament_info_t filament,bool is_extern_fila)
        {
            if("lite" == this->canvas_status_info.mcu_status.type)
            {
                if(is_extern_fila)
                {
                    canvas_dev.canvas_lite.extern_channel.filament = filament;
                }
                else
                {
                    canvas_dev.canvas_lite.channels[channel].filament = filament;
                }
                need_save_record = 1;
                SPDLOG_INFO("{} #1 channel:{} filament.type:{} filament.color:{} need_save_record:{}",__func__,channel,filament.type,filament.color,need_save_record);
            }
        }
        
        void CanvasDev::rfid_trig_det()
        {
            if(true == is_rfid_trig && 0x00 == canvas_action_state)
            {
                rfid_scan_time = get_monotonic();
                update_canvas_state(0x0f,0x01);
            }
        }
        
        /*****************************************************************************
         * @brief        : 同步检测信息
         * @return        {*}
         *****************************************************************************/
        void CanvasDev::sync_det_info()
        {
            get_cavans_mcu_info();

#if 1
            if(!canvas_status_info.mcu_status.cur_mcu_connect_status)
            {
                return;
            }
            static double cur_det_time = 0.;
            static double last_det_time = 0.;
            cur_det_time = get_monotonic();
            if(cur_det_time - last_det_time > 2. * 60.) //5. * 60.) //
            {
                last_det_time = cur_det_time;
                SPDLOG_DEBUG("canvas_status_info.host_status.pre_load_ch_status:{} {} {} {}",canvas_status_info.host_status.pre_load_ch_status[0],canvas_status_info.host_status.pre_load_ch_status[1],canvas_status_info.host_status.pre_load_ch_status[2],canvas_status_info.host_status.pre_load_ch_status[3]);
                SPDLOG_DEBUG("canvas_status_info.host_status.ch_status:{} {} {} {}",canvas_status_info.host_status.ch_status[0],canvas_status_info.host_status.ch_status[1],canvas_status_info.host_status.ch_status[2],canvas_status_info.host_status.ch_status[3]);
                SPDLOG_DEBUG("canvas_status_info.host_status.ex_fila_status:{}",canvas_status_info.host_status.ex_fila_status);
                SPDLOG_DEBUG("canvas_status_info.host_status.fila_origin:{}",canvas_status_info.host_status.fila_origin);
                SPDLOG_DEBUG("is_plug_in_filament_over:{}",is_plug_in_filament_over);
                SPDLOG_DEBUG("lite_connected:{} cur_mcu_connect_status:{}",lite_connected,canvas_status_info.mcu_status.cur_mcu_connect_status);
            }
#endif
        }

        json CanvasDev::get_canvas_power_outage_status(double eventtime)
        {
            json status;
            status["printing_used_canvas"] = printing_used_canvas;
            status["switch_filment_T"] = canvas_status_info.host_status.switch_filment_T;
            status["color_table_size"] = channel_color_table.size();
            for(auto ii = 0; ii < channel_color_table.size(); ++ii)
            {
                status["color_table" + std::to_string(ii)]["T"] = std::get<0>(channel_color_table[ii]);
                status["color_table" + std::to_string(ii)]["color"] = std::get<1>(channel_color_table[ii]);
                status["color_table" + std::to_string(ii)]["channel"] = std::get<2>(channel_color_table[ii]);
                SPDLOG_DEBUG("{} T:{} color:{} channel:{}","color_table" + std::to_string(ii),std::get<0>(channel_color_table[ii]),std::get<1>(channel_color_table[ii]),std::get<2>(channel_color_table[ii]));
            }
    
            SPDLOG_INFO("{} status.dump:{}",__func__,status.dump());
            return status;
        }

        json CanvasDev::get_canvas_status(double eventtime)
        {
            json status;
            status["auto_plug_in_filament"] = canvas_status_info.host_status.auto_plug_in_enable;
            if(canvas_status_info.host_status.is_runout_filament_report && !canvas_status_info.host_status.is_need_plut_in_filment)
            {
                status["canvas_runout_filament"] = true;
            }
            else
            {
                status["canvas_runout_filament"] = false;
            }
            return status;
        }

        json CanvasDev::get_status(double eventtime)
        {
            json status = {};
            status["type"] = canvas_dev.type;
            status["auto_plug_in_enable"] = canvas_dev.auto_plug_in_enable;
            status["active_did"] = canvas_dev.active_did;
            status["active_cid"] = canvas_status_info.host_status.fila_channel + 1;//canvas_dev.active_cid;
            status["canvas_lite"]["did"] = canvas_dev.canvas_lite.did;
            status["canvas_lite"]["connected"] = canvas_dev.canvas_lite.connected;
            for(auto ii = 0; ii <= CHANNEL_NUM; ++ii)
            {
                std::string channels_tmp = {};
                filament_channels_t channel_tmp = {};
                if(ii == CHANNEL_NUM)
                {
                    channels_tmp = "extern_channel";
                    channel_tmp = canvas_dev.canvas_lite.extern_channel;
                }
                else
                {
                    channels_tmp = "channels" + std::to_string(ii);
                    channel_tmp = canvas_dev.canvas_lite.channels[ii];
                }
                status["canvas_lite"][channels_tmp]["cid"] = channel_tmp.cid;
                status["canvas_lite"][channels_tmp]["status"] = channel_tmp.status;
                status["canvas_lite"][channels_tmp]["filament"]["manufacturer"] = channel_tmp.filament.manufacturer;
                status["canvas_lite"][channels_tmp]["filament"]["brand"] = channel_tmp.filament.brand;
                status["canvas_lite"][channels_tmp]["filament"]["code"] = channel_tmp.filament.code;
                status["canvas_lite"][channels_tmp]["filament"]["type"] = channel_tmp.filament.type;
                status["canvas_lite"][channels_tmp]["filament"]["detailed_type"] = channel_tmp.filament.detailed_type;
                status["canvas_lite"][channels_tmp]["filament"]["color"] = channel_tmp.filament.color;
                status["canvas_lite"][channels_tmp]["filament"]["diameter"] = channel_tmp.filament.diameter;
                status["canvas_lite"][channels_tmp]["filament"]["weight"] = channel_tmp.filament.weight;
                status["canvas_lite"][channels_tmp]["filament"]["date"] = channel_tmp.filament.date;
                status["canvas_lite"][channels_tmp]["filament"]["nozzle_min_temp"] = channel_tmp.filament.nozzle_min_temp;
                status["canvas_lite"][channels_tmp]["filament"]["nozzle_max_temp"] = channel_tmp.filament.nozzle_max_temp;
            }

            return status;
        }

        // Filament In/Out Control ch 1~4
        bool CanvasDev::feeder_filament_control(uint8_t ch, const FilamentControl& fc)
        {
            if (ch < 1 || ch > 4) {
                return false;
            }

            int16_t len = fc.mm;
            int16_t spd = std::abs(fc.mm_s);
            int16_t acc = std::abs(fc.mm_ss);

            std::vector<uint8_t> data;
            data.push_back(CANVAS_LITE_DEVICE_ID);
            data.push_back(CMD_FILA_CTRL_SINGLE);
            data.push_back(ch);
            data.push_back((len >> 8) & 0xFF);
            data.push_back((len >> 0) & 0xFF);
            data.push_back((spd >> 8) & 0xFF);
            data.push_back((spd >> 0) & 0xFF);
            data.push_back((acc >> 8) & 0xFF);
            data.push_back((acc >> 0) & 0xFF);

            std::vector<uint8_t> tx_frame = pack_short_frame(data);
            if (!updating) {
                std::vector<uint8_t> rx_frame =  rs485->send_and_wait(tx_frame, std::chrono::milliseconds(0));
                return check_short_frame_respond(CANVAS_LITE_DEVICE_ID, CMD_FILA_CTRL_SINGLE, rx_frame);
            }

            return false;
        }

        bool CanvasDev::feeders_filament_control(const std::array<FilamentControl, 4>& filament_control)
        {
            std::array<int16_t, 4> lens;
            std::array<int16_t, 4> spds;
            std::array<int16_t, 4> accs;

            for (std::size_t i = 0; i < 4; i++) {
                lens[i] = filament_control[i].mm;
                spds[i] = std::abs(filament_control[i].mm_s);
                accs[i] = std::abs(filament_control[i].mm_ss);
            }

            std::vector<uint8_t> data;
            data.push_back(CANVAS_LITE_DEVICE_ID);
            data.push_back(CMD_FILA_CTRL);

            for (std::size_t i = 0; i < 4; i++) {
                data.push_back((lens[i] >> 8) & 0xFF);
                data.push_back((lens[i] >> 0) & 0xFF);
                data.push_back((spds[i] >> 8) & 0xFF);
                data.push_back((spds[i] >> 0) & 0xFF);
                data.push_back((accs[i] >> 8) & 0xFF);
                data.push_back((accs[i] >> 0) & 0xFF);
            }

            std::vector<uint8_t> tx_frame = pack_short_frame(data);
            if (!updating) {
                std::vector<uint8_t> rx_frame =  rs485->send_and_wait(tx_frame, std::chrono::milliseconds(0));
                return check_short_frame_respond(CANVAS_LITE_DEVICE_ID, CMD_FILA_CTRL, rx_frame);
            }

            return false;
        }

        // chs bit3~bit0 : ch4~ch1
        bool CanvasDev::feeders_filament_stop(uint8_t chs)
        {
            switch (chs)
            {
            case 0:
                chs = 0b0001;
                break;
            
            case 1:
                chs = 0b0010;
                break;
            
            case 2:
                chs = 0b0100;
                break;
            
            case 3:
                chs = 0b1000;
                break;
            
            default:
                SPDLOG_WARN("{} chs:{}",__func__,chs);
                return false;
            }
            SPDLOG_DEBUG("{} chs:{}",__func__,chs);

            std::vector<uint8_t> data;
            data.push_back(CANVAS_LITE_DEVICE_ID);
            data.push_back(CMD_FEEDER_STOP);
            data.push_back(chs);

            std::vector<uint8_t> tx_frame = pack_short_frame(data);
            if (!updating) {
                std::vector<uint8_t> rx_frame =  rs485->send_and_wait(tx_frame, std::chrono::milliseconds(0));
                return check_short_frame_respond(CANVAS_LITE_DEVICE_ID, CMD_FEEDER_STOP, rx_frame);
            }
            return false;
        }

        bool CanvasDev::feeders_leds_control(const std::array<feeder_led_t, 4>& leds)
        {
            std::vector<uint8_t> data;
            data.push_back(CANVAS_LITE_DEVICE_ID);
            data.push_back(CMD_LED_CONTROL);

            uint8_t enable_control;
            for (std::size_t i = 0; i < leds.size(); i++) {
                if (leds[i].enable_control) {
                    enable_control |= (1 << i);
                } else {
                    enable_control &= ~(1 << i);
                }
            }
            data.push_back(enable_control);

            for (std::size_t i = 0; i < leds.size(); i++) {
                data.push_back(leds[i].red);
                data.push_back(leds[i].blue);
            }

            std::vector<uint8_t> tx_frame = pack_short_frame(data);
            if (!updating) {
                std::vector<uint8_t> rx_frame =  rs485->send_and_wait(tx_frame, std::chrono::milliseconds(0));
                return check_short_frame_respond(CANVAS_LITE_DEVICE_ID, CMD_LED_CONTROL, rx_frame);
            }
            return false;
        }

        bool CanvasDev::beeper_control(uint8_t times, uint16_t duration_ms)
        {
            std::vector<uint8_t> data;
            data.push_back(CANVAS_LITE_DEVICE_ID);
            data.push_back(CMD_BEEPER_CONTROL);
            data.push_back(times);
            data.push_back((duration_ms >> 8) & 0xFF);
            data.push_back((duration_ms >> 0) & 0xFF);

            std::vector<uint8_t> tx_frame = pack_short_frame(data);
            if (!updating) {
                std::vector<uint8_t> rx_frame =  rs485->send_and_wait(tx_frame, std::chrono::milliseconds(0));
                return check_short_frame_respond(CANVAS_LITE_DEVICE_ID, CMD_BEEPER_CONTROL, rx_frame);
            }
            return false;
        }

        std::string CanvasDev::read_software_version()
        {
            std::vector<uint8_t> data;
            data.push_back(CANVAS_LITE_DEVICE_ID);
            data.push_back(CMD_SOFTWARE_VERSION);

            std::vector<uint8_t> tx_frame = pack_short_frame(data);
            std::vector<uint8_t> rx_frame =  rs485->send_and_wait(tx_frame, std::chrono::milliseconds(100));
            if (check_short_frame_respond(CANVAS_LITE_DEVICE_ID, CMD_SOFTWARE_VERSION, rx_frame)) {
                std::vector<uint8_t> rx_data = unpack_short_frame(rx_frame);
                std::string str(rx_data.begin() + 2, rx_data.begin() + 2 + 11);
                // SPDLOG_INFO("Canvas Software Version: {}", str);
                return str;
            } else {
                return "";
            }
        }

        bool CanvasDev::read_rfid_data(RFID &rfid_)
        {
            std::vector<uint8_t> tx_data;

            tx_data.push_back(CANVAS_LITE_DEVICE_ID);
            tx_data.push_back(CMD_RFID_DATA);

            std::vector<uint8_t> tx_frame = pack_short_frame(tx_data);
            std::vector<uint8_t> rx_frame =  rs485->send_and_wait(tx_frame, std::chrono::milliseconds(100));
            if (!check_short_frame_respond(CANVAS_LITE_DEVICE_ID, CMD_RFID_DATA, rx_frame)) {
                // SPDLOG_ERROR("{}() Respond Check Error", __func__);
                return false;
            }
            std::vector<uint8_t> rx_data = unpack_short_frame(rx_frame);

            if (rx_data[2] && false == is_rfid_trig) {
                uint8_t* ptr = rx_data.data() + 3;
                std::lock_guard<std::mutex> lock(rfid_mtx);
                memcpy(&rfid_, ptr, sizeof(RFID));
                rfid_scan_uid = rfid_.uid;
                if(rfid_scan_uid > 0)
                {
                    is_rfid_trig = true;
                }
                return true;
            } else {
                return false;
            }

            return false;
        }

        bool CanvasDev::read_feeders_status()
        {
            std::vector<uint8_t> tx_data;
            std::vector<FeederStatus> feeders_;
            feeders_.resize(CHANNEL_NUM,{});

            tx_data.push_back(CANVAS_LITE_DEVICE_ID);
            tx_data.push_back(CMD_ALL_STATUS);

            std::vector<uint8_t> tx_frame = pack_short_frame(tx_data);
            std::vector<uint8_t> rx_frame =  rs485->send_and_wait(tx_frame, std::chrono::milliseconds(100));

            if (!check_short_frame_respond(CANVAS_LITE_DEVICE_ID, CMD_ALL_STATUS, rx_frame)) {
                // SPDLOG_ERROR("{}() Respond Check Error", __func__);
                return false;
            }

            std::vector<uint8_t> rx_data = unpack_short_frame(rx_frame);

            status = rx_data[2];
            std::size_t index = 3; // ID, Command
            for (std::size_t i = 0; i < 4; i++) {
                feeders_[i].status = rx_data[index + 0];
                feeders_[i].odo_value32 =    rx_data[index + 1] << 24 | rx_data[index + 2] << 16 | 
                                            rx_data[index + 3] << 8 | rx_data[index + 4] << 0;
                feeders_[i].encoder_value32 =    rx_data[index + 5] << 24 | rx_data[index + 6] << 16 | 
                                                rx_data[index + 7] << 7 | rx_data[index + 8] << 0;
                index += 9;

                feeders_[i].fila_abnormal   = (feeders_[i].status >> 7) & 0x01; 
                feeders_[i].dragging        = (feeders_[i].status >> 6) & 0x01;
                feeders_[i].drag_compensate_error = (feeders_[i].status >> 4) & 0x01;
                feeders_[i].fila_stalled    = (feeders_[i].status >> 3) & 0x01;
                feeders_[i].motor_crtl_err  = (feeders_[i].status >> 2) & 0x01;
                feeders_[i].fila_in         = (feeders_[i].status >> 1) & 0x01;
                feeders_[i].motor_fault     = (feeders_[i].status >> 0) & 0x01;
                // TODO: 64bit encoder data

                SPDLOG_TRACE("feeders[{}] fila_in = {} status = {}, odo_value32 = {}, encoder_value32 = {}", i, feeders_[i].fila_in , feeders_[i].status, feeders_[i].odo_value32, feeders_[i].encoder_value32);
            }
            feeders = feeders_;

            return true;
        }

        void CanvasDev::auto_update_firmware()
        {
            std::string firmware_path = has_canvas_bin(firmware_dir);

            if (firmware_path.empty()) {
                SPDLOG_WARN("Canvas firmware not found");
            } else {
                SPDLOG_INFO("Found Canvas firmware");

                // 正则：匹配中间的类似 x.x.x.x 的版本号
                std::regex pattern(R"((\d+\.\d+\.\d+\.\d+))");
                std::smatch match;

                if (std::regex_search(firmware_path, match, pattern)) {
                    SPDLOG_INFO("Extracted Canvas firmware version: {}", match[1].str());
                } else {
                    SPDLOG_WARN("No canvas firmware found");
                }

                std::string software_version =  read_software_version();

                if (software_version.empty()) { // 读版本失败
                    SPDLOG_ERROR("Failed to read Canvas software version");

                    // 1. Ping bootloader，回应表示APP异常，无回应表示Canvas未连接
                    // 2. update
                    // 3. 正常运行
                    std::string tty = "/dev/ttyS1";
                    int baudrate = 115200;
                    SerialBootloader sb(tty, baudrate);
                    sb.connect();
                    if (sb.ping()) {
                        SPDLOG_WARN("Canvas No App");
                        updating = true;
                        state_feedback("CANVAS_UPDATE", std::to_string(updating));
                    } else {
                        SPDLOG_INFO("Canvas Not Connected");
                    }
                    sb.disconnect();
                } else {
                    SPDLOG_INFO("Canvas Software Version: {}", software_version);

                    if (software_version == match[1].str()) {
                        SPDLOG_INFO("Canvas firmware version matched");
                    } else {
                        SPDLOG_INFO("Canvas firmware version mismatched");
                        SPDLOG_INFO("Start update firmware");
                        updating = true;
                        state_feedback("CANVAS_UPDATE", std::to_string(updating));
                    }
                }
            }

            if (updating) {
                if (update_firmware(firmware_path)) {
                    SPDLOG_ERROR("Canvas firmware update failed");
                } else {
                    SPDLOG_INFO("Canvas firmware updated successfully");
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                updating = false;
                state_feedback("CANVAS_UPDATE", std::to_string(updating));
            }
        }

        void CanvasDev::canvas_communication()
        {
            bool connected = false;
            bool last_lite_connected = lite_connected;
            const std::chrono::milliseconds period(100);
            auto next = std::chrono::steady_clock::now() + period;
            auto last = std::chrono::steady_clock::now();

            auto_update_firmware();

            while (true)
            {
                SPDLOG_TRACE("Thread {} Running", __func__);

                connected = read_feeders_status();

                if (connected) {
                    lite_connected = true;
                    last = std::chrono::steady_clock::now();
                } else {
                    if (std::chrono::steady_clock::now() - last > std::chrono::seconds(1)) {
                        lite_connected = false;
                    }
                }

                if ((lite_connected != last_lite_connected) && lite_connected) {
                    auto_update_firmware();
                }

                if (lite_connected != last_lite_connected) {
                    state_feedback("CANVAS_SOFTWARE_VERSION", read_software_version());
                }

                last_lite_connected = lite_connected;

                // SPDLOG_DEBUG("Thread RS485 Running lite_connected:{}",lite_connected);
                if (status & 0x01) {
                    if (read_rfid_data(rfid)) {
                        std::string output;
                        for (const uint8_t &byte : rfid.data) {
                            output += fmt::format("0x{:02X}, ", byte);
                        }
                        SPDLOG_DEBUG("RFID Tag UID: 0x{:016X}", rfid.uid);
                        SPDLOG_DEBUG("RFID Tag Data:\n{}", output);
                    }
                    
                } else {
                    // SPDLOG_DEBUG("None RFID Tag");
                }

                std::this_thread::sleep_until(next);
                next += period;
            }
        }

        std::shared_ptr<CanvasDev> canvas_dev_load_config(std::shared_ptr<ConfigWrapper> config)
        {
            SPDLOG_INFO("canvas_dev_load_config()");
            return std::make_shared<CanvasDev>(config);
        }

    }
}

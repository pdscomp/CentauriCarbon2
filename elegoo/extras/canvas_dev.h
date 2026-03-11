/***************************************************************************** 
 * @Author       : loping
 * @Date         : 2025-05-28 16:26:32
 * @LastEditors  : loping
 * @LastEditTime : 2025-09-06 11:43:04
 * @Description  : 
 * 
 * Copyright (c) 2025 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#ifndef __CANVAS_DEV_H__
#define __CANVAS_DEV_H__


#include <string>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <functional>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include "printer.h"
#include "configfile.h"
#include "common/any.h"
#include "rs485.h"

// pb3 s2 pb6 s5    耗材检测 风扇脱落 切刀检测 缠料检测 PB3 PB4 PB5 PB6
// host pd21 pullup rdid供电
namespace elegoo
{
    namespace extras
    {
        /* 通讯协议 */
        // |<--      帧头     -->|
        // 帧头  标志   长度  CRC8      负载n        CRC16
        // 0x3D  flag  len  0x00  0x00 ... 0x00  CRC_H CRC_L
        constexpr uint8_t FRAME_HEAD  = 0x3D;
        constexpr uint8_t FRAME_SHORT = 0x80;
        constexpr uint8_t FRAME_LONG  = 0x00;

        // Short Frame Index
        constexpr std::size_t INDEXS_HEAD = 0x00;    // 帧头索引
        constexpr std::size_t INDEXS_FLAG = 0x01;    // 帧标志索引
        constexpr std::size_t INDEXS_LEN  = 0x02;    // 长度索引
        constexpr std::size_t INDEXS_CRC8 = 0x03;    // CRC8索引
        constexpr std::size_t INDEXS_ID   = 0x04;    // 设备ID索引
        constexpr std::size_t INDEXS_CMD  = 0x05;    // 命令字索引

        // 负载n
        // Device ID 1Byte
        constexpr uint8_t CANVAS_LITE_DEVICE_ID   = 0x00;    // Canvas Lite Device ID: 0x00
        // Command: 1Byte
        constexpr uint8_t CMD_CONNECT_STATUS      = 0x00;    // 连接状态
        constexpr uint8_t CMD_HARDWARE_VERSION    = 0x01;    // 查询硬件版本
        constexpr uint8_t CMD_SOFTWARE_VERSION    = 0x02;    // 查询软件版本
        constexpr uint8_t CMD_FILAMENT_PLUG_IN    = 0x04;    // 查询是否插入料
        constexpr uint8_t CMD_ODO_VALUE           = 0x08;    // 查询编码器值
        constexpr uint8_t CMD_ENCODER_STATUS      = 0x10;    // 查询编码器状态
        constexpr uint8_t CMD_MOTOR_STATUS        = 0x20;    // 查询电机状态
        constexpr uint8_t CMD_RFID_DATA           = 0x40;    // 查询RFID数据
        constexpr uint8_t CMD_ALL_STATUS          = 0x7F;    // 查询所有状态
        constexpr uint8_t CMD_LED_CONTROL         = 0x81;    // LED控制
        constexpr uint8_t CMD_BEEPER_CONTROL      = 0x82;    // 蜂鸣器控制
        constexpr uint8_t CMD_FEEDER_STOP         = 0x90;    // 停止送料
        constexpr uint8_t CMD_FILA_CTRL           = 0x91;    // 送料控制
        constexpr uint8_t CMD_FILA_CTRL_SINGLE    = 0x92;    // 送料控制
        constexpr uint8_t CMD_FILA_CTRL_REACH_ACK = 0x93;    // 送料控制到达确认（仅单通道控制）
        constexpr uint8_t CMD_FILA_CTRL_BOTH_ACK  = 0x94;    // 送料控制收到应答+到达回应（仅单通道控制）

        // Response
        constexpr uint8_t RESPOND_OK          = 0x00;    // 响应成功
        constexpr uint8_t RESPOND_POS_REACHED = 0x01;    // 响应位置到达
        constexpr uint8_t RESPOND_ERROR       = 0x80;    // 响应错误

        // RFID
        constexpr std::size_t NTAG_MAX_UID_LEN = 0x08;
        constexpr std::size_t NTAG_BYTES_PER_PAGE = 0x04;
        constexpr std::size_t NTAG_END_PAGES = 0x2C;
        constexpr std::size_t NTAG_MAX_PAGES = NTAG_END_PAGES + 1;

        constexpr int32_t CHANNEL_NUM = 4;                  // 单个设备通道数

        // LED状态
        typedef enum
        {
            LED_OFF = 0,
            LED_ON,
            LED_BLINK_1Hz,
            LED_BREATHE,
            LED_BLINK_2Hz,
            LED_MAX,
        } feeder_led_state_e;

        typedef struct
        {
            bool enable_control;    // 是否启用LED控制
            feeder_led_state_e red; // 红色LED状态
            feeder_led_state_e blue; // 蓝色LED状态
        } feeder_led_t;

        typedef struct
        {
            // 多色下位机状态
            bool    moving_status[CHANNEL_NUM];                 // 通道电机是否转动
            int32_t motor_stalling_status[CHANNEL_NUM];         // 电机堵转状态: 0 未触发 1 已触发
            int32_t scan_rfid_uid;                              // rfuid > 0 获取到耗材数据
            int32_t ch_fila_status[CHANNEL_NUM];                // 通道耗材传感器状态: 0 未触发 1 已触发
            int32_t last_ch_fila_status[CHANNEL_NUM];           // 上一次通道耗材传感器状态: 0 未触发 1 已触发
            double  ch_position[CHANNEL_NUM];                   // 通道里程位置
            double  ch_fila_move_dist[CHANNEL_NUM];             // 通道耗材移动距离 单位 mm
            bool    cur_mcu_connect_status;                     // 串口连接状态
            bool    last_mcu_connect_status;
            int32_t did;                                        // 设备ID
            std::string type;                                   // "type": "lite", //设备类型
        }canvas_mcu_status_t;

        typedef struct
        {
            // 多色上位机状态
            int32_t edit_filament_status[CHANNEL_NUM];      //耗材编辑状态 0 未手动编辑 1 已手动编辑
            int32_t ex_fila_status;                         // 挤出机耗材传感器(断料检测)状态: 0 未触发 1 已触发
            int32_t cutting_knife_status;                   // 切刀状态: 0 未触发 1 已触发
            int32_t nozzle_fan_off_status;                  // 喷嘴风扇脱离状态: 0 未触发 1 已触发
            int32_t wrap_filament_status;                   // 缠料状态: 0 未触发 1 已触发
            int32_t switch_filment_T;                       // 切换耗材T
            std::string switch_filment_color;               // 切换耗材颜色
            std::string switch_filment_type;                // 切换耗材类型
            bool    is_switch_and_clean_filment;            // 切换耗材类型
            bool    is_need_plut_in_filment;                // 需要自动续料
            bool    is_runout_filament_report;              // 断料异常上报
            bool    is_wrap_filament_report;                // 缠料异常上报
            bool    is_locked_rotor_report;                 // 堵转异常上报
            bool    is_cutting_knife_report;                // 切刀异常上报
            bool    is_nozzle_fan_off_report;               // 喷嘴风扇脱落异常上报
            int32_t ch_status[CHANNEL_NUM];                 // 通道状态: 0 空闲 1 预进料 2 已进料
            int32_t fila_origin;                            // 耗材来源: 0 无 1-4 cid 5:CHANNEL_NUM + 1 未知
            int32_t fila_channel;
            int32_t cur_retry_action;                       // 0 空闲 1 进料 2 退料 3 切换耗材
            int32_t cur_retry_param;                        // 
            int32_t cur_retry_abnormal_det;                 // 
            uint8_t auto_plug_in_enable;
            int32_t pre_load_ch_status[CHANNEL_NUM];        // 预进料通道状态: 0 空闲 1 预进料中 2 预进料结束
            bool    is_pre_loading;
            double  before_load_ch_pos[CHANNEL_NUM];        // 进料前通道里程位置
        }canvas_host_status_t;

        typedef struct
        {
            canvas_mcu_status_t mcu_status;
            canvas_host_status_t host_status;
        }canvas_status_info_t;

        typedef struct
        {
            std::string manufacturer;           // "ELEGOO", //耗材厂商
            std::string brand;                  // "0xEEEEEEEE", //耗材厂商代码 "ELEGOO"
            std::string code;                   // "0x0001", //耗材编码
            std::string type;                   // "PLA", //耗材类型
            std::string detailed_type;          // "CF", //耗材详细类型
            std::string color;                  // "0xFF3700", //耗材颜色RGB
            std::string diameter;               // "1.75", //耗材直径mm
            std::string weight;                 // "1000", //耗材重量g
            std::string date;                   // "2502", //生产日期YYMM
            std::string nozzle_min_temp;        // 170.0, //喷嘴最小温度(℃)
            std::string nozzle_max_temp;        // 250.0 //喷嘴最大温度(℃)
            // std::string heater_bed_min_temp;    // 25.0, //热床最小温度(℃)
            // std::string heater_bed_max_temp;    // 60.0, //热床最大温度(℃)
            // std::string max_dry_time;           // 4h, // 最大烘干时间
            // std::string max_dry_temp;           // 55.0, // 最大烘干温度
            // std::string transparency;           // 透明度  0x00  0x00编码，表示完全透明
            // std::string fan_ctrl;               // 风扇控制  0x01  表示打印时风扇为开启状态
            // std::string header;                 // 标头  0x36  EPC-256标识
        }filament_info_t;

        typedef struct
        {
            int32_t cid;                //  1, //通道ID 似乎没必要
            int32_t status;             //  0, //通道状态：0 未知状态 1 已预进料 2 已进料
            filament_info_t filament;
        }filament_channels_t;

        typedef struct
        {
            int32_t connected;          //  1, //RS485连接状态
            int32_t did;                //  1, //设备ID
            filament_channels_t channels[CHANNEL_NUM];
            filament_channels_t extern_channel;
        }canvas_lite_t;

        typedef struct
        {
            int32_t canvas_lite_num;
            // canvas_lite_t *canvas_lite;
        }canvas_pro_t;

        typedef struct 
        {
            std::string type;       //"type": "lite", //设备类型
            uint32_t active_did;    //"active_did": 0, //已进料设备ID，0代表无，1~4
            uint32_t active_cid;    //"active_cid": 0, //已进料通道ID，0代表无，1~4
            uint8_t auto_plug_in_enable;    // 自动续料状态
            canvas_lite_t canvas_lite;
            std::vector<canvas_pro_t> canvas_pro_vec;
        }canvas_dev_t;

        constexpr int32_t SERIAL_CMD_FILAMENT_PRE_LOAD = 0x01;                                  // 预进料 
        constexpr int32_t SERIAL_CMD_MOTOR_CONTROL = SERIAL_CMD_FILAMENT_PRE_LOAD + 0x01;       // 通道电机控制  did cid speed[max speed: 269.55] (+/-)dist  mm/s
        constexpr int32_t SERIAL_CMD_RFID_REQUEST = SERIAL_CMD_MOTOR_CONTROL + 0x01;            // rfid扫描 
        constexpr int32_t SERIAL_CMD_LED_CONTROL = SERIAL_CMD_RFID_REQUEST + 0x01;              // led控制  red/blue 灭/长亮/闪烁 ledid
        
        #pragma pack(push, 1)  // 设置结构体按 1 字节对齐
        struct RFID
        {
            uint64_t uid;
            uint8_t data[NTAG_BYTES_PER_PAGE * NTAG_MAX_PAGES];
        };
        #pragma pack(pop)  // 恢复默认对齐

        struct FeederStatus
        {
            uint8_t status;
            // bit7~bit0
            bool fila_abnormal; // 送料异常
            bool dragging;      // 送料中
            bool reserve5;
            bool drag_compensate_error; // 送料补偿错误
            bool fila_stalled;  // 送料堵转
            bool motor_crtl_err;// 电机控制错误
            bool fila_in;       // 料丝插入
            bool motor_fault;   // 电机故障

            int32_t odo_value32;
            int32_t last_odo_value32;
            int64_t odo_value;
            int32_t encoder_value32;
            int32_t last_encoder_value32;
            int64_t encoder_value; 
        };

        struct FilamentControl
        {
            int16_t mm;     // mm       
            int16_t mm_s;   // mm/s
            int16_t mm_ss;  // mm/(s^2)
        };

        // Filament Type
        constexpr uint32_t PLA  = 0x00807665;
        constexpr uint32_t PETG = 0x80698471;
        constexpr uint32_t ABS  = 0x00656683;
        constexpr uint32_t TPU  = 0x00848085;
        constexpr uint32_t PA   = 0x00008065;
        constexpr uint32_t CPE  = 0x00678069;
        constexpr uint32_t PC   = 0x00008067;
        constexpr uint32_t PVA  = 0x00808665;
        constexpr uint32_t ASA  = 0x00658365;
        constexpr uint32_t BVOH = 0x42564F48;
        constexpr uint32_t EVA  = 0x00455641;
        constexpr uint32_t HIPS = 0x48495053;
        constexpr uint32_t PP   = 0x00005050;
        constexpr uint32_t PPA  = 0x00505041;

        // Filament PLA
        constexpr uint16_t PLA_         = 0x0000; // 聚乳酸
        constexpr uint16_t PLA_Plus     = 0x0001;
        constexpr uint16_t PLA_Hyper    = 0x0002;
        constexpr uint16_t PLA_Silk     = 0x0003;
        constexpr uint16_t PLA_CF       = 0x0004;
        constexpr uint16_t PLA_Carbon   = 0x0005;
        constexpr uint16_t PLA_Matte    = 0x0006;
        constexpr uint16_t PLA_Fluo     = 0x0007;
        constexpr uint16_t PLA_Wood     = 0x0008;
        // Filament PETG
        constexpr uint16_t PETG_        = 0x0100; // 共聚酯
        constexpr uint16_t PETG_CF      = 0x0101;
        constexpr uint16_t PETG_Hyper   = 0x0102;
        // Filament ABS
        constexpr uint16_t ABS_         = 0x0200; // 丙烯腈-丁二烯-苯乙烯共聚物
        constexpr uint16_t ABS_Hyper    = 0x0201;
        // Filament TPU
        constexpr uint16_t TPU_         = 0x0300; // 热塑性聚氨酯弹性体
        constexpr uint16_t TPU_Hyper    = 0x0301;
        // Filament PA
        constexpr uint16_t PA_          = 0x0400; // 聚酰胺（尼龙）
        constexpr uint16_t PA_CF        = 0x0401;
        constexpr uint16_t PA_Hyper     = 0x0402;
        // Filament CPE
        constexpr uint16_t CPE_         = 0x0500; // 饱和高分子材料
        constexpr uint16_t CPE_Hyper    = 0x0501;
        // Filament PC
        constexpr uint16_t PC_          = 0x0600; // 聚碳酸酯
        constexpr uint16_t PC_PCTG      = 0x0601;
        constexpr uint16_t PC_Hyper     = 0x0602;
        // Filament PVA
        constexpr uint16_t PVA_         = 0x0700; // 聚氯乙烯
        constexpr uint16_t PVA_Hyper    = 0x0701;
        // Filament ASA
        constexpr uint16_t ASA_         = 0x0800; // 三元共聚物工程塑料
        constexpr uint16_t ASA_Hyper    = 0x0801;
        // Filament BVOH
        constexpr uint16_t BVOH_        = 0x0900; // 乙烯-乙烯醇共聚物
        // Filament EVA
        constexpr uint16_t EVA_         = 0x0A00; // 乙烯-醋酸乙烯共聚物
        // Filament HIPS
        constexpr uint16_t HIPS_        = 0x0B00; // 高抗冲聚苯乙烯
        // Filament PP
        constexpr uint16_t PP_          = 0x0C00; // 聚丙烯
        constexpr uint16_t PP_CF        = 0x0C01;
        constexpr uint16_t PP_GF        = 0x0C02;
        // Filament PPA
        constexpr uint16_t PPA_         = 0x0D00; // 聚邻苯二甲酰胺
        constexpr uint16_t PPA_CF       = 0x0D01;
        constexpr uint16_t PPA_GF       = 0x0D02;

        // Filament Color
        constexpr uint8_t RED[3]    = { 0xFF, 0x37, 0x00}; // 红色
        constexpr uint8_t GREEN[3]  = { 0x33, 0xD7, 0x00}; // 绿色
        constexpr uint8_t BLUE[3]   = { 0x00, 0x80, 0xFF}; // 蓝色
        constexpr uint8_t ORIGIN[3] = { 0xFF, 0x8C, 0x00}; // 橙色
        constexpr uint8_t PURPLE[3] = { 0x73, 0x5D, 0xF9}; // 紫色
        constexpr uint8_t WHITE[3]  = { 0xFF, 0xFF, 0xFF}; // 白色
        constexpr uint8_t BLACK[3]  = { 0x00, 0x00, 0x00}; // 黑色
        constexpr uint8_t YELLOW[3] = { 0xFF, 0xC8, 0x00}; // 黄色
        constexpr uint8_t Cyan[3]   = { 0x44, 0xF1, 0xFF}; // 青色

        // Filament Transparency
        constexpr uint8_t TRANSPARENT = 0x00;   // 完全透明
        constexpr uint8_t OPAQUE = 0xFF;        // 完全不透明

        // Filament Fan Control
        constexpr uint8_t FAN_OFF   = 0x00; // 打印时关闭风扇
        constexpr uint8_t FAN_ON    = 0xFF; // 打印时开启风扇

        #pragma pack(push, 1)  // 设置结构体按 1 字节对齐
        struct Filament
        // struct __attribute__((packed)) Filament
        {
            uint8_t header;         // 标头  0x36  EPC-256标识
            uint32_t brand;         // 厂商代码  0xEEEEEEEE  表示ELEGOO
            uint16_t code;          // 耗材编码  0x0001  ELEGOO内部耗材编码
            uint32_t type;          // 材质类型  0x0807665（“PLA”）  转换成ASCII码后，表示“PLA”材质类型
            uint16_t name;          // 材质名称  0x0001  0x0001表示“PLA+”的耗材名称
            uint16_t date;          // 生产日期  0x1902  表示2025年2月份
            uint8_t rgb[3];         // 颜色编码  0xFF3700  表示红色，颜色编码采用RGB888
            uint8_t transparency;   // 透明度  0x00  0x00编码，表示完全透明
            uint16_t low_tmp;       // 最低打印温度  0x00B4  180℃
            uint16_t hig_tmp;       // 最高打印温度  0x00DC  220℃
            uint8_t low_bed_tmp;    // 打印热床最低温度  0x19  25℃
            uint8_t hig_bed_tmp;    // 打印热床最高温度  0x3C  60℃
            uint8_t hig_dry_tmp;    // 推荐最高烘干温度  0x37  55℃
            uint8_t max_dry_time;   // 推荐最长烘干时间  0x04  表示4h（小时），烘干时间以小时为单位
            uint16_t diameter;      // 耗材直径  0x00AF（175）  表示耗材直径为1.75mm
            uint16_t weight;        // 耗材重量  0x03E8（1000）  表示重量为1000g的耗材
            uint8_t fan_ctrl;       // 风扇控制  0x01  表示打印时风扇为开启状态
        };
        #pragma pack(pop)  // 恢复默认对齐

        class PrinterButtons;
        class IdleTimeout;
        class PrintStats;
        class PauseResume;
        
        class CanvasDev : public std::enable_shared_from_this<CanvasDev>
        {
        public:
            CanvasDev(std::shared_ptr<ConfigWrapper> config);
            ~CanvasDev();
            json get_canvas_power_outage_status(double eventtime);
            json get_canvas_status(double eventtime);
            json get_status(double eventtime);
            bool is_canvas_dev_connect();
            bool is_printing_used_canvas();
            void set_printing_used_canvas(bool state);
            bool get_ex_fila_status();
            bool is_canvas_lite_connected();
            int32_t get_T_channel(int32_t T);
            int32_t get_cur_channel();
            int32_t is_abnormal_state();
            void must_pause_work(bool from_cmd = false);
            //
            RFID get_rfid_raw();
            std::vector<FeederStatus> get_feeder_status();
            bool feeders_leds_control(const std::array<feeder_led_t, 4>& leds);
            bool feeder_filament_control(uint8_t ch, const FilamentControl& fc);
            bool feeders_filament_control(const std::array<FilamentControl, 4>& filament_control);
            bool feeders_filament_stop(uint8_t chs);
            bool beeper_control(uint8_t times, uint16_t duration_ms);
            std::string read_software_version();
            void auto_update_firmware();

        private:
            void canvas_init();
            void handle_connect();
            void handle_disconnect();
            void canvas_handle_ready();
            void canvas_shutdown();
            void save_record_info();
            void read_record_info();
            double canvas_callback(double eventtime);
            double abnormal_process(double eventtime);
            void serial_connect_det();
            void led_status_control(int32_t channel,feeder_led_state_e red = LED_BLINK_2Hz,feeder_led_state_e blue = LED_OFF,int32_t id = 0);
            void set_fila_origin(int32_t fila_origin);
            int32_t det_ch_fila_status(double eventtime);
            std::vector<int32_t> det_change_cid(int32_t ex_move_dist,int32_t change_dist_min,int32_t change_dist_max,bool from_cmd = true);
            void fila_origin_det(double eventtime);
            void sync_det_info();
            void rfid_trig_det();
            void sync_rfid_info(uint32_t id,uint32_t channel,filament_info_t filament,bool is_extern_fila = false);
            void request_rfid(uint32_t id,uint32_t channel,bool is_scan);
            int32_t update_canvas_state(int32_t action_state,int32_t stage_state);
            void state_det_process(double eventtime);
            void CMD_abnormal_state_process(std::shared_ptr<GCodeCommand> gcmd);
            bool clean_waste_and_start_switch_filament();
            void after_abnormal_pause_action();
            void state_feedback(std::string command,std::string result);
            void get_cavans_mcu_info();
            int32_t get_cavans_rfid_info();
            std::string uint32_to_hex_string(uint32_t value,std::string hex_prefix = "",char ch = '0',int32_t hex_w = 8,bool is_uppercase = true);
            int32_t send_with_respone(int32_t serial_cmd,Any param,double wait_time = 3.);
            // 霍尔开关
            double canvas_filament_det_handler(double eventtime, bool state);
            double canvas_nozzle_fan_off_handler(double eventtime, bool state);
            double canvas_cutting_knife_handler(double eventtime, bool state);
            double canvas_wrap_filament_handler(double eventtime, bool state);
            double canvas_model_det_handler(double eventtime, bool state);
            //
            void filament_det();
            void load_det();
            void unload_det();
            //gcode cmd
            void CMD_move_to_waste_box(std::shared_ptr<GCodeCommand> gcmd);
            void CMD_clean_waste_filament(std::shared_ptr<GCodeCommand> gcmd);
            void CMD_canvas_detect_filament(std::shared_ptr<GCodeCommand> gcmd);
            void CMD_canvas_cut_filament(std::shared_ptr<GCodeCommand> gcmd);
            void CMD_canvas_plug_out_filament(std::shared_ptr<GCodeCommand> gcmd);
            void CMD_canvas_plug_in_filament(std::shared_ptr<GCodeCommand> gcmd);
            void CMD_used_canvas_dev(std::shared_ptr<GCodeCommand> gcmd);
            void CMD_canvas_preload_filament(std::shared_ptr<GCodeCommand> gcmd);
            void CMD_canvas_load_filament(std::shared_ptr<GCodeCommand> gcmd);
            void CMD_canvas_unload_filament(std::shared_ptr<GCodeCommand> gcmd);
            void CMD_canvas_channel_motor_ctrl(std::shared_ptr<GCodeCommand> gcmd);
            void CMD_canvas_set_filament_info(std::shared_ptr<GCodeCommand> gcmd);
            void CMD_canvas_rfid_select(std::shared_ptr<GCodeCommand> gcmd);
            void CMD_canvas_rfid_scan(std::shared_ptr<GCodeCommand> gcmd);
            void CMD_canvas_rfid_cancel_scan(std::shared_ptr<GCodeCommand> gcmd);
            void CMD_canvas_set_led_status(std::shared_ptr<GCodeCommand> gcmd);
            void CMD_canvas_set_auto_plug_in(std::shared_ptr<GCodeCommand> gcmd);
            void CMD_canvas_set_color_table(std::shared_ptr<GCodeCommand> gcmd);
            void CMD_canvas_switch_filament(std::shared_ptr<GCodeCommand> gcmd);
            void CMD_canvas_abnormal_retry(std::shared_ptr<GCodeCommand> gcmd);
        private:
            std::shared_ptr<ConfigWrapper> config;
            std::shared_ptr<Printer> printer;
            std::shared_ptr<SelectReactor> reactor;
            std::shared_ptr<ReactorTimer> canvas_timer;
            std::shared_ptr<ReactorTimer> abnormal_det_timer;
            std::shared_ptr<GCodeDispatch> gcode;
            std::shared_ptr<IdleTimeout> idle_timeout;
            std::shared_ptr<PrintStats> print_stats;
            std::shared_ptr<PauseResume> pause_resume;
            std::shared_ptr<TemplateWrapper> runout_filament_gcode;
            std::shared_ptr<TemplateWrapper> move_to_waste_box_macro;
            std::shared_ptr<TemplateWrapper> clean_waste_filament_macro;
            std::shared_ptr<TemplateWrapper> filament_load_gcode_1;
            std::shared_ptr<TemplateWrapper> filament_load_gcode_2;
            
            int32_t baudrate;
            std::string serial_port;
            std::shared_ptr<RS485> rs485;
            std::shared_ptr<std::thread> comminucation_thread;
            std::shared_ptr<PrinterButtons> buttons;
            std::string name;
            canvas_dev_t canvas_dev;
            canvas_status_info_t canvas_status_info;
            int32_t need_save_record;
            // 0x00 空闲 0x01 预进料 0x02 进料 0x03 退料 0x04 rfid扫描 0x05 自动续料 0x06 切换耗材 0x07 耗材编辑
            // 0x08 喷嘴加热 0x09 耗材位置检测 0x0a 切断耗材 0x0b 抽回当前耗材 0x0c 送入新耗材 0x0d 冲刷旧耗材 0x0e [断料|缠料|堵料]暂停动作
            //  rfid扫描 0x0f
            int32_t canvas_action_state;
            /* 
            ** 统一使用 0x01 开始 0xff 结束 失败 oxfe
            ** 0 空闲
            ** 预进料: 1 开始 2 结束 
            // ** 进料: 1 喷嘴加热 2 耗材位置检测 3 切断耗材 4 抽回当前耗材 5 送入新耗材 6 冲刷旧耗材  前提: 已预进料
            // ** 退料: 1 加热 2 耗材位置检测 3 切断耗材 4 抽回当前耗材  前提: 已进料
            // ** 自动续料: 1 [进料]  前提: 已启动自动续料开关且断料
            // ** 切换耗材: 1 Z轴抬升 2 喷嘴加热 3 切断耗材 4 抽回当前耗材 5 送入新耗材 6 冲刷旧耗材  前提: 已进料
            ** rfid扫描: 1 启动扫描 2 取消扫描 3 完成扫描 前提: 已预进料且选择可扫描耗材cid
            ** 耗材编辑: 1 标识耗材编辑cid 2 刷新通道耗材信息 3 耗材信息本地持久化   前提: 已预进料且选择可编辑耗材cid
            */
            int32_t canvas_stage_state;
            int32_t canvas_abnormal_state;
            int32_t is_abnormal_error;
            int32_t gear_pole_number;
            int32_t motor_pole_number;
            int32_t motor_speed;
            int32_t motor_accel;
            int32_t pre_load_fila_dist;
            int32_t load_fila_dist;
            int32_t unload_fila_dist;
            int32_t fila_det_dist;
            int32_t mesh_gear_dist;
            double pre_load_fila_time;
            double load_fila_time;
            double unload_fila_time;
            double fila_det_time;
            double mesh_gear_time;
            filament_info_t filament_info;
            int32_t scan_id;
            int32_t scan_channel;
            int32_t select_channel;
            double rfid_scan_time;
            std::vector<std::tuple<std::string,std::string,std::string>> channel_color_table;
            bool pause_on_abnormal;
            bool is_abnormal_fan_off;
            bool is_plug_in_filament_over;
            // 异常状态
            int32_t is_need_abnormal_process;   // 0x01 断料 0x02 缠料 0x04 堵料 0x08 切刀 0x16 前盖脱落
            int32_t canvas_error_code;
            // 异常重试状态
            // 0x01 断料异常 0x02 前盖脱落异常 0x04 切刀异常 0x08 缠料异常 
            // 0x10 堵料异常 0x20 进料异常 0x40 退料异常 0x80 打印中切换异常 0x100 多色通讯异常
            // int32_t canvas_error_retry;
            double starting_usage;
            bool printing_used_canvas;
            bool is_sent_led_ctrl;
            std::mutex lock;

            uint8_t status;
            RFID rfid;
            std::vector<FeederStatus> feeders;
            std::array<feeder_led_t, CHANNEL_NUM> feeders_leds;
            uint8_t rfid_scan_uid;      // rfiduid > 0 获取到扫描数据
            bool is_rfid_trig;          // rfid触发
            bool lite_connected = false;
            std::mutex rfid_mtx;
            std::mutex mcu_status_mtx;
            bool updating = false; // 更新中

            bool read_rfid_data(RFID &rfid_);
            bool read_feeders_status();
            void canvas_communication();
        };

        std::shared_ptr<CanvasDev> canvas_dev_load_config(std::shared_ptr<ConfigWrapper> config);
    }
}



#endif
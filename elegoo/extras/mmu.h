/*****************************************************************************
 * @Author       : Ben
 * @Date         : 2025-08-22 10:28:48
 * @LastEditors  : loping
 * @LastEditTime : 2025-09-30 16:06:49
 * @Description  :
 *
 * Copyright (c) 2025 by ELEGOO, All Rights Reserved.
 *****************************************************************************/
#pragma once
#include "serial.h"
#include <thread>
#include <deque>
#include <vector>
#include <mutex>
#include <condition_variable>
#include "json.h"
#include "toolhead.h"

class ConfigWrapper;
class GCodeCommand;
class GCodeDispatch;
class SelectReactor;
class Printer;
class WebHooks;
class ReactorTimer;

namespace elegoo
{
namespace extras
{

class PrinterButtons;
class TemplateWrapper;
class PauseResume;
class PrintStats;
class VirtualSD;
class GCodeMove;

static constexpr uint8_t CANVAS_DEVICE_ID   = 0x00;    // Canvas Lite Device ID: 0x00
static constexpr uint8_t CHANNEL_NUMBER = 4;                  // 单个设备通道数

enum CanvasFrameFormat{
    MAGIC,
    FLAG,
    SEQ_H,
    SEQ_L,
    SHORT_LEN,
    LONG_LEN_H,
    LONG_LEN_L,
    CRC_8,
    DATA,
};


enum class CanvasCmd : uint8_t {
    CONNECT_STATUS      = 0x00,    // 连接状态
    HARDWARE_VERSION    = 0x01,    // 查询硬件版本
    SOFTWARE_VERSION    = 0x02,    // 查询软件版本
    FILAMENT_PLUG_IN    = 0x04,    // 查询是否插入料
    ODO_VALUE           = 0x08,    // 查询编码器值
    ENCODER_STATUS      = 0x10,    // 查询编码器状态
    MOTOR_STATUS        = 0x20,    // 查询电机状态
    RFID_DATA           = 0x40,    // 查询RFID数据
    ALL_STATUS          = 0x7F,    // 查询所有状态
    LED_CONTROL         = 0x81,    // LED控制
    BEEPER_CONTROL      = 0x82,    // 蜂鸣器控制
    FEEDER_STOP         = 0x90,    // 停止送料
    FILA_CTRL           = 0x91,    // 送料控制
    FILA_CTRL_SINGLE    = 0x92,    // 送料控制
    FILA_CTRL_REACH_ACK = 0x93,    // 送料控制到达确认（仅单通道控制）
    FILA_CTRL_BOTH_ACK  = 0x94,    // 送料控制收到应答+到达回应（仅单通道控制）
    ROCKER_CTRL         = 0xA0,    // 摆臂控制
};

enum class FeederLedState : uint8_t {
    Off = 0,
    On,
    Blink1Hz,
    Breathe,
    Blink2Hz,
    Max,
};

enum CanvasLedMode{
    RELEASE_CONTROL,
    RED_FLASHING,
    WHITE_BREATHING,
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
constexpr uint32_t PPS  = 0x00005053;  // 新增PPS类型

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
constexpr uint16_t ABS_GF       = 0x0202; // 新增ABS-GF
// Filament TPU
constexpr uint16_t TPU_         = 0x0300; // 热塑性聚氨酯弹性体
constexpr uint16_t TPU_Hyper    = 0x0301;
// Filament PA
constexpr uint16_t PA_          = 0x0400; // 聚酰胺（尼龙）
constexpr uint16_t PA_CF        = 0x0401;
constexpr uint16_t PA_Hyper     = 0x0402;
constexpr uint16_t PAHT_CF      = 0x0403; // 新增PAHT-CF
constexpr uint16_t PA6          = 0x0404; // 新增PA6
constexpr uint16_t PA6_CF       = 0x0405; // 新增PA6-CF
constexpr uint16_t PA12         = 0x0406; // 新增PA12
constexpr uint16_t PA12_CF      = 0x0407; // 新增PA12-CF
// Filament CPE
constexpr uint16_t CPE_         = 0x0500; // 氯化聚乙烯
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
// Filament PPS
constexpr uint16_t PPS_         = 0x0E00; // 新增PPS基础类型
constexpr uint16_t PPS_CF       = 0x0E02; // 新增PPS-CF

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



#pragma pack(push, 1)  // 设置结构体按 1 字节对齐
struct RFIDFilamentData
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
// LED配置结构
struct FeederLed {
    bool enable_control = false;      // 是否启用LED控制
    FeederLedState red  = FeederLedState::Off;  // 红灯状态
    FeederLedState blue = FeederLedState::Off;  // 蓝灯状态
};

struct FeederMotor
{
    int16_t mm;     // mm
    int16_t mm_s;   // mm/s
    int16_t mm_ss;  // mm/(s^2)
};


struct CanvasMCUStatus
{
    uint8_t status = 0;
    uint8_t feeders_status[CHANNEL_NUMBER] = {0};
    double ch_position[CHANNEL_NUMBER] = {0.0};
    double ch_fila_move_dist[CHANNEL_NUMBER] = {0.0};
    uint8_t fila_abnormal[CHANNEL_NUMBER] = {0};
    uint8_t dragging[CHANNEL_NUMBER] = {0};
    uint8_t drag_compensate_error[CHANNEL_NUMBER] = {0};
    uint8_t motor_slip[CHANNEL_NUMBER] = {0};
    uint8_t motor_crtl_err[CHANNEL_NUMBER] = {0};
    uint8_t fila_in[CHANNEL_NUMBER] = {0};
    uint8_t motor_fault[CHANNEL_NUMBER] = {0};
    uint8_t connect_status = 0;
    std::string software_version = "";
};


struct FilamentInfo
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
};

struct RFIDData
{
    uint64_t uid;
    uint8_t data[180];
    RFIDFilamentData rfid_filament_data;
    FilamentInfo filament_info;
};


struct CanvasLite
{
    int32_t did = 0;                //  1, //设备ID
    int32_t connected = 0;          //  1, //RS485连接状态
    int32_t status[CHANNEL_NUMBER] = {0};
    FilamentInfo filament_info[CHANNEL_NUMBER];
};

struct ExternalFilament
{
    int32_t status = 0;
    FilamentInfo filament_info;
};

struct CanvasHostStatus
{
    std::string type = "lite";
    CanvasLite canvas_lite;
    ExternalFilament extern_filament;
    int32_t feed_channel_request = -1;
    int32_t feed_channel_current = -1;
    int32_t switch_filment_T = -1;
    uint8_t ex_fila_status = 0;
    uint8_t cutting_knife_status = 0;
    uint8_t nozzle_fan_off_status = 0;
    uint8_t wrap_filament_status = 0;
    uint8_t auto_plug_in_enable = 0;
    double prefeeder_time[CHANNEL_NUMBER] = {0.0};
    uint8_t error_code = 0;
    int32_t report_error_code = 0;
    RFIDData rfid_data;
    uint8_t updating = 0;
};

struct CanvasStatus
{
    CanvasMCUStatus mcu_status;
    CanvasHostStatus host_status;
};

class CanvasProtocol
{
public:
    CanvasProtocol(std::shared_ptr<ConfigWrapper> config);
    ~CanvasProtocol();

    void on_compile_data(uint8_t data);
    void on_decompile_data(const std::vector<uint8_t>& data);
    void on_parse_data();

    CanvasStatus& get_canvas_status();

    bool read_software_version(bool is_block = false);
    bool read_rfid_data(bool is_block = false);
    bool read_feeders_status(bool is_block = false);
    bool feeders_leds_control(const std::array<FeederLed, CHANNEL_NUMBER>& feeders_leds, bool is_block = false);
    bool beeper_control(uint8_t times, uint16_t duration_ms, bool is_block = false);
    bool feeders_filament_stop(uint8_t chs, bool is_block = false);
    bool feeder_filament_control(uint8_t ch, const FeederMotor& feeder_motor, bool is_block = false);
    bool feeders_filament_control(const std::array<FeederMotor, CHANNEL_NUMBER>& feeders_motor, bool is_block = false);
    bool rocker_control(uint8_t ch, int8_t dir, bool is_block = false);


    void set_receive_data(const uint8_t* data, int length);
    std::vector<std::vector<uint8_t>> get_receive_data();

    void set_transmit_data(const uint8_t* data, int length);
    std::vector<uint8_t> get_transmit_data();

private:
    uint8_t get_crc8(const uint8_t* data, int start, int length);
    uint16_t get_crc16(const uint8_t* data, int start, int length);
    std::string uint32_to_hex_string(uint32_t value,std::string hex_prefix = "",char ch = '0',int32_t hex_w = 8,bool is_uppercase = true);
    void dispatch_command(const std::vector<uint8_t>& valid_data);
    std::vector<uint8_t> pack_short_frame(const std::vector<uint8_t>& data);

    private:
    void PARSE_CONNECT_STATUS(const std::vector<uint8_t>& valid_data);
    void PARSE_HARDWARE_VERSION(const std::vector<uint8_t>& valid_data);
    void PARSE_SOFTWARE_VERSION(const std::vector<uint8_t>& valid_data);
    void PARSE_FILAMENT_PLUG_IN(const std::vector<uint8_t>& valid_data);
    void PARSE_ODO_VALUE(const std::vector<uint8_t>& valid_data);
    void PARSE_ENCODER_STATUS(const std::vector<uint8_t>& valid_data);
    void PARSE_MOTOR_STATUS(const std::vector<uint8_t>& valid_data);
    void PARSE_RFID_DATA(const std::vector<uint8_t>& valid_data);
    void PARSE_ALL_STATUS(const std::vector<uint8_t>& valid_data);
    void PARSE_LED_CONTROL(const std::vector<uint8_t>& valid_data);
    void PARSE_BEEPER_CONTROL(const std::vector<uint8_t>& valid_data);
    void PARSE_FEEDER_STOP(const std::vector<uint8_t>& valid_data);
    void PARSE_FILA_CTRL(const std::vector<uint8_t>& valid_data);
    void PARSE_FILA_CTRL_SINGLE(const std::vector<uint8_t>& valid_data);
    void PARSE_FILA_CTRL_REACH_ACK(const std::vector<uint8_t>& valid_data);
    void PARSE_FILA_CTRL_BOTH_ACK(const std::vector<uint8_t>& valid_data);
    void PARSE_ROCKER_CTRL_ACK(const std::vector<uint8_t>& valid_data);
private:
    std::shared_ptr<SelectReactor> reactor;
    CanvasFrameFormat canvas_frame_format;
    uint32_t recv_length;
    uint8_t *recv_buffer;
    uint16_t crc_16;
    uint8_t crc_8;
    uint16_t data_len;
    std::deque<std::vector<uint8_t>> receive_buffers;
    std::deque<std::vector<uint8_t>> transmit_buffers;
    std::mutex mutex_receive;
    std::mutex mutex_transmit;
    std::mutex mutex_cv;
    std::condition_variable cv;
    CanvasStatus canvas_status;

    bool read_software_version_block;
    bool read_rfid_data_block;
    bool read_feeders_status_block;
    bool feeders_leds_control_block;
    bool beeper_control_block;
    bool feeders_filament_stop_block;
    bool feeder_filament_control_block;
    bool feeders_filament_control_block;
    bool rocker_control_block;
    bool is_boot_init;
    int32_t gear_pole_number;
    int32_t motor_pole_number;
};


class CanvasComm {
public:
    CanvasComm(std::string port, int baudrate);
    ~CanvasComm();

    void activate(std::shared_ptr<CanvasProtocol> canvas_protocol);
    void stop();
    void set_error(bool value);
    bool get_error();
private:
    void connect();
    void disconnect();

    void start_receive();
    void start_transmit();
    void start_analysis();
    void on_receive();
    void on_transmit();
    void on_analysis();

private:
    std::shared_ptr<CanvasProtocol> canvas_protocol;
    std::string port;
    int baudrate;
    std::shared_ptr<SerialPort> serial_port;
    std::thread* receive_thread;
    bool run_receive;
    std::thread* transmit_thread;
    bool run_transmit;
    std::thread* analysis_thread;
    bool run_analysis;
    uint8_t* recv_buffer;
    int buffer_length;
    bool is_connected;
    bool is_error;
};


class Canvas {
public:
    Canvas(std::shared_ptr<ConfigWrapper> config);
    ~Canvas();

    json get_status(double eventtime);
    json get_status_product_test(double eventtime);
    void CMD_canvas_load_filament(std::shared_ptr<GCodeCommand> gcmd);
    void CMD_canvas_unload_filament(std::shared_ptr<GCodeCommand> gcmd);
    void CMD_printer_unload_filament(std::shared_ptr<GCodeCommand> gcmd);
    void CMD_move_to_waste_box(std::shared_ptr<GCodeCommand> gcmd);
    void CMD_clean_waste_filament(std::shared_ptr<GCodeCommand> gcmd);
    void CMD_canvas_cut_filament(std::shared_ptr<GCodeCommand> gcmd);
    void CMD_canvas_plug_out_filament(std::shared_ptr<GCodeCommand> gcmd);
    void CMD_canvas_plug_in_filament(std::shared_ptr<GCodeCommand> gcmd);
    void CMD_canvas_switch_filament(std::shared_ptr<GCodeCommand> gcmd);
    void CMD_canvas_set_color_table(std::shared_ptr<GCodeCommand> gcmd);
    void CMD_canvas_channel_motor_ctrl(std::shared_ptr<GCodeCommand> gcmd);
    void CMD_canvas_set_filament_info(std::shared_ptr<GCodeCommand> gcmd);
    void CMD_canvas_detect_filament(std::shared_ptr<GCodeCommand> gcmd);
    void CMD_canvas_abnormal_retry(std::shared_ptr<GCodeCommand> gcmd);
    void CMD_canvas_rfid_select(std::shared_ptr<GCodeCommand> gcmd);
    void CMD_canvas_set_auto_plug_in(std::shared_ptr<GCodeCommand> gcmd);
    void CMD_M6211(std::shared_ptr<GCodeCommand> gcmd);
    void CMD_canvas_motor_control(std::shared_ptr<GCodeCommand> gcmd);

    void clear_abnormal_feedback();
    void set_error_code(int32_t error_code);
    bool get_connect_state();
    int32_t get_cur_channel();
    int32_t get_T_channel(int32_t T);
    json get_canvas_power_outage_status(double eventtime);
    std::shared_ptr<CanvasProtocol> get_canvas_protocol();

private:
    void handle_connect();
    void handle_disconnect();
    void handle_ready();
    void handle_shutdown();
    void handle_print_state(std::string value);
    void save_record_info();
    void read_record_info();


    void filament_det_handler(bool state);
    void nozzle_fan_off_handler(bool state);
    void wrap_filament_handler(bool state);
    double cutting_knife_handler(double eventtime, bool state);
    double model_det_handler(double eventtime, bool state);
    double info_report_handler(double eventtime);
    double filament_block_handler(double eventtime);

    void canvas_controller_thread();
    void state_feedback(std::string command,std::string result);

    double limit_check(double value,double min,double max);
    void clean_waste();
    bool extruder_retract(uint8_t channel);
    bool extruder_feed(uint8_t channel);
    void extruder_channel_check();
    void auto_prefeed_filament();
    void auto_refill_filament();
    void auto_update_firmware();
    bool update_firmware(std::string firmware_path);
    // void rock_arm_control(int32_t channel, FeederMotor feeder_motor, double wait_time);
    void rock_arm_control(uint8_t channel, int8_t dir, double wait_time);
    void led_mode_control(uint8_t channel, CanvasLedMode mode);
    void error_pause();
private:
    std::shared_ptr<CanvasProtocol> canvas_protocol;
    std::shared_ptr<ConfigWrapper> config;
    std::shared_ptr<GCodeDispatch> gcode;
    std::shared_ptr<Printer> printer;
    std::shared_ptr<SelectReactor> reactor;
    std::shared_ptr<VirtualSD> v_sd;
    std::shared_ptr<GCodeMove> gcode_move;
    std::shared_ptr<ToolHead> toolhead;
    std::shared_ptr<CanvasComm> canvas_comm;
    std::shared_ptr<PrinterButtons> buttons;
    std::shared_ptr<WebHooks> webhooks;
    std::shared_ptr<PauseResume> pause_resume;
    std::shared_ptr<PrintStats> print_stats;
    std::shared_ptr<PrinterExtruder> extruder;
    std::shared_ptr<ReactorTimer> rfid_report_timer;
    std::shared_ptr<ReactorTimer> filament_block_check;
    std::shared_ptr<std::thread> canvas_update_thread;
    std::function<json(double)> estimated_print_time;

    std::shared_ptr<TemplateWrapper> runout_filament_gcode;
    std::shared_ptr<TemplateWrapper> move_to_cutting_knift_macro;
    std::shared_ptr<TemplateWrapper> move_to_cutting_knift_retry_macro;
    std::shared_ptr<TemplateWrapper> release_cutting_knift_macro;
    std::shared_ptr<TemplateWrapper> release_cutting_knift_retry_macro;
    std::shared_ptr<TemplateWrapper> move_to_waste_box_macro;
    std::shared_ptr<TemplateWrapper> clean_waste_filament_macro;
    std::shared_ptr<TemplateWrapper> load_fila_ex_gcode_1;
    std::shared_ptr<TemplateWrapper> load_fila_ex_gcode_2;
    std::shared_ptr<TemplateWrapper> filament_load_gcode_1;
    std::shared_ptr<TemplateWrapper> filament_load_gcode_2;
    std::shared_ptr<TemplateWrapper> runout_gcode;
    std::shared_ptr<TemplateWrapper> channel_check_gcode_1;
    std::shared_ptr<TemplateWrapper> channel_check_gcode_2;

    std::vector<std::tuple<std::string,std::string,std::string>> channel_color_table;
    bool detected_rfid;
    bool is_report;

    int32_t motor_speed;
    int32_t pre_load_motor_speed;
    int32_t motor_accel;
    int32_t pre_load_fila_dist;
    int32_t load_fila_dist;
    int32_t unload_fila_dist;
    int32_t fila_det_dist;
    int32_t min_check_distance_1;
    int32_t max_check_distance_1;
    int32_t min_check_distance_2;
    int32_t max_check_distance_2;
    double load_wait_time;
    double load_fila_det_count;
    double load_fila_det_dist;
    double pre_load_fila_time;
    double load_fila_time;
    double unload_fila_time;
    double fila_det_time;
    int32_t mesh_gear_dist;
    double mesh_gear_time;
    //
    int32_t next_extruder;
    bool need_extrude_check;
    double flush_length_single;
    double flush_length;
    double old_filament_e_feedrate;
    double new_filament_e_feedrate;
    double fan_speed_1;
    double fan_speed_2;
    double cool_time;
    double old_filament_temp;
    double new_filament_temp;
    double nozzle_temperature_range_high;
    double old_retract_length_toolchange;
    double new_retract_length_toolchange;
    double printable_height;
    //
    double z_up_height;
    double e_flush_dist;
    double e_flush_ex_dist;
    double flush_accel;
    double x_flush_cool_pos;
    double x_flush_pos1;
    double x_flush_pos2;
    double x_flush_pos3;
    double x_flush_pos4;
    double y_flush_pos1;
    double y_flush_pos2;

    int32_t timeout_count;
    std::string record_file;
    std::string print_state;
    std::string canvas_version;
    std::string firmware_dir;
    bool initial_boot;
    bool save_filament_info;
    bool is_cancel;
    bool exec_retry;
    bool exit_retry;
    double last_extruder_pos;
    double last_channel_pos;
    int32_t last_check_channel;
};



std::shared_ptr<Canvas> canvas_dev_load_config(std::shared_ptr<ConfigWrapper> config);

}
}
/***************************************************************************** 
 * @Author       : error: git config user.name & please set dead value or install git
 * @Date         : 2025-02-27 14:39:27
 * @LastEditors  : Ben
 * @LastEditTime : 2025-03-29 17:28:27
 * @Description  : 
 * 
 * Copyright (c) 2025 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/
#pragma once
#include <vector>
#include <functional>
#include <unordered_map>
#include <cstdint>
#include <memory>
#include "mcu.h"
#include "reactor.h"
#include "query_adc.h"
#include "mcu.h"

namespace elegoo{
namespace extras{

class MCU_buttons {
public:
    MCU_buttons(std::shared_ptr<Printer> printer, std::shared_ptr<MCU> mcu);
    void setup_buttons(const std::vector<PinParams>& pins, std::function<double(double, int)> callback);
    void build_config();
    void handle_buttons_state(const json& params);
    std::vector<std::pair<std::string, int>> pin_list;

private:
    std::shared_ptr<SelectReactor> reactor;
    std::shared_ptr<MCU> mcu;
    std::vector<std::tuple<uint8_t, uint8_t, std::function<double(double, int)>>> callbacks;
    uint8_t invert;
    uint8_t last_button;
    std::shared_ptr<CommandWrapper> ack_cmd;
    uint8_t ack_count;
    int oid;
};

class MCU_ADC_buttons {
public:
    MCU_ADC_buttons(std::shared_ptr<Printer> printer, const std::string& pin, bool pullup);
    void setup_button(double min_value, double max_value, std::function<json(double, bool)> callback);

private:
    void adc_callback(double read_time, double read_value);
    void call_button(int button, bool state);

    std::shared_ptr<SelectReactor> reactor;
    std::vector<std::tuple<double, double, std::function<json(double, bool)>>> buttons;
    int last_button;
    int last_pressed;
    double last_debouncetime;
    int pullup;
    std::string pin;
    double min_value;
    double max_value;

    std::shared_ptr<MCU_adc> mcu_adc;
    std::shared_ptr<QueryADC> query_adc;
};

class BaseRotaryEncoder {
public:
    static const int R_START     = 0x0;
    static const int R_DIR_CW    = 0x10;
    static const int R_DIR_CCW   = 0x20;
    static const int R_DIR_MSK   = 0x30;

    using Callback = std::function<void(double)>;

    BaseRotaryEncoder(Callback cw_callback, Callback ccw_callback);
    virtual ~BaseRotaryEncoder() = default;
    virtual double encoder_callback(double eventtime, int state);

protected:
    Callback cw_callback;
    Callback ccw_callback;
    int encoder_state;
};

class FullStepRotaryEncoder : public BaseRotaryEncoder {
public:
    static const int R_CW_FINAL  = 0x1;
    static const int R_CW_BEGIN  = 0x2;
    static const int R_CW_NEXT   = 0x3;
    static const int R_CCW_BEGIN = 0x4;
    static const int R_CCW_FINAL = 0x5;
    static const int R_CCW_NEXT  = 0x6;
    static const int ENCODER_STATES[7][4];

    FullStepRotaryEncoder(Callback cw_callback, Callback ccw_callback);
    double encoder_callback(double eventtime, int state) override;
};

class HalfStepRotaryEncoder : public BaseRotaryEncoder {
public:
    static const int R_CCW_BEGIN   = 0x1;
    static const int R_CW_BEGIN    = 0x2;
    static const int R_START_M     = 0x3;
    static const int R_CW_BEGIN_M  = 0x4;
    static const int R_CCW_BEGIN_M = 0x5;

    static const int ENCODER_STATES[6][4];
    HalfStepRotaryEncoder(Callback cw_callback, Callback ccw_callback);

    double encoder_callback(double eventtime, int state) override;
};

class PrinterButtons {
public:
    using ButtonCallback = std::function<double(double)>;

    PrinterButtons(std::shared_ptr<ConfigWrapper> config);
    void register_adc_button(const std::string& pin, double min_val, double max_val, bool pullup, std::function<double(double, bool)> callback);
    void register_adc_button_push(const std::string& pin, double min_val, double max_val, bool pullup, ButtonCallback callback);
    void register_buttons(const std::vector<std::string>& pins, std::function<double(double, int)> scallback);
    void register_rotary_encoder(const std::string& pin1, const std::string& pin2, ButtonCallback cw_callback, ButtonCallback ccw_callback, int steps_per_detent);
    void register_button_push(const std::string& pin, std::function<double(double, int)> callback);

private:
    std::shared_ptr<Printer> printer;
    std::unordered_map<std::string, std::shared_ptr<MCU_buttons>> mcu_buttons;
    std::unordered_map<std::string, std::shared_ptr<MCU_ADC_buttons>> adc_buttons;
};

std::shared_ptr<PrinterButtons> buttons_load_config(std::shared_ptr<ConfigWrapper> config);
}
}
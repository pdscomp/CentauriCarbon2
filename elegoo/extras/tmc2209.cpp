/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2024-11-16 16:44:55
 * @LastEditors  : Jack
 * @LastEditTime : 2024-11-26 14:52:24
 * @Description  : TMC2209 configuration
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "tmc2209.h"
namespace elegoo
{
    namespace extras
    {

        const double TMC_FREQUENCY = 12000000.;
        // clang-format off
std::map<std::string, uint8_t> Registers = {
    {"GCONF", 0x00},      {"GSTAT", 0x01},        {"IFCNT", 0x02},
    {"SLAVECONF", 0x03},  {"OTP_PROG", 0x04},     {"OTP_READ", 0x05},
    {"IOIN", 0x06},       {"FACTORY_CONF", 0x07}, {"IHOLD_IRUN", 0x10},
    {"TPOWERDOWN", 0x11}, {"TSTEP", 0x12},        {"TPWMTHRS", 0x13},
    {"VACTUAL", 0x22},    {"MSCNT", 0x6a},        {"MSCURACT", 0x6b},
    {"CHOPCONF", 0x6c},   {"DRV_STATUS", 0x6f},   {"PWMCONF", 0x70},
    {"PWM_SCALE", 0x71},  {"PWM_AUTO", 0x72},     {"TCOOLTHRS", 0x14},
    {"COOLCONF", 0x42},   {"SGTHRS", 0x40},       {"SG_RESULT", 0x41}};

std::vector<std::string> ReadRegisters = {
    "GCONF",        "GSTAT",   "IFCNT",     "OTP_READ", "IOIN",
    "FACTORY_CONF", "TSTEP",   "MSCNT",     "MSCURACT", "CHOPCONF",
    "DRV_STATUS",   "PWMCONF", "PWM_SCALE", "PWM_AUTO", "SG_RESULT"};
        // clang-format on

        std::map<std::string, std::map<std::string, uint32_t>> Fields = {
            {"GCONF",
             {
                 {"i_scale_analog", 0x01},
                 {"internal_rsense", 0x01 << 1},
                 {"en_spreadcycle", 0x01 << 2},
                 {"shaft", 0x01 << 3},
                 {"index_otpw", 0x01 << 4},
                 {"index_step", 0x01 << 5},
                 {"pdn_disable", 0x01 << 6},
                 {"mstep_reg_select", 0x01 << 7},
                 {"multistep_filt", 0x01 << 8},
                 {"test_mode", 0x01 << 9},
             }},

            {"GSTAT",
             {
                 {"reset", 0x01},
                 {"drv_err", 0x01 << 1},
                 {"uv_cp", 0x01 << 2},
             }},

            {"IFCNT",
             {
                 {"ifcnt", 0xff},
             }},

            {"SLAVECONF",
             {
                 {"senddelay", 0xff << 8},
             }},

            {"OTP_PROG",
             {
                 {"otpbit", 0x07},
                 {"otpbyte", 0x03 << 4},
                 {"otpmagic", 0xff << 8},
             }},

            {"OTP_READ",
             {
                 {"otp_fclktrim", 0x1f},
                 {"otp_ottrim", 0x01 << 5},
                 {"otp_internalrsense", 0x01 << 6},
                 {"otp_tbl", 0x01 << 7},
                 {"otp_pwm_grad", 0x0f << 8},
                 {"otp_pwm_autograd", 0x01 << 12},
                 {"otp_tpwmthrs", 0x07 << 13},
                 {"otp_pwm_ofs", 0x01 << 16},
                 {"otp_pwm_reg", 0x01 << 17},
                 {"otp_pwm_freq", 0x01 << 18},
                 {"otp_iholddelay", 0x03 << 19},
                 {"otp_ihold", 0x03 << 21},
                 {"otp_en_spreadcycle", 0x01 << 23},
             }},
            // # TMC222x (SEL_A == 0)
            {"IOIN@TMC222x",
             {
                 {"pdn_uart", 0x01 << 1},
                 {"spread", 0x01 << 2},
                 {"dir", 0x01 << 3},
                 {"enn", 0x01 << 4},
                 {"step", 0x01 << 5},
                 {"ms1", 0x01 << 6},
                 {"ms2", 0x01 << 7},
                 {"sel_a", 0x01 << 8},
                 {"version", 0xff << 24},
             }},

            // # TMC220x (SEL_A == 1)
            {"IOIN@TMC220x",
             {
                 {"enn", 0x01},
                 {"ms1", 0x01 << 2},
                 {"ms2", 0x01 << 3},
                 {"diag", 0x01 << 4},
                 {"pdn_uart", 0x01 << 5},
                 {"step", 0x01 << 6},
                 {"sel_a", 0x01 << 7},
                 {"dir", 0x01 << 8},
                 {"version", 0xff << 24},
             }},

            {"FACTORY_CONF",
             {
                 {"fclktrim", 0x1f},
                 {"ottrim", 0x03 << 8},
             }},

            {"IHOLD_IRUN",
             {
                 {"ihold", 0x1f},
                 {"irun", 0x1f << 8},
                 {"iholddelay", 0x0f << 16},
             }},

            {"TPOWERDOWN",
             {
                 {"tpowerdown", 0xff},
             }},

            {"TSTEP",
             {
                 {"tstep", 0xfffff},
             }},

            {"TPWMTHRS",
             {
                 {"tpwmthrs", 0xfffff},
             }},

            {"VACTUAL",
             {
                 {"vactual", 0xffffff},
             }},

            {"MSCNT",
             {
                 {"mscnt", 0x3ff},
             }},

            {"MSCURACT",
             {
                 {"cur_a", 0x1ff},
                 {"cur_b", 0x1ff << 16},
             }},

            {"CHOPCONF",
             {
                 {"toff", 0x0f},
                 {"hstrt", 0x07 << 4},
                 {"hend", 0x0f << 7},
                 {"tbl", 0x03 << 15},
                 {"vsense", 0x01 << 17},
                 {"mres", 0x0f << 24},
                 {"intpol", 0x01 << 28},
                 {"dedge", 0x01 << 29},
                 {"diss2g", 0x01 << 30},
                 {"diss2vs", 0x01 << 31},
             }},

            {"DRV_STATUS",
             {
                 {"otpw", 0x01},
                 {"ot", 0x01 << 1},
                 {"s2ga", 0x01 << 2},
                 {"s2gb", 0x01 << 3},
                 {"s2vsa", 0x01 << 4},
                 {"s2vsb", 0x01 << 5},
                 {"ola", 0x01 << 6},
                 {"olb", 0x01 << 7},
                 {"t120", 0x01 << 8},
                 {"t143", 0x01 << 9},
                 {"t150", 0x01 << 10},
                 {"t157", 0x01 << 11},
                 {"cs_actual", 0x1f << 16},
                 {"stealth", 0x01 << 30},
                 {"stst", 0x01 << 31},

             }},

            {"PWMCONF",
             {
                 {"pwm_ofs", 0xff},
                 {"pwm_grad", 0xff << 8},
                 {"pwm_freq", 0x03 << 16},
                 {"pwm_autoscale", 0x01 << 18},
                 {"pwm_autograd", 0x01 << 19},
                 {"freewheel", 0x03 << 20},
                 {"pwm_reg", 0xf << 24},
                 {"pwm_lim", 0xf << 28},
             }},

            {"PWM_SCALE",
             {
                 {"pwm_scale_sum", 0xff},
                 {"pwm_scale_auto", 0x1ff << 16},
             }},

            {"PWM_AUTO",
             {
                 {"pwm_ofs_auto", 0xff},
                 {"pwm_grad_auto", 0xff << 16},
             }},

            {"COOLCONF",
             {{"semin", 0x0F << 0},
              {"seup", 0x03 << 5},
              {"semax", 0x0F << 8},
              {"sedn", 0x03 << 13},
              {"seimin", 0x01 << 15}}},
            {"IOIN",
             {{"enn", 0x01 << 0},
              {"ms1", 0x01 << 2},
              {"ms2", 0x01 << 3},
              {"diag", 0x01 << 4},
              {"pdn_uart", 0x01 << 6},
              {"step", 0x01 << 7},
              {"spread_en", 0x01 << 8},
              {"dir", 0x01 << 9},
              {"version", 0xff << 24}}},
            {"SGTHRS", {{"sgthrs", 0xFF << 0}}},
            {"SG_RESULT", {{"sg_result", 0x3FF << 0}}},
            {"TCOOLTHRS", {{"tcoolthrs", 0xfffff}}}};

        std::map<std::string, std::function<std::string(int)>>
            FieldFormatters = {
                {"i_scale_analog", [](int v)
                 { return v ? "1(ExtVREF)" : ""; }},
                {"shaft", [](int v)
                 { return v ? "1(Reverse)" : ""; }},
                {"reset", [](int v)
                 { return v ? "1(Reset)" : ""; }},
                {"drv_err", [](int v)
                 { return v ? "1(ErrorShutdown!)" : ""; }},
                {"uv_cp", [](int v)
                 { return v ? "1(Undervoltage!)" : ""; }},
                {"version",
                 [](int v)
                 {
                     std::stringstream ss;
                     ss << std::hex << v;
                     return ss.str();
                 }},
                {"mres",
                 [](int v)
                 {
                     std::stringstream ss;
                     ss << v << "(" << (0x100 >> v) << "dusteps)";
                     return ss.str();
                 }},
                {"otpw", [](int v)
                 { return v ? "1(OvertempWarning!)" : ""; }},
                {"ot", [](int v)
                 { return v ? "1(OvertempError!)" : ""; }},
                {"s2ga", [](int v)
                 { return v ? "1(ShortToGND_A!)" : ""; }},
                {"s2gb", [](int v)
                 { return v ? "1(ShortToGND_B!)" : ""; }},
                {"ola", [](int v)
                 { return v ? "1(OpenLoad_A!)" : ""; }},
                {"olb", [](int v)
                 { return v ? "1(OpenLoad_B!)" : ""; }},
                {"cs_actual",
                 [](int v)
                 { return v ? std::to_string(v) : "0(Reset?)"; }},

                {"sel_a",
                 [](int v)
                 {
                     std::vector<std::string> options = {"TMC222x", "TMC220x"};
                     std::stringstream ss;
                     ss << v << "(" << options[v] << ")";
                     return ss.str();
                 }},
                {"s2vsa", [](int v)
                 { return v ? "1(ShortToSupply_A!)" : ""; }},
                {"s2vsb", [](int v)
                 { return v ? "1(ShortToSupply_B!)" : ""; }}};

        std::vector<std::string> signed_fields = {"cur_a", "cur_b", "pwm_scale_auto"};

        TMC2209::TMC2209(std::shared_ptr<ConfigWrapper> config)
        {
            SPDLOG_DEBUG("TMC2209 init {}!", config->get_name());
            fields = std::make_shared<FieldHelper>(Fields, signed_fields, FieldFormatters);
            mcu_tmc = std::make_shared<MCU_TMC_uart>(config, Registers, fields, 3,
                                                     TMC_FREQUENCY);

            fields->set_field("pdn_disable", true);
            fields->set_field("senddelay", 2);

            pin_helper = std::make_shared<TMCVirtualPinHelper>(config, mcu_tmc);
            pin_helper->init();
            auto current_helper = std::make_shared<TMCCurrentHelper>(config, mcu_tmc);
            cmdhelper = std::make_shared<TMCCommandHelper>(config, mcu_tmc, current_helper);
            cmdhelper->setup_register_dump(ReadRegisters);

            fields->set_field("mstep_reg_select", true);
            tmc::TMCStealthchopHelper(config, mcu_tmc);
            tmc::TMCVcoolthrsHelper(config, mcu_tmc);

            auto set_config_field = [this](std::shared_ptr<ConfigWrapper> config,
                                           const std::string &field, int value)
            {
                fields->set_config_field(config, field, value);
            };

            set_config_field(config, "multistep_filt", 1);

            set_config_field(config, "toff", 3);
            set_config_field(config, "hstrt", 5);
            set_config_field(config, "hend", 0);
            set_config_field(config, "tbl", 2);

            set_config_field(config, "semin", 0);
            set_config_field(config, "seup", 0);
            set_config_field(config, "semax", 0);
            set_config_field(config, "sedn", 0);
            set_config_field(config, "seimin", 0);

            set_config_field(config, "iholddelay", 8);

            set_config_field(config, "pwm_ofs", 36);
            set_config_field(config, "pwm_grad", 14);
            set_config_field(config, "pwm_freq", 1);
            set_config_field(config, "pwm_autoscale", 1);
            set_config_field(config, "pwm_autograd", 1);
            set_config_field(config, "pwm_reg", 8);
            set_config_field(config, "pwm_lim", 12);

            set_config_field(config, "tpowerdown", 20);

            set_config_field(config, "sgthrs", 0);
            SPDLOG_INFO("TMC2209 init success!!");
        }

        json TMC2209::get_status(double eventtime)
        {
            return cmdhelper->get_status(eventtime);
        }

        std::pair<int, int> TMC2209::get_phase_offset()
        {
            return cmdhelper->get_phase_offset();
        }

        std::shared_ptr<TMC2209> tmc2209_load_config_prefix(
            std::shared_ptr<ConfigWrapper> config)
        {
            return std::make_shared<TMC2209>(config);
        }

    }
}
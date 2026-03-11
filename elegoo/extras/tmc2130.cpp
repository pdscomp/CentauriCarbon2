/***************************************************************************** 
 * @Author       , Jack
 * @Date         , 2024-12-18 12,08,35
 * @LastEditors  : Ben
 * @LastEditTime : 2025-02-28 12:09:42
 * @Description  , 
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/
#include "tmc2130.h"
#include <cmath>
#include <algorithm>

uint32_t TMC_FREQUENCY = 13200000;
// double MAX_CURRENT = 2.000;

const std::map<std::string, uint8_t> Registers = {
    {"GCONF", 0x00}, {"GSTAT", 0x01}, {"IOIN", 0x04}, {"IHOLD_IRUN", 0x10},
    {"TPOWERDOWN", 0x11}, {"TSTEP", 0x12}, {"TPWMTHRS", 0x13}, {"TCOOLTHRS", 0x14},
    {"THIGH", 0x15}, {"XDIRECT", 0x2d}, {"MSLUT0", 0x60}, {"MSLUT1", 0x61},
    {"MSLUT2", 0x62}, {"MSLUT3", 0x63}, {"MSLUT4", 0x64}, {"MSLUT5", 0x65},
    {"MSLUT6", 0x66}, {"MSLUT7", 0x67}, {"MSLUTSEL", 0x68}, {"MSLUTSTART", 0x69},
    {"MSCNT", 0x6a}, {"MSCURACT", 0x6b}, {"CHOPCONF", 0x6c}, {"COOLCONF", 0x6d},
    {"DCCTRL", 0x6e}, {"DRV_STATUS", 0x6f}, {"PWMCONF", 0x70}, {"PWM_SCALE", 0x71},
    {"ENCM_CTRL", 0x72}, {"LOST_STEPS", 0x73}
};

const std::vector<std::string> ReadRegisters = {
    "GCONF", "GSTAT", "IOIN", "TSTEP", "XDIRECT", "MSCNT", "MSCURACT",
    "CHOPCONF", "DRV_STATUS", "PWM_SCALE", "LOST_STEPS"
};

std::map<std::string, std::map<std::string, uint32_t>> Fields= {
    {"GCONF", 
        {
            {"i_scale_analog", 1<<0}, {"internal_rsense", 1<<1}, {"en_pwm_mode", 1<<2},
            {"enc_commutation", 1<<3}, {"shaft", 1<<4}, {"diag0_error", 1<<5},
            {"diag0_otpw", 1<<6}, {"diag0_stall", 1<<7}, {"diag1_stall", 1<<8},
            {"diag1_index", 1<<9}, {"diag1_onstate", 1<<10}, {"diag1_steps_skipped", 1<<11},
            {"diag0_int_pushpull", 1<<12}, {"diag1_pushpull", 1<<13},
            {"small_hysteresis", 1<<14}, {"stop_enable", 1<<15}, {"direct_mode", 1<<16},
            {"test_mode", 1<<17}
        }
    },
    {"GSTAT", {{"reset", 1<<0}, {"drv_err", 1<<1}, {"uv_cp", 1<<2}} },
    {"IOIN" , {
        {"step", 1<<0}, {"dir", 1<<1}, {"dcen_cfg4", 1<<2}, {"dcin_cfg5", 1<<3},
        {"drv_enn_cfg6", 1<<4}, {"dco", 1<<5}, {"version", 0xff << 24}
    } },
    {"IHOLD_IRUN" , {
        {"ihold", 0x1f << 0}, {"irun", 0x1f << 8}, {"iholddelay", 0x0f << 16}
    } },
    {"TPOWERDOWN" , { {"tpowerdown", 0xff} } },
    {"TSTEP" , { {"tstep", 0xfffff} } },
    {"TPWMTHRS" , { {"tpwmthrs", 0xfffff} } },
    {"TCOOLTHRS" , { {"tcoolthrs", 0xfffff} }},
    {"THIGH" , { {"thigh", 0xfffff} }},
    {"MSLUT0" , { {"mslut0", 0xffffffff} } },
    {"MSLUT1" , { {"mslut1", 0xffffffff} } },
    {"MSLUT2" , { {"mslut2", 0xffffffff} } },
    {"MSLUT3" , { {"mslut3", 0xffffffff} } },
    {"MSLUT4" , { {"mslut4", 0xffffffff} } },
    {"MSLUT5" , { {"mslut5", 0xffffffff} } },
    {"MSLUT6" , { {"mslut6", 0xffffffff} } },
    {"MSLUT7" , { {"mslut7", 0xffffffff} } },
    {"MSLUTSEL" , {
        {"x3",                       0xFF << 24},
        {"x2",                       0xFF << 16},
        {"x1",                       0xFF << 8},
        {"w3",                       0x03 << 6},
        {"w2",                       0x03 << 4},
        {"w1",                       0x03 << 2},
        {"w0",                       0x03 << 0},
    } },
    {"MSLUTSTART" , {
        {"start_sin",                0xFF << 0},
        {"start_sin90",              0xFF << 16},
    } },
    {"MSCNT" , { {"mscnt", 0x3ff} }},
    {"MSCURACT" ,{ {"cur_a", 0x1ff}, {"cur_b", 0x1ff << 16} } },
    {"CHOPCONF" , {
        {"toff", 0x0f}, {"hstrt", 0x07 << 4}, {"hend", 0x0f << 7}, {"fd3", 1<<11},
        {"disfdcc", 1<<12}, {"rndtf", 1<<13}, {"chm", 1<<14}, {"tbl", 0x03 << 15},
        {"vsense", 1<<17}, {"vhighfs", 1<<18}, {"vhighchm", 1<<19}, {"sync", 0x0f << 20},
        {"mres", 0x0f << 24}, {"intpol", 1<<28}, {"dedge", 1<<29}, {"diss2g", 1<<30}
    } },
    {"COOLCONF", {
        {"semin", 0x0f}, {"seup", 0x03 << 5}, {"semax", 0x0f << 8}, {"sedn", 0x03 << 13},
        {"seimin", 1<<15}, {"sgt", 0x7f << 16}, {"sfilt", 1<<24}
    }},
    {"DRV_STATUS", {
        {"sg_result", 0x3ff}, {"fsactive", 1<<15}, {"cs_actual", 0x1f << 16},
        {"stallguard", 1<<24}, {"ot", 1<<25}, {"otpw", 1<<26}, {"s2ga", 1<<27},
        {"s2gb", 1<<28}, {"ola", 1<<29}, {"olb", 1<<30}, {"stst", 1<<31}
    }},
    {"PWMCONF", {
        {"pwm_ampl", 0xff}, {"pwm_grad", 0xff << 8}, {"pwm_freq", 0x03 << 16},
        {"pwm_autoscale", 1<<18}, {"pwm_symmetric", 1<<19}, {"freewheel", 0x03 << 20}
    } },
    {"PWM_SCALE", { {"pwm_scale", 0xff} }},
    {"LOST_STEPS", { {"lost_steps", 0xfffff} }},
};

const std::vector<std::string> SignedFields = {"cur_a", "cur_b", "sgt"};

// 使用 std::stringstream 替代 std::format
std::string format_hex(int v) {
    std::stringstream ss;
    ss << "0x" << std::hex << v;
    return ss.str();
}

std::string format_mres(int v) {
    std::stringstream ss;
    ss << v << "(" << (0x100 >> v) << "usteps)";
    return ss.str();
}

// 定义所有 lambda 函数为常规函数
std::string format_i_scale_analog(int v) { return v ? "1(ExtVREF)" : ""; }
std::string format_shaft(int v) { return v ? "1(Reverse)" : ""; }
std::string format_reset(int v) { return v ? "1(Reset)" : ""; }
std::string format_drv_err(int v) { return v ? "1(ErrorShutdown!)" : ""; }
std::string format_uv_cp(int v) { return v ? "1(Undervoltage!)" : ""; }
std::string format_version(int v) { return format_hex(v); }
std::string format_mres_func(int v) { return format_mres(v); }
std::string format_otpw(int v) { return v ? "1(OvertempWarning!)" : ""; }
std::string format_ot(int v) { return v ? "1(OvertempError!)" : ""; }
std::string format_s2ga(int v) { return v ? "1(ShortToGND_A!)" : ""; }
std::string format_s2gb(int v) { return v ? "1(ShortToGND_B!)" : ""; }
std::string format_ola(int v) { return v ? "1(OpenLoad_A!)" : ""; }
std::string format_olb(int v) { return v ? "1(OpenLoad_B!)" : ""; }
std::string format_cs_actual(int v) { return v ? std::to_string(v) : "0(Reset?)"; }

// 使用命名函数代替 lambda
std::map<std::string, std::function<std::string(int)>> FieldFormatters = {
    {"i_scale_analog", format_i_scale_analog},
    {"shaft", format_shaft},
    {"reset", format_reset},
    {"drv_err", format_drv_err},
    {"uv_cp", format_uv_cp},
    {"version", format_version},
    {"mres", format_mres_func},
    {"otpw", format_otpw},
    {"ot", format_ot},
    {"s2ga", format_s2ga},
    {"s2gb", format_s2gb},
    {"ola", format_ola},
    {"olb", format_olb},
    {"cs_actual", format_cs_actual}
};

namespace elegoo {
namespace extras {

// TMCCurrentHelper::TMCCurrentHelper(std::shared_ptr<ConfigWrapper> config,
//                    std::shared_ptr<MCU_TMC_Base> mcu_tmc) {
// SPDLOG_INFO("TMCCurrentHelper init!");
//     printer = config->get_printer();
//     std::istringstream iss(config->get_name());
//     std::vector<std::string> name_parts;
//     std::string part;
//     while (iss >> part) {
//         name_parts.push_back(part);
//     }

//     std::ostringstream oss;
//     for (size_t i = 1; i < name_parts.size(); ++i) {
//     if (i > 1) {
//         oss << " ";
//     }
//     oss << name_parts[i];
//     }
//     name = oss.str();

//     this->mcu_tmc = mcu_tmc;
//     this->fields = mcu_tmc->get_fields();

    // double run_current = config->getdouble("run_current",  DOUBLE_INVALID,
    //                             DOUBLE_NONE, MAX_CURRENT, 0.0);
    // double hold_current = config->getdouble("run_current",  MAX_CURRENT,
    //                             DOUBLE_NONE, MAX_CURRENT, 0.0);

//     this->req_hold_current = hold_current;
//     this->sense_resistor = config->getdouble("sense_resistor", 0.110, DOUBLE_NONE, DOUBLE_NONE, 0.0);
//     int vsense, irun, ihold;
//     // vsense, irun, ihold = this->_calc_current(run_current, hold_current)

//     this->fields->set_field("vsense", vsense);
//     this->fields->set_field("ihold", ihold);
//     this->fields->set_field("irun", irun);
// SPDLOG_INFO("TMCCurrentHelper init success!!");
// }

// int TMCCurrentHelper::_calc_current_bits(double current, int vsense) {
//     double sense_resistor = this->sense_resistor + 0.020;
//     double vref = 0.32;
//     if(vsense) {
//         vref = 0.18;
//     }

//     int cs = int(32. * sense_resistor * current * std::sqrt(2.0) / vref + .5) - 1;
//     return std::max(0, std::min(31, cs));
// }

// double TMCCurrentHelper::_calc_current_from_bits(int cs, int vsence) {
//     double sense_resistor = this->sense_resistor + 0.020;
//     double vref = 0.32;
//     if(vsence) {
//         vref = 0.18;
//     }   

//     return (cs + 1) * vref / (32. * sense_resistor * std::sqrt(2.0));
// }

// std::vector<int> TMCCurrentHelper::_calc_current(double run_current, double hold_current) {
//     int vsense = 1;
//     auto irun = this->_calc_current_bits(run_current, 1);

//     if (irun == 31) {
//         auto cur = this->_calc_current_from_bits(irun, 1);
//         if( cur < run_current) {
//             auto irun2 = this->_calc_current_bits(run_current, 0);
//             auto cur2 = this->_calc_current_from_bits(irun2, 0);
//             if (abs(run_current - cur2) < abs(run_current - cur)) {
//                 vsense = 0;
//                 irun = irun2;
//             }
//         }
//     }
//     auto ihold = this->_calc_current_bits(std::min(hold_current, run_current), vsense);
//     return {vsense, irun, ihold};
// }

// std::vector<double> TMCCurrentHelper::get_current() {
//     auto irun = this->fields->get_field("irun");
//     auto ihold = this->fields->get_field("ihold");
//     auto vsense = this->fields->get_field("vsense");
//     auto run_current = this->_calc_current_from_bits(irun, vsense);
//     auto hold_current = this->_calc_current_from_bits(ihold, vsense);
//     return {run_current, hold_current, this->req_hold_current, MAX_CURRENT};


// }

// void TMCCurrentHelper::set_current(double run_current, double hold_current, double print_time) {
//     this->req_hold_current = hold_current;
//     int vsense, irun, ihold;
//     auto tmp = this->_calc_current(run_current, hold_current);
//     vsense = tmp[0];
//     irun = tmp[1];
//     ihold = tmp[2];  
//     if (vsense != this->fields->get_field("vsense")) {
//         int val = this->fields->set_field("vsense", vsense);
//         this->mcu_tmc->set_register("CHOPCONF", val, print_time);
//         this->fields->set_field("ihold", ihold);
//         val = this->fields->set_field("irun", irun);
//         this->mcu_tmc->set_register("IHOLD_IRUN", val, print_time);
//     }
// }

MCU_TMC_SPI_chain::MCU_TMC_SPI_chain(std::shared_ptr<ConfigWrapper> config, int chain_len)
{
    printer = config->get_printer();
    this->chain_len = chain_len;
    spi = nullptr;
    mutex =  printer->get_reactor()->mutex();
    // if (chain_len > 1) {
    //     spi = bus::MCU_SPI::from_config(/* parameters */, "tmc_spi_cs");
    // } else {
    //     spi = bus::MCU_SPI::from_config(/* parameters */);
    // }
}

std::vector<uint8_t> MCU_TMC_SPI_chain::_build_cmd(const std::vector<uint8_t>& data, int chain_pos) {
    std::vector<uint8_t> cmd;
    cmd.resize((chain_len - chain_pos) * 5);
    cmd.insert(cmd.end(), data.begin(), data.end());
    cmd.resize(cmd.size() + (chain_pos - 1) * 5, 0x00);
    return cmd;
}

uint32_t MCU_TMC_SPI_chain::reg_read(uint8_t reg, int chain_pos) {
    auto cmd = _build_cmd({reg, 0x00, 0x00, 0x00, 0x00}, chain_pos);
    spi->spi_send(cmd);
    if (printer->get_start_args().at("debugoutput") != "") {
        return 0;
    }
    auto params = spi->spi_transfer(cmd);
    auto pr = std::vector<uint8_t>(params.at("response").begin(), params.at("response").end());
    pr = std::vector<uint8_t>(pr.begin() + (chain_len - chain_pos) * 5,
                              pr.begin() + (chain_len - chain_pos + 1) * 5);
    return (static_cast<uint32_t>(pr[1]) << 24) |
           (static_cast<uint32_t>(pr[2]) << 16) |
           (static_cast<uint32_t>(pr[3]) << 8) | pr[4];
}

uint32_t MCU_TMC_SPI_chain::reg_write(uint8_t reg, uint32_t val, int chain_pos, double print_time) {
    uint32_t minclock = 0;
    if (!std::isnan(print_time)) {
        minclock = spi->get_mcu()->print_time_to_clock(print_time);
    }
    std::vector<uint8_t> data = {uint8_t((reg | 0x80) & 0xff), uint8_t((val >> 24) & 0xff), uint8_t((val >> 16) & 0xff),
                                 uint8_t((val >> 8) & 0xff), uint8_t(val & 0xff)};
    if (printer->get_start_args().at("debugoutput") != "") {
        spi->spi_send(_build_cmd(data, chain_pos), minclock);
        return val;
    }
    auto write_cmd = _build_cmd(data, chain_pos);
    auto dummy_read = _build_cmd({0x00, 0x00, 0x00, 0x00, 0x00}, chain_pos);
    auto params = spi->spi_transfer_with_preface(write_cmd, dummy_read, minclock);
    auto pr = std::vector<uint8_t>(params.at("response").begin(), params.at("response").end());
    pr = std::vector<uint8_t>(pr.begin() + (chain_len - chain_pos) * 5,
                              pr.begin() + (chain_len - chain_pos + 1) * 5);
    return (static_cast<uint32_t>(pr[1]) << 24) |
           (static_cast<uint32_t>(pr[2]) << 16) |
           (static_cast<uint32_t>(pr[3]) << 8) | pr[4];
}

std::pair<std::shared_ptr<MCU_TMC_SPI_chain>, int> lookup_tmc_spi_chain(std::shared_ptr<ConfigWrapper> config) {
    int chain_len = config->getint("chain_length", INT_NONE, 2);
    
    if (chain_len ==  INT_NONE) {
        // Simple, non daisy chained SPI connection
        auto tmc_spi = std::make_shared<MCU_TMC_SPI_chain>(config, 1);
        return std::make_pair(tmc_spi, 1);
    }

    // Shared SPI bus - lookup existing MCU_TMC_SPI_chain
    auto ppins_ptr = config->get_printer()->lookup_object("pins");
    auto ppins = any_cast<std::shared_ptr<PrinterPins>>(ppins_ptr);
    auto cs_pin_params = ppins->lookup_pin(config->get("cs_pin"), false, false, "tmc_spi_cs");

    // auto tmc_spi = cs_pin_params->get("class");

    std::string cs_pin = config->get_printer()->get_start_args().at("cs_pin");

    int chain_pos = config->getint("chain_position", INT_NONE, 1, chain_len);
    auto tmc_spi = std::make_shared<MCU_TMC_SPI_chain>(config, chain_len);

    // if chain_pos in tmc_spi.taken_chain_positions:
    //     raise config.error("TMC SPI chain can not have duplicate position")

    return std::make_pair(tmc_spi, chain_pos);
}



MCU_TMC_SPI::MCU_TMC_SPI(std::shared_ptr<ConfigWrapper> config, const NameToRegMap& name_to_reg,
                         std::shared_ptr<FieldHelper> fields, uint32_t freq)\
{
    printer = config->get_printer();
    std::istringstream iss(config->get_name());
    std::vector<std::string> name_parts;
    std::string part;
    while (iss >> part) {
        name_parts.push_back(part);
    }

    std::ostringstream oss;
    for (size_t i = 1; i < name_parts.size(); ++i) {
    if (i > 1) {
        oss << " ";
    }
    oss << name_parts[i];
    }
    name = oss.str();
      
    this->tmc_spi = lookup_tmc_spi_chain(config).first;
    this->chain_pos = lookup_tmc_spi_chain(config).second;
    this->mutex = tmc_spi->get_mutex();
    this->name_to_reg = name_to_reg;
    this->fields = fields;
    this->tmc_frequency = freq; 
      
}

std::shared_ptr<FieldHelper> MCU_TMC_SPI::get_fields() const {
    return fields;
}

uint32_t MCU_TMC_SPI::get_register(const std::string& reg_name) {
    auto it = name_to_reg.find(reg_name);
    if (it == name_to_reg.end()) {
        SPDLOG_INFO("Register not found: " + reg_name);
        return 0;
    }
    uint8_t reg = it->second;
    mutex->lock();
    auto retval = tmc_spi->reg_read(reg, chain_pos);
    mutex->unlock();
    return retval;
}

void MCU_TMC_SPI::set_register(const std::string& reg_name, int64_t val, double print_time) {
    auto it = name_to_reg.find(reg_name);
    if (it == name_to_reg.end()) {
        SPDLOG_INFO("Register not found: " + reg_name);
        return;
    }
    uint8_t reg = it->second;
    mutex->lock();
    for (int retry = 0; retry < 5; ++retry) {
        int64_t v = tmc_spi->reg_write(reg, val, chain_pos, print_time);
        if (v == val) {
            mutex->unlock();
            return;
        }
    }
    mutex->unlock();
    throw std::runtime_error("Unable to write tmc spi '" + name + "' register " + reg_name);
}

uint32_t MCU_TMC_SPI::get_tmc_frequency() const {
    return tmc_frequency;
}

TMC2130::TMC2130(std::shared_ptr<ConfigWrapper> config) {
    this->fields = std::make_shared<FieldHelper> (Fields, SignedFields, FieldFormatters);
    this->mcu_tmc = std::make_shared<MCU_TMC_SPI> (config, Registers, fields, TMC_FREQUENCY);

    // TMCVirtualPinHelper(config, mcu_tmc);
    auto current_helper = std::make_shared<TMCCurrentHelper> (config, mcu_tmc);
    auto cmdhelper = std::make_shared<TMCCommandHelper>(config, mcu_tmc, current_helper);
    cmdhelper->setup_register_dump(ReadRegisters);
    // this->get_phase_offset = cmdhelper->get_phase_offset;
    // this->get_status = cmdhelper->get_status;   

    // TMCWaveTableHelper(config, this->mcu_tmc);
    // TMCStealthchopHelper(config, this->mcu_tmc);
    // TMCVcoolthrsHelper(config, this->mcu_tmc);
    // TMCVhighHelper(config, this->mcu_tmc);

    auto set_config_field = [this](std::shared_ptr<ConfigWrapper> config,
                                    const std::string& field, int value) {
        fields->set_config_field(config, field, value);
    };
    set_config_field(config, "toff", 4);
    set_config_field(config, "hstrt", 0);
    set_config_field(config, "hend", 7);
    set_config_field(config, "tbl", 1);
    set_config_field(config, "vhighfs", 0);
    set_config_field(config, "vhighchm", 0);

    set_config_field(config, "semin", 0);
    set_config_field(config, "seup", 0);
    set_config_field(config, "semax", 0);
    set_config_field(config, "sedn", 0);
    set_config_field(config, "seimin", 0);
    set_config_field(config, "sgt", 0);
    set_config_field(config, "sfilt", 0);

    set_config_field(config, "iholddelay", 8);

    set_config_field(config, "pwm_ampl", 128);
    set_config_field(config, "pwm_grad", 4);
    set_config_field(config, "pwm_freq", 1);
    set_config_field(config, "pwm_autoscale", 1);

    set_config_field(config, "tpowerdown", 0);
}

std::shared_ptr<TMC2130> tmc2130_load_config_prefix(
    std::shared_ptr<ConfigWrapper> config) {
    return std::make_shared<TMC2130>(config);
    
}

}
}

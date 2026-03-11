/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2024-12-18 12:08:43
 * @LastEditors  : Ben
 * @LastEditTime : 2025-02-28 12:09:23
 * @Description  : 
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/
#pragma once

#include <map>
#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <functional>
#include "configfile.h"
#include "force_move.h"
#include "gcode_move.h"
#include "heaters.h"
#include "homing.h"
#include "json.h"
#include "mcu.h"
#include "printer.h"
#include "tmc_uart.h"
#include "tmc.h"
#include "bus.h"
#include "mcu_tmc_base.h"

namespace elegoo {
namespace extras {

class MCU_TMC_uart;
class FieldHelper;
class TMCCurrentHelper;
class TMCCommandHelper;

// class TMCCurrentHelper {
// public:
//     TMCCurrentHelper(std::shared_ptr<ConfigWrapper> config,
//                    std::shared_ptr<MCU_TMC_Base> mcu_tmc);
//     int _calc_current_bits(double current, int vsense);
//     double _calc_current_from_bits(int cs, int vsence);
//     std::vector<int> _calc_current(double run_current, double hold_current);
//     std::vector<double> get_current();
//     void set_current(double run_current, double hold_current, double print_time);

// private:
//     std::shared_ptr<Printer> printer;
//     std::string name;
//     std::shared_ptr<MCU_TMC_Base> mcu_tmc; 
//     std::shared_ptr<FieldHelper> fields;

//     double req_hold_current;
//     double sense_resistor;

// };

class MCU_TMC_SPI_chain {
public:
    MCU_TMC_SPI_chain(std::shared_ptr<ConfigWrapper> config, int chain_len = 1);

    std::vector<uint8_t> _build_cmd(const std::vector<uint8_t>& data, int chain_pos);
    uint32_t reg_read(uint8_t reg, int chain_pos);
    uint32_t reg_write(uint8_t reg, uint32_t val, int chain_pos, double print_time = DOUBLE_NONE);

    std::shared_ptr<ReactorMutex> get_mutex() {return mutex;};
private:
    std::shared_ptr<Printer> printer;
    int chain_len;
    std::shared_ptr<MCU_SPI> spi;
    std::vector<int> taken_chain_positions;
    std::shared_ptr<ReactorMutex> mutex;
};


class MCU_TMC_SPI : public MCU_TMC_Base{
public:
    using NameToRegMap = std::map<std::string, uint8_t>;
    using FieldsType = std::unordered_map<std::string, std::string>; // 根据实际需求调整

    MCU_TMC_SPI(std::shared_ptr<ConfigWrapper> config, const NameToRegMap& name_to_reg,
                std::shared_ptr<FieldHelper> fields, uint32_t freq);

    std::shared_ptr<FieldHelper> get_fields() const override;
    uint32_t get_register(const std::string& reg_name) override;
    void set_register(const std::string& reg_name, int64_t val, double print_time = DOUBLE_NONE) override;
    uint32_t get_tmc_frequency() const override;

private:
    std::shared_ptr<Printer> printer;
    std::string name;
    std::shared_ptr<MCU_TMC_SPI_chain> tmc_spi;
    int chain_pos;
    std::shared_ptr<ReactorMutex> mutex;
    NameToRegMap name_to_reg;
    std::shared_ptr<FieldHelper> fields;
    uint32_t tmc_frequency;
};

class TMC2130 {
public:
    TMC2130(std::shared_ptr<ConfigWrapper> config);

private:
    std::shared_ptr<FieldHelper> fields;
    std::shared_ptr<MCU_TMC_SPI> mcu_tmc;

    std::pair<int, int> get_phase_offset;
    std::map<std::string, int> get_status;

};

std::shared_ptr<TMC2130> tmc2130_load_config_prefix(
    std::shared_ptr<ConfigWrapper> config);

}
}

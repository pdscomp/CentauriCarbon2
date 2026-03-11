/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2024-12-20 20:26:45
 * @LastEditors  : Jack
 * @LastEditTime : 2024-12-21 12:11:05
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


namespace elegoo {
namespace extras {

std::string resolve_bus_name(std::shared_ptr<MCU> mcu, std::string param, std::string bus="");

class MCU_SPI {
public:
    MCU_SPI(std::shared_ptr<MCU> mcu, std::string bus, const std::string& pin, int mode, int speed,
            const std::tuple<std::string, std::string, std::string> sw_pins = {}, bool cs_active_high = false);

    void setup_shutdown_msg(const std::vector<uint8_t>& shutdown_seq);
    int get_oid() const;
    std::shared_ptr<MCU> get_mcu() const;
    std::shared_ptr<command_queue> get_command_queue() const;
    void build_config();
    void spi_send(const std::vector<uint8_t>& data, uint32_t minclock = 0, uint32_t reqclock = 0);
    json spi_transfer(const std::vector<uint8_t>& data, uint32_t minclock = 0, uint32_t reqclock = 0);
    json spi_transfer_with_preface(const std::vector<uint8_t>& preface_data,
                                                   const std::vector<uint8_t>& data,
                                                   uint32_t minclock = 0, uint32_t reqclock = 0);

private:
    std::shared_ptr<MCU> mcu;
    std::string bus;
    int oid;
    std::string config_fmt;
    std::shared_ptr<command_queue> cmd_queue;
    std::shared_ptr<CommandWrapper> spi_send_cmd;
    std::shared_ptr<CommandQueryWrapper>  spi_transfer_cmd;
};

std::shared_ptr<MCU_SPI> MCU_SPI_from_config(
    std::shared_ptr<ConfigWrapper> config, int mode,
    const std::string& pin_option = "cs_pin",
    int default_speed = 100000, const std::string* share_type = nullptr,
    bool cs_active_high = false);

}
}
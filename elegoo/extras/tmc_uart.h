/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2024-11-23 12:23:00
 * @LastEditors  : Ben
 * @LastEditTime : 2025-02-28 12:01:58
 * @Description  : Helper code for communicating with TMC stepper drivers via UART
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <vector>
#include "configfile.h"
#include "gcode_move.h"
#include "mcu.h"
#include "printer.h"
#include "tmc.h"
#include "mcu_tmc_base.h"

namespace elegoo
{
    namespace extras
    {

        class MCU_TMC_uart;
        class FieldHelper;
        class MCU_analog_mux
        {
        public:
            MCU_analog_mux(std::shared_ptr<MCU> mcu,
                           std::shared_ptr<command_queue> cmd_queue,
                           std::vector<std::string> select_pins_desc);
            ~MCU_analog_mux();
            void build_config();
            std::vector<bool> get_instance_id(std::vector<std::string> select_pins_desc);
            void activate(std::vector<bool> instance_id);

        private:
            std::shared_ptr<MCU> mcu;
            std::shared_ptr<command_queue> cmd_queue;

            std::vector<uint64_t> oids;
            std::vector<std::string> pins;
            std::vector<bool> pin_values;
            std::shared_ptr<CommandWrapper> update_pin_cmd;
        };

        // ######################################################################
        // # TMC uart communication
        // ######################################################################

        // # Share mutexes so only one active tmc_uart command on a single mcu at
        // # a time. This helps limit cpu usage on slower micro-controllers.

        class PrinterTMCUartMutexes
        {
        public:
            PrinterTMCUartMutexes();
            ~PrinterTMCUartMutexes();
            std::map<std::shared_ptr<MCU>, std::shared_ptr<ReactorMutex>> mcu_to_mutex;

        private:
        };

        std::shared_ptr<ReactorMutex> lookup_tmc_uart_mutex(std::shared_ptr<MCU> mcu);

        class MCU_TMC_uart_bitbang
        {
        public:
            MCU_TMC_uart_bitbang(
                std::shared_ptr<PinParams> rx_pin_params,
                std::shared_ptr<PinParams> tx_pin_params,
                std::vector<std::string> select_pins_desc);
            void build_config();
            std::vector<bool> register_instance(
                std::shared_ptr<PinParams> rx_pin_params,
                std::shared_ptr<PinParams> tx_pin_params,
                std::vector<std::string> select_pins_desc, int addr);
            uint8_t _calc_crc8(const std::vector<uint8_t> &data);
            std::vector<uint8_t> _add_serial_bits(const std::vector<uint8_t> &data);
            std::vector<uint8_t> _encode_read(uint8_t sync, uint8_t addr, uint8_t reg);
            std::vector<uint8_t> _encode_write(uint8_t sync, uint8_t addr, uint8_t reg,
                                               uint32_t val);
            int _decode_read(uint8_t reg, const std::vector<uint8_t> &data, uint32_t &val);
            int reg_read(std::vector<bool> instance_id, uint8_t addr, uint8_t reg, uint32_t &val);
            int reg_write(std::vector<bool> instance_id, uint8_t addr, uint8_t reg,
                          int64_t val, double print_time = DOUBLE_NONE);
            std::shared_ptr<ReactorMutex> get_mutex() { return mutex; }

        private:
            std::shared_ptr<MCU> mcu;
            int pullup;
            int oid;
            std::shared_ptr<std::string> rx_pin;
            std::shared_ptr<std::string> tx_pin;
            std::shared_ptr<command_queue> cmd_queue;
            std::shared_ptr<MCU_analog_mux> analog_mux;
            std::map<std::pair<std::vector<bool>, int>, bool> instances;
            std::shared_ptr<CommandQueryWrapper> tmcuart_send_cmd;
            std::shared_ptr<ReactorMutex> mutex;
        };

        std::shared_ptr<MCU_TMC_uart_bitbang> lookup_tmc_uart_bitbang(
            std::shared_ptr<ConfigWrapper> config, int max_addr,
            std::vector<bool> &instance_id, int &addr);

        class MCU_TMC_uart : public MCU_TMC_Base
        {
        public:
            MCU_TMC_uart(std::shared_ptr<ConfigWrapper> config,
                         std::map<std::string, uint8_t> name_to_reg,
                         std::shared_ptr<FieldHelper> fields, int max_addr,
                         int tmc_frequency);
            std::shared_ptr<FieldHelper> get_fields() const override;
            int _do_get_register(const std::string &reg_name, uint32_t &val);
            uint32_t get_register(const std::string &reg_name) override;
            void set_register(const std::string &reg_name, int64_t val,
                              double print_time = DOUBLE_NONE) override;
            uint32_t get_tmc_frequency() const override;

        private:
            std::shared_ptr<Printer> printer;
            std::string name;
            std::map<std::string, uint8_t> name_to_reg;
            std::shared_ptr<FieldHelper> fields;
            uint32_t ifcnt;
            int addr;
            std::vector<bool> instance_id;
            std::shared_ptr<ReactorMutex> mutex;
            std::mutex mtx;
            std::shared_ptr<MCU_TMC_uart_bitbang> mcu_uart;
            uint32_t tmc_frequency;
        };

    }
}

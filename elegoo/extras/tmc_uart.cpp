/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2024-11-23 12:23:17
 * @LastEditors  : coconut
 * @LastEditTime : 2025-03-24 16:31:21
 * @Description  : Helper code for communicating with TMC stepper drivers via UART
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "tmc_uart.h"
namespace elegoo
{
    namespace extras
    {
        MCU_analog_mux::MCU_analog_mux(std::shared_ptr<MCU> mcu,
                                       std::shared_ptr<command_queue> cmd_queue,
                                       std::vector<std::string> select_pins_desc)
        {
            this->mcu = mcu;
            this->cmd_queue = cmd_queue;

            auto ppins = any_cast<std::shared_ptr<PrinterPins>>(mcu->get_printer()->lookup_object("pins"));
            std::vector<std::shared_ptr<PinParams>> select_pin_params;
            for (const auto &spd : select_pins_desc)
            {
                select_pin_params.push_back(ppins->lookup_pin(spd, true));
            }

            for (const auto &pp : select_pin_params)
            {
                std::shared_ptr<std::string> pp_ptr =
                    std::static_pointer_cast<std::string>(pp->pin);
                pins.push_back(pp_ptr->data());
                oids.push_back(mcu->create_oid());
                pin_values.push_back(false);
            }

            size_t min_size = std::min({oids.size(), pins.size()});
            for (int i = 0; i < min_size; i++)
            {
                mcu->add_config_cmd("config_digital_out oid=" + std::to_string(oids[i]) +
                                    " pins=" + pins[i] +
                                    " value=0 default_value=0 max_duration=0");
            }

            update_pin_cmd = nullptr;
            mcu->register_config_callback(std::bind(&MCU_analog_mux::build_config, this));
        }

        MCU_analog_mux::~MCU_analog_mux() {}

        void MCU_analog_mux::build_config()
        {
            this->update_pin_cmd = this->mcu->lookup_command(
                "update_digital_out oid=%c value=%c", cmd_queue);
        }

        std::vector<bool> MCU_analog_mux::get_instance_id(
            std::vector<std::string> select_pins_desc)
        {
            auto ppins = any_cast<std::shared_ptr<PrinterPins>>(mcu->get_printer()->lookup_object("pins"));

            std::vector<std::shared_ptr<PinParams>> select_pin_params;
            for (const auto &spd : select_pins_desc)
            {
                select_pin_params.push_back(ppins->parse_pin(spd, true));
            }

            for (const auto &pin_params : select_pin_params)
            {
                if (pin_params->chip != this->mcu)
                {
                    throw elegoo::common::CommandError("TMC mux pins must be on the same mcu");
                }
            }
            std::vector<std::string> pins;
            for (const auto &pp : select_pin_params)
            {
                std::shared_ptr<std::string> pp_ptr =
                    std::static_pointer_cast<std::string>(pp->pin);
                pins.push_back(pp_ptr->data());
            }
            if (pins != this->pins)
            {
                throw elegoo::common::CommandError("All TMC mux instances must use identical pins");
            }
            std::vector<bool> instance_id;
            for (const auto &pp : select_pin_params)
            {
                instance_id.push_back(pp->invert);
            }
            return instance_id;
        }

        void MCU_analog_mux::activate(std::vector<bool> instance_id)
        {
            size_t min_size =
                std::min({oids.size(), pin_values.size(), instance_id.size()});
            for (int i = 0; i < min_size; i++)
            {
                if (pin_values[i] != instance_id[i])
                {
                    SPDLOG_DEBUG("update_pin_cmd on_restart=true");
                    update_pin_cmd->send({std::to_string(oids[i]), std::to_string(instance_id[i])});
                }
            }
            pin_values = instance_id;
        }

        PrinterTMCUartMutexes::PrinterTMCUartMutexes() { mcu_to_mutex.clear(); }

        PrinterTMCUartMutexes::~PrinterTMCUartMutexes() {}

        std::shared_ptr<ReactorMutex> lookup_tmc_uart_mutex(std::shared_ptr<MCU> mcu)
        {
            std::shared_ptr<Printer> printer = mcu->get_printer();

            std::shared_ptr<PrinterTMCUartMutexes> pmutexes_def = nullptr;
            std::shared_ptr<PrinterTMCUartMutexes> pmutexes = any_cast<std::shared_ptr<PrinterTMCUartMutexes>>(printer->lookup_object("tmc_uart", pmutexes_def));
            if (pmutexes_def == pmutexes)
            {
                SPDLOG_DEBUG("__func__:{}", __func__);
                pmutexes = std::make_shared<PrinterTMCUartMutexes>();
                printer->add_object("tmc_uart", pmutexes);
            }

            SPDLOG_DEBUG("__func__:{}", __func__);
            std::shared_ptr<ReactorMutex> mutex;
            auto it = pmutexes->mcu_to_mutex.find(mcu);
            SPDLOG_DEBUG("__func__:{}", __func__);
            if (it == pmutexes->mcu_to_mutex.end())
            {
                mutex = printer->get_reactor()->mutex();
                pmutexes->mcu_to_mutex[mcu] = mutex;
            }
            else
            {
                mutex = it->second;
            }
            SPDLOG_DEBUG("__func__:{}", __func__);

            return mutex;
        }

        const int TMC_BAUD_RATE = 40000;
        const int TMC_BAUD_RATE_AVR = 9000;

        MCU_TMC_uart_bitbang::MCU_TMC_uart_bitbang(
            std::shared_ptr<PinParams> rx_pin_params,
            std::shared_ptr<PinParams> tx_pin_params,
            std::vector<std::string> select_pins_desc)
        {
            mcu = std::static_pointer_cast<MCU>(rx_pin_params->chip);
            mutex = lookup_tmc_uart_mutex(mcu);
            pullup = rx_pin_params->pullup;
            rx_pin = std::static_pointer_cast<std::string>(rx_pin_params->pin);
            tx_pin = std::static_pointer_cast<std::string>(tx_pin_params->pin);
            oid = mcu->create_oid();
            cmd_queue = mcu->alloc_command_queue();
            analog_mux = nullptr;
            if (!select_pins_desc.empty())
            {
                analog_mux =
                    std::make_shared<MCU_analog_mux>(mcu, cmd_queue, select_pins_desc);
            }
            instances.clear();
            tmcuart_send_cmd = nullptr;
            mcu->register_config_callback([this]()
                                          { build_config(); });
        }

        void MCU_TMC_uart_bitbang::build_config()
        {
            int baud = TMC_BAUD_RATE;
            std::string mcu_type = mcu->get_constants().at("MCU");
            if (mcu_type.find("atmega") == 0 || mcu_type.find("at90usb") == 0)
            {
                baud = TMC_BAUD_RATE_AVR;
            }
            int bit_ticks = mcu->seconds_to_clock(1.0 / baud);
            mcu->add_config_cmd("config_tmcuart oid=" + std::to_string(oid) + " rx_pin=" +
                                rx_pin->data() + " pull_up=" + std::to_string(pullup) +
                                " tx_pin=" + tx_pin->data() + " bit_time=" +
                                std::to_string(bit_ticks));
            tmcuart_send_cmd = mcu->lookup_query_command(
                "tmcuart_send oid=%c write=%*s read=%c",
                "tmcuart_response oid=%c read=%*s", oid, cmd_queue, true);
        }

        std::vector<bool> MCU_TMC_uart_bitbang::register_instance(
            std::shared_ptr<PinParams> rx_pin_params,
            std::shared_ptr<PinParams> tx_pin_params,
            std::vector<std::string> select_pins_desc, int addr)
        {
            if (rx_pin_params->pin != rx_pin || tx_pin_params->pin != tx_pin ||
                (select_pins_desc.empty() != (analog_mux == nullptr)))
            {
                throw elegoo::common::CommandError("Shared TMC uarts must use the same pins");
            }
            std::vector<bool> instance_id;
            if (analog_mux != nullptr)
            {
                instance_id = analog_mux->get_instance_id(select_pins_desc);
            }
            if (instances.find({instance_id, addr}) != instances.end())
            {
                SPDLOG_ERROR("Shared TMC uarts need unique address or select_pins polarity!");
                return instance_id;
                // throw elegoo::common::CommandError("Shared TMC uarts need unique address or select_pins polarity");
            }
            instances[{instance_id, addr}] = true;
            return instance_id;
        }

        uint8_t MCU_TMC_uart_bitbang::_calc_crc8(const std::vector<uint8_t> &data)
        {
            uint8_t crc = 0;
            for (uint8_t b : data)
            {
                for (int i = 0; i < 8; ++i)
                {
                    if ((crc >> 7) ^ (b & 0x01))
                    {
                        crc = (crc << 1) ^ 0x07;
                    }
                    else
                    {
                        crc = (crc << 1);
                    }
                    crc &= 0xff;
                    b >>= 1;
                }
            }
            return crc;
        }

        std::vector<uint8_t> MCU_TMC_uart_bitbang::_add_serial_bits(
            const std::vector<uint8_t> &data)
        {
            std::vector<uint8_t> out;
            int pos = 0;

            for (uint8_t d : data)
            {
                uint32_t b = (static_cast<uint32_t>(d) << 1) | 0x200;

                for (int i = 0; i < 10; ++i)
                {
                    if (pos % 8 == 0)
                    {
                        out.push_back(0);
                    }

                    out.back() |= ((b >> i) & 0x1) << (pos % 8);

                    ++pos;
                }
            }

            return out;
        }

        std::vector<uint8_t> MCU_TMC_uart_bitbang::_encode_read(uint8_t sync,
                                                                uint8_t addr,
                                                                uint8_t reg)
        {
            std::vector<uint8_t> msg = {sync, addr, reg};
            msg.push_back(_calc_crc8(msg));
            return _add_serial_bits(msg);
        }

        std::vector<uint8_t> MCU_TMC_uart_bitbang::_encode_write(uint8_t sync,
                                                                 uint8_t addr,
                                                                 uint8_t reg,
                                                                 uint32_t val)
        {
            std::vector<uint8_t> msg = {sync,
                                        addr,
                                        reg,
                                        static_cast<uint8_t>((val >> 24) & 0xff),
                                        static_cast<uint8_t>((val >> 16) & 0xff),
                                        static_cast<uint8_t>((val >> 8) & 0xff),
                                        static_cast<uint8_t>(val & 0xff)};

            msg.push_back(_calc_crc8(msg));
            return _add_serial_bits(msg);
        }

        int MCU_TMC_uart_bitbang::_decode_read(uint8_t reg,
                                               const std::vector<uint8_t> &data, uint32_t &val)
        {
            if (data.size() != 10)
            {
                SPDLOG_ERROR("_decode_read error #0");
                return -1;
            }

            std::vector<uint8_t> mval(data.begin(), data.end());
            uint64_t low = 0, high = 0;
            for (size_t i = 0; i < 8; ++i)
            {
                low |= static_cast<uint64_t>(mval[i]) << (i * 8);
            }
            for (size_t i = 8; i < 10; ++i)
            {
                high |= static_cast<uint64_t>(mval[i]) << ((i - 8) * 8);
            }

            // 使用位操作提取寄存器值
            uint32_t reg_val = (static_cast<uint32_t>((low >> 31) & 0xff) << 24) |
                               (static_cast<uint32_t>((low >> 41) & 0xff) << 16) |
                               (static_cast<uint32_t>((low >> 51) & 0xff) << 8) |
                               static_cast<uint32_t>(((low >> 61) & 0x7) | ((high & 0x1F) << 3));

            std::vector<uint8_t> encoded_data = _encode_write(0x05, 0xff, reg, reg_val);
            if (data != encoded_data)
            {
                SPDLOG_ERROR("_decode_read error #1");
                return -1;
            }
            val = reg_val;
            return 0;
        }

        int MCU_TMC_uart_bitbang::reg_read(std::vector<bool> instance_id,
                                           uint8_t addr, uint8_t reg, uint32_t &val)
        {
            if (analog_mux != nullptr)
            {
                analog_mux->activate(instance_id);
            }

            std::vector<uint8_t> msg = _encode_read(0xf5, addr, reg);

            // printf("reg_read %.2x %.2x: ", addr, reg);
            // for(int i = 0; i < msg.size(); i++)
            //     printf("%.2x ", msg[i]);
            // printf("\n");

            std::vector<Any> data;
            data.push_back(std::to_string(oid));
            data.push_back(msg);
            data.push_back(std::to_string(10));

            // SPDLOG_DEBUG("addr:{},reg:{},data.size:{}",addr,reg,data.size());
            json params = tmcuart_send_cmd->send(data);
            if (params.empty())
            {
                SPDLOG_ERROR("reg_read addr {} reg {} error #0", addr, reg);
                return -1;
            }

            std::string read_str;
            if (params.contains("read") && params["read"].is_string())
            {
                read_str = params["read"].get<std::string>();
            }
            else
            {
                SPDLOG_ERROR("reg_read addr {} reg {} error #1", addr, reg);
                return -1;
            }

            std::vector<uint8_t> vec(read_str.begin(), read_str.end());
            return _decode_read(reg, vec, val);
        }

        int MCU_TMC_uart_bitbang::reg_write(std::vector<bool> instance_id,
                                            uint8_t addr, uint8_t reg, int64_t val,
                                            double print_time)
        {
            uint64_t minclock = 0;
            if (!std::isnan(print_time))
            {
                minclock = mcu->print_time_to_clock(print_time);
            }
            if (analog_mux != nullptr)
            {
                analog_mux->activate(instance_id);
            }
            std::vector<uint8_t> msg = _encode_write(0xf5, addr, reg | 0x80, val);
            std::vector<Any> data;
            data.push_back(std::to_string(oid));
            data.push_back(msg);
            data.push_back(std::to_string(0));
            // SPDLOG_DEBUG("__func__:{},addr:{},reg:{},data.size:{}",__func__,addr,reg,data.size());
            tmcuart_send_cmd->send(data, minclock);
            return 0;
        }

        std::shared_ptr<MCU_TMC_uart_bitbang> lookup_tmc_uart_bitbang(
            std::shared_ptr<ConfigWrapper> config, int max_addr,
            std::vector<bool> &instance_id, int &addr)
        {
            auto ppins = any_cast<std::shared_ptr<PrinterPins>>(config->get_printer()->lookup_object("pins"));
            auto rx_pin_params =
                ppins->lookup_pin(config->get("uart_pin"), false, true, "tmc_uart_rx");
            auto tx_pin_desc = config->get("tx_pin", "");
            std::shared_ptr<PinParams> tx_pin_params;
            if (tx_pin_desc.empty())
            {
                tx_pin_params = rx_pin_params;
            }
            else
            {
                tx_pin_params = ppins->lookup_pin(tx_pin_desc, false, false, "tmc_uart_tx");
            }

            if (rx_pin_params->chip != tx_pin_params->chip)
            {
                throw elegoo::common::CommandError("TMC uart rx and tx pins must be on the same mcu");
            }

            auto select_pins_desc = config->getlist("select_pins", {});

            addr = config->getint("uart_address", 0, 0, max_addr);

            std::shared_ptr<MCU_TMC_uart_bitbang> mcu_uart =
                std::static_pointer_cast<MCU_TMC_uart_bitbang>(rx_pin_params->pin_class);
            if (!mcu_uart)
            {
                mcu_uart = std::make_shared<MCU_TMC_uart_bitbang>(
                    rx_pin_params, tx_pin_params, select_pins_desc);
                rx_pin_params->pin_class = std::static_pointer_cast<void>(mcu_uart);
            }

            instance_id = mcu_uart->register_instance(rx_pin_params, tx_pin_params,
                                                      select_pins_desc, addr);

            return mcu_uart;
        }

        MCU_TMC_uart::MCU_TMC_uart(std::shared_ptr<ConfigWrapper> config,
                                   std::map<std::string, uint8_t> name_to_reg,
                                   std::shared_ptr<FieldHelper> fields, int max_addr,
                                   int tmc_frequency)
        {
            SPDLOG_DEBUG("MCU_TMC_uart init!");
            printer = config->get_printer();
            std::istringstream iss(config->get_name());
            std::vector<std::string> words;
            std::string word;
            while (iss >> word)
            {
                words.push_back(word);
            }
            name = (words.empty()) ? "" : words.back();

            this->name_to_reg = name_to_reg;
            this->fields = fields;
            ifcnt = INT_NONE;
            this->tmc_frequency = tmc_frequency;
            mcu_uart = lookup_tmc_uart_bitbang(config, max_addr, instance_id, addr);

            mutex = mcu_uart->get_mutex();
            SPDLOG_DEBUG("MCU_TMC_uart init success!!");
        }

        std::shared_ptr<FieldHelper> MCU_TMC_uart::get_fields() const
        {
            if (!fields)
                SPDLOG_ERROR("fields is empty!!!!!!");
            return fields;
        }

        int MCU_TMC_uart::_do_get_register(const std::string &reg_name, uint32_t &val)
        {
            int reg = name_to_reg.at(reg_name);
            uint32_t reg_val;

            if (printer->get_start_args().find("debugoutput") != printer->get_start_args().end())
            {
                val = 0;
                return 0;
            }

            for (int retry = 0; retry < 5; ++retry)
            {
                if (mcu_uart->reg_read(instance_id, addr, reg, reg_val) == 0)
                {
                    val = reg_val;
                    return 0;
                }
            }
            SPDLOG_ERROR("reg_name:{}, reg:{}", reg_name, reg);
            return -1;
        }

        uint32_t MCU_TMC_uart::get_register(const std::string &reg_name)
        {
            uint32_t val;
            mutex->lock();
            int ret = _do_get_register(reg_name, val);
            mutex->unlock();
            if (ret)
                throw elegoo::common::CommandError("Unable to read tmc uart '" + name + "'register " + reg_name);
            return val;
        }

        void MCU_TMC_uart::set_register(const std::string &reg_name, int64_t val,
                                        double print_time)
        {
            if (printer->get_start_args().find("debugoutput") != printer->get_start_args().end())
                return;
            SPDLOG_DEBUG("reg_name:{},val:{},print_time:{}", reg_name, val, print_time);

            uint8_t reg = name_to_reg.at(reg_name);

            mutex->lock();
            for (int retry = 0; retry < 5; ++retry)
            {
                int t_ifcnt = ifcnt;
                if (t_ifcnt == INT_NONE)
                {
                    if (_do_get_register("IFCNT", ifcnt) != 0)
                    {
                        mutex->unlock();
                        throw elegoo::common::CommandError("Unable to read tmc uart '" + name + "' register IFCNT");
                    }
                    t_ifcnt = ifcnt;
                }

                mcu_uart->reg_write(instance_id, addr, reg, val, print_time);

                if (_do_get_register("IFCNT", ifcnt) != 0)
                {
                    mutex->unlock();
                    throw elegoo::common::CommandError("Unable to read tmc uart '" + name + "' register IFCNT");
                }
                // SPDLOG_INFO("ifcnt {} t_ifcnt {}", ifcnt, t_ifcnt);
                if (ifcnt == (t_ifcnt + 1) & 0xff)
                {
                    mutex->unlock();
                    return;
                }
            }
            mutex->unlock();
            throw elegoo::common::CommandError("Unable to write tmc uart '" + name + "'register " + reg_name);
        }

        uint32_t MCU_TMC_uart::get_tmc_frequency() const
        {
            return tmc_frequency;
        }
    }
}
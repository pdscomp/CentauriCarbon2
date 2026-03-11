#include "por.h"
#include "host_warpper.h"
#include "reactor.h"
#include "clocksync.h"
#include "configfile.h"
#include "printer.h"
#include "pins.h"
#include "exception_handler.h"
#include <algorithm>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace elegoo
{
    namespace extras
    {

        POR::POR(std::shared_ptr<ConfigWrapper> config)
        {
            auto ppins = any_cast<std::shared_ptr<PrinterPins>>(config->get_printer()->lookup_object("pins"));
            reactor = config->get_printer()->get_reactor();
            shutdown = false;
            // 断电检测引脚
            // 当前检测脚接在主机上所以暂时用这种方案
            SPDLOG_DEBUG("POR #0");
            std::shared_ptr<PinParams> sensor_pin_params = ppins->lookup_pin(config->get("sensor_pin"), true, true);
            SPDLOG_DEBUG("POR #1");
            if (*sensor_pin_params->chip_name == "host")
            {
                SPDLOG_DEBUG("POR #2 ");
                // host_sensor_pin = std::static_pointer_cast<MCU_host_digital_pin>(ppins->setup_pin("host_digital_pin", config->get("sensor_pin")));
                host_sensor_pin = std::static_pointer_cast<MCU_host_digital_pin>(sensor_pin_params->chip->setup_pin("host_digital_pin", sensor_pin_params));
                host_sensor_pin->set_direction(1);
                sensor_pullup = sensor_pin_params->pullup;
                sensor_invert = sensor_pin_params->invert;
                SPDLOG_DEBUG("POR #3");
                SPDLOG_DEBUG("sensor_pin: pin {} chip_name {} invert {}", config->get("sensor_pin"), *sensor_pin_params->chip_name, sensor_pin_params->invert);

                // 注册定时器轮询...
                rest_time = config->getdouble("rest_time", 0.01);
#if JITTER
                double jitter_time = config->getdouble("jitter_time", 0.2);
                jitter_times = std::max(int(jitter_time / rest_time + 0.5), 1);
                jitter_counter = 0;
                SPDLOG_INFO("por jitter time {} jitter times {}", jitter_time, jitter_times);
#endif
                next_charge_time = get_monotonic() + config->getdouble("charge_time", 30);
                query_timer = reactor->register_timer(
                    [this](double eventtime)
                    {
                        // 检查充电是否完成
                        if (!charge_done && eventtime > next_charge_time)
                        {
                            SPDLOG_INFO("POR CHARGE DONE!");
                            host_ctrl_pin->set_digital(1);
                            charge_done = 1;
                        }

                        uint8_t val = host_sensor_pin->get_digital();
                        if (val ^ sensor_invert)
                        {
                            // SPDLOG_WARN("power_off!!!");
#if JITTER

                            if (jitter_counter < jitter_times)
                            {
                                SPDLOG_WARN("jitter_counter %d jitter_times %d", jitter_counter, jitter_times);
                                jitter_counter++;
                            }
                            else
#endif
                            {
                                trig_shutdown();
                            }
                        }
                        else
                        {
#if JITTER
                            jitter_counter = 0;
#endif
                        }
                        return eventtime + rest_time;
                    },
                    _NEVER, "por");
                elegoo::common::SignalManager::get_instance().register_signal(
                    "elegoo:ready",
                    std::function<void()>([this]()
                                          { reactor->update_timer(query_timer, _NOW); }));
            }
            else
            {
                // rest_time = config->getdouble("rest_time", 0.001);
                // sample_time = config->getdouble("sample_time", .000015);
                // sample_count = config->getint("sample_count", 4);
                // mcu = std::static_pointer_cast<MCU>(sensor_pin_params->chip);
                // sensor_pin = *sensor_pin_params->pin;
                // sensor_pullup = sensor_pin_params->pullup;
                // sensor_invert = sensor_pin_params->invert;
                // oid = mcu->create_oid();
            }

            // 超级电容控制引脚
            std::shared_ptr<PinParams> ctrl_pin_params = ppins->lookup_pin(config->get("ctrl_pin"), true, false);
            if (*sensor_pin_params->chip_name == "host")
            {
                // 默认关闭供电
                host_ctrl_pin = std::static_pointer_cast<MCU_host_digital_pin>(ctrl_pin_params->chip->setup_pin("host_digital_pin", ctrl_pin_params));
                host_ctrl_pin->set_direction(0);
                host_ctrl_pin->set_digital(0);
            }

            std::shared_ptr<PinParams> charge_pin_params = ppins->lookup_pin(config->get("charge_pin"), true, false);
            if (*charge_pin_params->chip_name == "host")
            {
                // 默认开启充电,
                host_charge_pin = std::static_pointer_cast<MCU_host_digital_pin>(charge_pin_params->chip->setup_pin("host_digital_pin", charge_pin_params));
                host_charge_pin->set_direction(0);
                host_charge_pin->set_digital(1);
            }

            // 解析关机引脚
            std::vector<std::string> __shutdown_pins = elegoo::common::split(config->get("shutdown_pins", ""), ",");

            for (auto pin : __shutdown_pins)
            {
                std::shared_ptr<PinParams> pin_params = ppins->lookup_pin(pin, true, false, "ignore");
                SPDLOG_DEBUG("shutdown_pins: pin {} chip_name {} invert {}", pin, *pin_params->chip_name, pin_params->invert);
                if (*pin_params->chip_name == "host")
                {
                    // std::shared_ptr<MCU_host_digital_pin> shutdown_pin =
                    //     std::static_pointer_cast<MCU_host_digital_pin>(ppins->setup_pin("host_digital_pin", pin));
                    std::shared_ptr<MCU_host_digital_pin> shutdown_pin = std::static_pointer_cast<MCU_host_digital_pin>(pin_params->chip->setup_pin("host_digital_pin", pin_params));
                    host_shutdown_pins.push_back({shutdown_pin, pin_params->invert});
                }
                else
                {
                    std::shared_ptr<MCU> mcu = std::static_pointer_cast<MCU>(pin_params->chip);
                    mcus.insert(mcu);
                    shutdown_pins.push_back({mcu, *pin_params->pin, pin_params->invert});
                }
            }

            // 注册配置函数
            for (auto it = mcus.begin(); it != mcus.end(); it++)
            {
                (*it)->register_config_callback([=]()
                                                { build_config(*it); });
            }

            auto gcode = any_cast<std::shared_ptr<GCodeDispatch>>(config->get_printer()->lookup_object("gcode"));
            gcode->register_command("POR_TRIG_SHUTDOWN",
                                    [this](std::shared_ptr<GCodeCommand> gcmd)
                                    {
                                        cmd_POR_TRIG_SHUTDOWN(gcmd);
                                    });

            elegoo::common::SignalManager::get_instance().register_signal(
                "mcu:mcu_shutdown",
                std::function<void()>([this]()
                                      { handle_mcu_shutdown(); }));
        }

        POR::~POR()
        {
        }

        void POR::build_config(std::shared_ptr<MCU> mcu)
        {
            std::ostringstream shutdown_pin;
            std::ostringstream shutdown_value;
            if (oids.find(mcu) == oids.end())
            {
                uint32_t oid = mcu->create_oid();
                oids.insert({mcu, oid});
                auto cmd_queue = mcu->alloc_command_queue();
                set_cmds.insert({mcu, mcu->lookup_command("shutdown_pins_set oid=%c", cmd_queue)});

                std::vector<std::tuple<std::string, int>> all_pins;
                for (auto sp : shutdown_pins)
                {
                    std::shared_ptr<MCU> __mcu;
                    std::string pin;
                    int invert;
                    std::tie(__mcu, pin, invert) = sp;
                    if (__mcu == mcu)
                        all_pins.push_back({pin, invert});
                }

                mcu->add_config_cmd("config_shutdown_pins oid=" + std::to_string(oid));
                for (auto sp : all_pins)
                {
                    std::string pin;
                    int invert;
                    std::tie(pin, invert) = sp;
                    SPDLOG_DEBUG("mcu {} pin {} invert {}", mcu->get_name(), pin, invert);

                    mcu->add_config_cmd("shutdown_pins_add oid=" + std::to_string(oid) +
                                        " pin=" + pin +
                                        " value=" + std::to_string(invert));
                }
            }
        }

        void POR::trig_shutdown()
        {
            if (shutdown)
                return;
            SPDLOG_WARN("trig_shutdonw!");

            for (auto it = host_shutdown_pins.begin(); it != host_shutdown_pins.end(); it++)
            {
                std::shared_ptr<MCU_host_digital_pin> shutdown_pin;
                int value;
                std::tie(shutdown_pin, value) = *it;
                shutdown_pin->set_direction(0);
                shutdown_pin->set_digital(value);
            }
            for (auto it = mcus.begin(); it != mcus.end(); it++)
                set_cmds[*it]->send({std::to_string(oids[*it])});

            elegoo::common::SignalManager::get_instance().emit_signal("por:power_off");
            SPDLOG_WARN("por:power_off handle done");
            system("sync");
            host_ctrl_pin->set_digital(0);
            host_charge_pin->set_digital(0);
            system("reboot");
            shutdown = true;
        }

        void POR::cmd_POR_TRIG_SHUTDOWN(std::shared_ptr<GCodeCommand> gcmd)
        {
            trig_shutdown();
        }

        void POR::handle_mcu_shutdown()
        {
            SPDLOG_INFO("por handle_mcu_shutdown");
            host_ctrl_pin->set_digital(0);
            host_charge_pin->set_digital(0);
            charge_done = 1;
        }

        std::shared_ptr<POR> por_load_config(std::shared_ptr<ConfigWrapper> config)
        {
            std::shared_ptr<POR> por = std::make_shared<POR>(config);
            return por;
        }
    }
}
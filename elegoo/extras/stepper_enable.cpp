/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2024-11-23 12:21:16
 * @LastEditors  : Ben
 * @LastEditTime : 2024-11-30 12:22:24
 * @Description  : Support for enable pins on stepper motor drivers
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "stepper_enable.h"
#include "kinematics.h"

#include "logger.h"
namespace elegoo
{
    namespace extras
    {

        const double DISABLE_STALL_TIME = 0.100;
        const std::string cmd_SET_STEPPER_ENABLE_help =
            "Enable/disable individual stepper by name";

        PrinterStepperEnable::PrinterStepperEnable(
            std::shared_ptr<ConfigWrapper> config)
        {
            SPDLOG_INFO("PrinterStepperEnable init!");
            this->printer = config->get_printer();
            this->enable_lines.clear();
            elegoo::common::SignalManager::get_instance().register_signal(
                "gcode:request_restart",
                std::function<void(double)>([this](double print_time)
                                            { _handle_request_restart(); }));

            auto gcode =
                any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));
            gcode->register_command("M18",
                                    std::bind(&PrinterStepperEnable::cmd_M18, this));
            gcode->register_command("M84",
                                    std::bind(&PrinterStepperEnable::cmd_M18, this));
            gcode->register_command(
                "SET_STEPPER_ENABLE",
                std::bind(&PrinterStepperEnable::cmd_SET_STEPPER_ENABLE, this,
                          std::placeholders::_1),
                false, cmd_SET_STEPPER_ENABLE_help);
        }

        PrinterStepperEnable::~PrinterStepperEnable()
        {
        }

        void PrinterStepperEnable::register_stepper(
            std::shared_ptr<ConfigWrapper> config,
            std::shared_ptr<MCU_stepper> mcu_stepper)
        {
            auto name = mcu_stepper->get_name();
            auto enable = setup_enable_pin(this->printer, config->get("enable_pin", ""));
            this->enable_lines[name] =
                std::make_shared<EnableTracking>(mcu_stepper, enable);
            SPDLOG_DEBUG("enable_lines.size:{},mcu_stepper->get_name():{}", enable_lines.size(), mcu_stepper->get_name());
        }

        void PrinterStepperEnable::motor_off()
        {
            auto toolhead =
                any_cast<std::shared_ptr<ToolHead>>(printer->lookup_object("toolhead"));
            toolhead->dwell(DISABLE_STALL_TIME);

            auto print_time = toolhead->get_last_move_time();
            for (auto it = enable_lines.begin(); it != enable_lines.end(); ++it)
            {
                auto el = it->second;
                el->motor_disable(print_time);
            }
            // 清除归零状态
            toolhead->get_kinematic()->clear_homing_state({0, 1, 2});
            elegoo::common::SignalManager::get_instance().emit_signal(
                "stepper_enable:motor_off" +
                std::to_string(print_time));
            toolhead->dwell(DISABLE_STALL_TIME);
        }

        void PrinterStepperEnable::motor_debug_enable(const std::string &stepper,
                                                      int enable)
        {
            auto toolhead =
                any_cast<std::shared_ptr<ToolHead>>(printer->lookup_object("toolhead"));
            toolhead->dwell(DISABLE_STALL_TIME);
            auto print_time = toolhead->get_last_move_time();

            auto el = this->enable_lines[stepper];
            if (enable && el)
            {
                el->motor_enable(print_time);
                SPDLOG_INFO("{} has been manually enabled", stepper);
            }
            else
            {
                el->motor_disable(print_time);
                SPDLOG_INFO("{} has been manually disabled", stepper);
            }

            toolhead->dwell(DISABLE_STALL_TIME);
        }

        json PrinterStepperEnable::get_status() const
        {
            json steppers_json;
            for (auto it = enable_lines.begin(); it != enable_lines.end(); ++it)
            {
                auto el = it->second;
                steppers_json[it->first] = it->second->is_motor_enabled();
            }
            json retval;
            retval["steppers"] = steppers_json;
            return retval;
        }

        void PrinterStepperEnable::_handle_request_restart() { this->motor_off(); }

        void PrinterStepperEnable::cmd_M18() { this->motor_off(); }

        void PrinterStepperEnable::cmd_SET_STEPPER_ENABLE(
            std::shared_ptr<GCodeCommand> gcmd)
        {
            auto stepper_name = gcmd->get("STEPPER", "");
            if (this->enable_lines.find(stepper_name) == enable_lines.end())
            {
                gcmd->respond_info("SET_STEPPER_ENABLE: Invalid stepper " + stepper_name, true);
                return;
            }
            int stepper_enable = gcmd->get_int("ENABLE", 1);
            this->motor_debug_enable(stepper_name, stepper_enable);
        }

        std::shared_ptr<EnableTracking> PrinterStepperEnable::lookup_enable(
            const std::string &name)
        {
            if (enable_lines.find(name) == enable_lines.end())
            {
                std::string error_info = "Unknown stepper " + name;
                throw elegoo::common::CommandError(error_info);
            }
            return enable_lines[name];
        }

        std::vector<std::string> PrinterStepperEnable::get_steppers()
        {
            std::vector<std::string> retval;
            for (auto it = enable_lines.begin(); it != enable_lines.end(); ++it)
            {
                retval.push_back(it->first);
            }
            return retval;
        }

        StepperEnablePin::StepperEnablePin(std::shared_ptr<MCU_digital_out> mcu_enable,
                                           int enable_count)
            : mcu_enable(mcu_enable), enable_count(enable_count), is_dedicated(true) {}

        void StepperEnablePin::set_enable(double print_time)
        {
            if (enable_count == 0)
            {
                mcu_enable->set_digital(print_time, 1);
            }
            enable_count++;
        }

        void StepperEnablePin::set_disable(double print_time)
        {
            enable_count--;
            if (enable_count == 0)
            {
                mcu_enable->set_digital(print_time, 0);
            }
        }

        std::shared_ptr<StepperEnablePin> setup_enable_pin(
            std::shared_ptr<Printer> printer, const std::string &pin)
        {
            if (pin.empty())
            {
                auto enable = std::make_shared<StepperEnablePin>(nullptr, 9999);
                enable->set_dedicated(false);
                return enable;
            }

            auto ppins =
                any_cast<std::shared_ptr<PrinterPins>>(printer->lookup_object("pins"));
            auto pin_params = ppins->lookup_pin(pin, true, false, "stepper_enable");

            std::shared_ptr<void> enable_ptr = pin_params->pin_class;
            if (enable_ptr)
            {
                auto enable = std::static_pointer_cast<StepperEnablePin>(enable_ptr);
                enable->set_dedicated(false);
                return enable;
            }

            auto mcu_enable = (std::static_pointer_cast<MCU>(pin_params->chip))
                                  ->setup_pin("digital_out", pin_params);
            std::static_pointer_cast<MCU_digital_out>(mcu_enable)
                ->setup_max_duration(0.0);

            auto enable = std::make_shared<StepperEnablePin>(
                std::static_pointer_cast<MCU_digital_out>(mcu_enable), 0);
            pin_params->pin_class = std::static_pointer_cast<void>(enable);
            return enable;
        }

        EnableTracking::EnableTracking(std::shared_ptr<MCU_stepper> stepper,
                                       std::shared_ptr<StepperEnablePin> enable)
        {
            this->stepper = stepper;
            this->enable = enable;
            this->callbacks.clear();
            this->is_enabled = false;
            this->stepper->add_active_callback(
                std::bind(&EnableTracking::motor_enable, this, std::placeholders::_1));
        }

        EnableTracking::~EnableTracking() {}

        void EnableTracking::register_state_callback(
            std::function<void(double, bool)> callback)
        {
            this->callbacks.push_back(callback);
        }

        void EnableTracking::motor_enable(double print_time)
        {
            if (!this->is_enabled)
            {
                for (auto &cb : this->callbacks)
                {
                    cb(print_time, true);
                }
                this->enable->set_enable(print_time);
                this->is_enabled = true;
            }
        }

        void EnableTracking::motor_disable(double print_time)
        {
            if (this->is_enabled)
            {
                for (auto &cb : this->callbacks)
                {
                    cb(print_time, false);
                }
                this->enable->set_disable(print_time);
                this->is_enabled = false;
                this->stepper->add_active_callback(
                    std::bind(&EnableTracking::motor_enable, this, std::placeholders::_1));
            }
        }
        bool EnableTracking::is_motor_enabled() const { return this->is_enabled; }
        bool EnableTracking::has_dedicated_enable() const
        {
            return this->enable->IsDedicated();
        }

        std::shared_ptr<PrinterStepperEnable> stepper_enable_load_config(
            std::shared_ptr<ConfigWrapper> config)
        {
            return std::make_shared<PrinterStepperEnable>(config);
        }

    }
}
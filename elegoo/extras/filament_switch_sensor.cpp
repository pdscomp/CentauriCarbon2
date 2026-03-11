/***************************************************************************** 
 * @Author       : Gary
 * @Date         : 2025-02-27 11:52:19
 * @LastEditors  : loping
 * @LastEditTime : 2025-09-26 14:56:53
 * @Description  : 
 * 
 * Copyright (c) 2025 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/
#include "filament_switch_sensor.h"
#include "idle_timeout.h"
#include "print_stats.h"
#include "mmu.h"
#include "buttons.h"
#include <iostream>
#include <memory>
#include <stdexcept>

namespace elegoo {
namespace extras {
RunoutHelper::RunoutHelper(std::shared_ptr<ConfigWrapper> config)
    : config(config),
      printer(config->get_printer()),
      reactor(printer->get_reactor()), 
      gcode(any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"))),
      runout_pause(config->getboolean("pause_on_runout", BoolValue::BOOL_TRUE)),
      min_event_systime(reactor->NEVER),
      filament_present(false),
      canvas_enabled(1),
      rt_gcode("") {
    
    std::vector<std::string> split_name = elegoo::common::split(config->get_name()); 
    name = split_name.back();
    
    if (runout_pause) {
        pause_resume = any_cast<std::shared_ptr<PauseResume>>(printer->load_object(config, "pause_resume"));
    }

    gcode_macro = any_cast<std::shared_ptr<PrinterGCodeMacro>>(printer->load_object(config, "gcode_macro"));

    canvas = any_cast<std::shared_ptr<elegoo::extras::Canvas>>(
        printer->lookup_object("canvas_dev",std::shared_ptr<elegoo::extras::Canvas>()));

    if (runout_pause || !config->get("runout_gcode", "").empty()) {
        SPDLOG_DEBUG("obtain runout_gcode");
        runout_gcode = gcode_macro->load_template(config, "runout_gcode", "");
    }
    if (!config->get("insert_gcode", "").empty()) {
        insert_gcode = gcode_macro->load_template(config, "insert_gcode", "");
    }

    pause_delay = config->getdouble("pause_delay", 0.5, DOUBLE_NONE, DOUBLE_NONE, 0.0);
    event_delay = config->getdouble("event_delay", 3.0, DOUBLE_NONE, DOUBLE_NONE, 0.0);
    sensor_enabled = config->getint("enable", 1);
    elegoo::common::SignalManager::get_instance().register_signal(
        "elegoo:ready",
        std::function<void()>([this](){
            SPDLOG_DEBUG("elegoo:ready !");
            _handle_ready();
            SPDLOG_DEBUG("elegoo:ready !");
        })
    );

    elegoo::common::SignalManager::get_instance().register_signal(
        "canvas:enable",
        std::function<void()>([this](){
            _handle_canvas_enable();
        })
    );
    
    elegoo::common::SignalManager::get_instance().register_signal(
        "canvas:disable",
        std::function<void()>([this](){
            _handle_canvas_disable();
        })
    );

    gcode->register_mux_command("QUERY_FILAMENT_SENSOR", "SENSOR", name, 
        [this](std::shared_ptr<GCodeCommand> gcmd) { this->cmd_QUERY_FILAMENT_SENSOR(gcmd); },
        "Query the status of the Filament Sensor");

    gcode->register_mux_command("SET_FILAMENT_SENSOR", "SENSOR", name, 
        [this](std::shared_ptr<GCodeCommand> gcmd) { this->cmd_SET_FILAMENT_SENSOR(gcmd); },
        "Sets the filament sensor on/off");
}

void RunoutHelper::_handle_ready() {
    min_event_systime = get_monotonic() + 2.0;
    SPDLOG_DEBUG("_handle_ready min_event_systime {}", min_event_systime);
}

void RunoutHelper::_handle_canvas_enable()
{
    canvas_enabled = 1;
}

void RunoutHelper::_handle_canvas_disable()
{
    canvas_enabled = 0;
}


json RunoutHelper::_runout_event_handler(double eventtime) {
    SPDLOG_INFO("_runout_event_handler name: {}", name);
    std::string pause_prefix = "";
    if (runout_pause) {
        pause_resume->send_pause_command();
        pause_prefix = "PAUSE\n";
        reactor->pause(eventtime + pause_delay);
    }
    // _exec_gcode(pause_prefix, runout_gcode);

    try {
        gcode->run_script(pause_prefix);
        if(canvas && !canvas->get_connect_state())
        {
            runout_gcode->run_gcode_from_command();
            gcode->run_script("\nM400");

            if(name == "encoder_sensor")
            {
                gcode->respond_ecode("", elegoo::common::ErrorCode::EXTERNAL_FILA_ERROR, 
                    elegoo::common::ErrorLevel::WARNING);
            }
        }
        else
        {
            elegoo::common::SignalManager::get_instance().emit_signal(
                "canvas:auto_refill");
        }
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Script running error: {}", e.what());
    }

    min_event_systime = get_monotonic() + event_delay;
    return json::object();
}


json RunoutHelper::_insert_event_handler(double eventtime) {
    SPDLOG_INFO("_insert_event_handler  name: {}",name);
    _exec_gcode("", insert_gcode);
    return json::object();
}

void RunoutHelper::_exec_gcode(const std::string& prefix, const std::shared_ptr<TemplateWrapper>& template_) {
    try {
        gcode->run_script(prefix);
        template_->run_gcode_from_command();
        gcode->run_script("\nM400");
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Script running error: {}", e.what());
    }
    min_event_systime = get_monotonic() + event_delay;
}

void RunoutHelper::note_filament_present(bool is_filament_present) {
    SPDLOG_DEBUG("name:{}, note_filament_present is_filament_present {} filament_present {} sensor_enabled {} canvas_enabled {} ",
        name, is_filament_present, filament_present, sensor_enabled, canvas_enabled);
    if (is_filament_present == filament_present) {
        return;
    }

    filament_present = is_filament_present;
    if(name == "filament_sensor") {
        elegoo::common::SignalManager::get_instance().emit_signal<bool>(
            "elegoo:filament_det", filament_present);
    }

    double eventtime = get_monotonic();

    if (eventtime < min_event_systime || !sensor_enabled || !canvas_enabled) {
        return;
    }
     
    SPDLOG_INFO("name:{}, note_filament_present is_filament_present {} filament_present {} sensor_enabled {} canvas_enabled {} ",
        name, is_filament_present, filament_present, sensor_enabled, canvas_enabled);
    // auto idle_timeout = any_cast<std::shared_ptr<IdleTimeout>>(printer->lookup_object("idle_timeout"));
    // bool is_printing = idle_timeout->get_status(eventtime)["state"].get<std::string>() == "Printing";
    auto print_stats = any_cast<std::shared_ptr<PrintStats>>(printer->load_object(config, "print_stats"));
    bool is_printing = print_stats->get_status(eventtime)["state"].get<std::string>() == "printing";

    if (is_filament_present) {
        if (!is_printing && insert_gcode != nullptr) {
            min_event_systime = reactor->NEVER;
            reactor->register_callback([this](double eventtime) { return this->_insert_event_handler(eventtime); });
        }
    } else if (is_printing && runout_gcode != nullptr) {//(is_printing && runout_gcode != nullptr) {
        min_event_systime = reactor->NEVER;
        reactor->register_callback([this](double eventtime) { return this->_runout_event_handler(eventtime); });
    }
}

json RunoutHelper::get_status(double eventtime) const {

    json status;
    status["filament_detected"] = filament_present; //临时使用
    status["enabled"] = sensor_enabled;

    return status;
}

void RunoutHelper::cmd_QUERY_FILAMENT_SENSOR(std::shared_ptr<GCodeCommand> gcmd) {
    const std::string msg = filament_present
                          ? "Filament Sensor " + name + ": filament detected"
                          : "Filament Sensor " + name + ": filament not detected";
    gcmd->respond_info(msg, true);
    SPDLOG_DEBUG("msg {}", msg);
}

void RunoutHelper::cmd_SET_FILAMENT_SENSOR(std::shared_ptr<GCodeCommand> gcmd) {
    sensor_enabled = gcmd->get_int("ENABLE", 1);
    SPDLOG_INFO("cmd_SET_FILAMENT_SENSOR:{}", sensor_enabled);
    // std::string cfgname = config->get_name();
    // std::shared_ptr<PrinterConfig> configfile =
    //     any_cast<std::shared_ptr<PrinterConfig>>(printer->lookup_object("configfile"));
    // configfile->set(cfgname, "enable", std::to_string(sensor_enabled));
    // configfile->cmd_SAVE_CONFIG(gcode->create_gcode_command("SAVE_CONFIG", "SAVE_CONFIG", std::map<std::string, std::string>()));
}

SwitchSensor::SwitchSensor(std::shared_ptr<ConfigWrapper> config) {
    auto printer = config->get_printer();
    buttons = any_cast<std::shared_ptr<PrinterButtons>>(printer->load_object(config, "buttons"));
    std::string switch_pin = config->get("switch_pin");

    buttons->register_buttons({switch_pin}, [this](double eventtime, bool state) {
        return this->_button_handler(eventtime, state);
    });

    runout_helper = std::make_shared<RunoutHelper>(config);
    get_status = std::bind(&RunoutHelper::get_status, runout_helper, std::placeholders::_1);    
}

double SwitchSensor::_button_handler(double eventtime, bool state) {
    SPDLOG_DEBUG("_button_handler state = {}", state);
    runout_helper->note_filament_present(state);
    return 0.0;
}

std::shared_ptr<SwitchSensor> switch_sensor_load_config_prefix(std::shared_ptr<ConfigWrapper> config) {
    SPDLOG_DEBUG("switch_sensor_load_config_prefix");
    return std::make_shared<SwitchSensor>(config);
}

}
}
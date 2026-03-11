/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2025-09-01 15:25:53
 * @LastEditors  : loping
 * @LastEditTime : 2025-09-28 14:34:14
 * @Description  : 
 * 
 * Copyright (c) 2025 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/
#include "cover_switch_sensor.h"
#include "printer.h"
#include "configfile.h"
#include "buttons.h"
#include "idle_timeout.h"
#include "pause_resume.h"

namespace elegoo
{
namespace extras
{

CoverSensor::CoverSensor(std::shared_ptr<ConfigWrapper> config) : 
    config(config),
    printer(config->get_printer()),
    reactor(printer->get_reactor()), 
    gcode(any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"))),
    min_event_systime(reactor->NEVER),
    is_cover_drop(false)
{
    gcode_macro = any_cast<std::shared_ptr<PrinterGCodeMacro>>(printer->load_object(config, "gcode_macro"));
    pause_resume = any_cast<std::shared_ptr<PauseResume>>(printer->load_object(config, "pause_resume"));
    
    if (!config->get("cover_drop_gcode", "").empty()) {
        cover_drop_gcode = gcode_macro->load_template(config, "cover_drop_gcode", "");
    }

    std::shared_ptr<PrinterButtons> buttons = 
        any_cast<std::shared_ptr<PrinterButtons>>(printer->load_object(config, "buttons"));
    std::string switch_pin = config->get("switch_pin");
    buttons->register_buttons({switch_pin}, [this](double eventtime, bool state) {
        return this->button_handler(eventtime, state);
    });

 
    event_delay = config->getdouble("event_delay", 3.0, DOUBLE_NONE, DOUBLE_NONE, 0.0);

    elegoo::common::SignalManager::get_instance().register_signal(
        "elegoo:ready",
        std::function<void()>([this](){
            handle_ready();
        })
    );
}

CoverSensor::~CoverSensor()
{

}

void CoverSensor::handle_ready()
{
    min_event_systime = get_monotonic() + 2.0;
    
}

double CoverSensor::button_handler(double eventtime, bool state)
{
    if (is_cover_drop == state) {
        return 0;
    }
    
    is_cover_drop = state;

    elegoo::common::SignalManager::get_instance().emit_signal<bool>(
        "elegoo:cover_off", is_cover_drop);

    json res;
    res["command"] = "CANVAS_FAN_OFF_FEEDBACK"; 
    res["result"] = std::to_string(is_cover_drop);    
    SPDLOG_INFO("{}",res.dump());
    gcode->respond_feedback(res);

    auto idle_timeout = any_cast<std::shared_ptr<IdleTimeout>>(printer->lookup_object("idle_timeout"));
    bool is_printing = idle_timeout->get_status(eventtime)["state"].get<std::string>() == "Printing";

    SPDLOG_INFO("front cover detect is_printing:{} state:{} is_cover_drop:{}", is_printing,state,is_cover_drop);
    if (is_cover_drop) {
        if (is_printing && cover_drop_gcode != nullptr)
        {
            if (get_monotonic() < min_event_systime) {
                return 0;
            }
        
            min_event_systime = reactor->NEVER;
            reactor->register_callback([this](double eventtime) { return this->cover_drop_event_handler(eventtime); });
        }
        else
        {
            gcode->respond_ecode("", elegoo::common::ErrorCode::CANVAS_FAN_OFF, elegoo::common::ErrorLevel::WARNING);
        }
    }
    else
    {
        gcode->respond_ecode("", elegoo::common::ErrorCode::CANVAS_FAN_OFF, elegoo::common::ErrorLevel::RESUME);
    }

    return 0;
}

json CoverSensor::cover_drop_event_handler(double eventtime)
{
    SPDLOG_INFO("cover_drop_event_handler");
    std::string pause_prefix = "";
    pause_resume->send_pause_command();
    pause_prefix = "PAUSE GOTO_WASTE_BOX=0\n";
    reactor->pause(eventtime + 1.0);
    
    exec_gcode(pause_prefix, cover_drop_gcode);
    gcode->respond_ecode("", elegoo::common::ErrorCode::CANVAS_FAN_OFF, elegoo::common::ErrorLevel::WARNING);
    return json::object();
}

json CoverSensor::get_status(double eventtime) const
{
    json status;
    status["front_cover_detected"] = is_cover_drop;

    return status;
}

void CoverSensor::exec_gcode(const std::string& prefix, const std::shared_ptr<TemplateWrapper>& template_) 
{
    try {
        gcode->run_script(prefix);
        template_->run_gcode_from_command();
        gcode->run_script("\nM400");
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Script running error: {}", e.what());
    }
    min_event_systime = get_monotonic() + event_delay;
}


std::shared_ptr<CoverSensor> cover_switch_sensor_load_config(std::shared_ptr<ConfigWrapper> config)
{
    return std::make_shared<CoverSensor>(config);
}
}
}
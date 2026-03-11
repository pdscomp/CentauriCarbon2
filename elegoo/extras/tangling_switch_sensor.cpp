/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2025-09-01 16:39:09
 * @LastEditors  : Ben
 * @LastEditTime : 2025-09-05 20:41:53
 * @Description  : 
 * 
 * Copyright (c) 2025 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "tangling_switch_sensor.h"
#include "printer.h"
#include "configfile.h"
#include "buttons.h"
#include "idle_timeout.h"
#include "pause_resume.h"

namespace elegoo
{
namespace extras
{

TanglingSensor::TanglingSensor(std::shared_ptr<ConfigWrapper> config) : 
    config(config),
    printer(config->get_printer()),
    reactor(printer->get_reactor()), 
    gcode(any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"))),
    min_event_systime(reactor->NEVER),
    is_tangling(false),
    canvas_enable(1)
{
    gcode_macro = any_cast<std::shared_ptr<PrinterGCodeMacro>>(printer->load_object(config, "gcode_macro"));
    pause_resume = any_cast<std::shared_ptr<PauseResume>>(printer->load_object(config, "pause_resume"));
    
    if (!config->get("tangling_gcode", "").empty()) {
        tangling_gcode = gcode_macro->load_template(config, "tangling_gcode", "");
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

    elegoo::common::SignalManager::get_instance().register_signal(
        "canvas:enable",
        std::function<void()>([this](){
            handle_canvas_enable();
        })
    );
    
    elegoo::common::SignalManager::get_instance().register_signal(
        "canvas:disable",
        std::function<void()>([this](){
            handle_canvas_disable();
        })
    );
}

TanglingSensor::~TanglingSensor()
{

}

void TanglingSensor::handle_ready()
{
    min_event_systime = get_monotonic() + 2.0;
}

void TanglingSensor::handle_canvas_enable()
{
    canvas_enable = 1;
}

void TanglingSensor::handle_canvas_disable()
{
    canvas_enable = 0;
}


double TanglingSensor::button_handler(double eventtime, bool state)
{
    if (is_tangling == state) {
        return 0;
    }
    SPDLOG_INFO("tangling detect state:{}", state);
    
    is_tangling = state;

    // elegoo::common::SignalManager::get_instance().emit_signal<bool>(
    //     "elegoo:wrap_det", is_tangling);

    if (get_monotonic() < min_event_systime || !canvas_enable) {
        return 0;
    }
     
    auto idle_timeout = any_cast<std::shared_ptr<IdleTimeout>>(printer->lookup_object("idle_timeout"));
    bool is_printing = idle_timeout->get_status(eventtime)["state"].get<std::string>() == "Printing";

    if (is_tangling) {
        if (is_printing && tangling_gcode != nullptr) {
            min_event_systime = reactor->NEVER;
            reactor->register_callback([this](double eventtime) { return this->tangling_event_handler(eventtime); });
        }
    }

    return 0;
}

json TanglingSensor::tangling_event_handler(double eventtime)
{
    SPDLOG_INFO("tangling_event_handler");
    std::string pause_prefix = "";
    pause_resume->send_pause_command();
    pause_prefix = "PAUSE\n";
    reactor->pause(eventtime + 1.0);
    
    exec_gcode(pause_prefix, tangling_gcode);

    gcode->respond_ecode("", elegoo::common::ErrorCode::CANVAS_WRAP_FILA, 
        elegoo::common::ErrorLevel::WARNING);

    return json::object();
}

json TanglingSensor::get_status(double eventtime) const
{
    json status;
    status["tangling_detected"] = is_tangling;

    return status;
}

void TanglingSensor::exec_gcode(const std::string& prefix, const std::shared_ptr<TemplateWrapper>& template_) 
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


std::shared_ptr<TanglingSensor> tangling_switch_sensor_load_config(std::shared_ptr<ConfigWrapper> config)
{
    return std::make_shared<TanglingSensor>(config);
}
}
}
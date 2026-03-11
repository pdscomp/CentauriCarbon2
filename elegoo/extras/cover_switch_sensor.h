/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2025-09-01 15:25:53
 * @LastEditors  : Ben
 * @LastEditTime : 2025-09-11 21:24:21
 * @Description  : 
 * 
 * Copyright (c) 2025 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/
#pragma once

#include <string>
#include <functional>
#include <memory>
#include "gcode_macro.h"

class Printer;
class SelectReactor;
class GCodeDispatch;
class ConfigWrapper;
class WebHooks;
namespace elegoo
{
namespace extras
{

class TemplateWrapper;
class IdleTimeout;
class PauseResume;


class CoverSensor
{
public:
    CoverSensor(std::shared_ptr<ConfigWrapper> config);
    ~CoverSensor();
    
    double button_handler(double eventtime, bool state);
    json cover_drop_event_handler(double eventtime);
    json get_status(double eventtime) const;

private:
    void exec_gcode(const std::string& prefix, const std::shared_ptr<TemplateWrapper>& template_);
    void handle_ready();
private:
    std::shared_ptr<Printer> printer;
    std::shared_ptr<SelectReactor> reactor;
    std::shared_ptr<GCodeDispatch> gcode;
    std::shared_ptr<ConfigWrapper> config;
    std::shared_ptr<TemplateWrapper> cover_drop_gcode;
    std::shared_ptr<PrinterGCodeMacro> gcode_macro;
    std::shared_ptr<PauseResume> pause_resume;
    std::shared_ptr<WebHooks> webhooks;
    bool is_cover_drop;
    double event_delay;
    double min_event_systime;
};


std::shared_ptr<CoverSensor> cover_switch_sensor_load_config(std::shared_ptr<ConfigWrapper> config);

}
}

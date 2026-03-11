/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2025-09-01 16:39:22
 * @LastEditors  : Ben
 * @LastEditTime : 2025-09-04 10:43:23
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

namespace elegoo
{
namespace extras
{

class TemplateWrapper;
class IdleTimeout;
class PauseResume;


class TanglingSensor
{
public:
    TanglingSensor(std::shared_ptr<ConfigWrapper> config);
    ~TanglingSensor();
    
    double button_handler(double eventtime, bool state);
    json tangling_event_handler(double eventtime);
    json get_status(double eventtime) const;

private:
    void exec_gcode(const std::string& prefix, const std::shared_ptr<TemplateWrapper>& template_);
    void handle_ready();
    void handle_canvas_enable();
    void handle_canvas_disable();
private:
    std::shared_ptr<Printer> printer;
    std::shared_ptr<SelectReactor> reactor;
    std::shared_ptr<GCodeDispatch> gcode;
    std::shared_ptr<ConfigWrapper> config;
    std::shared_ptr<TemplateWrapper> tangling_gcode;
    std::shared_ptr<PrinterGCodeMacro> gcode_macro;
    std::shared_ptr<PauseResume> pause_resume;
    bool is_tangling;
    double event_delay;
    double min_event_systime;
    int canvas_enable;
};


std::shared_ptr<TanglingSensor> tangling_switch_sensor_load_config(std::shared_ptr<ConfigWrapper> config);

}
}

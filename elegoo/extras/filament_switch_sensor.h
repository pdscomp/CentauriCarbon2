/***************************************************************************** 
 * @Author       : Gary
 * @Date         : 2025-02-27 11:49:08
 * @LastEditors  : Ben
 * @LastEditTime : 2025-09-11 20:39:02
 * @Description  : 
 * 
 * Copyright (c) 2025 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/
#pragma once

#include <string>
#include <functional>
#include <memory>
#include"configfile.h"
#include "pause_resume.h"
#include "gcode_macro.h"
#include "buttons.h"

namespace elegoo {
namespace extras {

class Canvas;    
class RunoutHelper {
public:
    RunoutHelper(std::shared_ptr<ConfigWrapper> config);
    void note_filament_present(bool is_filament_present);
    void cmd_QUERY_FILAMENT_SENSOR(std::shared_ptr<GCodeCommand> gcmd);
    void cmd_SET_FILAMENT_SENSOR(std::shared_ptr<GCodeCommand> gcmd);
    json get_status(double eventtime) const;

private:
    void _handle_ready();
    void _handle_canvas_enable();
    void _handle_canvas_disable();
    json _runout_event_handler(double eventtime);
    json _insert_event_handler(double eventtime);
    void _exec_gcode(const std::string& prefix, const std::shared_ptr<TemplateWrapper>& template_);
    
    std::string name;
    std::shared_ptr<Printer> printer;
    std::shared_ptr<SelectReactor> reactor;
    std::shared_ptr<GCodeDispatch> gcode;
    std::shared_ptr<ConfigWrapper> config;
    bool runout_pause;
    std::shared_ptr<PauseResume> pause_resume;
    std::shared_ptr<PrinterGCodeMacro> gcode_macro;
    std::shared_ptr<TemplateWrapper> runout_gcode;
    std::shared_ptr<TemplateWrapper> insert_gcode;
    std::shared_ptr<Canvas> canvas;
    std::string rt_gcode;
    std::string it_gcode;
    double pause_delay;
    double event_delay;
    double min_event_systime;
    bool filament_present;
    int sensor_enabled;
    int canvas_enabled;
};

class SwitchSensor {
public:
    explicit SwitchSensor(std::shared_ptr<ConfigWrapper> config);
    double _button_handler(double eventtime, bool state);
    std::function<json(double)> get_status;
    
private:
    std::shared_ptr<RunoutHelper> runout_helper;
    std::shared_ptr<PrinterButtons> buttons;
    
};

std::shared_ptr<SwitchSensor> switch_sensor_load_config_prefix(std::shared_ptr<ConfigWrapper> config);

}
}
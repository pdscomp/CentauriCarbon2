/*****************************************************************************
 * @Author       : Ben
 * @Date         : 2024-11-27 12:04:25
 * @LastEditors  : loping
 * @LastEditTime : 2025-09-28 20:53:38
 * @Description  : Pause/Resume functionality with position capture/restore
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#pragma once
#include <memory>
#include "json.h"
#include "fan_generic.h"
class Printer;
class ConfigWrapper;
class WebRequest;
class GCodeDispatch;
class GCodeCommand;
class ToolHead;

namespace elegoo {
namespace extras {
class VirtualSD;
class TemplateWrapper;
class PrinterHeaters;
class ControllerFan;
class PrinterHeaterFan;
class PrinterFanGeneric;
class PrinterGCodeMacro;
class GCodeMove;
class Heater;
class PrinterFan;
class Canvas;
class PrinterFanCavity;
class PauseResume
{
public:
    PauseResume(std::shared_ptr<ConfigWrapper> config);
    ~PauseResume();

    void handle_connect();
    void handle_ready();
    json get_status(double eventtime);
    bool is_sd_active();
    void power_outage_resume_pause();
    void save_state(std::string state_name);
    json get_pause_state();
    void resume_state(std::string state_name);
    void send_pause_command();
    void cmd_PAUSE(std::shared_ptr<GCodeCommand> gcmd);
    void send_resume_command();
    bool abnormal_check();
    void cmd_RESUME(std::shared_ptr<GCodeCommand> gcmd);
    void cmd_CLEAR_PAUSE(std::shared_ptr<GCodeCommand> gcmd);
    void cmd_CANCEL_PRINT(std::shared_ptr<GCodeCommand> gcmd);

private:
    void handle_cancel_request(std::shared_ptr<WebRequest> web_request);
    void handle_pause_request(std::shared_ptr<WebRequest> web_request);
    void handle_resume_request(std::shared_ptr<WebRequest> web_request);

private:
    std::shared_ptr<Printer> printer;
    std::shared_ptr<GCodeDispatch> gcode;
    std::shared_ptr<Canvas> canvas;
    std::shared_ptr<VirtualSD> v_sd;
    double recover_velocity;
    bool is_paused;
    bool sd_paused;
    int32_t is_goto_waste_box;
    bool pause_command_sent;
    std::shared_ptr<PrinterGCodeMacro> gcode_macro;
    std::shared_ptr<GCodeMove> gcode_move;
    std::shared_ptr<SelectReactor> reactor;
    std::shared_ptr<TemplateWrapper> after_pause_gcode_macro1;
    std::shared_ptr<TemplateWrapper> after_pause_gcode_macro2;
    std::shared_ptr<TemplateWrapper> before_resume_gcode_macro1;
    std::shared_ptr<TemplateWrapper> before_resume_gcode_macro2;
    std::shared_ptr<TemplateWrapper> before_resume_gcode_macro3;
    std::shared_ptr<TemplateWrapper> cancel_print_gcode_macro1;
    std::shared_ptr<TemplateWrapper> cancel_print_gcode_macro2;
    std::shared_ptr<PrinterHeaters> pheaters;
    std::map<std::string, std::shared_ptr<Heater>> heaters;
    std::shared_ptr<ToolHead> toolhead;
    std::shared_ptr<PrinterFan> fan;
    std::shared_ptr<PrinterFanCavity> generic_box_fan;
    std::shared_ptr<PrinterFanGeneric> generic_fan1;
    json state;
    bool is_cover_drop = false;
    bool is_verify_heater_fault = false;
    bool is_adc_temp_fault = false;
};

std::shared_ptr<PauseResume> pause_resume_load_config(
        std::shared_ptr<ConfigWrapper> config);

}
}
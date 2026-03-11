/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2024-11-12 15:38:06
 * @LastEditors  : Ben
 * @LastEditTime : 2025-05-28 23:35:54
 * @Description  : Support for a heated bed
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "heater_bed.h"
#include "any.h"
namespace elegoo {
namespace extras {

PrinterHeaterBed::PrinterHeaterBed(std::shared_ptr<ConfigWrapper> config)
{
    SPDLOG_INFO(" PrinterHeaterBed Init!");
    printer = config->get_printer();
    std::shared_ptr<PrinterHeaters> pheaters = 
        any_cast<std::shared_ptr<PrinterHeaters>>(printer->load_object(config, "heaters"));

    heater = pheaters->setup_heater(config, "B");

    get_status = [this](double eventtime){
        return heater->get_status(eventtime);};


        
    std::shared_ptr<GCodeDispatch> gcode = 
        any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));
    gcode->register_command("M140", 
    [this](std::shared_ptr<GCodeCommand> gcmd){ 
        cmd_M140(gcmd); 
    });

    gcode->register_command("M190", 
    [this](std::shared_ptr<GCodeCommand> gcmd){ 
        cmd_M190(gcmd); 
    });
    SPDLOG_INFO(" PrinterHeaterBed Init success!");
}

PrinterHeaterBed::~PrinterHeaterBed()
{

}

std::pair<bool, std::string> PrinterHeaterBed::stats(double eventtime)
{
    return heater->stats(eventtime);
}

void PrinterHeaterBed::cmd_M140(std::shared_ptr<GCodeCommand> gcmd, bool wait)
{
    SPDLOG_DEBUG("__func__:{},wait:{}",__func__,wait);
    double temp = gcmd->get_double("S", 0);

    std::shared_ptr<PrinterHeaters> pheaters = 
        any_cast<std::shared_ptr<PrinterHeaters>>(printer->lookup_object("heaters"));

    pheaters->set_temperature(heater, temp, wait);
    SPDLOG_DEBUG("__func__:{},wait:{}",__func__,wait);
}

void PrinterHeaterBed::cmd_M190(std::shared_ptr<GCodeCommand> gcmd, bool wait)
{
    json res;
    res["command"] = "M2202";
    res["result"] = "1405";
    gcmd->respond_feedback(res);
    cmd_M140(gcmd, true);
}



std::shared_ptr<PrinterHeaterBed> heater_bed_load_config(std::shared_ptr<ConfigWrapper> config) {
    return std::make_shared<PrinterHeaterBed>(config);
}

}
}
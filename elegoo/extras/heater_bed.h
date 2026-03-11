/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2024-11-28 20:15:19
 * @LastEditors  : coconut
 * @LastEditTime : 2025-03-18 12:31:10
 * @Description  : Support for a heated bed
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#pragma once
#include <memory>
#include <functional>
#include "json.h"
#include "printer.h"
#include "heaters.h"

class GCodeCommand;
class ConfigWrapper;
class Printer;
namespace elegoo {
namespace extras {

class PrinterHeaters;
class Heater;
class PrinterHeaterBed
{
public:
    PrinterHeaterBed(std::shared_ptr<ConfigWrapper> config);
    ~PrinterHeaterBed();

    void cmd_M140(std::shared_ptr<GCodeCommand> gcmd, bool wait=false);
    void cmd_M190(std::shared_ptr<GCodeCommand> gcmd, bool wait=true);

    std::function<json(double)> get_status; 
    std::pair<bool, std::string> stats(double eventtime);

private:
    std::shared_ptr<Printer> printer;
    std::shared_ptr<Heater> heater;
};


std::shared_ptr<PrinterHeaterBed> heater_bed_load_config(std::shared_ptr<ConfigWrapper> config);

}
}
/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2025-04-11 14:19:13
 * @LastEditors  : loping
 * @LastEditTime : 2025-08-02 18:44:05
 * @Description  : 
 * 
 * Copyright (c) 2025 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/
#pragma once
#include <memory>
#include <map>
#include <set>
#include <string>

class ConfigWrapper;
class GCodeCommand;
class GCodeDispatch;
class Printer;

namespace elegoo
{
namespace extras
{

class TemplateWrapper;
class CanvasDev;

class FilamentLoadUnload
{
public:
    FilamentLoadUnload(std::shared_ptr<ConfigWrapper> config);
    ~FilamentLoadUnload();

    void cmd_M600(std::shared_ptr<GCodeCommand> gcmd);

private:
    void filament_loading_process(std::shared_ptr<GCodeCommand> gcmd);
    void filament_unloading_process(std::shared_ptr<GCodeCommand> gcmd);

private:
    std::shared_ptr<GCodeDispatch> gcode;
    std::shared_ptr<Printer> printer;
    std::shared_ptr<TemplateWrapper> filament_load_gcode_1;
    std::shared_ptr<TemplateWrapper> filament_load_gcode_2;
    std::shared_ptr<TemplateWrapper> filament_unload_gcode_1;
    std::shared_ptr<TemplateWrapper> filament_unload_gcode_2;
};

std::shared_ptr<FilamentLoadUnload> filament_load_unload_load_config(std::shared_ptr<ConfigWrapper> config);


}
}

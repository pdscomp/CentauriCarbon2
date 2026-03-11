/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2025-04-11 14:18:55
 * @LastEditors  : Ben
 * @LastEditTime : 2025-09-06 19:02:26
 * @Description  : 
 * 
 * Copyright (c) 2025 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/
#include "filament_load_unload.h"
#include "filament_switch_sensor.h"
#include "gcode_macro.h"
#include "printer.h"

namespace elegoo
{
namespace extras
{

FilamentLoadUnload::FilamentLoadUnload(std::shared_ptr<ConfigWrapper> config) {

    gcode = any_cast<std::shared_ptr<GCodeDispatch>>(config->get_printer()->lookup_object("gcode"));
    printer = config->get_printer();

    gcode->register_command("M600", 
        [this](std::shared_ptr<GCodeCommand> gcmd){ 
            cmd_M600(gcmd); 
        }); 

    std::shared_ptr<elegoo::extras::PrinterGCodeMacro> gcode_macro
        = any_cast<std::shared_ptr<elegoo::extras::PrinterGCodeMacro>>(config->get_printer()->load_object(config, "gcode_macro"));
    filament_load_gcode_1 = gcode_macro->load_template(config, "filament_load_gcode_1", "\n");
    filament_load_gcode_2 = gcode_macro->load_template(config, "filament_load_gcode_2", "\n");
    filament_unload_gcode_1 = gcode_macro->load_template(config, "filament_unload_gcode_1", "\n");
    filament_unload_gcode_2 = gcode_macro->load_template(config, "filament_unload_gcode_2", "\n");
}

FilamentLoadUnload::~FilamentLoadUnload() {


}

void FilamentLoadUnload::cmd_M600(std::shared_ptr<GCodeCommand> gcmd) {

    int index = gcmd->get_int("S", 0);
    if(index == 1) {
        filament_loading_process(gcmd);
    } else if (index == 2) {
        filament_unloading_process(gcmd);
    }
}

void FilamentLoadUnload::filament_loading_process(std::shared_ptr<GCodeCommand> gcmd) {
    double pre_set_temp = gcmd->get_double("TEMP", 250.);
    filament_load_gcode_1->run_gcode_from_command();

    std::shared_ptr<DummyExtruder> extruder = 
        any_cast<std::shared_ptr<ToolHead>>(printer->lookup_object("toolhead"))->get_extruder();
    std::pair<double, double> temp = extruder->get_heater()->get_temp(0);
    // if(temp.first < (temp.second - 30))
    // {
    //     gcode->run_script_from_command("M109 S" + std::to_string(temp.second));
    // }
    if(temp.first < pre_set_temp)
    {
        gcode->run_script_from_command("M109 S" + std::to_string(pre_set_temp));
    }
    std::shared_ptr<SelectReactor> reactor = printer->get_reactor();
    reactor->pause(get_monotonic() + 2.0);

    json res;
    res["command"] = "M2202";
    res["result"] = "1134";
    gcode->respond_feedback(res);
    
    std::vector<std::shared_ptr<SwitchSensor>> all_sensors;
    std::map<std::string, Any> sensor_map = printer->lookup_objects("filament_switch_sensor");
    for (auto &pair : sensor_map) {
        all_sensors.push_back(any_cast<std::shared_ptr<SwitchSensor>>(pair.second));
    }

    if(all_sensors.size() > 0) {
        std::shared_ptr<SelectReactor> reactor = printer->get_reactor();
        double start_time = get_monotonic();

        while (!printer->is_shutdown()) {
            if (all_sensors[0]->get_status(0)["filament_detected"].get<bool>()) {
                SPDLOG_INFO("Filament detected!");
                break;
            }

            if(get_monotonic() > start_time + 5 * 60) {
                SPDLOG_INFO("Filament detected timeout!");
                break;
            }

            reactor->pause(get_monotonic() + 1.0);
        }
    }
    reactor->pause(get_monotonic() + 2.0);
    filament_load_gcode_2->run_gcode_from_command();
}


void FilamentLoadUnload::filament_unloading_process(std::shared_ptr<GCodeCommand> gcmd) {
    json res;
    res["command"] = "M2202";
    res["result"] = "1142";
    gcode->respond_feedback(res);

    double pre_set_temp = gcmd->get_double("TEMP", 170.);
    std::shared_ptr<DummyExtruder> extruder = 
        any_cast<std::shared_ptr<ToolHead>>(printer->lookup_object("toolhead"))->get_extruder();
    std::pair<double, double> temp = extruder->get_heater()->get_temp(0);
    if(temp.first < pre_set_temp)
    {
        gcode->run_script_from_command("M109 S" + std::to_string(pre_set_temp));
    }
    filament_unload_gcode_1->run_gcode_from_command();  
    filament_unload_gcode_2->run_gcode_from_command();  
}

std::shared_ptr<FilamentLoadUnload> filament_load_unload_load_config(std::shared_ptr<ConfigWrapper> config) {
    std::shared_ptr<FilamentLoadUnload> filament_load_unload = std::make_shared<FilamentLoadUnload>(config);
    return filament_load_unload;
}

}
}
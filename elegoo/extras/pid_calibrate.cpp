/*****************************************************************************
 * @Author       : Louisa
 * @Date         : 2024-11-22 15:58:51
 * @LastEditors  : Ben
 * @LastEditTime : 2025-03-20 12:23:59
 * @Description  : pid_calibrate is a functional module in the Elegoo
 * firmware used to calibrate PID (Proportional-Integral-Derivative) controllers.
 * PID controllers are widely used in 3D printers for temperature control to
 * ensure that heaters (such as the heated bed or extruder) can quickly and
 * accurately reach and maintain the desired temperature. Through pid_calibrate,
 * users can perform an automatic calibration process to optimize PID parameters,
 * thereby improving the accuracy and stability of temperature control.
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "pid_calibrate.h"
#include "printer.h"
#include "heaters.h"
namespace elegoo {
namespace extras {
PIDCalibrate::PIDCalibrate(std::shared_ptr<ConfigWrapper> config)
{
    printer = config->get_printer();
    gcode = any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));

    gcode->register_command("PID_CALIBRATE",
        [this](std::shared_ptr<GCodeCommand> gcmd){

            cmd_PID_CALIBRATE(gcmd);

        }, false, "Run PID calibration test");

    std::shared_ptr<elegoo::extras::PrinterGCodeMacro> gcode_macro
        = any_cast<std::shared_ptr<elegoo::extras::PrinterGCodeMacro>>(config->get_printer()->load_object(config, "gcode_macro"));
    pid_calibrate_gcode = gcode_macro->load_template(config, "pid_calibrate_gcode", "\n");

    SPDLOG_INFO("PIDCalibrate init success!!");
}

PIDCalibrate::~PIDCalibrate()
{

}

void PIDCalibrate::cmd_PID_CALIBRATE(std::shared_ptr<GCodeCommand> gcmd)
{
    pid_calibrate_gcode->run_gcode_from_command();
    std::string heater_name = gcmd->get("HEATER");
    double target = gcmd->get_double("TARGET");
    int write_file = gcmd->get_int("WRITE_FILE", 0);
    json res;
    res["command"] = "pid_calibrate";
    res["result"] = "preheating";
    gcmd->respond_feedback(res);    
    std::shared_ptr<PrinterHeaters> pheaters =
        any_cast<std::shared_ptr<PrinterHeaters>>(printer->lookup_object("heaters"));

    std::shared_ptr<Heater> heater;
    try {
        heater = pheaters->lookup_heater(heater_name);
    } catch (const std::runtime_error& e) {
        res["result"] = "failed";
        gcmd->respond_feedback(res);
        throw elegoo::common::CommandError(e.what());
    }


    any_cast<std::shared_ptr<ToolHead>>(printer->lookup_object("toolhead"))->get_last_move_time();
    std::shared_ptr<ControlAutoTune>  calibrate =
        std::make_shared<ControlAutoTune>(heater, target);

    std::shared_ptr<ControlBase> old_control =
        heater->set_control(calibrate);

    res["result"] = "detecting";
    gcmd->respond_feedback(res); 

    try {
        pheaters->set_temperature(heater, target, true);
    } catch (const std::runtime_error& e) {
        heater->set_control(old_control);
        res["result"] = "failed";
        gcmd->respond_feedback(res);
        throw elegoo::common::CommandError("set_temperature error");
    }
    heater->set_control(old_control);

    if (write_file)
    {
        calibrate->write_file("/tmp/heattest.txt");
    }

    if (calibrate->check_busy(0.0f, 0.0f, 0.0f))
    {
        res["result"] = "failed";
        gcmd->respond_feedback(res);
        throw elegoo::common::CommandError("PID calibration interrupted");
    }

    std::tuple<double, double, double> val = calibrate->calc_final_pid();

    std::cout << "Autotune: final: Kp=" << std::get<0>(val) <<
        " Ki=" << std::get<1>(val) << " Kd=" << std::get<2>(val) << std::endl;

    std::ostringstream info_stream;
    info_stream << "PID parameters: pid_Kp=" << std::get<0>(val)
                << " pid_Ki=" << std::get<1>(val) << " pid_Kd=" << std::get<2>(val) << "\n"
                << "The SAVE_CONFIG command will update the printer config file\n"
                << "with these parameters and restart the printer.";

    gcmd->respond_info(info_stream.str(), true);

    std::string cfgname = heater->get_name();
    std::shared_ptr<PrinterConfig> configfile =
        any_cast<std::shared_ptr<PrinterConfig>>(printer->lookup_object("configfile"));
    configfile->set(cfgname, "control", "pid");
    configfile->set(cfgname, "pid_Kp", std::to_string(std::get<0>(val)));
    configfile->set(cfgname, "pid_Ki", std::to_string(std::get<1>(val)));
    configfile->set(cfgname, "pid_Kd", std::to_string(std::get<2>(val)));
    res["result"] = "completed";
    gcmd->respond_feedback(res);  
    // configfile->cmd_SAVE_CONFIG(gcode->create_gcode_command("SAVE_CONFIG", "SAVE_CONFIG", std::map<std::string, std::string>()));
}

std::shared_ptr<PIDCalibrate> pid_calibrate_load_config(
        std::shared_ptr<ConfigWrapper> config) {
    return std::make_shared<PIDCalibrate>(config);
}


}
}

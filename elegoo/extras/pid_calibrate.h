/***************************************************************************** 
 * @Author       : Louisa
 * @Date         : 2024-11-22 15:58:51
 * @LastEditors  : Ben
 * @LastEditTime : 2025-05-24 11:21:09
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

#pragma once
#include <memory>

class ConfigWrapper;
class ControlAutoTune;
class GCodeCommand;
class Printer;
class GCodeDispatch;

namespace elegoo {
namespace extras {
class Heater;
class TemplateWrapper;
class PIDCalibrate
{
public:
    PIDCalibrate(std::shared_ptr<ConfigWrapper> config);
    ~PIDCalibrate();
    void cmd_PID_CALIBRATE(std::shared_ptr<GCodeCommand> gcmd);

private:
    std::shared_ptr<Printer> printer;
    std::shared_ptr<GCodeDispatch> gcode;
    std::shared_ptr<TemplateWrapper> pid_calibrate_gcode;
};

// class ControlAutoTune  //移至heaters实现
// {
// };

std::shared_ptr<PIDCalibrate> pid_calibrate_load_config(
        std::shared_ptr<ConfigWrapper> config);
}
}

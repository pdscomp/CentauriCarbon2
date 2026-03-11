/***************************************************************************** 
 * @Author       : Louisa
 * @Date         : 2024-11-22 15:58:51
 * @LastEditors  : coconut
 * @LastEditTime : 2025-04-07 13:24:42
 * @Description  : led is a functional module in the Elegoo firmware used 
 * to control the LED lights on the printer. By configuring the led module, 
 * users can achieve various controls over the LED lights, including brightness 
 * adjustment, color changes, and flashing patterns. These features not only 
 * enhance the visual appearance of the printer but can also be used for status 
 * indication and fault diagnosis.
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#pragma once
#include <memory>
#include <functional>
#include <map>
#include "configfile.h"
#include "gcode.h"
#include "printer.h"
#include "display.h"
#include "gcode_macro.h"

class Printer;
class ConfigWrapper;
class GCodeDispatch;
class ReactorTimer;
class PrinterGCodeMacro;
class PrinterDisplayTemplate;
class DisplayTemplate;
class PrinterPins;
struct MacroTemplateContext;
namespace elegoo {
namespace extras {
class LEDHelper
{
public:
    LEDHelper(std::shared_ptr<ConfigWrapper> config,
        std::function<void(std::vector<std::tuple<double, double, double, double>>, 
        double)> update_func, int led_count=1);
    ~LEDHelper();

    int get_led_count();
    void set_color(int index, const std::tuple<double, double, double, double>& color);
    void check_transmit(double print_time);
    void cmd_SET_LED(std::shared_ptr<GCodeCommand> gcmd);
    json get_status(double eventtime=0);
    std::string name;
private:
    std::shared_ptr<Printer> printer;
    std::function<void(std::vector<std::tuple<double, double, double, double>>, 
        double)> update_func;
    std::vector<std::tuple<double, double, double, double>> led_state;
    int led_count;
    bool need_transmit; 
};


class PrinterLED
{
public:
    PrinterLED(std::shared_ptr<ConfigWrapper> config);
    ~PrinterLED();

    std::shared_ptr<LEDHelper> setup_helper(std::shared_ptr<ConfigWrapper> config,
        std::function<void(const std::vector<std::tuple<double, double, double, double>>&, 
        double)> update_func, int led_count=1);
    void cmd_SET_LED_TEMPLATE(std::shared_ptr<GCodeCommand> gcmd);

private:
    void activate_timer();
    void activate_template(std::shared_ptr<LEDHelper> led_helper,
        int index, std::shared_ptr<DisplayTemplate> display_template, 
        const std::map<std::string, std::string>& lparams);
    double render(double eventtime);
    std::vector<std::string> split(const std::string& str, char delimiter);
private:
    std::shared_ptr<Printer> printer;
    std::map<std::string, std::shared_ptr<LEDHelper>> led_helpers;
    std::shared_ptr<ReactorTimer> render_timer;
    // std::function<std::map<std::string, std::function<void()>>(double)> create_template_context;
    std::map<std::string, std::shared_ptr<DisplayTemplate>> templates;
    std::function<MacroTemplateContext(double)> create_template_context;
    std::map<std::pair<std::shared_ptr<LEDHelper>, int>, 
        std::tuple<std::tuple<std::shared_ptr<DisplayTemplate>, std::vector<std::pair<std::string, std::string>>>, 
        std::shared_ptr<DisplayTemplate>, std::map<std::string, std::string>>> active_templates;
    std::string name;
};



class PrinterPWMLED
{
public:
    PrinterPWMLED(std::shared_ptr<ConfigWrapper> config);
    ~PrinterPWMLED();

    void update_leds(
        const std::vector<std::tuple<double, double, double, double>>& led_state,
        double print_time);
    json get_status(double eventtime);
    
private:
    std::shared_ptr<Printer> printer;
    std::shared_ptr<LEDHelper> led_helper;
    std::vector<std::pair<size_t, std::shared_ptr<void>>> pins;
    std::tuple<double, double, double, double> prev_color;
    double last_print_time;
};


std::shared_ptr<PrinterPWMLED> led_load_config_prefix(
    std::shared_ptr<ConfigWrapper>& config);

}
}
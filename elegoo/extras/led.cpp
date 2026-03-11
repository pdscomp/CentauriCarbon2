/***************************************************************************** 
 * @Author       : Louisa
 * @Date         : 2024-11-22 15:58:51
 * @LastEditors  : coconut
 * @LastEditTime : 2025-04-07 13:27:09
 * @Description  : led is a functional module in the Elegoo firmware used 
 * to control the LED lights on the printer. By configuring the led module, 
 * users can achieve various controls over the LED lights, including brightness 
 * adjustment, color changes, and flashing patterns. These features not only 
 * enhance the visual appearance of the printer but can also be used for status 
 * indication and fault diagnosis.
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "led.h"
#include "configfile.h"
#include "gcode.h"
#include "printer.h"
#include "display.h"
#include "gcode_macro.h"
namespace elegoo {
namespace extras {
LEDHelper::LEDHelper(std::shared_ptr<ConfigWrapper> config,
        std::function<void(std::vector<std::tuple<double, double, double, double>>, 
        double)> update_func, int led_count)
    : update_func(update_func), led_count(led_count)
{
    printer = config->get_printer();
    need_transmit = false;

    double red = config->getdouble("initial_RED", 0, 0, 1);
    double green = config->getdouble("initial_GREEN", 0, 0, 1);
    double blue = config->getdouble("initial_BLUE", 0, 0, 1);
    double white = config->getdouble("initial_WHITE", 0, 0, 1);
    
    std::tuple<double, double, double, double> single_led_state = std::make_tuple(red, green, blue, white);
    led_state.assign(led_count, single_led_state);

    std::istringstream iss(config->get_name());
    std::string word;
    while (iss >> word) 
    {
        name = word;
    }
    std::shared_ptr<GCodeDispatch> gcode = any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));
    gcode->register_mux_command("SET_LED", "LED", name,
    [this](std::shared_ptr<GCodeCommand> gcmd){
        cmd_SET_LED(gcmd);
    },
    "Set the color of an LED"
    );
    
}

LEDHelper::~LEDHelper()
{

}


int LEDHelper::get_led_count()
{
    return led_count;
}

void LEDHelper::set_color(int index, const std::tuple<double, double, double, double>& color)
{
    if (index == 0) 
    {
        std::vector<std::tuple<double, double, double, double>> new_led_state(led_count, color);
        if (led_state == new_led_state) 
        {
            return;
        }
        led_state = new_led_state;
    }
    else 
    {
        if (led_state[index - 1] == color) 
        {
            return;
        }
        led_state[index - 1] = color;
    }

    need_transmit = true;
}

void LEDHelper::check_transmit(double print_time)
{
    if (!need_transmit) 
    {
        return;
    }
    need_transmit = false;

    try {
        update_func(led_state, print_time);
    } catch (...) {
        
    }
}

void LEDHelper::cmd_SET_LED(std::shared_ptr<GCodeCommand> gcmd)
{
    double red = gcmd->get_double("RED", 0,0,1);
    double green = gcmd->get_double("GREEN", 0,0,1);
    double blue = gcmd->get_double("BLUE", 0,0,1);
    double white = gcmd->get_double("WHITE",  0,0,1);

    int index = gcmd->get_int("INDEX", INT_NONE,1, led_count);
    int transmit = gcmd->get_int("TRANSMIT", 1);
    int sync = gcmd->get_int("SYNC", 1);

    std::tuple<double, double, double, double> color = {red, green, blue, white};

    auto lookahead_bgfunc = [this, index, color, transmit](double print_time) {
        set_color(index, color);
        if (transmit) 
        {
            check_transmit(print_time);
        }
    };

    if (sync) 
    {
        std::shared_ptr<ToolHead> toolhead = any_cast<std::shared_ptr<ToolHead>>(printer->lookup_object("toolhead"));
        if (toolhead) 
        {
            toolhead->register_lookahead_callback(lookahead_bgfunc);
        }
    } 
    else 
    {
        lookahead_bgfunc(0.0);  // 立即发送更新
    }
}

json LEDHelper::get_status(double eventtime)
{
    json status; 
    json color_data = json::array();

    for (const auto& color : led_state) 
    {
        json color_object;
        color_object["red"] = std::get<0>(color);
        color_object["green"] = std::get<1>(color);
        color_object["blue"] = std::get<2>(color);
        color_object["white"] = std::get<3>(color);
        color_data.push_back(color_object);
    }

    status["color_data"] = color_data;
    return status; 
}


PrinterLED::PrinterLED(std::shared_ptr<ConfigWrapper> config)
{
    printer = config->get_printer();

    std::shared_ptr<PrinterDisplayTemplate> dtemplates
        = lookup_display_templates(config);
    templates = dtemplates->get_display_templates();

    std::shared_ptr<PrinterGCodeMacro> gcode_macro = 
        any_cast<std::shared_ptr<PrinterGCodeMacro>>(printer->lookup_object("gcode_macro"));
    
    create_template_context = [gcode_macro](double eventtime=0){
        return gcode_macro->create_template_context(eventtime);};

    std::shared_ptr<GCodeDispatch> gcode = any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));
    gcode->register_command("SET_LED_TEMPLATE", 
    [this](std::shared_ptr<GCodeCommand> gcmd){ 
        cmd_SET_LED_TEMPLATE(gcmd); 
    }, false, 
    "Assign a display_template to an LED");
}

PrinterLED::~PrinterLED()
{

}


std::shared_ptr<LEDHelper> PrinterLED::setup_helper(std::shared_ptr<ConfigWrapper> config,
    std::function<void(const std::vector<std::tuple<double, double, double, double>>&, 
    double)> update_func, int led_count)
{
    std::shared_ptr<LEDHelper> led_helper = std::make_shared<LEDHelper>(config, update_func, led_count);

    std::istringstream iss(config->get_name());
    std::string word;
    while (iss >> word) 
    {
        name = word;
    }
    
    led_helpers[name] = led_helper;

    return led_helper;
}

void PrinterLED::cmd_SET_LED_TEMPLATE(std::shared_ptr<GCodeCommand> gcmd)
{
    std::string led_name = gcmd->get("LED");
    auto it = led_helpers.find(led_name);
    if (it == led_helpers.end()) 
    {
        // gcmd.error("Unknown LED '" + led_name + "'");
    }

    std::shared_ptr<LEDHelper> led_helper = it->second;
    int led_count = led_helper->get_led_count();

    int index = gcmd->get_int("INDEX", INT_NONE, 1, led_count);

    std::shared_ptr<DisplayTemplate> template_ptr;
    std::map<std::string, std::string> lparams;
    std::string tpl_name = gcmd->get("TEMPLATE");
    if (!tpl_name.empty()) 
    {
        auto tpl_it = templates.find(tpl_name);
        if (tpl_it == templates.end()) 
        {
            // gcmd.error("Unknown display_template '" + tpl_name + "'");
        }
        template_ptr = tpl_it->second;

        std::map<std::string, std::string> tparams = template_ptr->get_params();
        for (const auto& val : gcmd->get_command_parameters()) 
        {
            if (val.first.find("PARAM_") != 0) 
            {
                continue;
            }
            std::string p = val.first.substr(6);  // 移除 "PARAM_" 前缀
            if (tparams.find(p) == tparams.end()) 
            {
                // gcmd.error("Invalid display_template parameter: " + p);
            }

            try {
                lparams[p] = std::stoi(val.second);
            } catch (const std::invalid_argument&) {
                // gcmd.error("Unable to parse '" + value + "' as a literal");
            }
        }
    }

    if (index > 0) 
    {
        activate_template(led_helper, index, template_ptr, lparams);
    } 
    else 
    {
        for (int i = 1; i <= led_count; ++i) 
        {
            activate_template(led_helper, i, template_ptr, lparams);
        }
    }

    activate_timer();
}

void PrinterLED::activate_timer()
{
    if (render_timer != nullptr || active_templates.empty()) 
    {
        return;
    }

    std::shared_ptr<SelectReactor> reactor = printer->get_reactor();
    render_timer = reactor->register_timer(
        [this](double eventtime) { return render(eventtime); }, _NOW, std::string("led ") + name);
}

void PrinterLED::activate_template(std::shared_ptr<LEDHelper> led_helper,
    int index, std::shared_ptr<DisplayTemplate> display_template, 
    const std::map<std::string, std::string>& lparams)
{
    std::pair<std::shared_ptr<LEDHelper>, int> key = std::make_pair(led_helper, index);

    if (display_template) 
    {
        std::vector<std::pair<std::string, std::string>> sortedParams(lparams.begin(), lparams.end());

        std::tuple<std::shared_ptr<DisplayTemplate>, std::vector<std::pair<std::string, std::string>>> uid 
            = std::make_tuple(display_template, sortedParams);

        active_templates[key] = std::make_tuple(uid, display_template, lparams);
        return;
    }

    auto it = active_templates.find(key);
    if (it != active_templates.end()) 
    {
        active_templates.erase(it);
    }
}

double PrinterLED::render(double eventtime)
{
    if (active_templates.empty()) 
    {
        std::shared_ptr<SelectReactor> reactor = printer->get_reactor();
        if(render_timer) {
            reactor->unregister_timer(render_timer);
            render_timer = nullptr;
        }
        return _NEVER;
    }

    MacroTemplateContext context = create_template_context(eventtime);

    auto render = 
    [&](const std::string& name, const std::map<std::string, std::string>& kwargs) -> std::string {
        // Assuming templates_ and context handling is defined
        return templates[name]->render(context, kwargs);
    };
    context.render = render;

    std::map<std::shared_ptr<LEDHelper>, bool> need_transmit;
    std::map<std::tuple<std::shared_ptr<DisplayTemplate>, 
        std::vector<std::pair<std::string, std::string>>>,
        std::tuple<double, double, double, double>> rendered;

    for (const auto& active_template : active_templates) 
    {
        const auto& uid = active_template.first;
        const auto& element = active_template.second;

        auto it = rendered.find(std::get<0>(element));
        std::tuple<double, double, double, double> color;
        if (it == rendered.end()) 
        {
            // Rendering template
            try {
                std::string text = std::get<1>(element)->render(context, std::get<2>(element));
                std::vector<std::string> parts = split(text, ','); // Assuming split function exists
                std::vector<double> color_parts;
                for (const auto& part : parts) 
                {
                    double val = clamp(std::stod(part), 0.0, 1.0);
                    color_parts.push_back(val);
                }

                while (color_parts.size() < 4) 
                {
                    color_parts.push_back(0.0);
                }
                color = std::make_tuple(color_parts[0], color_parts[1], color_parts[2], color_parts[3]);
                rendered[std::get<0>(element)] = color;
            } catch (const std::exception& e) {
                SPDLOG_ERROR("LED template render error:  {}",e.what());
                color = std::make_tuple(0.0, 0.0, 0.0, 0.0);
            }
        } 

        need_transmit[uid.first] = true;
        uid.first->set_color(uid.second, color);
    }

    for (std::pair<std::shared_ptr<LEDHelper>, bool> val : need_transmit) 
    {
        val.first->check_transmit(0);
    }

    return eventtime + 0.5; 
}

std::vector<std::string> PrinterLED::split(const std::string& str, char delimiter) 
{
    // Simple split implementation
    std::vector<std::string> result;
    size_t start = 0;
    size_t end = str.find(delimiter);
    while (end != std::string::npos) {
        result.push_back(str.substr(start, end - start));
        start = end + 1;
        end = str.find(delimiter, start);
    }
    result.push_back(str.substr(start));
    return result;
}

PrinterPWMLED::PrinterPWMLED(std::shared_ptr<ConfigWrapper> config)
{
SPDLOG_INFO("PrinterPWMLED init!");
    printer = config->get_printer();
    std::shared_ptr<PrinterPins> ppins = 
        any_cast<std::shared_ptr<PrinterPins>>(printer->lookup_object("pins"));
    double cycle_time = config->getdouble("cycle_time", 0.01, DOUBLE_NONE, 5, 0);
    bool hardware_pwm = config->getboolean("hardware_pwm", BoolValue::BOOL_FALSE);

    pins.clear();
    std::vector<std::string> color_names = {"red", "green", "blue", "white"};
    for (size_t i = 0; i < color_names.size(); ++i) 
    {
        std::string pin_name = config->get(color_names[i] + "_pin", "");
        if (pin_name.empty()) 
        {
            continue;
        }

        std::shared_ptr<void> mcu_pin = ppins->setup_pin("pwm", pin_name);
        // mcu_pin->setup_max_duration(0.0);
        // mcu_pin->setup_cycle_time(cycle_time, hardware_pwm);
        pins.emplace_back(i, mcu_pin);
    }

    if (!pins.empty()) 
    {
        // ppins.error("No LED pin definitions found in '" + config.get_name() + "'");
    }
    last_print_time = 0;

    std::shared_ptr<PrinterLED> pled = any_cast<std::shared_ptr<PrinterLED>>(printer->load_object(config, "led"));

    pled->setup_helper(config, [this](std::vector<std::tuple<double, double, double, double>> led_state, 
        double print_time) { update_leds(led_state, print_time); }, 1);

    json first_color = led_helper->get_status()["color_data"][0];
    std::tuple<double, double, double, double> color = std::make_tuple(
        first_color["red"].get<double>(),
        first_color["green"].get<double>(),
        first_color["blue"].get<double>(),
        first_color["white"].get<double>()
    );
    prev_color = color;
    for (const std::pair<size_t, std::shared_ptr<void>>& val : pins) 
    {
        // mcu_pin->setup_start_value(std::get<0>(color), 0.0);
    }
SPDLOG_INFO("PrinterPWMLED init success!!");
}

PrinterPWMLED::~PrinterPWMLED()
{

}


void PrinterPWMLED::update_leds(
    const std::vector<std::tuple<double, double, double, double>>& led_state, 
    double print_time)
{
    if (!print_time) 
    {
        double eventtime = get_monotonic();
        // std::shared_ptr<void> mcu = pins[0].second->get_mcu();
        // print_time = mcu->estimated_print_time(eventtime) + 0.1;
    }

    print_time = std::max(print_time, last_print_time + 0.1);

    std::tuple<double, double, double, double> color = led_state[0];

    // 更新引脚状态
    for (size_t idx = 0; idx < pins.size(); ++idx) 
    {
        double prev_val = std::get<0>(prev_color);
        double new_val = std::get<0>(color);

        if (idx == 0) 
        {
            prev_val = std::get<0>(prev_color);
            new_val = std::get<0>(color);
        } 
        else if (idx == 1) 
        {
            prev_val = std::get<1>(prev_color);
            new_val = std::get<1>(color);
        } 
        else if (idx == 2) 
        {
            prev_val = std::get<2>(prev_color);
            new_val = std::get<2>(color);
        } 
        else if (idx == 3) 
        {
            prev_val = std::get<3>(prev_color);
            new_val = std::get<3>(color);
        }

        if (prev_val != new_val) 
        {
            // mcu_pin->set_pwm(print_time, new_val);
        }
    }

    prev_color = color;
}

json PrinterPWMLED::get_status(double eventtime)
{
    return led_helper->get_status(eventtime);
}


std::shared_ptr<PrinterPWMLED> led_load_config_prefix(
    std::shared_ptr<ConfigWrapper>& config) {
  return std::make_shared<PrinterPWMLED>(config);
}



}
}
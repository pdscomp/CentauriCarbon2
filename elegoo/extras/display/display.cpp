/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2024-11-29 20:46:56
 * @LastEditors  : Jack
 * @LastEditTime : 2024-12-16 16:00:25
 * @Description  : 
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "display.h"
#include "printer.h"
#include "gcode_macro.h"

namespace elegoo {
namespace extras {
DisplayTemplate::DisplayTemplate()
{

}

DisplayTemplate::~DisplayTemplate()
{

}

std::map<std::string, std::string> DisplayTemplate::get_params()
{

}

std::string DisplayTemplate::render(
    struct MacroTemplateContext context, const std::map<std::string, std::string>& kwargs)
{

}


DisplayGroup::DisplayGroup()
{

}

DisplayGroup::~DisplayGroup()
{

}

void DisplayGroup::show()
{

}

PrinterDisplayTemplate::PrinterDisplayTemplate(std::shared_ptr<ConfigWrapper> config)
{

}

PrinterDisplayTemplate::~PrinterDisplayTemplate()
{

}


std::map<std::string, std::shared_ptr<DisplayTemplate>> PrinterDisplayTemplate::get_display_templates()
{
    return {};
}

void PrinterDisplayTemplate::get_display_data_groups()
{

}

void PrinterDisplayTemplate::get_display_glyphs()
{

}

void PrinterDisplayTemplate::load_config()
{

}

void PrinterDisplayTemplate::parse_glyph()
{

}

PrinterLCD::PrinterLCD()
{

}

PrinterLCD::~PrinterLCD()
{

}


void PrinterLCD::get_dimensions()
{

}

void PrinterLCD::handle_ready()
{

}

void PrinterLCD::screen_update_event()
{

}

void PrinterLCD::request_redraw()
{

}

void PrinterLCD::draw_text()
{

}

void PrinterLCD::draw_progress_bar()
{

}

void PrinterLCD::cmd_SET_DISPLAY_GROUP()
{

}

std::shared_ptr<PrinterDisplayTemplate> lookup_display_templates(std::shared_ptr<ConfigWrapper> config)
{
    std::shared_ptr<Printer> printer = config->get_printer();
    std::shared_ptr<PrinterDisplayTemplate> dt = 
        any_cast<std::shared_ptr<PrinterDisplayTemplate>>(printer->lookup_object("display_template", nullptr));
    if (dt == nullptr) 
    {
        dt = std::make_shared<PrinterDisplayTemplate>(config);
        printer->add_object("display_template", dt);
    }
    return dt;
}

}
}
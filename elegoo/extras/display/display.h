/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2024-11-11 15:20:29
 * @LastEditors  : Jack
 * @LastEditTime : 2024-12-16 15:59:43
 * @Description  : 
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#pragma once
#include <memory>
#include <string>
#include <map>
#include "json.h"
class ConfigWrapper;
class TemplateWrapper;
namespace elegoo {
namespace extras {

struct MacroTemplateContext;
class DisplayTemplate
{
public:
    DisplayTemplate();
    ~DisplayTemplate();
    std::map<std::string, std::string> get_params();
    std::string render(struct MacroTemplateContext context, const std::map<std::string, std::string>& kwargs);

private:


};

class DisplayGroup
{
public:
    DisplayGroup();
    ~DisplayGroup();
    void show();
};


class PrinterDisplayTemplate
{
public:
    PrinterDisplayTemplate(std::shared_ptr<ConfigWrapper> config);
    ~PrinterDisplayTemplate();

    std::map<std::string, std::shared_ptr<DisplayTemplate>> get_display_templates();
    void get_display_data_groups();
    void get_display_glyphs();
    void load_config();

private:
    void parse_glyph();

};


class PrinterLCD
{
public:
    PrinterLCD();
    ~PrinterLCD();

    void get_dimensions();
    void handle_ready();
    void screen_update_event();
    void request_redraw();
    void draw_text();
    void draw_progress_bar();
    void cmd_SET_DISPLAY_GROUP();
private:

};

std::shared_ptr<PrinterDisplayTemplate> lookup_display_templates(std::shared_ptr<ConfigWrapper> config);

}
}
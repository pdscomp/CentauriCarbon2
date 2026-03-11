/*****************************************************************************
 * @Author       : Ben
 * @Date         : 2024-11-28 20:15:19
 * @LastEditors  : Jack
 * @LastEditTime : 2024-12-17 21:55:23
 * @Description  : Add ability to define custom g-code macros
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#pragma once

#include <memory>
#include <functional>
#include "json.h"
#include <string>
class Printer;
class ConfigWrapper;
class GCodeDispatch;
class GCodeCommand;
namespace elegoo
{
    namespace extras
    {

        struct MacroTemplateContext;
        class PrinterGCodeMacro;
        class GetStatusWrapper
        {
        public:
            GetStatusWrapper(std::shared_ptr<Printer> printer,
                             double eventtime);
            ~GetStatusWrapper();

        private:
            std::string get_item(const std::string &val);
            bool contains(const std::string &val);
            void iterate();

        private:
            std::shared_ptr<Printer> printer;
            double eventtime;
            std::map<std::string, std::string> cache;
        };

        class TemplateWrapper
        {
        public:
            TemplateWrapper(std::shared_ptr<Printer> printer, const std::string &name, const std::string &script);
            ~TemplateWrapper();
            std::string render(MacroTemplateContext *context = nullptr);

            void run_gcode_from_command();
        private:
            std::shared_ptr<GCodeDispatch> gcode;
            std::shared_ptr<Printer> printer;
            std::string name;
            std::string script;
        };

        class PrinterGCodeMacro
        {
        public:
            PrinterGCodeMacro(std::shared_ptr<ConfigWrapper> config);
            ~PrinterGCodeMacro();

            std::shared_ptr<TemplateWrapper> load_template(std::shared_ptr<ConfigWrapper> config,
                                                           const std::string &option, const std::string &default_value = "");
            MacroTemplateContext create_template_context(double eventtime = 0);

        private:
            std::string action_emergency_stop(const std::string &msg = "action_emergency_stop");
            std::string action_respond_info(const std::string &msg);
            void action_raise_error(const std::string &msg);
            std::string action_call_remote_method(const std::string &method,
                                                  const std::map<std::string, std::string> &kwargs);

        private:
            std::shared_ptr<Printer> printer;
        };

        struct MacroTemplateContext
        {
            std::shared_ptr<GetStatusWrapper> printer;
            std::function<void(const std::string &)> action_emergency_stop;
            std::function<void(const std::string &)> action_respond_info;
            std::function<void(const std::string &)> action_raise_error;
            std::function<void(const std::string &, const std::map<std::string, std::string> &)> action_call_remote_method;
            std::function<void(const std::string &, const std::map<std::string, std::string> &)> render;
            std::map<std::string, std::string> params;
            std::string rawparams;
        };

        class GCodeMacro
        {
        public:
            GCodeMacro(std::shared_ptr<ConfigWrapper> config);
            ~GCodeMacro();

            void handle_connect();
            json get_status(double eventtime);
            void cmd_SET_GCODE_VARIABLE(std::shared_ptr<GCodeCommand> gcmd);
            void cmd(std::shared_ptr<GCodeCommand> gcmd);

        private:
            std::vector<std::string> split_string(const std::string &str, char delimiter);
            std::string to_upper(const std::string &str);

        private:
            std::shared_ptr<Printer> printer;
            std::shared_ptr<TemplateWrapper> template_wrapper;
            json variables;
            std::shared_ptr<GCodeDispatch> gcode;
            std::string rename_existing;
            std::string cmd_desc;
            std::string alias;
            bool in_script;
        };
        std::shared_ptr<PrinterGCodeMacro> gcode_macro_load_config(
            std::shared_ptr<ConfigWrapper> config);

        std::shared_ptr<GCodeMacro> gcode_macro_load_config_prefix(
            std::shared_ptr<ConfigWrapper> config);

    }
}

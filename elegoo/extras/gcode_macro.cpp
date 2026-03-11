/*****************************************************************************
 * @Author       : Ben
 * @Date         : 2024-11-28 20:15:19
 * @LastEditors  : coconut
 * @LastEditTime : 2025-03-05 09:42:35
 * @Description  : Add ability to define custom g-code macros
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "gcode_macro.h"
#include "printer.h"
namespace elegoo
{
    namespace extras
    {

        GetStatusWrapper::GetStatusWrapper(
            std::shared_ptr<Printer> printer,
            double eventtime) : printer(printer),
                                eventtime(eventtime)
        {
        }

        GetStatusWrapper::~GetStatusWrapper()
        {
        }

        std::string GetStatusWrapper::get_item(const std::string &val)
        {
            // auto it = cache.find(val);
            // if (it != cache.end())
            // {
            //     return it->second;
            // }

            // // auto po = printer->lookup_object<>(val, nullptr);
            // if (!po || !po->hasGetStatus())
            // {
            //     throw std::out_of_range("Key not found");
            // }

            // if (eventtime == 0)
            // {
            //     eventtime = get_monotonic();
            // }

            // std::string status = po->getStatus(eventtime);
            // cache[val] = status;
            return "";
        }

        bool GetStatusWrapper::contains(const std::string &val)
        {
            try
            {
                get_item(val);
            }
            catch (const std::out_of_range &)
            {
                return false;
            }
            return true;
        }

        void GetStatusWrapper::iterate()
        {
            for (const auto &pair : printer->lookup_objects())
            {
                if (contains(pair.first))
                {
                    std::cout << "Object: " << pair.first << std::endl;
                }
            }
        }

        TemplateWrapper::TemplateWrapper(std::shared_ptr<Printer> printer, const std::string &name, const std::string &script)
        {
            this->name = name;
            this->script = script;
            std::string::size_type pos = 0;
            while ((pos = this->script.find("\\n")) != std::string::npos)
                this->script.replace(pos, 2, "\n");
            gcode = any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));
            // SPDLOG_INFO("TemplateWrapper {}", script);
        }

        TemplateWrapper::~TemplateWrapper()
        {
        }

        std::string TemplateWrapper::render(MacroTemplateContext *context)
        {
            return "";
        }

        void TemplateWrapper::run_gcode_from_command()
        {
            // SPDLOG_INFO("run_gcode_from_command {}", script);
            gcode->run_script_from_command(script);
        }

        PrinterGCodeMacro::PrinterGCodeMacro(std::shared_ptr<ConfigWrapper> config)
        {
            printer = config->get_printer();
        }

        PrinterGCodeMacro::~PrinterGCodeMacro()
        {
        }

        std::shared_ptr<TemplateWrapper> PrinterGCodeMacro::load_template(
            std::shared_ptr<ConfigWrapper> config,
            const std::string &option, const std::string &default_value)
        {
            std::string name = config->get_name() + ":" + option;
            std::string script;
            if (default_value.empty())
                script = config->get(option);
            else
                script = config->get(option, default_value);
            std::shared_ptr<TemplateWrapper> _template = std::make_shared<TemplateWrapper>(printer, name, script);
            return _template;
        }

        MacroTemplateContext PrinterGCodeMacro::create_template_context(double eventtime)
        {
            MacroTemplateContext context;
            context.printer = std::make_shared<GetStatusWrapper>(printer, eventtime);
            context.action_emergency_stop = [this](const std::string &msg = "action_emergency_stop")
            {
                action_emergency_stop(msg);
            };
            context.action_respond_info = [this](const std::string &msg)
            {
                action_respond_info(msg);
            };
            context.action_raise_error = [this](const std::string &msg)
            {
                action_raise_error(msg);
            };
            context.action_call_remote_method = [this](const std::string &method, const std::map<std::string, std::string> &kwargs)
            {
                action_call_remote_method(method, kwargs);
            };
            return context;
        }

        std::string PrinterGCodeMacro::action_emergency_stop(const std::string &msg)
        {
            printer->invoke_shutdown("Shutdown due to " + msg);
            return "";
        }

        std::string PrinterGCodeMacro::action_respond_info(const std::string &msg)
        {
            any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"))->respond_info(msg);
            return "";
        }

        void PrinterGCodeMacro::action_raise_error(const std::string &msg)
        {
            throw elegoo::common::CommandError(msg);
        }

        std::string PrinterGCodeMacro::action_call_remote_method(
            const std::string &method, const std::map<std::string, std::string> &kwargs)
        {
            std::shared_ptr<WebHooks> wh =
                any_cast<std::shared_ptr<WebHooks>>(printer->lookup_object("webhooks"));
            // wh->call_remote_method(method, kwargs);
            return "";
        }

        GCodeMacro::GCodeMacro(std::shared_ptr<ConfigWrapper> config)
        {
            SPDLOG_INFO("GCodeMacro init!");
            std::string section_name = config->get_name();

            if (std::count(section_name.begin(), section_name.end(), ' ') > 2)
            {
                throw std::invalid_argument("Name of section '" + section_name + "' contains illegal whitespace");
            }

            auto name_parts = split_string(section_name, ' ');
            std::string name = name_parts[1];

            alias = to_upper(name);
            printer = config->get_printer();

            std::shared_ptr<PrinterGCodeMacro> gcode_macro =
                any_cast<std::shared_ptr<PrinterGCodeMacro>>(printer->lookup_object("gcode_macro"));

            template_wrapper = gcode_macro->load_template(config, "gcode");
            gcode = any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));
            rename_existing = config->get("rename_existing", "");
            cmd_desc = config->get("description", "G-Code macro");

            if (!rename_existing.empty())
            {
                if (gcode->is_traditional_gcode(alias) != gcode->is_traditional_gcode(rename_existing))
                {
                    throw std::invalid_argument("G-Code macro rename of different types ('" + alias + "' vs '" + rename_existing + "')");
                }
                elegoo::common::SignalManager::get_instance().register_signal(
                    "elegoo:connect",
                    std::function<void()>([this]()
                                          {
                SPDLOG_DEBUG("GCodeMacro connect~~~~~~~~~~~~~~~~~");
                handle_connect();
                SPDLOG_DEBUG("GCodeMacro connect~~~~~~~~~~~~~~~~~ success!"); }));
            }
            else
            {
                gcode->register_command(alias, [this](std::shared_ptr<GCodeCommand> gcmd)
                                        { cmd(gcmd); }, false, cmd_desc);
            }

            gcode->register_mux_command("SET_GCODE_VARIABLE", "MACRO", name, [this](std::shared_ptr<GCodeCommand> gcmd)
                                        { cmd_SET_GCODE_VARIABLE(gcmd); }, "Set the value of a G-Code macro variable");

            in_script = false;
            std::string prefix = "variable_";
            for (const auto &option : config->get_prefix_options(prefix))
            {
                try
                {
                    std::string option_value = config->get(option);

                    json literal;

                    // 解析 JSON 数据
                    try
                    {
                        literal = json::parse(option_value);
                    }
                    catch (const json::parse_error &e)
                    {
                        throw std::invalid_argument("Invalid JSON format");
                    }

                    // 格式化 JSON 数据，去掉空格和换行
                    std::string json_str = literal.dump(-1); // -1 表示不使用缩进

                    // 更新变量
                    variables[option.substr(prefix.size())] = literal;

                    // 输出处理结果
                    std::cout << "Processed option: " << option << "\n";
                    std::cout << "JSON value: " << json_str << std::endl;
                }
                catch (const std::exception &e)
                {
                    // std::string error_msg = "Option '" + option + "' in section '" + config->get_name() + "' is not a valid literal: " + e.what();
                    // config->error(error_msg);
                }
            }
            SPDLOG_INFO("GCodeMacro init success!!");
        }

        GCodeMacro::~GCodeMacro()
        {
        }

        void GCodeMacro::handle_connect()
        {
            std::function<void(std::shared_ptr<GCodeCommand>)> prev_cmd = gcode->register_command(alias, nullptr);

            if (prev_cmd == nullptr)
            {
                SPDLOG_WARN("Existing command '" + alias + "' not found in gcode_macro rename");
                // throw std::runtime_error("Existing command '" + alias + "' not found in gcode_macro rename");
            }

            std::string pdesc = "Renamed builtin of '" + alias + "'";
            gcode->register_command(rename_existing,
                                    prev_cmd, false, pdesc);

            gcode->register_command(alias, [this](std::shared_ptr<GCodeCommand> gcmd)
                                    { cmd(gcmd); }, false, cmd_desc);
        }

        json GCodeMacro::get_status(double eventtime)
        {
            return variables;
        }

        void GCodeMacro::cmd_SET_GCODE_VARIABLE(std::shared_ptr<GCodeCommand> gcmd)
        {
            std::string variable = gcmd->get("VARIABLE");
            std::string value = gcmd->get("VALUE");

            // 检查变量是否存在
            if (!variables.contains(variable))
            {
                // gcmd->error("Unknown gcode_macro variable '" + variable + "'");
            }

            json literal;
            // 使用字符串流解析 JSON 数据
            std::istringstream ss(value);
            try
            {
                literal = json::parse(ss);
            }
            catch (const json::parse_error &e)
            {
                // 处理解析错误
                std::cerr << "Unable to parse '" << value << "' as a literal: " << e.what() << std::endl;
                return;
            }

            // 更新变量
            variables[variable] = literal;

            // 输出更新后的变量值
            std::cout << "Updated variable '" << variable << "' to: " << literal.dump(4) << std::endl;
        }

        void GCodeMacro::cmd(std::shared_ptr<GCodeCommand> gcmd)
        {
            if (in_script)
            {
                // gcmd.error("Macro " + alias + " called recursively");
            }

            MacroTemplateContext context;
            // kwparams.update(self.template.create_template_context())
            context.params = gcmd->get_command_parameters();
            context.rawparams = gcmd->get_raw_command_parameters();
            in_script = true;

            try
            {
                template_wrapper->run_gcode_from_command();
            }
            catch (const std::exception &e)
            {
                std::cerr << "Error: " << e.what() << std::endl;
            }
            in_script = false;
        }

        std::vector<std::string> GCodeMacro::split_string(const std::string &str, char delimiter)
        {
            std::vector<std::string> parts;
            size_t start = 0;
            size_t end = str.find(delimiter);
            while (end != std::string::npos)
            {
                parts.push_back(str.substr(start, end - start));
                start = end + 1;
                end = str.find(delimiter, start);
            }
            parts.push_back(str.substr(start));
            return parts;
        }

        std::string GCodeMacro::to_upper(const std::string &str)
        {
            std::string upper_str = str;
            for (auto &c : upper_str)
            {
                c = std::toupper(c);
            }
            return upper_str;
        }

        std::shared_ptr<PrinterGCodeMacro> gcode_macro_load_config(
            std::shared_ptr<ConfigWrapper> config)
        {
            return std::make_shared<PrinterGCodeMacro>(config);
        }

        std::shared_ptr<GCodeMacro> gcode_macro_load_config_prefix(
            std::shared_ptr<ConfigWrapper> config)
        {
            return std::make_shared<GCodeMacro>(config);
        }

    }
}
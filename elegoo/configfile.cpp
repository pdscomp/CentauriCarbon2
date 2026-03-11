/*****************************************************************************
 * @Author       : Louisa
 * @Date         : 2024-11-25 09:53:16
 * @LastEditors  : loping
 * @LastEditTime : 2025-08-25 15:40:01
 * @Description  : Code for reading and writing the Elegoo config file
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "configfile.h"
#include "printer.h"
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <sstream>
#include <regex>
#include <type_traits>
ConfigWrapper::ConfigWrapper(std::shared_ptr<Printer> printer,
                             std::shared_ptr<CSimpleIniA> fileconfig,
                             std::map<std::pair<std::string, std::string>, std::string> access_tracking,
                             std::string section) : printer(printer),
                                                    fileconfig(fileconfig), access_tracking(access_tracking),
                                                    section(section)
{
    // SPDLOG_INFO("create ConfigWrapper success! section: {}", section);
}

ConfigWrapper::~ConfigWrapper()
{
}

std::shared_ptr<Printer> ConfigWrapper::get_printer()
{
    return printer;
}

std::string ConfigWrapper::get_name()
{
    return section;
}

std::string ConfigWrapper::get(const std::string &option,
                               const std::string &default_value, bool note_valid)
{
    SPDLOG_DEBUG("section:{}, option:{}, default_value:{}", section, option, default_value);
    const char *value = fileconfig->GetValue(section.c_str(), option.c_str(), nullptr);
    if (value == nullptr)
    {
        if (default_value != "_invalid_")
        {
            return default_value;
        }
        else
        {
            SPDLOG_ERROR("Option {} in section {} must be specified!", option, section);
            throw elegoo::common::ConfigParserError(
                "Option " + option + " in section " + section + " must be specified");
            return "";
        }
    }
    return std::string(value);
}

int ConfigWrapper::getint(const std::string &option, int default_value,
                          int minval, int maxval, bool note_valid)
{
    const char *value = fileconfig->GetValue(section.c_str(), option.c_str(), nullptr);
    if (value == nullptr)
    {
        if (default_value != INT_NONE)
        {
            return default_value;
        }
        else
        {
            SPDLOG_ERROR("Option {}  in section {} must be specified!", option, section);
            throw elegoo::common::ConfigParserError(
                "Option " + option + " in section " + section + " must be specified");
        }
    }

    int i_value;
    try
    {
        i_value = std::stoi(value);
    }
    catch (const std::invalid_argument &e)
    {
        SPDLOG_ERROR("Option {} in section {}: cannot be converted to int.", option, section);
    }
    catch (const std::out_of_range &e)
    {
        SPDLOG_ERROR("Option {} in section {}: is too large to be converted to int.", option, section);
    }

    if (minval != INT_NONE && i_value < minval)
    {
        throw elegoo::common::ValueError(
            "Option " + option + " in section " + section +
            " must have minimum of " + std::to_string(minval));
    }

    if (maxval != INT_NONE && i_value > maxval)
    {
        throw elegoo::common::ValueError(
            "Option " + option + " in section " + section +
            " must have maximum of " + std::to_string(maxval));
    }

    return i_value;
}

double ConfigWrapper::getdouble(const std::string &option, double default_value,
                                double minval, double maxval, double above, double below, bool note_valid)
{
    const char *value = fileconfig->GetValue(section.c_str(), option.c_str(), nullptr);
    if (value == nullptr)
    {
        if (!std::isinf(default_value))
        {
            return default_value;
        }
        else
        {
            SPDLOG_ERROR("Option {}  in section {} must be specified!", option, section);
            throw elegoo::common::ConfigParserError(
                "Option " + option + " in section " + section + " must be specified");
        }
    }

    double d_value;
    try
    {
        d_value = std::stod(value);
    }
    catch (const std::invalid_argument &e)
    {
        SPDLOG_ERROR("Option {} in section {}: cannot be converted to double.", option, section);
    }
    catch (const std::out_of_range &e)
    {
        SPDLOG_ERROR("Option {} in section {}: is too large to be converted to double.", option, section);
    }

    if (!std::isnan(minval) && d_value < minval)
    {
        throw elegoo::common::ValueError(
            "Option " + option + " in section " + section +
            " must have minimum of " + std::to_string(minval));
    }

    if (!std::isnan(maxval) && d_value > maxval)
    {
        throw elegoo::common::ValueError(
            "Option " + option + " in section " + section +
            " must have maximum of " + std::to_string(maxval));
    }

    if (!std::isnan(above) && d_value <= above)
    {
        throw elegoo::common::ValueError(
            "Option " + option + " in section " + section +
            " must have above " + std::to_string(above));
    }

    if (!std::isnan(below) && d_value >= below)
    {
        throw elegoo::common::ValueError(
            "Option " + option + " in section " + section +
            " must have below " + std::to_string(below));
    }

    return d_value;
}

bool ConfigWrapper::getboolean(const std::string &option,
                               BoolValue default_value, bool note_valid)
{
    const char *value = fileconfig->GetValue(section.c_str(), option.c_str(), nullptr);
    if (value == nullptr)
    {
        if (default_value != BoolValue::BOOL_NONE)
        {
            return static_cast<int>(default_value) != 0;
        }
        else
        {
            SPDLOG_ERROR("Option {}  in section {} must be specified!", option, section);
            throw elegoo::common::ConfigParserError(
                "Option " + option + " in section " + section + " must be specified");
        }
    }

    return fileconfig->GetBoolValue(section.c_str(), option.c_str());
}

int ConfigWrapper::getintchoice(const std::string &option,
                                int default_value,
                                bool note_valid)
{
    return getint(option, default_value, INT_NONE, INT_NONE, note_valid);
}

std::string ConfigWrapper::getstringchoice(const std::string &option,
                                           const std::string &default_value,
                                           bool note_valid)
{
    return get(option, default_value, note_valid);
}

std::vector<std::string> ConfigWrapper::getlist(const std::string &option,
                                                const std::vector<std::string> &default_value, char sep, bool note_valid)
{
    const char *value = fileconfig->GetValue(section.c_str(), option.c_str(), nullptr);
    if (value == nullptr)
    {
        if (default_value.size() == 1 && default_value.at(0) == "_invalid_")
        {
            throw elegoo::common::ConfigParserError(
                "Option " + option + " in section " + section + " must be specified");
        }
        else
        {
            return default_value;
        }
    }

    std::string str(value);
    std::vector<std::string> vStr = elegoo::common::split(str, std::string(1, sep));
    return vStr;
}

std::vector<int> ConfigWrapper::getintlist(const std::string &option,
                                           const std::vector<int> &default_value, char sep, bool note_valid)
{
    const char *value = fileconfig->GetValue(section.c_str(), option.c_str(), nullptr);
    if (value == nullptr)
    {
        if (!default_value.empty())
        {
            return default_value;
        }
        else
        {
            throw elegoo::common::ConfigParserError(
                "Option " + option + " in section " + section + " must be specified");
        }
    }

    std::string str(value);
    std::vector<std::string> vStr = elegoo::common::split(str, std::string(1, sep));
    std::vector<int> vInt;
    for (const std::string &str : vStr)
    {
        try
        {
            vInt.push_back(std::stoi(str));
        }
        catch (const std::invalid_argument &e)
        {
            SPDLOG_ERROR("Option {} in section {}: cannot be converted to int.", option, section);
        }
        catch (const std::out_of_range &e)
        {
            SPDLOG_ERROR("Option {} in section {}: is too large to be converted to int.", option, section);
        }
    }
    return vInt;
}

std::vector<double> ConfigWrapper::getdoublelist(const std::string &option,
                                                 const std::vector<double> &default_value, char sep, bool note_valid)
{
    const char *value = fileconfig->GetValue(section.c_str(), option.c_str(), nullptr);
    if (value == nullptr)
    {
        if (!default_value.empty())
        {
            return default_value;
        }
        else
        {
            SPDLOG_ERROR("Option " + option + " in section " + section + " must be specified");
            throw elegoo::common::ConfigParserError(
                "Option " + option + " in section " + section + " must be specified");
            return default_value;
        }
    }

    std::string str(value);
    std::vector<std::string> vStr = elegoo::common::split(str, std::string(1, sep));
    std::vector<double> vDouble;
    for (const std::string &str : vStr)
    {
        try
        {
            vDouble.push_back(std::stod(str));
        }
        catch (const std::invalid_argument &e)
        {
            SPDLOG_ERROR("Option {} in section {}: cannot be converted to double.", option, section);
        }
        catch (const std::out_of_range &e)
        {
            SPDLOG_ERROR("Option {} in section {}: is too large to be converted to double.", option, section);
        }
    }
    return vDouble;
}

std::vector<std::pair<int, int>> ConfigWrapper::getintpairs(const std::string &option,
                                                            const std::vector<std::pair<int, int>> &default_value,
                                                            std::vector<std::string> sep, bool note_valid)
{
    const char *value = fileconfig->GetValue(section.c_str(), option.c_str(), nullptr);
    if (value == nullptr)
    {
        if (!default_value.empty())
        {
            return default_value;
        }
        else
        {
            SPDLOG_ERROR("Option " + option + " in section " + section + " must be specified");
            throw elegoo::common::ConfigParserError(
                "Option " + option + " in section " + section + " must be specified");
            return default_value;
        }
    }

    std::vector<std::pair<int, int>> intpairs;
    std::string str(value);
    std::vector<std::string> vStr_1 = elegoo::common::split(str, sep.at(0));
    int first;
    int second;
    for (const std::string &str : vStr_1)
    {
        std::vector<std::string> vStr_2 = elegoo::common::split(str, sep.at(1));
        if (vStr_2.size() == 2)
        {
            try
            {
                first = std::stoi(vStr_2.at(0));
                second = std::stoi(vStr_2.at(1));
            }
            catch (const std::invalid_argument &e)
            {
                SPDLOG_ERROR("Option {} in section {}: cannot be converted to int.", option, section);
            }
            catch (const std::out_of_range &e)
            {
                SPDLOG_ERROR("Option {} in section {}: is too large to be converted to int.", option, section);
            }
            intpairs.push_back(std::make_pair(first, second));
        }
        else
        {
            SPDLOG_ERROR("Unable to parse option " + option + " in section " + section);
            throw elegoo::common::ConfigParserError(
                "Unable to parse option " + option + " in section " + section);
            return default_value;
        }
    }

    return intpairs;
}

std::vector<std::pair<double, double>> ConfigWrapper::getdoublepairs(const std::string &option,
                                                                     const std::vector<std::pair<double, double>> &default_value,
                                                                     std::vector<std::string> sep, bool note_valid)
{
    const char *value = fileconfig->GetValue(section.c_str(), option.c_str(), nullptr);
    if (value == nullptr)
    {
        if (!default_value.empty())
        {
            return default_value;
        }
        else
        {
            SPDLOG_ERROR("Option " + option + " in section " + section + " must be specified");
            throw elegoo::common::ConfigParserError(
                "Option " + option + " in section " + section + " must be specified");
            return default_value;
        }
    }

    std::vector<std::pair<double, double>> doublepairs;
    std::string str(value);
    std::vector<std::string> vStr_1 = elegoo::common::split(str, sep.at(0));
    double first;
    double second;
    for (const std::string &str : vStr_1)
    {
        std::vector<std::string> vStr_2 = elegoo::common::split(str, sep.at(1));
        if (vStr_2.size() == 2)
        {
            try
            {
                first = std::stod(vStr_2.at(0));
                second = std::stod(vStr_2.at(1));
            }
            catch (const std::invalid_argument &e)
            {
                SPDLOG_ERROR("Option {} in section {}: cannot be converted to double.", option, section);
            }
            catch (const std::out_of_range &e)
            {
                SPDLOG_ERROR("Option {} in section {}: is too large to be converted to double.", option, section);
            }
            doublepairs.push_back(std::make_pair(first, second));
        }
        else
        {
            SPDLOG_ERROR("Unable to parse option " + option + " in section " + section);
            throw elegoo::common::ConfigParserError(
                "Unable to parse option " + option + " in section " + section);
            return default_value;
        }
    }

    return doublepairs;
}

std::vector<std::vector<double>> ConfigWrapper::getdoublevectors(const std::string &option,
                                                                 const std::vector<std::vector<double>> &default_value,
                                                                 std::vector<std::string> sep, bool note_valid)
{
    const char *value = fileconfig->GetValue(section.c_str(), option.c_str(), nullptr);
    if (value == nullptr)
    {
        if (!default_value.empty())
        {
            return default_value;
        }
        else
        {
            SPDLOG_ERROR("Option " + option + " in section " + section + " must be specified");
            throw elegoo::common::ConfigParserError(
                "Option " + option + " in section " + section + " must be specified");
            return default_value;
        }
    }

    std::vector<std::vector<double>> doublevectors;
    std::string str(value);
    std::vector<std::string> vStr_1 = elegoo::common::split(str, sep.at(0));

    for (const std::string &str : vStr_1)
    {
        if (!str.empty())
        {
            std::vector<std::string> vStr_2 = elegoo::common::split(str, sep.at(1));
            std::vector<double> vec;
            for (const std::string &str : vStr_2)
            {
                try
                {
                    vec.push_back(std::stod(elegoo::common::strip(str)));
                }
                catch (...)
                {
                    SPDLOG_ERROR("Option {} in section {}: cannot be converted to double.", option, section);
                }
            }
            doublevectors.push_back(vec);
        }
    }

    return doublevectors;
}

std::shared_ptr<ConfigWrapper> ConfigWrapper::getsection(const std::string &section)
{
    return std::make_shared<ConfigWrapper>(printer, fileconfig, access_tracking, section);
}

bool ConfigWrapper::has_section(const std::string &section)
{
    return fileconfig->GetSection(section.c_str()) != nullptr;
}

std::vector<std::shared_ptr<ConfigWrapper>> ConfigWrapper::get_prefix_sections(const std::string &prefix)
{
    std::vector<std::shared_ptr<ConfigWrapper>> sections;
    CSimpleIniA::TNamesDepend sectionNames;

    fileconfig->GetAllSections(sectionNames);
    for (const auto &section : sectionNames)
    {
        std::string section_name = section.pItem;
        if (section_name.rfind(prefix, 0) == 0)
        {
            sections.push_back(getsection(section_name));
        }
    }
    return sections;
}

std::vector<std::string> ConfigWrapper::get_prefix_options(const std::string &prefix)
{
    std::vector<std::string> options;
    CSimpleIniA::TNamesDepend keys;

    fileconfig->GetAllKeys(section.c_str(), keys);

    for (const auto &key : keys)
    {
        std::string option_name = key.pItem;
        if (option_name.rfind(prefix, 0) == 0)
        {
            options.push_back(option_name);
        }
    }

    return options;
}

void ConfigWrapper::deprecate(const std::string &option, const std::string &value)
{
    if (fileconfig->GetValue(section.c_str(), option.c_str(), nullptr))
    {
        return;
    }

    std::string msg;
    if (value.empty())
    {
        msg = "Option '" + option + "' in section '" + section + "' is deprecated.";
    }
    else
    {
        msg = "Value '" + value + "' in option '" + option + "' in section '" + section + "' is deprecated.";
    }
}

std::vector<std::pair<double, double>> ConfigWrapper::getlists(const std::string &option, char *sep, int count, const std::string &default_value, bool note_valid)
{
    return std::vector<std::pair<double, double>>(); // need to accomplishment
}

PrinterConfig::PrinterConfig(std::shared_ptr<Printer> printer) : printer(printer), save_config_pending(false)
{
    std::shared_ptr<GCodeDispatch> gcode = any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));
    gcode->register_command("SAVE_CONFIG",
                            [this](std::shared_ptr<GCodeCommand> gcmd)
                            {
                                cmd_SAVE_CONFIG(gcmd);
                            });
    status_warnings = json::array();
    status_save_pending = json::object();
}

PrinterConfig::~PrinterConfig()
{
}

std::shared_ptr<Printer> PrinterConfig::get_printer()
{
    return printer;
}

std::shared_ptr<ConfigWrapper> PrinterConfig::read_config(const std::string &filename)
{
    return build_config_wrapper(read_config_file(filename), filename);
}

std::shared_ptr<ConfigWrapper> PrinterConfig::read_main_config()
{
    std::string filename = printer->get_start_args()["config_file"];
    std::string autosave_filename = printer->get_start_args()["autosave_file"];

    // printf("read_main_config #0\n");
    // 读取二进制文件为字符串
    std::string data = read_config_file(filename);
    // 访问是否存在,不存在就新建一个
    if (access(autosave_filename.c_str(), F_OK) != 0)
    {
        FILE *fp = fopen(autosave_filename.c_str(), "wb+");
        fclose(fp);
        chmod(autosave_filename.c_str(), 0777);
    }
    std::string auto_savedata = read_config_file(autosave_filename);
    data += auto_savedata;

    // printf("read_main_config #1\n");
    // 切分数据为常规数据与自动保存数据
    std::pair<std::string, std::string> pair_data = find_autosave_data(data);
    // printf("read_main_config #2\n");
    // 创建常规配置
    std::shared_ptr<ConfigWrapper> regular_config = build_config_wrapper(pair_data.first, filename);
    // printf("read_main_config #3\n");
    std::string autosave_data = strip_duplicates(pair_data.second, regular_config);
    // printf("read_main_config #4\n");
    // 创建自动保存配置
    autosave = build_config_wrapper(pair_data.second, filename);
    // printf("read_main_config #5\n");
    std::shared_ptr<ConfigWrapper> cfg = build_config_wrapper(pair_data.first + pair_data.second, filename);
    // printf("read_main_config #6\n");
    build_status(cfg);
    return cfg;
}

void PrinterConfig::check_unused_options(std::shared_ptr<ConfigWrapper> config)
{
    std::shared_ptr<CSimpleIniA> fileconfig = config->fileconfig;
    auto objects = printer->lookup_objects();
    // auto sections = autosave->fileconfig->sections();
    build_status(config);
}

void PrinterConfig::log_config()
{
    // 还没定好日记库， 暂时不实现
}

void PrinterConfig::runtime_warning(const std::string &msg)
{
    std::cerr << "Warning: " << msg << std::endl;
    // json res = {{"type", "runtime_warning"}, {"message", msg}};
    // runtime_warnings.push_back(res);
    // status_warnings = runtime_warnings;
    // status_warnings.update(deprecate_warnings);
}

void PrinterConfig::deprecate(const std::string &section, const std::string &option,
                              const std::string &value, const std::string &msg)
{
    std::tuple<std::string, std::string, std::string> key = std::make_tuple(section, option, value);
    deprecated[key] = msg;
}

json PrinterConfig::get_status(double eventtime)
{
    json status;
    status["config"] = this->status_raw_config;
    status["settings"] = this->status_raw_config;
    // status["settings"] = this->status_settings;
    status["warnings"] = this->status_warnings;
    status["save_config_pending"] = this->save_config_pending;
    status["save_config_pending_items"] = this->status_save_pending;
    return status;
}

void PrinterConfig::set(const std::string &section, const std::string &option,
                        const std::string &value)
{
    // status_save_pending 实际上没有什么意义，只是用来记录当前哪些配置是动态变更的
    // 添加段
    if (autosave->fileconfig->GetSection(section.c_str()) == nullptr)
        autosave->fileconfig->SetValue(section.c_str(), option.c_str(), value.c_str());
    else
        autosave->fileconfig->SetValue(section.c_str(), option.c_str(), value.c_str());
    if (status_save_pending.find(section) == status_save_pending.end())
        status_save_pending[section] = std::map<std::string, std::string>(); // 创建新节
    status_save_pending[section][option] = value;
    save_config_pending = true;
    SPDLOG_INFO("save_config: set [{}] {} {}", section, option, value);
}

void PrinterConfig::remove_section(std::string section)
{
    if (autosave->fileconfig->GetSection(section.c_str()) != nullptr)
    {
        autosave->fileconfig->Delete(section.c_str(), nullptr);
        status_save_pending.erase(section);
        save_config_pending = true;
    }
    else if (status_save_pending.find(section) != status_save_pending.end())
    {
        status_save_pending.erase(section);
        save_config_pending = true;
    }
}

const std::string AUTOSAVE_HEADER = R"(
#*# <---------------------- SAVE_CONFIG ---------------------->
#*# DO NOT EDIT THIS BLOCK OR BELOW. The contents are auto-generated.
#*#)";

void PrinterConfig::cmd_SAVE_CONFIG(std::shared_ptr<GCodeCommand> gcmd)
{
    std::shared_ptr<GCodeDispatch> gcode = any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));
    gcode->run_script_from_command("M400");
    std::string autosave_data = build_config_string(autosave);
    std::vector<std::string> lines;
    std::istringstream iss(autosave_data);
    std::string line;
    while (std::getline(iss, line))
    {
        std::string processed_line = trim("#*# " + line);
        lines.push_back(processed_line);
    }
    std::string header = "\n\n" + trim(AUTOSAVE_HEADER);
    lines.insert(lines.begin(), header);
    lines.push_back("");
    autosave_data = join(lines);

    // 读取自动保存文件
    std::string cfgname = printer->get_start_args()["autosave_file"];
    // 访问是否存在,不存在就新建一个
    if (access(cfgname.c_str(), F_OK) != 0)
    {
        FILE *fp = fopen(cfgname.c_str(), "wb+");
        chmod(cfgname.c_str(), 0777);
        fclose(fp);
    }

    std::string data = read_config_file(cfgname);
    std::pair<std::string, std::string> pair_data = find_autosave_data(data);
    std::shared_ptr<ConfigWrapper> config = build_config_wrapper(pair_data.first, cfgname);
    pair_data.first = strip_duplicates(pair_data.first, autosave);
    data = trim(pair_data.first, false) + autosave_data;
    std::string backup_name = cfgname + "_backup";
    std::string temp_name = cfgname + "_temp";
    // 检查文件名是否以 ".cfg" 结尾
    if (cfgname.size() >= 4 && cfgname.substr(cfgname.size() - 4) == ".cfg")
    {
        backup_name = cfgname.substr(0, cfgname.size() - 4) + "_backup.cfg";
        temp_name = cfgname.substr(0, cfgname.size() - 4) + "_temp.cfg";
    }
    SPDLOG_INFO("CFG {} BACKUP {} TEMP {}", cfgname, backup_name, temp_name);

    try
    {
        std::ofstream temp_file(temp_name);
        if (!temp_file.is_open())
        {
            throw std::runtime_error("Unable to open temp file for writing");
        }
        temp_file << data;
        temp_file.close();
        chmod(temp_name.c_str(), 0777);

        // 重命名原始配置文件为备份文件
        if (std::rename(cfgname.c_str(), backup_name.c_str()) != 0)
        {
            throw std::runtime_error("Unable to rename original config file to backup");
        }
        // 重命名临时文件为配置文件
        if (std::rename(temp_name.c_str(), cfgname.c_str()) != 0)
        {
            throw std::runtime_error("Unable to rename temp file to config file");
        }
    }
    catch (const std::exception &e)
    {
        // 打印错误消息并抛出异常
        std::cerr << "Exception during SAVE_CONFIG: " << e.what() << std::endl;
        throw std::runtime_error("Unable to write config file during SAVE_CONFIG");
    }
}

std::string PrinterConfig::read_config_file(const std::string &filename)
{
    std::ifstream file(filename);
    if (!file.is_open())
    {
        std::string msg = "Unable to open config file " + filename;
        std::cerr << msg << std::endl; // 输出错误信息
        throw std::runtime_error(msg);
    }

    std::stringstream buffer;
    buffer << file.rdbuf(); // 读取文件内容
    std::string data = buffer.str();
    file.close();

    // 使用正则表达式替换所有 \r\n 为 \n
    data = std::regex_replace(data, std::regex("\r\n"), "\n");
    return data;
}

std::pair<std::string, std::string> PrinterConfig::find_autosave_data(const std::string &data)
{
    std::string regular_data = data;
    std::string autosave_data;
    size_t pos = data.find(AUTOSAVE_HEADER);
    // 将配置分成常规配置与自动保存配置
    if (pos != std::string::npos)
    {
        regular_data = data.substr(0, pos);
        autosave_data = strip(data.substr(pos + AUTOSAVE_HEADER.length()));
    }

    // 检查错误并去掉行前缀
    if (regular_data.find("\n#*# ") != std::string::npos)
    {
        std::cerr << "Warning: Can't read autosave from config file - autosave state corrupted" << std::endl;
        return {data, ""};
    }
    std::vector<std::string> lines = elegoo::common::split(autosave_data, "\n");
    std::vector<std::string> out;
    for (const auto &line : lines)
    {
        if ((!(line.substr(0, 3) == "#*#") || (line.length() >= 4 && line.substr(0, 4) != "#*# ")) && !autosave_data.empty())
        {
            std::cerr << "Can't read autosave from config file - modifications after header" << std::endl;
            return {regular_data, ""};
        }
        out.push_back(line.size() > 4 ? line.substr(4) : ""); // 去掉前4个字符
    }

    out.push_back(""); // 添加额外的空行
    return {regular_data, elegoo::common::join(out, "\n")};
}

std::string PrinterConfig::strip_duplicates(const std::string &data, std::shared_ptr<ConfigWrapper> config)
{
    std::vector<std::string> lines = elegoo::common::split(data, "\n");

    std::regex comment_r{R"([#;].*$)"};
    std::regex value_r{R"([^A-Za-z0-9_].*$)"};

    std::string section;
    bool is_dup_field = false;

    for (size_t lineno = 0; lineno < lines.size(); ++lineno)
    {
        // printf("lines: %s\n", lines[lineno].c_str());
        std::string pruned_line = std::regex_replace(lines[lineno], comment_r, "");
        pruned_line = elegoo::common::strip(pruned_line);
        // printf("pruned_line: %s\n", pruned_line.c_str());
        if (pruned_line.empty())
        {
            continue;
        }
        // if (pruned_line[0] == ' ' || pruned_line[0] == '\t') {  // 如果是缩进行
        //     if (is_dup_field) {
        //         lines[lineno] = "#" + lines[lineno];  // 注释掉该行
        //     }
        //     continue;
        // }

        is_dup_field = false;
        if (pruned_line[0] == '[')
        {
            section = elegoo::common::strip(pruned_line.substr(1, pruned_line.size() - 2));
            // printf("section %s\n", section.c_str());
            continue;
        }

        // 查找地一个"="来获取名称，而不是通过正则表达式
        // std::string field = std::regex_replace(pruned_line, value_r, "");
        size_t start = pruned_line.find("=");
        std::string field = (start == std::string::npos) ? "" : pruned_line.substr(0, start);

        // printf("field %s\n", field.c_str());
        if (config->fileconfig->GetValue(section.c_str(), field.c_str(), nullptr))
        {
            is_dup_field = true;
            lines[lineno] = "#" + lines[lineno]; // 注释掉该行
            // printf("lines2: %s\n", lines[lineno].c_str());
        }
    }

    return elegoo::common::join(lines, "\n");
}

void PrinterConfig::parse_config_buffer(std::vector<std::string> &buffer,
                                        const std::string &filename, std::shared_ptr<CSimpleIniA> fileconfig)
{
    if (buffer.empty())
    {
        return;
    }
    std::string data = elegoo::common::join(buffer, "\n");
    buffer.clear();
    SI_Error rc = fileconfig->LoadData(data);
    if (rc < 0)
    {
        std::cerr << "Failed to load INI data!" << std::endl;
        return;
    }
}

void PrinterConfig::resolve_include(const std::string &source_filename,
                                    const std::string &include_spec, std::shared_ptr<CSimpleIniA> fileconfig,
                                    std::vector<std::string> &visited)
{
    std::string dirname = getDirectoryName(source_filename);
    // std::string include_spec = trim(include_spec);
    std::string include_glob = joinPaths(dirname, include_glob);
}

void PrinterConfig::parse_config(const std::string &data, const std::string &filename,
                                 std::shared_ptr<CSimpleIniA> fileconfig, std::vector<std::string> &visited)
{
    std::string path = getDirectoryName(filename);
    auto it = std::find(visited.begin(), visited.end(), path);
    if (it != visited.end())
    {
        throw std::runtime_error("Recursive include of config file '" + filename + "'");
    }
    std::vector<std::string> lines = elegoo::common::split(data, "\n");
    std::vector<std::string> out;
    std::regex pattern(R"(\[([^\]]+)\])", std::regex::optimize);

    for (auto line : lines)
    {
        std::size_t pos = line.find('#');
        if (pos != std::string::npos)
        {
            line = line.substr(0, pos);
        }
        std::smatch match;
        bool result = std::regex_search(line, match, pattern);
        std::string header = match[1];
        if (result && header.find("include ") == 0)
        {
            parse_config_buffer(out, filename, fileconfig);
            std::string include_spec = elegoo::common::strip(header.substr(8));
            resolve_include(filename, include_spec, fileconfig, visited);
        }
        else
        {
            out.push_back(line);
        }
    }

    parse_config_buffer(out, filename, fileconfig);
    it = std::find(visited.begin(), visited.end(), path);
    if (it != visited.end())
    {
        visited.erase(it);
    }
}

std::shared_ptr<ConfigWrapper> PrinterConfig::build_config_wrapper(
    const std::string &data, const std::string &filename)
{
    std::shared_ptr<CSimpleIniA> fileconfig = std::make_shared<CSimpleIniA>();
    std::vector<std::string> visited;
    parse_config(data, filename, fileconfig, visited);
    std::map<std::pair<std::string, std::string>, std::string> access_tracking;
    return std::make_shared<ConfigWrapper>(printer, fileconfig, access_tracking, "printer");
}

std::string PrinterConfig::build_config_string(std::shared_ptr<ConfigWrapper> config)
{
    std::string sstream;
    config->fileconfig->Save(sstream);
    return trim(sstream);
}

void PrinterConfig::build_status(std::shared_ptr<ConfigWrapper> config)
{
    status_raw_config.clear();
    auto sections = config->get_prefix_sections("");
    for (auto section : sections)
    {
        status_raw_config[section->get_name()] = json::object();
        auto options = section->get_prefix_options("");
        for (auto option : options)
        {
            status_raw_config[section->get_name()][option] = section->get(option, "", false);
        }
    }

    // status_settings.clear();
    // for (auto s : config->access_tracking)
    // {
    //     if (!status_settings.contains(s.first.first))
    //     {
    //         status_settings[s.first.first][s.first.second] = s.second;
    //     }
    // }

    // deprecate_warnings = json::array();
    // json res;
    // for (auto it : deprecated)
    // {
    //     res.clear();
    //     if (std::get<2>(it.first).empty())
    //     {
    //         res = {"type", "deprecated_option"};
    //     }
    //     else
    //     {
    //         res = {{"type", "deprecated_option"}, {"value", it.second}};
    //     }
    //     res["message"] = it.second;
    //     res["section"] = std::get<0>(it.first);
    //     res["option"] = std::get<1>(it.first);
    //     deprecate_warnings.push_back(res);
    // }

    // status_warnings = runtime_warnings;
    // status_warnings.update(deprecate_warnings);
}

void PrinterConfig::disallow_include_conflicts(const std::string &regular_data,
                                               const std::string &cfgname, std::shared_ptr<GCodeDispatch> gcode)
{
    std::shared_ptr<ConfigWrapper> config = build_config_wrapper(regular_data, cfgname);
}

std::string PrinterConfig::trim(const std::string &str,
                                bool remove_front, bool remove_back)
{
    size_t start = 0;
    size_t end = str.length();

    if (remove_front)
    {
        while (start < end && std::isspace(str[start]))
        {
            ++start;
        }
    }

    if (remove_back)
    {
        while (end > start && std::isspace(str[end - 1]))
        {
            --end;
        }
    }

    // 返回处理后的子字符串
    return str.substr(start, end - start);
}

std::string PrinterConfig::join(const std::vector<std::string> &lines)
{
    std::ostringstream oss;
    for (const auto &line : lines)
    {
        oss << line << "\n";
    }
    return oss.str();
}

std::string PrinterConfig::getDirectoryName(const std::string &filepath)
{
    size_t pos = filepath.find_last_of("/\\");
    return (pos == std::string::npos) ? "" : filepath.substr(0, pos);
}

std::string PrinterConfig::joinPaths(const std::string &dir, const std::string &file)
{
    // 确保目录末尾有一个斜杠
    std::string joined = dir;
    if (!joined.empty() && joined.back() != '/' && joined.back() != '\\')
    {
        joined += '/';
    }
    joined += file;
    return joined;
}

std::string PrinterConfig::strip(const std::string &str)
{
    std::string result = str;
    result.erase(result.begin(), std::find_if(result.begin(), result.end(), [](unsigned char ch)
                                              { return !std::isspace(ch); }));
    result.erase(std::find_if(result.rbegin(), result.rend(), [](unsigned char ch)
                              { return !std::isspace(ch); })
                     .base(),
                 result.end());
    return result;
}

// 字符串分割函数，类似于 Python 的 split
std::vector<std::string> PrinterConfig::split(const std::string &str, char delimiter)
{
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (std::getline(tokenStream, token, delimiter))
    {
        tokens.push_back(strip(token)); // 去掉首尾空格
    }
    return tokens;
}

std::string PrinterConfig::get_current_time_str()
{
    std::time_t t = std::time(nullptr);
    char buffer[20];
    std::strftime(buffer, sizeof(buffer), "-%Y%m%d_%H%M%S", std::localtime(&t));
    return std::string(buffer);
}
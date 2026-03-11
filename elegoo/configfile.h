/***************************************************************************** 
 * @Author       : Louisa
 * @Date         : 2024-11-25 09:53:18
 * @LastEditors  : Ben
 * @LastEditTime : 2025-04-19 17:21:52
 * @Description  : Code for reading and writing the Elegoo config file
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#pragma once
#include <memory>
#include <map>
#include <unordered_map>
#include <vector>
#include <string>
#include <set>
#include <functional>
#include <limits>
#include "SimpleIni.h"
#include "gcode.h"

class Printer;


class ConfigWrapper
{
public:
    ConfigWrapper(std::shared_ptr<Printer> printer,
        std::shared_ptr<CSimpleIniA> fileconfig,
        std::map<std::pair<std::string, std::string>, std::string> access_tracking,
        std::string section);
    ~ConfigWrapper();

    std::shared_ptr<Printer> get_printer();

    std::string get_name();

    std::string get(const std::string& option, 
        const std::string& default_value="_invalid_", 
        bool note_valid=true);

    int getint(const std::string& option, 
        int default_value=INT_NONE, 
        int minval=INT_NONE, 
        int maxval=INT_NONE, 
        bool note_valid=true);

    double getdouble(const std::string& option, 
        double default_value=DOUBLE_INVALID, 
        double minval=DOUBLE_NONE, 
        double maxval=DOUBLE_NONE, 
        double above=DOUBLE_NONE, 
        double below=DOUBLE_NONE, 
        bool note_valid=true);

    bool getboolean(const std::string& option, 
        BoolValue default_value=BoolValue::BOOL_NONE, 
        bool note_valid=true);

    int getintchoice(const std::string& option,
        int default_value=std::numeric_limits<int>::min(), 
        bool note_valid=true);

    std::string getstringchoice(const std::string& option, 
        const std::string& default_value="_invalid_", 
        bool note_valid=true);

    std::vector<std::string> getlist(const std::string& option, 
        const std::vector<std::string>& default_value={"_invalid_"}, char sep=',', bool note_valid = true);
    std::vector<int> getintlist(const std::string& option, 
        const std::vector<int>& default_value={},
        char sep=',', bool note_valid = true);
    std::vector<double> getdoublelist(const std::string& option, 
        const std::vector<double>& default_value={},
        char sep=',', bool note_valid = true);
    std::vector<std::pair<int, int>> getintpairs(const std::string& option, 
        const std::vector<std::pair<int, int>>& default_value={},
        std::vector<std::string> sep={",",":"}, bool note_valid = true);
    std::vector<std::pair<double, double>> getdoublepairs(const std::string& option, 
        const std::vector<std::pair<double, double>>& default_value={}, 
        std::vector<std::string> sep={",",":"}, bool note_valid = true);
    std::vector<std::vector<double>> getdoublevectors(const std::string& option, 
        const std::vector<std::vector<double>>& default_value={},
        std::vector<std::string> sep={" ",","}, bool note_valid = true);        
    std::shared_ptr<ConfigWrapper> getsection(const std::string& section);
    bool has_section(const std::string& section);
    std::vector<std::shared_ptr<ConfigWrapper>> get_prefix_sections(const std::string& prefix);
    std::vector<std::string> get_prefix_options(const std::string& prefix);
    void deprecate(const std::string& option, const std::string& value="");
    std::shared_ptr<CSimpleIniA> fileconfig;
    std::map<std::pair<std::string, std::string>, std::string> access_tracking;
    std::vector<std::pair<double, double>> getlists(const std::string& option, 
        char* sep, int count, const std::string& default_value="", bool note_valid = true);
private:
    std::shared_ptr<Printer> printer;
    std::string section;
};

struct Warning {
    std::string type;
    std::string message;
    std::string section;
    std::string option;
    std::string value;
};

struct Status{
    std::vector<Warning> runtime_warnings;
    std::vector<Warning> deprecate_warnings;
    std::vector<Warning> status_warnings;
    std::map<std::string, std::string> status_raw_config;
    std::map<std::string, std::map<std::string, std::string>> status_save_pending; 
};

class PrinterConfig
{
public:
    PrinterConfig(std::shared_ptr<Printer> printer);
    ~PrinterConfig();

    std::shared_ptr<Printer> get_printer();
    std::shared_ptr<ConfigWrapper> read_config(const std::string& filename);
    std::shared_ptr<ConfigWrapper> read_main_config();
    void check_unused_options(std::shared_ptr<ConfigWrapper> config);
    void log_config();
    void runtime_warning(const std::string& msg);
    void deprecate(const std::string& section, const std::string& option, 
        const std::string& value = "", const std::string& msg = "");
    json get_status(double eventtime);
    void set(const std::string& section, const std::string& option, 
        const std::string& value);
    void remove_section(std::string section);
    void cmd_SAVE_CONFIG(std::shared_ptr<GCodeCommand> gcmd);

private:
    std::string read_config_file(const std::string& filename);
    std::pair<std::string, std::string> find_autosave_data(const std::string& data);
    std::string strip_duplicates(const std::string& data, 
        std::shared_ptr<ConfigWrapper> config);
    void parse_config_buffer(std::vector<std::string>& buffer, 
        const std::string& filename, std::shared_ptr<CSimpleIniA> fileconfig);
    void resolve_include(const std::string& source_filename, 
        const std::string& include_spec, std::shared_ptr<CSimpleIniA> fileconfig,
        std::vector<std::string>& visited);
    void parse_config(const std::string& data, const std::string& filename,
        std::shared_ptr<CSimpleIniA> fileconfig, std::vector<std::string>& visited);
    std::shared_ptr<ConfigWrapper> build_config_wrapper(const std::string& data, 
        const std::string& filename);
    std::string build_config_string(std::shared_ptr<ConfigWrapper> config);
    void build_status(std::shared_ptr<ConfigWrapper> config);
    void disallow_include_conflicts(const std::string& regular_data, 
        const std::string& cfgname, std::shared_ptr<GCodeDispatch> gcode);
    std::string trim(const std::string& str,
        bool remove_front = true, bool remove_back = true);
    std::string join(const std::vector<std::string>& lines);
    std::string getDirectoryName(const std::string& filepath);
    std::string joinPaths(const std::string& dir, const std::string& file);

    std::string strip(const std::string& str);
    std::vector<std::string> split(const std::string& str, char delimiter);
    std::string get_current_time_str();
private:
    std::shared_ptr<Printer> printer;
    std::shared_ptr<ConfigWrapper> autosave;
    std::map<std::tuple<std::string, std::string, std::string>, std::string> deprecated;
    json runtime_warnings;
    json deprecate_warnings;
    json status_warnings;
    json status_settings;
    json status_raw_config;
    std::map<std::string, std::map<std::string, std::string>> status_save_pending;
    bool save_config_pending;
};
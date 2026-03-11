/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2025-02-24 20:58:52
 * @LastEditors  : Ben
 * @LastEditTime : 2025-02-25 10:40:45
 * @Description  : 
 * 
 * Copyright (c) 2025 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#pragma once

#include <memory>
#include <functional>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include "json.h"

class Printer;
class ToolHead;
class ConfigWrapper;
class ReactorTimer;

namespace elegoo {
namespace extras {


class PrinterSysStats
{
public:
    PrinterSysStats(std::shared_ptr<ConfigWrapper> config);
    ~PrinterSysStats();

    std::pair<bool, std::string> stats(double eventtime);
    json get_status(double eventtime);
private:
    void disconnect();

private:
    double last_process_time;
    double total_process_time;
    double last_load_avg;
    int last_mem_avail;
    std::ifstream mem_file;
};


class PrinterStats
{
public:
    PrinterStats(std::shared_ptr<ConfigWrapper> config);
    ~PrinterStats();

    void handle_ready();
    double generate_stats(double eventtime);

private:
    std::shared_ptr<Printer> printer;
    std::shared_ptr<ReactorTimer> stats_timer;
    std::vector<std::function<std::pair<bool, std::string>(double)>> stats_cb;
};

std::shared_ptr<PrinterStats> statistics_load_config(std::shared_ptr<ConfigWrapper> config);


}
}
/*****************************************************************************
 * @Author       : Ben
 * @Date         : 2025-02-24 20:58:29
 * @LastEditors  : coconut
 * @LastEditTime : 2025-03-18 17:17:10
 * @Description  :
 *
 * Copyright (c) 2025 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "statistics.h"
#include "printer.h"
#include "extras_factory.h"
#include <stdlib.h>

namespace elegoo {
namespace extras {

PrinterSysStats::PrinterSysStats(std::shared_ptr<ConfigWrapper> config) {
    std::shared_ptr<Printer> printer = config->get_printer();

    last_process_time = 0;
    total_process_time = 0;
    last_load_avg = 0;
    last_mem_avail = 0;

    try {
        mem_file.open("/proc/meminfo", std::ios::in);
        if (!mem_file.is_open()) {
            throw std::runtime_error("Failed to open /proc/meminfo");
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    elegoo::common::SignalManager::get_instance().register_signal(
        "elegoo:disconnect",
        std::function<void()>([this](){
            disconnect();
        })
    );
}

PrinterSysStats::~PrinterSysStats() {

}

std::pair<bool, std::string> PrinterSysStats::stats(double eventtime) {

    // 获取CPU时间
    double ptime = static_cast<double>(std::clock()) / CLOCKS_PER_SEC; // 获取CPU时间
    double pdiff = ptime - last_process_time;
    last_process_time = ptime;
    if (pdiff > 0.0) {
        total_process_time += pdiff;
    }

    // 获取系统负载
    double loadavg[3];
    if(getloadavg(loadavg, sizeof(loadavg) / sizeof(loadavg[0])) != -1)
        last_load_avg = loadavg[0];

    std::ostringstream oss;
    oss << "sysload=" << std::fixed << std::setprecision(2) << last_load_avg
        << " cputime=" << std::fixed << std::setprecision(3) << total_process_time;

    // 获取内存
    if (mem_file.is_open()) {
        std::vector<std::string> lines;
        std::string line;
        mem_file.seekg(0, std::ios::beg);
        while (std::getline(mem_file, line)) {
            lines.push_back(line);
        }
        for (size_t i = 0; i < lines.size(); ++i) {
            if (lines.at(i).find("MemAvailable:") == 0) {
                auto vs = common::split(lines.at(i));
                std::vector<std::string> strip_vs;
                for(auto s : vs)
                {
                    bool blank = true;
                    for(auto c : s)
                    {
                        if(!isblank(c))
                            blank = false;
                    }
                    if(blank)
                        continue;
                    strip_vs.push_back(s);
                }
                last_mem_avail = std::stoi(strip_vs[1]);
                oss << " memavail=" << last_mem_avail;
                break;
            }
        }
    }

    return {false, oss.str()};
}

json PrinterSysStats::get_status(double eventtime) {
    json status;
    status["sysload"] = last_load_avg;
    status["cputime"] = total_process_time;
    status["memavail"] = last_mem_avail;
    return status;
}

void PrinterSysStats::disconnect() {
    if (mem_file.is_open()) {
        mem_file.close();
    }
}

PrinterStats::PrinterStats(std::shared_ptr<ConfigWrapper> config) {
    printer = config->get_printer();

    std::shared_ptr<SelectReactor> reactor = printer->get_reactor();
    stats_timer = reactor->register_timer(
        [this](double eventtime) {
            return generate_stats(eventtime);
        }, _NEVER, "stats"
    );

    elegoo::common::SignalManager::get_instance().register_signal(
        "elegoo:ready",
        std::function<void()>([this](){
            handle_ready();
        })
    );
}

PrinterStats::~PrinterStats() {

}

void PrinterStats::handle_ready() {
    std::map<std::string, Any> objects = printer->lookup_objects();
    for (const auto& pair : objects) {
        std::function<std::pair<bool, std::string>(double)> fun = elegoo::extras::get_stats_function(pair.second);
        if(fun) {
            stats_cb.push_back(fun);
        }
    }

    if(printer->get_start_args().find("debugoutput")==printer->get_start_args().end()) {
        std::shared_ptr<SelectReactor> reactor = printer->get_reactor();
        reactor->update_timer(stats_timer, _NOW);
    }
}

double PrinterStats::generate_stats(double eventtime) {
    std::vector<std::pair<bool, std::string>> stats;
    bool output = false;
    std::ostringstream oss;
    for (size_t i = 0; i < stats_cb.size(); i++) {
        stats.push_back(stats_cb.at(i)(eventtime));
        if(stats[i].first)
        output = true;
    }
    if(output)
    {
        oss << "Stats " << std::fixed << std::setprecision(1) <<  eventtime << ":";
        for (size_t i = 0; i < stats_cb.size(); i++)
            oss << " " << stats[i].second;
        // SPDLOG_WARN(oss.str());
    }
    return eventtime + 1;
}

std::shared_ptr<PrinterStats> statistics_load_config(std::shared_ptr<ConfigWrapper> config) {
    std::shared_ptr<Printer> printer = config->get_printer();
    printer->add_object("system_stats", std::make_shared<PrinterSysStats>(config));
    return std::make_shared<PrinterStats>(config);
}

}
}
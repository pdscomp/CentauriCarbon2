/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2024-11-25 10:09:42
 * @LastEditors  : Ben
 * @LastEditTime : 2024-11-30 12:24:15
 * @Description  : Support fans that are enabled when a heater is on
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "heater_fan.h"
#include "fan.h"
#include "heaters.h"
#include "printer.h"

namespace elegoo
{
    namespace extras
    {
        const double PIN_MIN_TIME = 0.100;

        PrinterHeaterFan::PrinterHeaterFan(std::shared_ptr<ConfigWrapper> config)
        {
            SPDLOG_INFO("PrinterHeaterFan init!");
            printer = config->get_printer();
            std::shared_ptr<PrinterHeaters> pheaters =
                any_cast<std::shared_ptr<PrinterHeaters>>(
                    printer->load_object(config, "heaters"));
            elegoo::common::SignalManager::get_instance().register_signal(
                "elegoo:ready",
                std::function<void()>([this]()
                                      { handle_ready(); }));
            std::istringstream iss(config->get_name());
            std::string word;
            while (iss >> word) 
            {
                name = word;
            }

            heater_names = config->getlist("heater");
            heater_temp = config->getdouble("heater_temp", 50);

            fan = std::make_shared<Fan>(config, 1.);
            fan_speed = config->getdouble("fan_speed", 1, 0, 1);
            last_speed = 0;
            SPDLOG_INFO("PrinterHeaterFan init success!!");
        }

        PrinterHeaterFan::~PrinterHeaterFan() {}

        void PrinterHeaterFan::handle_ready()
        {
            SPDLOG_DEBUG("handle_ready init ENTER!!");
            std::shared_ptr<PrinterHeaters> pheaters = any_cast<std::shared_ptr<PrinterHeaters>>(printer->lookup_object("heaters"));

            heaters.clear();
            for (const std::string &name : heater_names)
            {
                heaters.push_back(pheaters->lookup_heater(name));
            }
            std::shared_ptr<SelectReactor> reactor = printer->get_reactor();
            fan_timer = reactor->register_timer(
                [this](double eventtime)
                { return callback(eventtime); },
                get_monotonic() + PIN_MIN_TIME, std::string("heater_fan ") + name);
            SPDLOG_INFO("{} : {}___ ok", __FUNCTION__, __LINE__);
        }

        json PrinterHeaterFan::get_status(double eventtime)
        {
            return fan->get_status(eventtime);
        }

        double PrinterHeaterFan::callback(double eventtime)
        {
            double speed = 0.0;
            for (std::shared_ptr<Heater> heater : heaters)
            {
                std::pair<double, double> val = heater->get_temp(eventtime);
                if (val.second != 0.0 || val.first > heater_temp)
                    speed = fan_speed;
            }
            if (speed != last_speed)
            {
                last_speed = speed;
                fan->set_speed(speed);
            }
            return eventtime + 1.0;
        }

        std::shared_ptr<PrinterHeaterFan> heater_fan_load_config_prefix(
            std::shared_ptr<ConfigWrapper> config)
        {
            return std::make_shared<PrinterHeaterFan>(config);
        }

    }
}

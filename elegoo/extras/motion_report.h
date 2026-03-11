/*****************************************************************************
 * @Author       : Ben
 * @Date         : 2024-11-28 20:15:20
 * @LastEditors  : coconut
 * @LastEditTime : 2025-02-10 21:43:10
 * @Description  : Diagnostic tool for reporting stepper and kinematic positions
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#pragma once
#include "printer.h"
#include "stepper.h"
#include "bulk_sensor.h"
#include "configfile.h"
// #include "mcu_stepper.h"

namespace elegoo
{
    namespace extras
    {

        const double NEVER_TIME = 9999999999999999.;
        const double STATUS_REFRESH_TIME = 0.250;

        class DumpStepper
        {
        public:
            DumpStepper(std::shared_ptr<Printer> printer, std::shared_ptr<MCU_stepper> mcu_stepper);
            std::pair<std::vector<pull_history_steps>, std::vector<std::pair<pull_history_steps, int>>> get_step_queue(uint64_t start_clock, uint64_t end_clock);
            void log_steps(const std::vector<pull_history_steps> &data);
            json process_batch(double eventtime);
            std::shared_ptr<MCU_stepper> mcu_stepper;

        private:
            std::shared_ptr<Printer> printer;
            long long last_batch_clock;
            std::shared_ptr<BatchBulkHelper> batch_bulk;
            std::vector<pull_history_steps> extract_data(const std::vector<std::pair<pull_history_steps, int>> &res);
        };

        class DumpTrapQ
        {
        private:
            std::shared_ptr<Printer> _printer;
            std::string _name;
            std::shared_ptr<trapq> _trapq;
            std::tuple<double, double, double> last_batch_msg;
            // BatchBulkHelper batch_bulk;
            std::shared_ptr<BatchBulkHelper> batch_bulk;

        public:
            DumpTrapQ(std::shared_ptr<Printer> printer, const std::string &name, std::shared_ptr<trapq> trapq);
            std::pair<std::vector<pull_move>, std::vector<std::pair<pull_move *, int>>> extract_trapq(double start_time, double end_time);
            void log_trapq(const std::vector<pull_move> &data);
            std::pair<std::tuple<double, double, double>, double> get_trapq_position(double print_time);
            json _process_batch(double eventtime);
        };

        class PrinterMotionReport
        {
        public:
            PrinterMotionReport(std::shared_ptr<ConfigWrapper> config);
            void register_stepper(std::shared_ptr<ConfigWrapper> config, std::shared_ptr<MCU_stepper>);
            double _dump_shutdown(double eventtime);
            json get_status(double eventtime);
            std::map<std::string, std::shared_ptr<DumpTrapQ>> trapqs;

        private:
            void _connect();
            void _shutdown();
            std::shared_ptr<Printer> printer;
            std::map<std::string, std::shared_ptr<DumpStepper>> steppers;
            double next_status_time = 0.0;
            json last_status;
        };

        std::shared_ptr<PrinterMotionReport> motion_report_load_config(std::shared_ptr<ConfigWrapper> config);
    }
}
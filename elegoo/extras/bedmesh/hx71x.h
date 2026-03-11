/*****************************************************************************
 * @Author       : Coconut
 * @Date         : 2025-02-12 11:05:41
 * @LastEditors  : coconut
 * @LastEditTime : 2025-02-12 18:26:41
 * @Description  : HX711/HX717/CS1237 Support
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#pragma once
#include <memory>
#include <functional>
#include <map>
#include "configfile.h"
#include "printer.h"
#include "mcu.h"
#include "bulk_sensor.h"
#include "pins.h"

class Printer;
class MCU;
class ConfigWrapper;
class ReactorTimer;
class PrinterPins;
class BulkDataQueue;
class FixedFreqReader;

namespace elegoo
{
    namespace extras
    {
        class BatchBulkHelper;

        class HX71xBase
        {
        public:
            struct HX71xBaseSamples
            {
                double ptime;
                int32_t raw_val;
                double val;
            };

            struct HX71xBaseClientSamples
            {
                std::vector<HX71xBase::HX71xBaseSamples> hx71x_samples;
                int last_error_count;
                int overflows;
            };

            HX71xBase(std::shared_ptr<ConfigWrapper> config, std::string sensor_type, const std::map<int, int> &sample_rate_options, int default_sample_rate, const std::map<std::string, int> &gain_options, std::string default_gain);
            virtual ~HX71xBase();

            std::shared_ptr<MCU> get_mcu();
            uint32_t get_samples_per_second();
            std::pair<int32_t, int32_t> get_range();
            void add_client(BatchBulkHelper::ClientCallback callback);
            void attach_endstop(int endstop_oid);

        private:
            std::shared_ptr<Printer> printer;
            std::shared_ptr<MCU> mcu;
            std::shared_ptr<BulkDataQueue> bulk_queue;
            std::shared_ptr<FixedFreqReader> ffreader;
            std::shared_ptr<BatchBulkHelper> batch_bulk;
            std::shared_ptr<CommandWrapper> query_hx71x_cmd;
            std::shared_ptr<CommandWrapper> cs123x_write_cmd;
            std::shared_ptr<CommandQueryWrapper> cs123x_read_cmd;
            std::shared_ptr<CommandWrapper> config_endstop_cmd;

            std::string name;
            std::string sensor_type;

            int last_error_count;
            int consecutive_fails;
            int oid;

            std::string dout_pin;
            std::string sclk_pin;
            uint32_t sps;
            uint32_t gain_channel;
            int ref_output_enable;
            bool config_done;

            void _build_config();
            std::vector<HX71xBase::HX71xBaseSamples> _convert_samples(std::vector<FixedFreqReader::FixedFreqReaderSamples> &samples);
            void _start_measurements();
            void _finish_measurements();
            Any _process_batch(double eventtime);
            void _write_reg(uint8_t val);
            uint8_t _read_reg();
        };

        class HX711 : public HX71xBase
        {
        public:
            HX711(std::shared_ptr<ConfigWrapper> config) : HX71xBase(config, "hx711",
                                                                     {{80, 80}, {10, 10}}, 80,
                                                                     {{"A-128", 1}, {"B-32", 2}, {"A-64", 3}}, "A-128") {}
            ~HX711() {}
        };

        class HX717 : public HX71xBase
        {
        public:
            HX717(std::shared_ptr<ConfigWrapper> config) : HX71xBase(config, "hx717",
                                                                     {{320, 320}, {80, 80}, {20, 20}, {10, 10}}, 320,
                                                                     {{"A-128", 1}, {"B-64", 2}, {"A-64", 3}, {"B-8", 4}}, "A-128") {}
            ~HX717() {}
        };

        class CS1237 : public HX71xBase
        {
        public:
            CS1237(std::shared_ptr<ConfigWrapper> config) : HX71xBase(config, "cs1237",
                                                                      {{1280, 1280}, {640, 640}, {40, 40}, {10, 10}}, 320,
                                                                      {{"A-1", 1}, {"A-2", 2}, {"A-64", 3}, {"A-128", 4}}, "A-128") {}
            ~CS1237() {}
        };
    }
}
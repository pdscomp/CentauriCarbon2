/***************************************************************************** 
 * @Author       : Loping
 * @Date         : 2025-2-25 11:03:36
 * @LastEditors  : Loping
 * @LastEditTime : 2025-2-25 11:03:36
 * @Description  : Support for reading acceleration data from an LIS2DH chip
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/
#ifndef __LIS2DH_H__
#define __LIS2DH_H__

#include <iostream>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <functional>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include "printer.h"
#include "configfile.h"
#include "extras/bulk_sensor.h"
#include "extras/bus.h"
#include "extras/adxl345.h"

namespace elegoo 
{
    namespace extras 
    {
        // Printer class that controls LIS2DH chip
        class Lis2dh : public std::enable_shared_from_this<Lis2dh>, public AccelChip
        {
        public:
            Lis2dh(std::shared_ptr<ConfigWrapper> config);
            ~Lis2dh();
            void init(std::shared_ptr<ConfigWrapper> config) override ;
            uint8_t read_reg(uint8_t reg) override ;
            void set_reg(uint8_t reg, uint8_t val,uint64_t minclock = 0) override ;
            std::shared_ptr<AccelQueryHelper> start_internal_client() override ;
        private:
            void _build_config() override ;
            void _convert_samples(std::vector<FixedFreqReader::FixedFreqReaderSamples> &samples) override ;
            void _start_measurements() override ;
            void _finish_measurements() override ;
            Any _process_batch(double eventtime) override ;
        private:
            std::shared_ptr<AccelCommandHelper> accel_comm;
            // std::shared_ptr<Printer> printer;
            // std::map<int32_t,double> axes_map;
            // int32_t data_rate;
            // std::shared_ptr<MCU_SPI> spi;
            // std::shared_ptr<MCU> mcu;
            // uint32_t oid;
            std::shared_ptr<CommandWrapper> query_lis2dh_cmd;
            // std::shared_ptr<FixedFreqReader> ffreader;
            // int32_t last_error_count;
            // std::shared_ptr<BatchBulkHelper> batch_bulk;
            // std::string name;
        };

        std::shared_ptr<Lis2dh> lis2dh_load_config(std::shared_ptr<ConfigWrapper> config);
        std::shared_ptr<Lis2dh> lis2dh_load_config_prefix(std::shared_ptr<ConfigWrapper> config);
    }
}

#endif

/*****************************************************************************
 * @Author       : Ben
 * @Date         : 2024-11-28 20:15:19
 * @LastEditors  : coconut
 * @LastEditTime : 2025-02-12 18:03:07
 * @Description  : Tools for reading bulk sensor data from the mcu.
 * This "bulk sensor" module facilitates the processing of sensor chip
 * measurements that do not require the host to respond with low
 * latency.  This module helps collect these measurements into batches
 * that are then processed periodically by the host code (as specified
 * by BatchBulkHelper.batch_interval).  It supports the collection of
 * thousands of sensor measurements per second.
 *
 * Processing measurements in batches reduces load on the mcu, reduces
 * bandwidth to/from the mcu, and reduces load on the host.  It also
 * makes it easier to export the raw measurements via the webhooks
 * system (aka API Server).
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#pragma once
#include <iostream>
#include <vector>
#include <functional>
#include <stdexcept>
#include <memory>
#include <chrono>
#include <thread>
#include <mutex>
#include "printer.h"
#include "webhooks.h"
#include "reactor.h"
#include "any.h"
#include "endian.h"

namespace elegoo
{
    namespace extras
    {

#define BATCH_INTERVAL 0.500

        class BatchBulkHelper
        {
        public:
            using BatchCallback = std::function<Any(double)>;
            using ClientCallback = std::function<bool(const Any &)>;

            BatchBulkHelper(std::shared_ptr<Printer> printer, BatchCallback batch_cb,
                            std::function<void()> start_cb = nullptr,
                            std::function<void()> stop_cb = nullptr,
                            double batch_interval = BATCH_INTERVAL);

            void add_client(ClientCallback client_cb);
            void add_mux_endpoint(const std::string &path, const std::string &key,
                                  const std::string &value, json &webhooks_start_resp,
                                  std::function<json(const Any &)> any2json = nullptr);
            json webhooks_start_resp;

        private:
            std::shared_ptr<Printer> printer;
            BatchCallback batch_cb;
            std::function<void()> start_cb;
            std::function<void()> stop_cb;
            double batch_interval;
            bool is_started;
            std::vector<ClientCallback> client_cbs;
            std::thread batch_thread;
            std::mutex mutex;
            std::function<double(double)> batch_timer;
            std::shared_ptr<ReactorTimer> timer_handler;
            std::function<json(const Any &)> any2json;
            void _start();
            void _stop();
            double _proc_batch(double eventtime);
            void _add_api_client(WebRequest *web_request);
        };

        class BatchWebhooksClient
        {
        public:
            BatchWebhooksClient(WebRequest *web_request, std::function<json(const Any &)> any2json);
            bool handle_batch(const Any &msg);

        private:
            std::shared_ptr<ClientConnection> cconn; // 客户端连接
            json template_data;                      // 响应模板
            std::function<json(const Any &)> any2json;
        };

        class BulkDataQueue
        {
        public:
            struct BulkDataQueueSamples
            {
                uint16_t sequence;
                std::vector<uint8_t> data;
            };

            BulkDataQueue(std::shared_ptr<MCU> mcu, const std::string &msg_name = "sensor_bulk_data", uint32_t oid = -1);
            void init(void);
            void _handle_data(const json &params);
            std::vector<BulkDataQueueSamples> pull_queue();
            void clear_queue();

        private:
            std::shared_ptr<MCU> mcu; // 引用到 MCU
            std::mutex lock;          // 互斥锁
            std::vector<BulkDataQueueSamples> raw_samples;
            uint32_t oid;
            std::string msg_name;
        };

        class ClockSyncRegression
        {
        public:
            ClockSyncRegression(std::shared_ptr<MCU> mcu, double chip_clock_smooth, double decay = 1.0 / 20.0);
            void reset(double mcu_clock, double chip_clock);
            void update(double mcu_clock, double chip_clock);
            void set_last_chip_clock(double chip_clock);
            std::tuple<double, double, double> get_clock_translation();
            std::tuple<double, double, double> get_time_translation();

        private:
            std::shared_ptr<MCU> mcu;
            double chip_clock_smooth;
            double decay;
            double last_chip_clock;
            double last_exp_mcu_clock;
            double last_mcu_clock;
            double mcu_clock_avg;
            double mcu_clock_variance;
            double chip_clock_avg;
            double chip_clock_covariance;
        };

        // 1.该类用于FixedFreqReader类解析传感器上报的二进制数据，目前对不同的unpack_fmt是硬编码形式实现，添加新的unpack_fmt要更新该类代码。
        // 2.如果使用非32位ARM处理器，需要确认处理器是否为小端和'unsigned int'类型大小
        class Unpack
        {
        public:
            Unpack(const std::string &unpack_fmt) : _unpack_fmt(unpack_fmt), _little_endian(true), _size(0)
            {
                // 1. 识别大小端
                char endian = _unpack_fmt.at(0);
                if (endian == '>')
                    _little_endian = false;
                if (endian == '<' || endian == '>')
                    _unpack_fmt.erase(0, 1);

                // 2. 根据符号计算大小与转换函数
                if (_unpack_fmt == "i")
                {
                    _size = sizeof(int);
                    _from = std::bind(&Unpack::from_i, this, std::placeholders::_1, std::placeholders::_2);
                }
                else if (_unpack_fmt == "I")
                {
                    _size = sizeof(unsigned int);
                    _from = std::bind(&Unpack::from_I, this, std::placeholders::_1, std::placeholders::_2);
                }
                else if (_unpack_fmt == "BBBBB")
                {
                    _size = sizeof(unsigned char) * 5;
                    _from = std::bind(&Unpack::from_BBBBB, this, std::placeholders::_1, std::placeholders::_2);
                }
                else if (_unpack_fmt == "hhh")
                {
                    _size = sizeof(unsigned short) * 3;
                    _from = std::bind(&Unpack::from_hhh, this, std::placeholders::_1, std::placeholders::_2);
                }
                else
                {
                    throw std::runtime_error("unknown fmt");
                }
            }

            Any from(const std::vector<uint8_t> &data, size_t offset)
            {
                return std::move(_from(data, offset));
            }

            size_t size()
            {
                return _size;
            }

        private:
            std::string _unpack_fmt;
            size_t _size;
            bool _little_endian;
            std::function<Any(const std::vector<uint8_t> &data, size_t offset)> _from;

            // 假设ARM为小端且使用32位处理器...
            Any from_i(const std::vector<uint8_t> &data, size_t offset)
            {
                int32_t value;
                std::memcpy(&value, data.data() + offset, sizeof(int32_t));
                if (!_little_endian)
                    __bswap_32(value);
                // SPDLOG_INFO("from_i: value {}", value);
                return value;
            }

            Any from_I(const std::vector<uint8_t> &data, size_t offset)
            {
                uint32_t value;
                std::memcpy(&value, data.data() + offset, sizeof(uint32_t));
                if (!_little_endian)
                    __bswap_32(value);
                return value;
            }

            Any from_BBBBB(const std::vector<uint8_t> &data, size_t offset)
            {
                return std::move(std::vector<uint8_t>(data.begin() + offset, data.begin() + offset + 5));
            }

            Any from_hhh(const std::vector<uint8_t> &data, size_t offset)
            {
                std::vector<uint8_t> value(data.begin() + offset, data.begin() + offset + 6);
                if (!_little_endian)
                    for (auto &v : value)
                        v = __bswap_16(v);
                return std::move(value);
            }
        };

        class FixedFreqReader
        {
        public:
            struct FixedFreqReaderSamples
            {
                double ptime;
                Any data;
            };

            FixedFreqReader(std::shared_ptr<MCU> mcu, double chip_clock_smooth, const std::string &unpack_fmt);
            void setup_query_command(const std::string &msgformat, uint32_t oid, std::shared_ptr<command_queue> cq);
            int get_last_overflows();
            void note_start();
            void note_end();
            std::vector<FixedFreqReaderSamples> pull_samples();

        private:
            void _clear_duration_filter();
            void _update_clock(bool is_reset = false);

            std::shared_ptr<MCU> mcu;
            ClockSyncRegression clock_sync;
            uint32_t bytes_per_sample;
            uint32_t samples_per_block;
            uint64_t last_sequence;
            uint64_t max_query_duration;
            uint64_t last_overflows;
            std::shared_ptr<BulkDataQueue> bulk_queue;
            int oid;
            std::shared_ptr<CommandQueryWrapper> query_status_cmd;
            Unpack unpack;
        };
    }
}

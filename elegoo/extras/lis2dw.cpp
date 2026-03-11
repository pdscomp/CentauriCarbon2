/***************************************************************************** 
 * @Author       : Loping
 * @Date         : 2025-2-25 11:03:36
 * @LastEditors  : loping
 * @LastEditTime : 2025-06-10 15:30:49
 * @Description  : Support for reading acceleration data from an LIS2DW chip
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/
#include "lis2dw.h"


namespace elegoo 
{
    namespace extras 
    {
        // LIS2DW registers
        static uint8_t REG_LIS2DW_WHO_AM_I_ADDR = 0x0F;
        static uint8_t REG_LIS2DW_CTRL_REG1_ADDR = 0x20;
        static uint8_t REG_LIS2DW_CTRL_REG2_ADDR = 0x21;
        static uint8_t REG_LIS2DW_CTRL_REG3_ADDR = 0x22;
        static uint8_t REG_LIS2DW_CTRL_REG4_ADDR = 0x23;
        static uint8_t REG_LIS2DW_CTRL_REG5_ADDR = 0x24;
        static uint8_t REG_LIS2DW_CTRL_REG6_ADDR = 0x25;
        static uint8_t REG_LIS2DW_STATUS_REG_ADDR = 0x27;
        static uint8_t REG_LIS2DW_OUT_XL_ADDR = 0x28;
        static uint8_t REG_LIS2DW_OUT_XH_ADDR = 0x29;
        static uint8_t REG_LIS2DW_OUT_YL_ADDR = 0x2A;
        static uint8_t REG_LIS2DW_OUT_YH_ADDR = 0x2B;
        static uint8_t REG_LIS2DW_OUT_ZL_ADDR = 0x2C;
        static uint8_t REG_LIS2DW_OUT_ZH_ADDR = 0x2D;
        static uint8_t REG_LIS2DW_FIFO_CTRL   = 0x2E;
        static uint8_t REG_LIS2DW_FIFO_SAMPLES = 0x2F;
        static uint8_t REG_MOD_READ = 0x80;
        // static uint8_t REG_MOD_MULTI = 0x40;
        // static uint8_t REG_MOD_MULTI_C0 = 0xC0;
        // static uint8_t REG_MOD_WRITE_40 = 0x40;
        // static uint8_t LIS2DW_DEV_ID = 0x44;
        static uint8_t LIS2DW_DEV_ID = 0x33;

        static double FREEFALL_ACCEL = 9.80665;
        // static double SCALE = (FREEFALL_ACCEL * 1.952 / 4);
        static double SCALE = (FREEFALL_ACCEL / 16 * 3.90625);
        static double SCALE_HR_2G = (FREEFALL_ACCEL / 16 * 1.);
        static double SCALE_HR_8G = (FREEFALL_ACCEL / 16 * 4.);
        static double BATCH_UPDATES = 0.100; 

        #define HR_8G 1

        // Printer class that controls LIS2DW chip
        Lis2dw::Lis2dw(std::shared_ptr<ConfigWrapper> config)
        {
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            this->printer = config->get_printer();
            gcode = any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));
            // this->axes_map = read_axes_map(config,SCALE,SCALE,SCALE);
            if(HR_8G)
            {
                this->axes_map = read_axes_map(config,SCALE_HR_8G,SCALE_HR_8G,SCALE_HR_8G);
            }
            else
            {
                this->axes_map = read_axes_map(config,SCALE_HR_2G,SCALE_HR_2G,SCALE_HR_2G);
            }
            this->data_rate = 1344;//1600;

            SPDLOG_DEBUG("__func__:{} #1",__func__);
            // Setup mcu sensor_lis2dw bulk query code
            this->spi = MCU_SPI_from_config(config, 3, "cs_pin", 5000000);
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            this->mcu = this->spi->get_mcu();
            uint32_t oid = this->oid = mcu->create_oid();
            this->query_lis2dw_cmd = {};
            this->mcu->add_config_cmd("config_lis2dw oid=" + std::to_string(oid) + " spi_oid=" + std::to_string(this->spi->get_oid()));
            this->mcu->add_config_cmd("query_lis2dw oid=" + std::to_string(oid) + " rest_ticks=0" + std::to_string(oid),false,true);
            this->mcu->register_config_callback(
                    [this]()
                    {
                        _build_config();
                    });

            SPDLOG_DEBUG("__func__:{} #1",__func__);
            // Bulk sample message reading
            double chip_smooth = this->data_rate * BATCH_UPDATES * 2;
            this->ffreader = std::make_shared<FixedFreqReader>(mcu,chip_smooth,"<hhh");
            this->last_error_count = 0;

            SPDLOG_DEBUG("__func__:{} #1",__func__);
            // Process messages in batches
            this->batch_bulk = std::make_shared<BatchBulkHelper>(this->printer,
                    [this](double eventtime)->Any
                    {
                        return _process_batch(eventtime);
                    },
                    [this]()
                    {
                        _start_measurements();
                    },
                    [this]()
                    {
                        _finish_measurements();
                    },
                    BATCH_UPDATES);
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            std::vector<std::string> split_name = elegoo::common::split(config->get_name());
            this->name = split_name.back();
            std::vector<std::string> hdr = {"time","x_acceleration","y_acceleration","z_acceleration"};
            json resp = {};
            resp["header"] = hdr;
            this->batch_bulk->add_mux_endpoint("lis2dw/dump_lis2dw", "sensor",this->name, resp);
            SPDLOG_DEBUG("__func__:{} #1",__func__);
        }

        Lis2dw::~Lis2dw()
        {
            SPDLOG_DEBUG("~Lis2dw");
        }
        void Lis2dw::init(std::shared_ptr<ConfigWrapper> config)
        {
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            this->accel_comm = std::make_shared<AccelCommandHelper>(config,shared_from_this());
        }
        
        void Lis2dw::_build_config()
        {
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            std::shared_ptr<command_queue> cmdqueue = this->spi->get_command_queue();
            this->query_lis2dw_cmd = this->mcu->lookup_command("query_lis2dw oid=%c rest_ticks=%u", cmdqueue);
            this->ffreader->setup_query_command("query_lis2dw_status oid=%c",this->oid, cmdqueue);
        }

        uint8_t Lis2dw::read_reg(uint8_t reg)
        {
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            std::vector<uint8_t> data;
            data.emplace_back(reg | REG_MOD_READ);
            // data.emplace_back(reg);
            SPDLOG_DEBUG("__func__:{} #1 {} {}",__func__,reg,reg | REG_MOD_READ);
            data.emplace_back(0x00);
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            json params = this->spi->spi_transfer(data);
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            if (params.contains("response") && params["response"].is_string())
            {
                std::string resp = params["response"].get<std::string>();
                std::vector<uint8_t> response(resp.begin(),resp.end());
                SPDLOG_DEBUG("__func__:{} #1 ,reg:{},response.size:{} {} {}",__func__,reg,response.size(),response[0],response[1]);
                return response[1];
            }
            else
            {
                SPDLOG_DEBUG("__func__:{} #1",__func__);
                return -1;
            }
        }

        void Lis2dw::set_reg(uint8_t reg, uint8_t val,uint64_t minclock)
        {
            SPDLOG_DEBUG("__func__:{} #1 reg:{},val:{}",__func__,reg,val);
            std::vector<uint8_t> data;
            data.emplace_back(reg);
            data.emplace_back(val & 0xFF);
            this->spi->spi_send(data,minclock);
            uint8_t stored_val = read_reg(reg);
            if(stored_val != val)
            {
                char buffer[128] = {};
                std::snprintf(buffer, sizeof(buffer), "Failed to set LIS2DW register [0x%x] to 0x%x: got 0x%x. ",reg, val, stored_val);
                std::string str(buffer);
                std::string failed = str + "This is generally indicative of connection problems " + "(e.g. faulty wiring) or a faulty lis2dw chip.";
                SPDLOG_ERROR(failed);
                throw elegoo::common::CommandError(failed);
            }
        }

        std::shared_ptr<AccelQueryHelper> Lis2dw::start_internal_client()
        {
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            std::shared_ptr<AccelQueryHelper> aqh = std::make_shared<AccelQueryHelper>(this->printer);
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            this->batch_bulk->add_client(
                    [aqh](Any msg)->bool
                    {
                        SPDLOG_DEBUG("__func__:{} #1",__func__);
                        return aqh->handle_batch(msg);
                    });
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            return aqh;
        }

        json Lis2dw::get_accel_status()
        {
            return this->accel_status;
        }

        void Lis2dw::_convert_samples(std::vector<FixedFreqReader::FixedFreqReaderSamples> &samples)
        {
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            if(this->axes_map.size() != 3)
            {
                SPDLOG_ERROR("axes_map.size is not 3!");
                throw elegoo::common::CommandError("axes_map.size is not 3!");
            }

            int32_t pos[3] = {},ii = 0,count = 0;
            double scale[3] = {};
            for(auto axes : this->axes_map)
            {
                pos[ii] = std::get<0>(axes);
                scale[ii] = std::get<1>(axes);
                SPDLOG_DEBUG("__func__:{},pos[ii:{}]:{},scale[ii]:{},it->first:{},it->second:{}",__func__,ii,pos[ii],scale[ii],std::get<0>(axes),std::get<0>(axes));
                if(++ii >= 3)
                {
                    break;
                }
            }

            SPDLOG_DEBUG("__func__:{} #1",__func__);
            for(auto sample : samples)
            {
                int16_t rx, ry, rz;
                double xt,yt,zt;
                double ptime = sample.ptime;
                std::vector<uint8_t> data = any_cast<std::vector<uint8_t>>(sample.data);
                if(data.size() != 2*strlen("hhh"))
                {
                    SPDLOG_ERROR("data.size():{} != strlen('hhh'):{}",data.size(),strlen("hhh"));
                    throw elegoo::common::CommandError("data.size() != strlen('hhh')");
                }
                else
                {
                    rx = (data[1] << 8 | data[0]);
                    ry = (data[3] << 8 | data[2]);
                    rz = (data[5] << 8 | data[4]);
                    if(HR_8G)
                    {
                        xt = (float)(rx >> 4) * 4;//SCALE_HR_8G;
                        yt = (float)(ry >> 4) * 4;//SCALE_HR_8G;
                        zt = (float)(rz >> 4) * 4;//SCALE_HR_8G;
                    }
                    else
                    {
                        xt = (float)(rx >> 4) * 1;//SCALE_HR_2G;
                        yt = (float)(ry >> 4) * 1;//SCALE_HR_2G;
                        zt = (float)(rz >> 4) * 1;//SCALE_HR_2G;
                    }
                }

                std::vector<double> raw_xyz = {(double)rx,(double)ry,(double)rz};
                double x = static_cast<long long>(round(raw_xyz[pos[0]] * scale[0] * 1000000))/1000000.;
                double y = static_cast<long long>(round(raw_xyz[pos[1]] * scale[1] * 1000000))/1000000.;
                double z = static_cast<long long>(round(raw_xyz[pos[2]] * scale[2] * 1000000))/1000000.;
                double pt = static_cast<long long>(round(ptime * 1000000))/1000000.;
                std::vector<double> dat = {x,y,z};
                // std::vector<double> dat = {xt,yt,zt};
                samples[count].ptime = pt;
                samples[count].data = dat;
                if(count > samples.size() -5)
                {
                    SPDLOG_DEBUG("__func__:{},pos[0]:{},pos[1]:{},pos[2]:{},scale[0]:{},scale[1]:{},scale[2]:{}",__func__,pos[0],pos[1],pos[2],scale[0],scale[1],scale[2]);
                    SPDLOG_DEBUG("__func__:{},data.size:{},data[0]:{},data[1]:{},data[2]:{},data[3]:{},data[4]:{},data[5]:{}\n",__func__,data.size(),data[0],data[1],data[2],data[3],data[4],data[5]);
                    SPDLOG_DEBUG("__func__:{},data.size:{},rx:{},ry:{},rz:{},xt:{},yt:{},zt:{}\n",__func__,data.size(),rx,ry,rz,xt,yt,zt);
                    SPDLOG_DEBUG("__func__:{},count:{},samples.size:{},dat.size:{},pt:{},x:{},y:{},z:{}\n",__func__,count,samples.size(),dat.size(),pt,x,y,z);
                }
                count += 1;
            }
            SPDLOG_DEBUG("__func__:{} #1",__func__);
        }

        uint8_t Lis2dw::get_lis2dw_id()
        {
            return read_reg(REG_LIS2DW_WHO_AM_I_ADDR);
        }

        void Lis2dw::_start_measurements()
        {
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            // In case of miswiring, testing lis2dw device ID prevents treating
            // noise or wrong signal as a correctly initialized device
            uint8_t dev_id = read_reg(REG_LIS2DW_WHO_AM_I_ADDR); 
            char buffer[128] = {};
            std::snprintf(buffer, sizeof(buffer), "lis2dw_dev_id: 0x%x",dev_id);
            std::string str(buffer);
            SPDLOG_INFO(str);
            if(dev_id != LIS2DW_DEV_ID)
            {
                this->accel_status["accel_status"] = false;
                memset(buffer,0,sizeof(buffer));
                std::snprintf(buffer, sizeof(buffer), "Invalid lis2dw id (got 0x%x vs 0x%x).",dev_id,LIS2DW_DEV_ID);
                std::string str(buffer);
                std::string err = str + "This is generally indicative of connection problems\n" + 
                        "(e.g. faulty wiring) or a faulty lis2dw chip.";
                SPDLOG_ERROR(err);
                gcode->respond_ecode(str, elegoo::common::ErrorCode::LIS2DW_SENSOR, elegoo::common::ErrorLevel::WARNING);
                throw elegoo::common::CommandError(err);
            }
            else
            {
                this->accel_status["accel_status"] = true;
            }

            SPDLOG_DEBUG("__func__:{} #1",__func__);
            // Setup chip in requested query rate
            // ODR9, +-8g, low-pass filter, Low-noise abled
            if(HR_8G)
            {
                set_reg(REG_LIS2DW_CTRL_REG4_ADDR, 0xA8);
            }
            else
            {
                set_reg(REG_LIS2DW_CTRL_REG4_ADDR, 0x88);
            }
            // Continuous mode: If the FIFO is full
            // the new sample overwrites the older sample.
            // set_reg(REG_LIS2DW_FIFO_CTRL, 0xC0);
            // High-Performance mode 1.344 kHz / Low-Power mode 5.376 kHz   
            // High-Performance Mode (12-bit resolution)
            if(HR_8G)
            {
                set_reg(REG_LIS2DW_CTRL_REG1_ADDR, 0x97);
            }
            else
            {
                set_reg(REG_LIS2DW_CTRL_REG1_ADDR, 0x17);
            }

            // Start bulk reading
            uint64_t rest_ticks = this->mcu->seconds_to_clock(4. / this->data_rate);
            this->query_lis2dw_cmd->send({
                    std::to_string(this->oid), 
                    std::to_string(rest_ticks)
                });
            // set_reg(REG_LIS2DW_FIFO_CTRL, 0xC0);
            SPDLOG_INFO("LIS2DW starting '{}' measurements",this->name);

            SPDLOG_DEBUG("__func__:{} #1",__func__);
            // Initialize clock tracking
            this->ffreader->note_start();
            this->last_error_count = 0;
        }

        void Lis2dw::_finish_measurements()
        {
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            // Halt bulk reading
            // set_reg(REG_LIS2DW_FIFO_CTRL, 0x00);
            this->query_lis2dw_cmd->send({
                    std::to_string(this->oid), 
                    std::to_string(0)
                });
            SPDLOG_DEBUG("__func__:{} #1 rest_ticks:{}",__func__,0);
            this->ffreader->note_end();
            SPDLOG_INFO("LIS2DW finished '{}' measurements", this->name);
            // set_reg(REG_LIS2DW_FIFO_CTRL, 0x00);
        }

        Any Lis2dw::_process_batch(double eventtime)
        {
            std::vector<FixedFreqReader::FixedFreqReaderSamples> samples = this->ffreader->pull_samples();
            _convert_samples(samples);
            if(!samples.size())
            {
                return {};
            }

            auto first_sample_time = samples.front().ptime;
            auto last_sample_time = samples.back().ptime;
            // for(auto sample : samples)
            // {
            //     printf("%.2f ",sample.ptime);
            // }

            std::map<std::string,Any> msg;
            msg["data"] = samples;
            msg["errors"] = this->last_error_count;
            msg["overflows"] = this->ffreader->get_last_overflows();

            return std::move(msg);
        }
        
        std::shared_ptr<AccelChip> lis2dw_load_config(std::shared_ptr<ConfigWrapper> config)
        {
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            std::shared_ptr<Lis2dw> lis2dw = std::make_shared<Lis2dw>(config);
            lis2dw->init(config);
            return lis2dw;
        }
        std::shared_ptr<AccelChip> lis2dw_load_config_prefix(std::shared_ptr<ConfigWrapper> config)
        {
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            std::shared_ptr<Lis2dw> lis2dw = std::make_shared<Lis2dw>(config);
            lis2dw->init(config);
            return lis2dw;
        }
    }
}

/***************************************************************************** 
 * @Author       : Loping
 * @Date         : 2025-2-25 11:03:36
 * @LastEditors  : loping
 * @LastEditTime : 2025-08-07 14:53:28
 * @Description  : Support for reading acceleration data from an adxl345 chip
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/
#include "adxl345.h"
#include "toolhead.h"


namespace elegoo 
{
    namespace extras 
    {
        // ADXL345 registers
        static int32_t REG_DEVID = 0x00;
        static int32_t REG_BW_RATE = 0x2C;
        static int32_t REG_POWER_CTL = 0x2D;
        static int32_t REG_DATA_FORMAT = 0x31;
        static int32_t REG_FIFO_CTL = 0x38;
        static int32_t REG_MOD_READ = 0x80;
        static int32_t REG_MOD_MULTI = 0x40;

        static int32_t ADXL345_DEV_ID = 0xe5;
        static int32_t SET_FIFO_CTL = 0x90;

        static double FREEFALL_ACCEL = (9.80665 * 1000.);
        static double SCALE_XY = (0.003774 * FREEFALL_ACCEL); // 1 / 265 (at 3.3V) mg/LSB
        static double SCALE_Z =  (0.003906 * FREEFALL_ACCEL); // 1 / 256 (at 3.3V) mg/LSB

        static std::map<int32_t,int32_t> QUERY_RATES = {
            {25, 0x8}, 
            {50, 0x9}, 
            {100, 0xa}, 
            {200, 0xb}, 
            {400, 0xc},
            {800, 0xd}, 
            {1600, 0xe}, 
            {3200, 0xf},
        };

        AccelQueryHelper::AccelQueryHelper(std::shared_ptr<Printer> printer)
        {
            this->printer = printer;
            this->is_finished = false;
            std::shared_ptr<ToolHead> toolhead = any_cast<std::shared_ptr<ToolHead>>(printer->lookup_object("toolhead"));
            this->request_end_time = this->request_start_time = toolhead->get_last_move_time();
            this->msgs = {};
            this->samples = {};

        }
        
        AccelQueryHelper::~AccelQueryHelper()
        {
            SPDLOG_DEBUG("~AccelQueryHelper");
        }

        void AccelQueryHelper::finish_measurements()
        {
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            std::shared_ptr<ToolHead> toolhead = any_cast<std::shared_ptr<ToolHead>>(printer->lookup_object("toolhead"));
            this->request_end_time = toolhead->get_last_move_time();
            toolhead->wait_moves();
            this->is_finished = true;
            SPDLOG_DEBUG("__func__:{} #1 this->is_finished:{}",__func__,this->is_finished);
        }

        bool AccelQueryHelper::handle_batch(Any msg)
        {
            if(this->is_finished)
            {
                SPDLOG_DEBUG("__func__:{} #1 this->msgs.size:{},this->is_finished:{}",__func__,this->msgs.size(),this->is_finished);
                return false;
            }

            if(this->msgs.size() >= 10000)
            {
                SPDLOG_DEBUG("__func__:{} #1 this->msgs.size:{},this->is_finished:{}",__func__,this->msgs.size(),this->is_finished);
                // Avoid filling up memory with too many samples
                return false;
            }
            this->msgs.emplace_back(msg);
            SPDLOG_DEBUG("__func__:{} #1 this->msgs.size:{},this->is_finished:{}",__func__,this->msgs.size(),this->is_finished);
            return true;
        }

        bool AccelQueryHelper::has_valid_samples()
        {
            SPDLOG_DEBUG("__func__:{} #1 {}",__func__,this->msgs.size());
            for(auto msg : this->msgs)
            {
                std::map<std::string,Any> msg_map = any_cast<std::map<std::string,Any>>(msg);
                std::vector<FixedFreqReader::FixedFreqReaderSamples> samples = 
                        any_cast<std::vector<FixedFreqReader::FixedFreqReaderSamples>>(msg_map["data"]);
                if(!samples.size())
                {
                    SPDLOG_INFO("!samples.size()");
                    continue;
                }
                auto first_sample_time = samples.front().ptime;
                auto last_sample_time = samples.back().ptime;
                // for(auto sample : samples)
                // {
                //     printf("%.6f ",sample.ptime);
                // }
                // printf("__func__:%s,__LINE__:%d samples.size:%d,,samples.back().ptime:%.6f,first_sample_time:%.6f,last_sample_time:%.6f\n",__func__,__LINE__,samples.size(),samples.back().ptime,first_sample_time,last_sample_time);
                SPDLOG_DEBUG("__func__:{} #1 {} VS {}, {} VS {}",__func__,first_sample_time,this->request_end_time,last_sample_time,this->request_start_time);
                if(first_sample_time > this->request_end_time || last_sample_time < this->request_start_time)
                {
                    continue;
                }
                // The time intervals [first_sample_time, last_sample_time]
                // and [request_start_time, request_end_time] have non-zero
                // intersection. It is still theoretically possible that none
                // of the samples from msgs fall into the time interval
                // [request_start_time, request_end_time] if it is too narrow
                // or on very heavy data losses. In practice, that interval
                // is at least 1 second, so this possibility is negligible.
                SPDLOG_DEBUG("__func__:{} is true! #1 {} VS {}, {} VS {}",__func__,first_sample_time,this->request_end_time,last_sample_time,this->request_start_time);
                return true;
            }
            return false;
        }

        std::vector<AccelMeasurement> AccelQueryHelper::get_samples()
        {
            if(!this->msgs.size())
            {
                return this->samples;
            }

            int32_t total = 0, count =0;
            std::vector<AccelMeasurement> samples;
            for(auto msg : this->msgs)
            {
                std::map<std::string,Any> msg_map = any_cast<std::map<std::string,Any>>(msg);
                std::vector<FixedFreqReader::FixedFreqReaderSamples> data = 
                        any_cast<std::vector<FixedFreqReader::FixedFreqReaderSamples>>(msg_map["data"]);
                for(auto dat : data)
                {
                    auto samp_time = dat.ptime;
                    if(samp_time < this->request_start_time)
                    {
                        continue;
                    }
                    if(samp_time > this->request_end_time)
                    {
                        break;
                    }
                    std::vector<double> accel_meas = any_cast<std::vector<double>>(dat.data);
                    if(accel_meas.size() == 3)
                    {
                        AccelMeasurement sample;
                        sample.time = samp_time;
                        sample.accel_x = accel_meas[0];
                        sample.accel_y = accel_meas[1];
                        sample.accel_z = accel_meas[2];
                        samples.push_back(sample);
                    }
                    else
                    {
                        SPDLOG_ERROR("accel_meas.size() != 3 ,not have x,y,z");
                        throw elegoo::common::CommandError("accel_meas.size() != 3 ,not have x,y,z");
                    }
                }
            }
            this->samples = std::move(samples);
            return this->samples;
        }

        void AccelQueryHelper::write_impl(std::tuple<std::string,std::vector<AccelMeasurement>> tp)
        {
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            int fd = open(std::get<0>(tp).c_str(), O_WRONLY | O_CREAT);// | O_APPEND);
            if (fd == -1) 
            {
                SPDLOG_ERROR("Error opening file: " + std::get<0>(tp));
                return; 
            }
        
            std::string impl = "#time,accel_x,accel_y,accel_z\n";
            write(fd,impl.c_str(),impl.size());
            // std::vector<std::shared_ptr<AccelMeasurement>> samples = get_samples();
            SPDLOG_DEBUG("__func__:{} #1 samples.size:{}",__func__,std::get<1>(tp).size());
            for(auto sample : std::get<1>(tp))
            {
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(6) << sample.time << "," << sample.accel_x << "," << sample.accel_y << "," << sample.accel_z << "\n";
                write(fd,oss.str().c_str(),oss.str().size());
            }
        
            if (close(fd) == -1) 
            {
                SPDLOG_ERROR("Error closing file: " + std::get<0>(tp));
            }
        }

        void AccelQueryHelper::write_to_file(std::string filename)
        {
            std::unique_ptr<std::thread> accel_query_thread(new std::thread(&AccelQueryHelper::write_impl, this, (std::tuple<std::string,std::vector<AccelMeasurement>>){filename,get_samples()}));
            accel_query_thread->detach();
            // accel_query_thread->join();
        }
    
        // Helper class for G-Code commands
        AccelCommandHelper::AccelCommandHelper(std::shared_ptr<ConfigWrapper> config, std::shared_ptr<AccelChip> chip)
        {
            this->printer = config->get_printer();
            this->chip = chip;
            this->bg_client = {};
            std::vector<std::string> name_parts = elegoo::common::split(config->get_name());
            this->base_name = name_parts.front();
            this->name = name_parts.back();
            register_commands(this->name);
            if(name_parts.size() == 1)
            {
                if("adxl345" == this->name || !config->has_section("adxl345"))
                {
                    register_commands("");
                }
            }
        }
        
        AccelCommandHelper::~AccelCommandHelper()
        {
            SPDLOG_DEBUG("~AccelCommandHelper");
        }

        void AccelCommandHelper::register_commands(std::string name)
        {
            std::shared_ptr<GCodeDispatch> gcode = 
                    any_cast<std::shared_ptr<GCodeDispatch>>(this->printer->lookup_object("gcode"));
            gcode->register_mux_command("ACCELEROMETER_MEASURE", "CHIP", name,
                    [this](std::shared_ptr<GCodeCommand> gcmd){
                        cmd_ACCELEROMETER_MEASURE(gcmd);
                    },
                    this->cmd_ACCELEROMETER_MEASURE_help);
            gcode->register_mux_command("ACCELEROMETER_QUERY", "CHIP", name,
                    [this](std::shared_ptr<GCodeCommand> gcmd){
                        cmd_ACCELEROMETER_QUERY(gcmd);
                    },
                    this->cmd_ACCELEROMETER_QUERY_help);
            gcode->register_mux_command("ACCELEROMETER_DEBUG_READ", "CHIP", name,
                    [this](std::shared_ptr<GCodeCommand> gcmd){
                        cmd_ACCELEROMETER_DEBUG_READ(gcmd);
                    },
                    this->cmd_ACCELEROMETER_DEBUG_READ_help);
            gcode->register_mux_command("ACCELEROMETER_DEBUG_WRITE", "CHIP", name,
                    [this](std::shared_ptr<GCodeCommand> gcmd){
                        cmd_ACCELEROMETER_DEBUG_WRITE(gcmd);
                    },
                    this->cmd_ACCELEROMETER_DEBUG_WRITE_help);
        }
        
        void AccelCommandHelper::cmd_ACCELEROMETER_MEASURE(std::shared_ptr<GCodeCommand> gcmd)
        {
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            if(!this->bg_client)
            {
                // Start measurements
                this->bg_client = this->chip->start_internal_client();
                gcmd->respond_info("accelerometer measurements started",true);
                return ;
            }
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            // End measurements
            char buffer[20] = {};
            std::time_t t = std::time(nullptr);
            std::strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", std::localtime(&t));
            std::string time_fmt(buffer);
            std::string name = gcmd->get("NAME",time_fmt);
            std::string check_name = {};
            std::string charsToRemove = "-_";
            for (auto ch : name)
            {
                if (charsToRemove.find(ch) == std::string::npos) {
                    check_name += ch;
                }
            }
            if(!std::all_of(check_name.begin(), check_name.end(), ::isalnum))
            {
                SPDLOG_ERROR("Invalid NAME parameter");
                throw elegoo::common::CommandError("Invalid NAME parameter");
            }
            std::shared_ptr<AccelQueryHelper> bg_client = this->bg_client;
            this->bg_client = nullptr;
            bg_client->finish_measurements();
            // Write data to file
            std::string filename = {};
            if(this->base_name == this->name)
            {
                filename = "/tmp/" + this->base_name + "-" + this->name + ".csv";
            }
            else
            {
                filename = "/tmp/" + this->base_name + "-" + this->name + "-" + name + ".csv";
            }
            SPDLOG_DEBUG("__func__:{} #1 filename:{}",__func__,filename);
            bg_client->write_to_file(filename);
            bg_client = nullptr;
            gcmd->respond_info("Writing raw accelerometer data to " + filename + " file",true);
        }

        void AccelCommandHelper::cmd_ACCELEROMETER_QUERY(std::shared_ptr<GCodeCommand> gcmd)
        {
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            std::shared_ptr<AccelQueryHelper> aclient = this->chip->start_internal_client();
            std::shared_ptr<ToolHead> toolhead = 
                    any_cast<std::shared_ptr<ToolHead>>(this->printer->lookup_object("toolhead"));
            toolhead->dwell(1.);
            aclient->finish_measurements();
            std::vector<AccelMeasurement> values = aclient->get_samples();
            if(!values.size())
            {
                SPDLOG_ERROR("No accelerometer measurements found");
                throw elegoo::common::CommandError("No accelerometer measurements found");
            }
            AccelMeasurement aaccel_meas = values.back();
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(6); 
            oss << "accelerometer values (x, y, z): " << aaccel_meas.accel_x << "," << aaccel_meas.accel_y << "," << aaccel_meas.accel_z;
            gcmd->respond_info(oss.str(),true);
        }

        void AccelCommandHelper::cmd_ACCELEROMETER_DEBUG_READ(std::shared_ptr<GCodeCommand> gcmd)
        {
            uint8_t reg = gcmd->get_int("REG",INT_NONE,0,126);
            uint8_t val = this->chip->read_reg(reg);
            std::ostringstream oss;
            oss << std::hex << "Accelerometer REG[0x" << reg << "] = 0x" << val;
            printf("__func__:%s,reg:0x%02x,val:0x%02x\n",__func__,reg,val);
            gcmd->respond_info(oss.str(),true);
        }

        void AccelCommandHelper::cmd_ACCELEROMETER_DEBUG_WRITE(std::shared_ptr<GCodeCommand> gcmd)
        {
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            uint8_t reg = gcmd->get_int("REG",INT_NONE,0,126);
            uint8_t val = gcmd->get_int("VAL",INT_NONE,0,255);
            printf("__func__:%s,__LINE__:%d,reg:0x%02x,val:0x%02x\n",__func__,__LINE__,reg,val);
            this->chip->set_reg(reg,val);
        }

        // Helper to read the axes_map parameter from the config
        std::vector<std::tuple<int32_t,double>> read_axes_map(std::shared_ptr<ConfigWrapper> config,double scale_x,double scale_y,double scale_z)
        {
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            std::map<std::string,std::pair<int32_t,double>> am = {
                {"x", {0, scale_x}}, 
                {"y", {1, scale_y}}, 
                {"z", {2, scale_z}},
                {"-x", {0, -scale_x}}, 
                {"-y", {1, -scale_y}}, 
                {"-z", {2, -scale_z}}
            };
            std::vector<std::string> dvalue = {"x","y","z"};
            std::vector<std::string> axes_map = config->getlist("axes_map", dvalue);
            std::vector<std::tuple<int32_t,double>> axes_mp = {};
            bool is_valid = false;

            for(auto axes : axes_map)
            {
                axes = elegoo::common::strip(axes);
                auto it = am.find(axes);
                if(it == am.end())
                {
                    continue;
                }
                std::tuple<int32_t,double> tup(am[axes].first,am[axes].second);
                SPDLOG_DEBUG("__func__:{},axes:{} {} {}",__func__,axes,am[axes].first,am[axes].second);
                axes_mp.emplace_back(tup);
                is_valid = true;
            }

            if(!is_valid)
            {
                SPDLOG_ERROR("Invalid axes_map parameter");
                throw elegoo::common::CommandError("Invalid axes_map parameter");
            }

            for(auto axes : axes_mp)
            {
                SPDLOG_DEBUG("__func__:{},std::get<0>(axes):{},std::get<1>(axes):{}",__func__,std::get<0>(axes),std::get<1>(axes));
            }
            return axes_mp;
        }

        static double BATCH_UPDATES = 0.100;

        // Printer class that controls ADXL345 chip
        Adxl345::Adxl345(std::shared_ptr<ConfigWrapper> config)
        {
            this->printer = config->get_printer();
            this->axes_map = read_axes_map(config,SCALE_XY,SCALE_XY,SCALE_Z);
            this->data_rate = config->getint("rate",3200);
            auto it = QUERY_RATES.find(this->data_rate);
            if(it == QUERY_RATES.end())
            {
                SPDLOG_ERROR("Invalid rate parameter:{}" + std::to_string(this->data_rate));
                throw elegoo::common::CommandError("Invalid rate parameter:{}" + std::to_string(this->data_rate));
            }

            // Setup mcu sensor_adxl345 bulk query code
            this->spi = MCU_SPI_from_config(config, 3, "cs_pin", 5000000);
            this->mcu = this->spi->get_mcu();
            uint32_t oid = this->oid = mcu->create_oid();
            this->query_adxl345_cmd = {};
            this->mcu->add_config_cmd("config_adxl345 oid=%d " + std::to_string(oid) + "spi_oid=%d" + std::to_string(this->spi->get_oid()));
            this->mcu->add_config_cmd("query_adxl345 oid=%d " + std::to_string(oid) + "rest_ticks=0" + std::to_string(oid),false,true);
            this->mcu->register_config_callback(
                    [this]()
                    {
                        _build_config();
                    });

            // Bulk sample message reading
            double chip_smooth = this->data_rate * BATCH_UPDATES * 2;
            this->ffreader = std::make_shared<FixedFreqReader>(mcu,chip_smooth,"BBBBB");
            this->last_error_count = 0;

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
            std::vector<std::string> split_name = elegoo::common::split(config->get_name());
            this->name = split_name.back();
            std::vector<std::string> hdr = {"time","x_acceleration","y_acceleration","z_acceleration"};
            json resp = {};
            resp["header"] = hdr;
            this->batch_bulk->add_mux_endpoint("adxl345/dump_adxl345", "sensor",this->name, resp);
        }

        Adxl345::~Adxl345()
        {
            SPDLOG_DEBUG("~Adxl345");
        }
        
        void Adxl345::init(std::shared_ptr<ConfigWrapper> config)
        {
            std::make_shared<AccelCommandHelper>(config,shared_from_this());
        }

        void Adxl345::_build_config()
        {
            std::shared_ptr<command_queue> cmdqueue = this->spi->get_command_queue();
            this->query_adxl345_cmd = this->mcu->lookup_command("query_adxl345 oid=%c rest_ticks=%u", cmdqueue);
            this->ffreader->setup_query_command("query_adxl345_status oid=%c",this->oid, cmdqueue);
        }

        uint8_t Adxl345::read_reg(uint8_t reg)
        {
            std::vector<uint8_t> data;
            data.emplace_back(reg | REG_MOD_READ);
            data.emplace_back(0x00);
            json params = this->spi->spi_transfer(data);
            if (params.contains("response") && params["response"].is_string())
            {
                std::string resp = params["response"].get<std::string>();
                std::vector<uint8_t> response(resp.begin(),resp.end());
                return response[1];
            }
            else
            {
                return -1;
            }
        }

        void Adxl345::set_reg(uint8_t reg, uint8_t val,uint64_t minclock)
        {
            std::vector<uint8_t> data;
            data.emplace_back(reg);
            data.emplace_back(val & 0xFF);
            this->spi->spi_send(data,minclock);
            uint8_t stored_val = read_reg(reg);
            if(stored_val != val)
            {
                std::ostringstream oss;
                oss << std::hex;
                oss << "Failed to set ADXL345 register [0x" << reg << "] to 0x" << val << ": got 0x" << stored_val << ". ";
                std::string failed = oss.str() + "This is generally indicative of connection problems " + "(e.g. faulty wiring) or a faulty adxl345 chip.";
                SPDLOG_ERROR(failed);
                throw elegoo::common::CommandError(failed);
            }
        }

        std::shared_ptr<AccelQueryHelper> Adxl345::start_internal_client()
        {
            std::shared_ptr<AccelQueryHelper> aqh = std::make_shared<AccelQueryHelper>(this->printer);
            this->batch_bulk->add_client(
                    [aqh](Any msg)->bool
                    {
                        return aqh->handle_batch(msg);
                    });
            return aqh;
        }

        json Adxl345::get_accel_status()
        {
            return this->accel_status;
        }

        // Measurement decoding
        void Adxl345::_convert_samples(std::vector<FixedFreqReader::FixedFreqReaderSamples> &samples)
        {
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
                if(++ii >= 3)
                {
                    break;
                }
            }

            for(auto sample : samples)
            {
                uint32_t xlow, ylow, zlow, xzhigh, yzhigh;
                double ptime = sample.ptime;
                std::vector<uint8_t> data = any_cast<std::vector<uint8_t>>(sample.data);
                if(data.size() != strlen("BBBBB"))
                {
                    SPDLOG_ERROR("data.size() != strlen('BBBBB')");
                    throw elegoo::common::CommandError("data.size() != strlen('BBBBB')");
                }
                else
                {
                    xlow = data[0];
                    ylow = data[1];
                    zlow = data[2];
                    xzhigh = data[3];
                    yzhigh = data[4];
                }

                if(yzhigh & 0x80)
                {
                    this->last_error_count += 1;
                    continue;
                }

                uint32_t rx,ry,rz;
                rx = (xlow | ((xzhigh & 0x1f) << 8)) - ((xzhigh & 0x10) << 9);
                ry = (ylow | ((yzhigh & 0x1f) << 8)) - ((yzhigh & 0x10) << 9);
                rz = ((zlow | ((xzhigh & 0xe0) << 3) | ((yzhigh & 0xe0) << 6)) - ((yzhigh & 0x40) << 7));
                std::vector<uint32_t> raw_xyz = {rx,ry,rz};
                double x = static_cast<long long>(round(raw_xyz[pos[0]] * scale[0] * 1000000))/1000000.;
                double y = static_cast<long long>(round(raw_xyz[pos[1]] * scale[1] * 1000000))/1000000.;
                double z = static_cast<long long>(round(raw_xyz[pos[2]] * scale[2] * 1000000))/1000000.;
                double pt = static_cast<long long>(round(ptime * 1000000))/1000000.;
                std::vector<double> dat = {x,y,z};

                samples[count].ptime = pt;
                samples[count].data = dat;
                count += 1;
            }
            samples.erase(samples.begin() + count,samples.end());
        }

        // Start, stop, and process message batches
        void Adxl345::_start_measurements()
        {
            // In case of miswiring, testing ADXL345 device ID prevents treating
            // noise or wrong signal as a correctly initialized device
            uint8_t dev_id = read_reg(REG_DEVID);
            if(dev_id != ADXL345_DEV_ID)
            {
                std::ostringstream oss;
                oss << std::hex;
                oss << "Invalid adxl345 id (got 0x" << dev_id << " vs 0x" << ADXL345_DEV_ID << ").\n";
                std::string err = oss.str() + 
                        "This is generally indicative of connection problems\n" + 
                        "(e.g. faulty wiring) or a faulty adxl345 chip.";
                SPDLOG_ERROR(err);
                throw elegoo::common::CommandError(err);
            }

            // Setup chip in requested query rate
            set_reg(REG_POWER_CTL, 0x00);
            set_reg(REG_DATA_FORMAT, 0x0B);
            set_reg(REG_FIFO_CTL, 0x00);
            set_reg(REG_BW_RATE, QUERY_RATES[this->data_rate]);
            set_reg(REG_FIFO_CTL, SET_FIFO_CTL);

            // Start bulk reading
            uint64_t rest_ticks = this->mcu->seconds_to_clock(4. / this->data_rate);
            this->query_adxl345_cmd->send({
                    std::to_string(this->oid), 
                    std::to_string(rest_ticks)
                });
            set_reg(REG_POWER_CTL, 0x08);
            SPDLOG_INFO("ADXL345 starting '{}' measurements",this->name);

            // Initialize clock tracking
            this->ffreader->note_start();
            this->last_error_count = 0;
        }

        void Adxl345::_finish_measurements()
        {
            // Halt bulk reading
            this->set_reg(REG_POWER_CTL, 0x00);
            this->query_adxl345_cmd->send_wait_ack({
                    std::to_string(this->oid), 
                    std::to_string(0)
                });
            this->ffreader->note_end();
            SPDLOG_INFO("ADXL345 finished '{}' measurements", this->name);
        }

        Any Adxl345::_process_batch(double eventtime)
        {
            std::vector<FixedFreqReader::FixedFreqReaderSamples> samples = this->ffreader->pull_samples();
            _convert_samples(samples);
            if(!samples.size())
            {
                return {};
            }

            // auto first_sample_time = samples.begin()->ptime;
            // auto last_sample_time = samples.end()->ptime;
            // for(auto sample : samples)
            // {
            //     printf("%.2f ",sample.ptime);
            // }
            // printf("__func__:%s,__LINE__:%d samples.size:%d\n",__func__,__LINE__,samples.size());
            // SPDLOG_DEBUG("__func__:{} #1 {} {}",__func__,first_sample_time,last_sample_time);

            std::map<std::string,Any> msg;
            msg["data"] = samples;
            msg["errors"] = this->last_error_count;
            msg["overflows"] = this->ffreader->get_last_overflows();

            return std::move(msg);
        }
        
        std::shared_ptr<Adxl345> adxl345_load_config(std::shared_ptr<ConfigWrapper> config)
        {
            std::shared_ptr<Adxl345> adxl345 = std::make_shared<Adxl345>(config);
            adxl345->init(config);
            return adxl345;
        }

        std::shared_ptr<Adxl345> adxl345_load_config_prefix(std::shared_ptr<ConfigWrapper> config)
        {
            std::shared_ptr<Adxl345> adxl345 = std::make_shared<Adxl345>(config);
            adxl345->init(config);
            return adxl345;
        }
    }
}
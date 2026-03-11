/*****************************************************************************
 * @Author       : Ben
 * @Date         : 2024-11-28 20:15:20
 * @LastEditors  : loping
 * @LastEditTime : 2025-09-27 15:10:32
 * @Description  : Tracking of PWM controlled heaters and their temperature control
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "heaters.h"
#include "printer.h"
// #include <filesystem>
#include <fstream>
#include "extras/thermistor.h"
#include "print_stats.h"
// #undef SPDLOG_DEBUG
// #define SPDLOG_DEBUG SPDLOG_INFO

const double KELVIN_TO_CELSIUS = -273.15;
const double MAX_HEAT_TIME = 5.0;
const double AMBIENT_TEMP = 25.;
const double PID_PARAM_BASE = 255.;
const double TUNE_PID_DELTA = 5.0;
const double MAX_MAINTHREAD_TIME = 5.0;

namespace elegoo
{
    namespace extras
    {
        SensorFactory::SensorFactory(std::string name, std::shared_ptr<LinearBase> linear)
        {
            sensor_name = name;
            if (linear)
            {
                this->linear_ = linear;
            }
        }

        SensorFactory::~SensorFactory()
        {
        }

        std::shared_ptr<HeaterBase> SensorFactory::create_sensor(
            std::shared_ptr<ConfigWrapper> config)
        {
            SPDLOG_DEBUG("__func__:{},sensor_name:{}", __func__, sensor_name);
            if (sensor_name == "AHT10")
            {
                return nullptr;
            }
            else if (sensor_name == "BME280")
            {
                return nullptr;
            }
            else if (sensor_name == "DS18B20")
            {
                return nullptr;
            }
            else if (sensor_name == "PrinterADCtoTemperature")
            {
                SPDLOG_DEBUG("__func__:{},sensor_name:{}", __func__, sensor_name);
                return std::dynamic_pointer_cast<HeaterBase>(std::make_shared<PrinterADCtoTemperature>(config, this->linear_));
            }
            else
            {
                return nullptr;
            }
            return nullptr;
        }

        Heater::Heater(std::shared_ptr<ConfigWrapper> config,
                       std::shared_ptr<HeaterBase> sensor)
            : sensor(sensor), last_temp(0), smoothed_temp(0),
              target_temp(0), last_temp_time(0), next_pwm_time(0),
              last_pwm_value(0)
        {
            printer = config->get_printer();
            gcode = any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));
            name = config->get_name();
            SPDLOG_INFO("Heater init! name: {}", name);
            auto pos = name.find_last_of(' ');
            short_name = (pos != std::string::npos) ? name.substr(pos + 1) : name;

            min_temp = config->getdouble("min_temp", DOUBLE_INVALID, KELVIN_TO_CELSIUS);
            max_temp = config->getdouble("max_temp", DOUBLE_INVALID, DOUBLE_NONE, DOUBLE_NONE, min_temp);
            ot_bed = config->getdouble("ot_bed", 130.);

            std::shared_ptr<PrinterADCtoTemperature> heater_sensor = std::dynamic_pointer_cast<PrinterADCtoTemperature>(sensor);
            if (!heater_sensor && !sensor)
            {
                SPDLOG_DEBUG("__func__:{},name:{}", __func__, name);
            }
            SPDLOG_DEBUG("__func__:{},name:{},min_temp:{},max_temp:{}", __func__, name, min_temp, max_temp);
            heater_sensor->setup_minmax(min_temp, max_temp);
            heater_sensor->setup_callback(
                [this](double read_time, double temp)
                {
                    temperature_callback(read_time, temp);
                });

            pwm_delay = heater_sensor->get_report_time_delta();
            min_extrude_temp = config->getdouble("min_extrude_temp", 170,
                                                 min_temp, max_temp);

            auto start_args = printer->get_start_args();
            bool is_fileoutput = (start_args.find("debugoutput") != start_args.end());

            can_extrude = (min_extrude_temp <= 0.0f) || is_fileoutput;
            max_power = config->getdouble("max_power", 1, DOUBLE_NONE, 1, 0);
            smooth_time = config->getdouble("smooth_time",
                                            1, DOUBLE_NONE, DOUBLE_NONE, 0);
            inv_smooth_time = 1.0f / smooth_time;
            is_shutdown = false;
            std::string heater_pin = config->get("heater_pin","");

            SPDLOG_INFO("__func__:{},heater_pin:{}", __func__, heater_pin);
            std::shared_ptr<PrinterPins> ppins =
                any_cast<std::shared_ptr<PrinterPins>>(printer->lookup_object("pins"));
            if(!heater_pin.empty())
            {
                mcu_pwm = std::dynamic_pointer_cast<MCU_pwm>(ppins->setup_pin("pwm", heater_pin));
                double pwm_cycle_time = config->getdouble("pwm_cycle_time", 0.1, DOUBLE_NONE,
                                                        pwm_delay, 0);
                mcu_pwm->setup_cycle_time(pwm_cycle_time);
                mcu_pwm->setup_max_duration(MAX_HEAT_TIME);
            }
            // printer->load_object(config, "verify_heater " + short_name);
            printer->load_object(config, "pid_calibrate");
            SPDLOG_DEBUG("__func__:{},short_name:{}", __func__, short_name);
            gcode->register_mux_command("SET_HEATER_TEMPERATURE", "HEATER", short_name, [this](std::shared_ptr<GCodeCommand> gcmd)
                                        { cmd_SET_HEATER_TEMPERATURE(gcmd); }, "Sets a heater temperature");
            this->is_verify_heater_fault = false;
            this->is_adc_temp_fault = false;
            elegoo::common::SignalManager::get_instance().register_signal(
                "adc_temp:fault",
                std::function<void(std::string heater_name)>([this](std::string heater_name) {
                    if (printer->is_shutdown())
                    {
                        return;
                    } 

                    if(heater_name == this->get_name())
                    {
                        is_adc_temp_fault = true;
                        if(heater_name == "heater_bed")
                        {
                            gcode->respond_ecode("adc temperature fault", elegoo::common::ErrorCode::ADC_TEMPERATURE_HEATED_BED, 
                                elegoo::common::ErrorLevel::WARNING);
                        }
                        else if(heater_name == "extruder")
                        {
                            gcode->respond_ecode("adc temperature fault", elegoo::common::ErrorCode::ADC_TEMPERATURE_EXTRUDER, 
                                elegoo::common::ErrorLevel::WARNING);

                        }
                        else
                        {
                            SPDLOG_INFO("adc_temp:fault heater_name:{} is_adc_temp_fault {} return",heater_name, is_adc_temp_fault);
                            return;
                        }

                        std::shared_ptr<PrintStats> print_stats = 
                            any_cast<std::shared_ptr<PrintStats>>(printer->lookup_object("print_stats", std::shared_ptr<PrintStats>()));
            
                        if(print_stats && print_stats->get_status(get_monotonic())["state"] == "printing") 
                        {
                            SPDLOG_INFO("gcode->run_script PAUSE {} {}",heater_name, is_adc_temp_fault);
                            gcode->run_script("PAUSE\n");
                            gcode->respond_ecode("adc temperature fault", elegoo::common::ErrorCode::ADC_TEMPERATURE_EXTRUDER, 
                                elegoo::common::ErrorLevel::WARNING);
                        }
                        SPDLOG_INFO("adc_temp:fault heater_name:{} is_adc_temp_fault {}",heater_name, is_adc_temp_fault);
                    }
                }));
            elegoo::common::SignalManager::get_instance().register_signal(
                "adc_temp:normal",
                std::function<void(std::string heater_name)>([this](std::string heater_name) {
                    if(heater_name == this->get_name())
                    {
                        is_adc_temp_fault = false;
                        if(heater_name == "heater_bed")
                        {
                            gcode->respond_ecode("adc temperature fault", elegoo::common::ErrorCode::ADC_TEMPERATURE_HEATED_BED, 
                                elegoo::common::ErrorLevel::RESUME);
                        }
                        else if(heater_name == "extruder")
                        {
                            gcode->respond_ecode("adc temperature fault", elegoo::common::ErrorCode::ADC_TEMPERATURE_EXTRUDER, 
                                elegoo::common::ErrorLevel::RESUME);

                        }
                        SPDLOG_INFO("adc_temp:normal heater_name:{} is_adc_temp_fault {}",heater_name, is_adc_temp_fault);
                    }
                }));
            elegoo::common::SignalManager::get_instance().register_signal(
                "verify_heater:fault",
                std::function<void(std::string)>([this](std::string heater_name) {
                    if (printer->is_shutdown())
                    {
                        return;
                    }   

                    if(heater_name == this->get_name())
                    {
                        is_verify_heater_fault = true;
                        if(heater_name == "heater_bed")
                        {
                            gcode->respond_ecode("Heater heater_bed not heating at expected rate", elegoo::common::ErrorCode::VERIFY_HEATER_HEATED_BED, 
                                elegoo::common::ErrorLevel::WARNING);
                        }
                        else if(heater_name == "extruder")
                        {
                            gcode->respond_ecode("Heater extruder not heating at expected rate", elegoo::common::ErrorCode::VERIFY_HEATER_EXTRUDER,
                                elegoo::common::ErrorLevel::WARNING);
                        }

                        std::shared_ptr<PrintStats> print_stats = 
                            any_cast<std::shared_ptr<PrintStats>>(printer->lookup_object("print_stats", std::shared_ptr<PrintStats>()));
            
                        if(print_stats && print_stats->get_status(get_monotonic())["state"] == "printing") 
                        {
                            gcode->run_script("PAUSE\n");
                        }
                        SPDLOG_INFO("verify_heater:fault heater_name:{} is_verify_heater_fault {}",heater_name, is_verify_heater_fault);
                    }
                }));
            elegoo::common::SignalManager::get_instance().register_signal(
                "verify_heater:normal",
                std::function<void(std::string)>([this](std::string heater_name) {
                    if(heater_name == this->get_name())
                    {
                        is_verify_heater_fault = false;
                        if(heater_name == "heater_bed")
                        {
                            gcode->respond_ecode("Heater heater_bed not heating at expected rate", elegoo::common::ErrorCode::VERIFY_HEATER_HEATED_BED, 
                                elegoo::common::ErrorLevel::RESUME);
                        }
                        else if(heater_name == "extruder")
                        {
                            gcode->respond_ecode("Heater extruder not heating at expected rate", elegoo::common::ErrorCode::VERIFY_HEATER_EXTRUDER,
                                elegoo::common::ErrorLevel::RESUME);
                        }
                        SPDLOG_INFO("verify_heater:normal heater_name:{} is_verify_heater_fault {}",heater_name, is_verify_heater_fault);
                    }
                }));
            elegoo::common::SignalManager::get_instance().register_signal(
                "elegoo:shutdown",
                std::function<void()>([this]()
                                      {
            SPDLOG_DEBUG("elegoo:shutdown !");
            handle_shutdown();
            SPDLOG_DEBUG("elegoo:shutdown !"); }));
            SPDLOG_WARN("Heater init success!!!!");
        }

        Heater::~Heater()
        {
        }

        void Heater::set_pwm(double read_time, double value)
        {
            if(is_adc_temp_fault)// || is_verify_heater_fault)
            {
                // SPDLOG_INFO("is_adc_temp_fault is {} {} name:{},value:{}",is_adc_temp_fault,is_verify_heater_fault,this->name,value);
                value = 0.0;
            }
            
            if (target_temp <= 0.0 || is_shutdown)
            {
                // SPDLOG_DEBUG("read_time:{},value:{},next_pwm_time:{},last_pwm_value:{},is_shutdown:{},this->name:{}",read_time,value,next_pwm_time,last_pwm_value,is_shutdown,this->name);
                value = 0.0;
            }

            if ((read_time < next_pwm_time || last_pwm_value == 0.0) &&
                std::abs(value - last_pwm_value) < 0.05)
            {
                // SPDLOG_INFO("this->name:{},read_time:{},value:{},next_pwm_time:{},last_pwm_value:{}",this->name,read_time,value,next_pwm_time,last_pwm_value);
                return;
            }
            
            double pwm_time = read_time + pwm_delay;
            next_pwm_time = pwm_time + 0.75 * MAX_HEAT_TIME;
            last_pwm_value = value;
            // SPDLOG_INFO("this->name:{},is_shutdown:{},target_temp:{},last_temp:{},read_time:{},value:{},last_pwm_value:{},next_pwm_time:{},pwm_time:{}",this->name, is_shutdown, target_temp, last_temp, read_time, value, last_pwm_value, next_pwm_time, pwm_time);
            mcu_pwm->set_pwm(pwm_time, value);
        }

        void Heater::setup_callback(std::function<void(double, double)> callback)
        {
            std::lock_guard<std::mutex> lock(this->lock);
            callbacks.push_back(callback);
        }

        void Heater::temperature_callback(double read_time, double temp)
        {
            if(name == "heater_bed")
            {
                if(temp > this->ot_bed)
                {
                    if(false == is_ot_bed_report)
                    {
                        is_ot_bed_report = true;
                        SPDLOG_INFO("{} name:{} temp:{} ot_bed:{} is_ot_bed_report:{}",__func__,name,temp,ot_bed,is_ot_bed_report);
                        gcode->respond_ecode("OT_BED", elegoo::common::ErrorCode::OT_BED, 
                            elegoo::common::ErrorLevel::WARNING );
                    }
                }
                else
                {
                    is_ot_bed_report = false;
                }
            }
            std::lock_guard<std::mutex> lock(this->lock); // 自动管理锁的作用域
            double time_diff = read_time - last_temp_time;
            last_temp = temp;
            last_temp_time = read_time;
            control->temperature_update(read_time, temp, target_temp);

            double temp_diff = temp - smoothed_temp;
            double adj_time = std::min(time_diff * inv_smooth_time, 1.0);
            smoothed_temp += temp_diff * adj_time;
            // 调用
            for (auto &callback : callbacks)
                callback(read_time, smoothed_temp);
            can_extrude = (smoothed_temp >= min_extrude_temp);

            // 调试
            #define HEATER_DEBUG_PRINTER_TIME (2.) //  (10.)  // 
            if (last_read_time == 0. || read_time - last_read_time > HEATER_DEBUG_PRINTER_TIME * 60.)
            {
                SPDLOG_INFO("__func__:{},heater_name:{},read_time:{},last_temp:{},temp:{},target_temp:{},smoothed_temp:{}",__func__,this->name,read_time,last_temp,temp,target_temp,smoothed_temp);
                last_read_time = read_time;
            }
            // SPDLOG_DEBUG("__func__:{},can_extrude:{},smoothed_temp:{},min_extrude_temp:{}\n\n\n",__func__,can_extrude,smoothed_temp,min_extrude_temp);
        }

        std::string Heater::get_name()
        {
            return name;
        }

        double Heater::get_pwm_delay()
        {
            return pwm_delay;
        }

        double Heater::get_max_power()
        {
            return max_power;
        }

        double Heater::get_smooth_time()
        {
            return smooth_time;
        }

        void Heater::set_temp(double degrees)
        {
            if (degrees && (degrees < min_temp || degrees > max_temp))
            {
                throw elegoo::common::CommandError(
                    "Requested temperature (" + std::to_string(degrees) +
                    ") out of range (" + std::to_string(min_temp) +
                    ":" + std::to_string(max_temp) + ")");
            }
            std::lock_guard<std::mutex> lock(this->lock);
            target_temp = degrees;
            // SPDLOG_DEBUG("__func__:{},target_temp:{}", __func__, target_temp);
        }

        std::pair<double, double> Heater::get_temp(double eventtime)
        {
            double print_time = mcu_pwm->get_mcu()->estimated_print_time(eventtime) - 5.0;

            std::lock_guard<std::mutex> lock(this->lock);

            if (last_temp_time < print_time)
            {
                // SPDLOG_DEBUG("__func__:{},smoothed_temp:{},target_temp:{}", __func__, smoothed_temp, target_temp);
                return std::make_pair(0.0, target_temp);
            }
            // SPDLOG_DEBUG("__func__:{},smoothed_temp:{},target_temp:{}",__func__,smoothed_temp,target_temp);
            return std::make_pair(smoothed_temp, target_temp);
        }

        bool Heater::check_busy(double eventtime)
        {
            std::lock_guard<std::mutex> lock(this->lock);
            return control->check_busy(eventtime, smoothed_temp, target_temp);
        }

        std::shared_ptr<ControlBase> Heater::set_control(
            std::shared_ptr<ControlBase> control)
        {
            std::lock_guard<std::mutex> lock(this->lock);
            std::shared_ptr<ControlBase> old_control = this->control;
            this->control = control;
            this->target_temp = 0.0;
            return old_control;
        }

        void Heater::alter_target(double target_temp)
        {
            if (target_temp)
            {
                target_temp = std::max(this->min_temp, std::min(this->max_temp, target_temp));
            }
            this->target_temp = target_temp;
        }

        std::pair<bool, std::string> Heater::stats(double eventtime)
        {
            double est_print_time = mcu_pwm->get_mcu()->estimated_print_time(eventtime);
            if (!printer->is_shutdown())
            {
                // ?
                // verify_mainthread_time = est_print_time + MAX_MAINTHREAD_TIME;
            }

            this->lock.lock();
            double target_temp = this->target_temp;
            double last_temp = this->last_temp;
            double last_pwm_value = this->last_pwm_value;
            bool is_active = target_temp > 0.0f || last_temp > 50.0f;
            this->lock.unlock();

            std::ostringstream result;
            result << this->short_name << ": target=" << target_temp
                   << " temp=" << last_temp << " pwm=" << last_pwm_value;

            return {is_active, result.str()};
        }

        json Heater::get_status(double eventtime)
        {
            this->lock.lock();
            double target_temp = this->target_temp;
            double last_temp = this->last_temp;
            double smoothed_temp = this->smoothed_temp;
            double last_pwm_value = this->last_pwm_value;
            bool is_adc_temp_fault = this->is_adc_temp_fault;
            bool is_verify_heater_fault = this->is_verify_heater_fault;
            this->lock.unlock();

            json status;
            status["temperature"] = round(smoothed_temp * 100.0f) / 100.0f;
            status["target"] = target_temp;
            status["last_temp"] = last_temp;
            status["power"] = last_pwm_value;
            status["is_adc_temp_fault"] = is_adc_temp_fault;
            status["is_verify_heater_fault"] = is_verify_heater_fault;
            // SPDLOG_DEBUG("Heater::get_status:{}",status.dump());
            return status;
        }

        void Heater::cmd_SET_HEATER_TEMPERATURE(std::shared_ptr<GCodeCommand> gcmd)
        {
            double temp = std::stod(gcmd->get("TARGET", "0"));
            SPDLOG_INFO("__func__:{}   gcmd->commandline:{},temp:{}", __func__, gcmd->commandline, temp);
            std::shared_ptr<PrinterHeaters> pheaters = any_cast<std::shared_ptr<PrinterHeaters>>(printer->lookup_object("heaters"));

            pheaters->set_temperature(shared_from_this(), temp);
        }

        void Heater::handle_shutdown()
        {
            is_shutdown = true;
            SPDLOG_INFO("{} : {}___ ok", __FUNCTION__, __LINE__);
        }

        ControlBangBang::ControlBangBang(std::shared_ptr<Heater> heater,
                                         std::shared_ptr<ConfigWrapper> config) : heater(heater), heating(false)
        {
            heater_max_power = heater->get_max_power();

            max_delta = config->getdouble("max_delta",
                                          2, DOUBLE_NONE, DOUBLE_NONE, 0);
        }

        ControlBangBang::~ControlBangBang()
        {
        }

        void ControlBangBang::temperature_update(
            double read_time, double temp, double target_temp)
        {
            if (heating && temp >= target_temp + max_delta)
            {
                heating = false;
            }
            else if (!heating && temp <= target_temp - max_delta)
            {
                heating = true;
            }
            // SPDLOG_DEBUG("heating:{},read_time:{},temp:{},target_temp:{},max_delta:{}", heating, read_time, temp, target_temp, max_delta);

            if (heating)
            {
                heater->set_pwm(read_time, heater_max_power);
            }
            else
            {
                heater->set_pwm(read_time, 0.0);
            }
        }

        bool ControlBangBang::check_busy(double eventtime,
                                         double smoothed_temp, double target_temp)
        {
            return smoothed_temp < target_temp - max_delta;
        }

        const double PID_SETTLE_DELTA = 5.;
        const double PID_SETTLE_SLOPE = .5;

        ControlPID::ControlPID(std::shared_ptr<Heater> heater,
                               std::shared_ptr<ConfigWrapper> config) : heater(heater)
        {
            heater_max_power = heater->get_max_power();
            Kp = config->getdouble("pid_Kp") / PID_PARAM_BASE;
            Ki = config->getdouble("pid_Ki") / PID_PARAM_BASE;
            Kd = config->getdouble("pid_Kd") / PID_PARAM_BASE;

            min_deriv_time = heater->get_smooth_time();
            temp_integ_max = 0.0;

            if (Ki != 0.0)
            {
                temp_integ_max = heater_max_power / Ki;
            }

            prev_temp = AMBIENT_TEMP;
            prev_temp_time = 0.0;
            prev_temp_deriv = 0.0;
            prev_temp_integ = 0.0;
        }

        ControlPID::~ControlPID()
        {
        }

        void ControlPID::temperature_update(
            double read_time, double temp, double target_temp)
        {
            double time_diff = read_time - prev_temp_time;

            double temp_diff = temp - prev_temp;

            // SPDLOG_DEBUG("time_diff:{},read_time:{},prev_temp_time:{},temp:{},target_temp:{},prev_temp:{},temp_diff:{}",time_diff,read_time,prev_temp_time,temp,target_temp,prev_temp,temp_diff);
            double temp_deriv;
            if (time_diff >= min_deriv_time)
            {
                temp_deriv = temp_diff / time_diff;
            }
            else
            {
                temp_deriv = (prev_temp_deriv * (min_deriv_time - time_diff) + temp_diff) / min_deriv_time;
            }

            double temp_err = target_temp - temp;
            double temp_integ = prev_temp_integ + temp_err * time_diff;

            temp_integ = std::max(0.0, std::min(temp_integ_max, temp_integ));

            // SPDLOG_DEBUG("temp_deriv:{},min_deriv_time:{},temp_err:{},temp_integ:{},temp_integ_max:{}",temp_deriv,min_deriv_time,temp_err,temp_integ,temp_integ_max);
            double co = Kp * temp_err + Ki * temp_integ - Kd * temp_deriv;

            double bounded_co = std::max(0.0, std::min(heater_max_power, co));

            // SPDLOG_DEBUG("co:{},bounded_co:{},Kp:{},temp_err:{},Ki:{},temp_integ:{},Kd:{},temp_deriv:{},heater_max_power:{}",co,bounded_co,Kp,temp_err,Ki,temp_integ,Kd,temp_deriv,heater_max_power);
            // Set PWM for the heater
            heater->set_pwm(read_time, bounded_co);

            // Store state for next measurement
            prev_temp = temp;
            prev_temp_time = read_time;
            prev_temp_deriv = temp_deriv;

            if (co == bounded_co)
            {
                prev_temp_integ = temp_integ;
            }
        }

        bool ControlPID::check_busy(double eventtime,
                                    double smoothed_temp, double target_temp)
        {
            double temp_diff = target_temp - smoothed_temp;
            return (std::abs(temp_diff) > PID_SETTLE_DELTA || std::abs(prev_temp_deriv) > PID_SETTLE_SLOPE);
        }

        ControlAutoTune::ControlAutoTune(std::shared_ptr<Heater> heater, double target)
            : heater(heater)
        {
            if (heater)
            {
                heater_max_power = heater->get_max_power();
            }
            // heater_max_power = heater->get_max_power();
            calibrate_temp = target;
            heating = false;
            peak = 0.;
            peak_time = 0.;
            last_pwm = 0;
        }

        ControlAutoTune::~ControlAutoTune()
        {
        }

        void ControlAutoTune::set_pwm(double read_time, double value)
        {
            if (value != last_pwm)
            {
                pwm_samples.push_back({read_time + heater->get_pwm_delay(), value});
                last_pwm = value;
            }
            heater->set_pwm(read_time, value);
        }

        void ControlAutoTune::temperature_update(double read_time, double temp, double target_temp)
        {
            temp_samples.push_back({read_time, temp});

            if (heating && temp >= target_temp)
            {
                heating = false;
                check_peaks();
                heater->alter_target(calibrate_temp - TUNE_PID_DELTA);
            }
            else if (!heating && temp <= target_temp)
            {
                heating = true;
                check_peaks();
                heater->alter_target(calibrate_temp);
            }

            if (heating)
            {
                set_pwm(read_time, heater_max_power);
                if (temp < peak)
                {
                    peak = temp;
                    peak_time = read_time;
                }
            }
            else
            {
                set_pwm(read_time, 0.0f);
                if (temp > peak)
                {
                    peak = temp;
                    peak_time = read_time;
                }
            }
        }

        bool ControlAutoTune::check_busy(double eventtime,
                                         double smoothed_temp, double target_temp)
        {
            if (heating || peaks.size() < 12)
            {
                return true;
            }
            return false;
        }

        void ControlAutoTune::check_peaks()
        {
            peaks.push_back(std::make_pair(peak, peak_time));

            if (heating)
            {
                peak = 9999999.0f;
            }
            else
            {
                peak = -9999999.0f;
            }

            if (peaks.size() < 4)
            {
                return;
            }

            calc_pid(peaks.size() - 1);
        }

        std::tuple<double, double, double> ControlAutoTune::calc_pid(int pos)
        {
            if (pos < 2 || pos >= peaks.size())
            {
                throw std::out_of_range("Invalid position for peak data");
            }

            double temp_diff = peaks[pos].first - peaks[pos - 1].first;
            double time_diff = peaks[pos].second - peaks[pos - 2].second;

            double amplitude = 0.5 * std::abs(temp_diff);
            double Ku = 4.0 * heater_max_power / (M_PI * amplitude);
            double Tu = time_diff;

            double Ti = 0.5 * Tu;
            double Td = 0.125 * Tu;
            double Kp = 0.6 * Ku * PID_PARAM_BASE;
            double Ki = Kp / Ti;
            double Kd = Kp * Td;

            // 打印日志信息
            std::cout << "Autotune: raw=" << temp_diff << "/" << heater_max_power
                      << " Ku=" << Ku << " Tu=" << Tu
                      << " Kp=" << Kp << " Ki=" << Ki << " Kd=" << Kd << std::endl;

            return std::make_tuple(Kp, Ki, Kd);
        }

        std::tuple<double, double, double> ControlAutoTune::calc_final_pid()
        {
            std::vector<std::pair<double, int>> cycle_times;
            for (int pos = 4; pos < peaks.size(); ++pos)
            {
                double cycle_time = peaks[pos].second - peaks[pos - 2].second;
                cycle_times.push_back(std::make_pair(cycle_time, pos));
            }

            std::sort(cycle_times.begin(), cycle_times.end(),
                      [](const std::pair<double, int> &a, const std::pair<double, int> &b)
                      {
                          return a.first < b.first;
                      });

            int midpoint_pos = cycle_times[cycle_times.size() / 2].second;

            return calc_pid(midpoint_pos);
        }

        void ControlAutoTune::write_file(const std::string &filename)
        {
            std::ofstream file(filename);

            if (!file.is_open())
            {
                std::cerr << "Error opening file for writing!" << std::endl;
                return;
            }

            for (const auto &sample : pwm_samples)
            {
                file << "pwm: " << sample.first << " " << sample.second << std::endl;
            }

            for (const auto &sample : temp_samples)
            {
                file << sample.first << " " << sample.second << std::endl;
            }

            file.close();
        }

        PrinterHeaters::PrinterHeaters(std::shared_ptr<ConfigWrapper> config)
        {
            SPDLOG_INFO("PrinterHeaters init!");
            printer = config->get_printer();
            has_started = have_load_sensors = false;
            elegoo::common::SignalManager::get_instance().register_signal(
                "elegoo:ready",
                std::function<void()>([this]()
                                      {
            SPDLOG_DEBUG("elegoo:ready !");
            handle_ready();
            SPDLOG_DEBUG("elegoo:ready !"); }));
            elegoo::common::SignalManager::get_instance().register_signal(
                "gcode:request_restart",
                std::function<void(double)>([this](double print_time)
                                            { turn_off_all_heaters(print_time); }));

            std::shared_ptr<GCodeDispatch> gcode =
                any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));
            gcode->register_command("TURN_OFF_HEATERS", [this](std::shared_ptr<GCodeCommand> gcmd)
                                    { cmd_TURN_OFF_HEATERS(gcmd); }, false, "Turn off all heaters");

            gcode->register_command("M105", [this](std::shared_ptr<GCodeCommand> gcmd)
                                    { cmd_M105(gcmd); }, true);

            gcode->register_command("TEMPERATURE_WAIT", [this](std::shared_ptr<GCodeCommand> gcmd)
                                    { cmd_TEMPERATURE_WAIT(gcmd); }, false, "Wait for a temperature on a sensor");
            SPDLOG_INFO("PrinterHeaters init success!");

            print_stop = false;
            elegoo::common::SignalManager::get_instance().register_signal(
                "gcode:CANCEL_PRINT_REQUEST",
                std::function<void()>([this]() {
                    print_stop = true;
                    SPDLOG_INFO("print_stop {}", print_stop);
                }));
            elegoo::common::SignalManager::get_instance().register_signal(
                "gcode:CANCEL_PRINT_DONE",
                std::function<void()>([this]() {
                    print_stop = false;
                    SPDLOG_INFO("print_stop {}", print_stop);
                }));
        }

        PrinterHeaters::~PrinterHeaters()
        {
        }

        void PrinterHeaters::load_config(std::shared_ptr<ConfigWrapper> config)
        {
            have_load_sensors = true;
            std::shared_ptr<PrinterConfig> pconfig =
                any_cast<std::shared_ptr<PrinterConfig>>(printer->lookup_object("configfile"));

            std::string file = __FILE__;
            size_t pos = file.find_last_of("/\\");
            std::string filename = OPT_INST "/temperature_sensors.cfg";
            SPDLOG_DEBUG("__func__:{},filename:{}", __func__, filename);
            try
            {
                std::shared_ptr<ConfigWrapper> dconfig = pconfig->read_config(filename);
                for (std::shared_ptr<ConfigWrapper> c : dconfig->get_prefix_sections(""))
                {
                    SPDLOG_INFO("c->get_name(): {}", c->get_name());
                    printer->load_object(dconfig, c->get_name());
                }
            }
            catch (const std::exception &e)
            {
                // throw config.config_error("Cannot load config '" + filename + "'");
            }
            SPDLOG_INFO("PrinterHeaters::load_config success!");
        }

        void PrinterHeaters::add_sensor_factory(const std::string &sensor_type,
                                                std::shared_ptr<SensorFactory> sensor_factory)
        {
            sensor_factories[sensor_type] = sensor_factory;
        }

        std::shared_ptr<Heater> PrinterHeaters::setup_heater(
            std::shared_ptr<ConfigWrapper> config,
            const std::string &gcode_id)
        {
            std::string heater_name = config->get_name();
            SPDLOG_DEBUG("__func__:{},heater_name:{}", __func__, heater_name);
            size_t pos = heater_name.find_last_of(' ');
            if (pos != std::string::npos)
            {
                heater_name = heater_name.substr(pos + 1);
            }

            if (heaters.find(heater_name) != heaters.end())
            {
                // config->error("Heater " + heater_name + " already registered");
                SPDLOG_WARN("__func__:{},heater_name:{} already registered", __func__, heater_name);
            }

            auto sensor = setup_sensor(config);
            auto heater = std::make_shared<Heater>(config, sensor);
            std::string algo = config->get("control","");
            if (algo == "watermark")
            {
                heater->control = std::make_shared<ControlBangBang>(heater, config);
            }
            else if (algo == "pid")
            {
                heater->control = std::make_shared<ControlPID>(heater, config);
            }
            SPDLOG_DEBUG("__func__:{},algo:{}", __func__, algo);
            heaters[heater_name] = heater;

            register_sensor(config, heater, gcode_id);
            SPDLOG_WARN("debug  name {}, gcode_id {}", config->get_name(), gcode_id);
            available_heaters.push_back(config->get_name());

            return heater;
        }

        std::vector<std::string> PrinterHeaters::get_all_heaters()
        {
            return available_heaters;
        }

        std::map<std::string, std::shared_ptr<Heater>> PrinterHeaters::get_heaters()
        {
            return heaters;
        }

        std::shared_ptr<Heater> PrinterHeaters::lookup_heater(const std::string &heater_name)
        {
            auto it = heaters.find(heater_name);
            if (it == heaters.end())
            {
                SPDLOG_INFO("Unknown heater '" + heater_name + "'");
                return nullptr;
                // throw std::runtime_error("Unknown heater '" + heater_name + "'");
            }
            return it->second;
        }

        std::shared_ptr<HeaterBase> PrinterHeaters::setup_sensor(
            std::shared_ptr<ConfigWrapper> config)
        {

            if (!have_load_sensors)
            {
                this->load_config(config);
            }

            std::string sensor_type = config->get("sensor_type");

            SPDLOG_DEBUG("__func__:{},sensor_type:{}", __func__, sensor_type);
            auto it = sensor_factories.find(sensor_type);
            if (it == sensor_factories.end())
            {
                SPDLOG_ERROR("Unknown temperature sensor '" + sensor_type + "'");
                // throw std::runtime_error("Unknown temperature sensor '" + sensor_type + "'");
                return nullptr;
            }
            SPDLOG_INFO("setup_sensor success!");
            return it->second->create_sensor(config);
        }

        void PrinterHeaters::register_sensor(
            std::shared_ptr<ConfigWrapper> config,
            std::shared_ptr<Heater> psensor,
            const std::string &gcode_id)
        {
            available_sensors.push_back(config->get_name());

            std::string gcode = gcode_id;
            if (gcode.empty())
            {
                gcode = config->get("gcode_id", "");
                if (gcode.empty())
                {
                    return;
                }
            }

            if (gcode_id_to_sensor.find(gcode) != gcode_id_to_sensor.end())
            {
                throw std::runtime_error("G-Code sensor id " + gcode + " already registered");
            }

            gcode_id_to_sensor[gcode] = psensor;
        }

        void PrinterHeaters::register_monitor(std::shared_ptr<ConfigWrapper> config)
        {
            available_monitors.push_back(config->get_name());
        }

        json PrinterHeaters::get_status(double eventtime)
        {
            json status;
            status["available_heaters"] = available_heaters;
            status["available_sensors"] = available_sensors;
            status["available_monitors"] = available_monitors;
            return status;
        }

        void PrinterHeaters::turn_off_all_heaters(double print_time)
        {
            for (auto &heater_pair : heaters)
            {
                heater_pair.second->set_temp(0.0);
            }
            SPDLOG_INFO("{} : {}___ ok", __FUNCTION__, __LINE__);
        }

        void PrinterHeaters::cmd_TURN_OFF_HEATERS(std::shared_ptr<GCodeCommand> gcmd)
        {
            turn_off_all_heaters();
            SPDLOG_INFO("{} : {}___ ok!", __FUNCTION__, __LINE__);
        }

        void PrinterHeaters::cmd_M105(std::shared_ptr<GCodeCommand> gcmd)
        {
            std::string msg = get_temp(get_monotonic());

            bool did_ack = gcmd->ack(msg);
            if (!did_ack)
            {
                gcmd->respond_raw(msg);
            }
            SPDLOG_INFO("{} : {}___ ok!", __FUNCTION__, __LINE__);
        }

        void PrinterHeaters::set_temperature(std::shared_ptr<Heater> heater, double temp, bool wait)
        {
            SPDLOG_DEBUG("__func__:{},temp:{},wait:{}", __func__, temp, wait);
            std::shared_ptr<ToolHead> toolhead = any_cast<std::shared_ptr<ToolHead>>(printer->lookup_object("toolhead"));
            if (toolhead)
            {
                toolhead->register_lookahead_callback([](double pt)
                                                      { std::cout << "Lookahead callback triggered with point: " << pt << std::endl; });
            }
            heater->set_temp(temp);

            if (wait && temp)
            {
                wait_for_temperature(heater);
            }
            // SPDLOG_DEBUG("__func__:{},wait:{}", __func__, wait);
        }

        void PrinterHeaters::cmd_TEMPERATURE_WAIT(std::shared_ptr<GCodeCommand> gcmd)
        {
            std::string sensor_name = gcmd->get("SENSOR");
            auto it = std::find(available_sensors.begin(), available_sensors.end(), sensor_name);
            if (it != available_sensors.end())
            {
                std::cerr << "Unknown sensor '" << sensor_name << "'" << std::endl;
                return;
            }

            double min_temp = gcmd->get_double("MINIMUM", DOUBLE_NONE);
            double max_temp = gcmd->get_double("MAXIMUM", DOUBLE_NONE);
            if (std::isnan(min_temp) && std::isnan(max_temp))
            {
                throw elegoo::common::CommandError("Error on 'TEMPERATURE_WAIT': missing MINIMUM or MAXIMUM.");
            }

            if (!printer->get_start_args()["debugoutput"].empty())
            {
                return;
            }

            std::shared_ptr<Heater> sensor;
            if (heaters.find(sensor_name) != heaters.end())
            {
                sensor = heaters[sensor_name];
            }
            else
            {
                // sensor = printer->lookup_object(sensor_name);
            }

            std::shared_ptr<ToolHead> toolhead =
                any_cast<std::shared_ptr<ToolHead>>(printer->lookup_object("toolhead"));
            std::shared_ptr<SelectReactor> reactor = printer->get_reactor();
            double eventtime = get_monotonic();
            while (!printer->is_shutdown())
            {
                std::pair<double, double> val = sensor->get_temp(eventtime);
                if (val.first >= min_temp && val.first <= max_temp)
                {
                    return;
                }
                double print_time = toolhead->get_last_move_time();
                gcmd->respond_raw("Temperature: " + std::to_string(val.first));
                eventtime = reactor->pause(eventtime + 1.0);
            }
            SPDLOG_INFO("{} : {}___ ok!", __FUNCTION__, __LINE__);
        }

        void PrinterHeaters::handle_ready()
        {
            has_started = true;
        }

        std::string PrinterHeaters::get_temp(double eventtime)
        {
            // SPDLOG_DEBUG("__func__:{},has_started:{}", __func__, has_started);
            std::ostringstream out;
            if (has_started)
            {
                bool first = true;
                for (const auto &pair : gcode_id_to_sensor)
                {
                    auto val = pair.second->get_temp(eventtime);
                    if (!first)
                    {
                        out << " ";
                    }
                    out << pair.first << ":" << val.first << " /" << val.second;
                    first = false;
                }
            }

            if (out.str().empty())
            {
                SPDLOG_DEBUG("__func__:{},return is 'T:0'", __func__);
                return "T:0";
            }
            SPDLOG_DEBUG("__func__:{},out:{}", __func__, out.str());
            return out.str();
        }

        // 等待温度
        void PrinterHeaters::wait_for_temperature(std::shared_ptr<Heater> heater)
        {
            if (printer->get_start_args().find("debugoutput") != printer->get_start_args().end())
            {
                return;
            }

            std::shared_ptr<ToolHead> toolhead =
                any_cast<std::shared_ptr<ToolHead>>(printer->lookup_object("toolhead"));
            std::shared_ptr<GCodeDispatch> gcode =
                any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));
            std::shared_ptr<SelectReactor> reactor = printer->get_reactor();

            double eventtime = get_monotonic();
            // 打印停止打断等待,加热器异常中断等待
            bool is_wait_for_temperature = true;
            while (is_wait_for_temperature)
            {
                is_wait_for_temperature = !printer->is_shutdown() && heater->check_busy(eventtime) && !print_stop && !heater->is_verify_heater_fault;
                if(!is_wait_for_temperature)
                {
                    json res;
                    res["command"] = "M2202";
                    res["result"] = "1066";
                    gcode->respond_feedback(res);
                    SPDLOG_INFO("{} is_wait_for_temperature {}",__func__,is_wait_for_temperature);
                    break;
                }
                double print_time = toolhead->get_last_move_time();
                std::string temp_status = get_temp(eventtime);
                SPDLOG_DEBUG("__func__:{},temp_status:{} print_stop:{} is_verify_heater_fault:{}", __func__, temp_status,print_stop,heater->is_verify_heater_fault);
                gcode->respond_raw(temp_status);
                eventtime = reactor->pause(eventtime + 1.0);
            }
            // SPDLOG_DEBUG("__func__:{}", __func__);
        }
        
        std::shared_ptr<PrinterHeaters> heaters_load_config(std::shared_ptr<ConfigWrapper> config)
        {
            return std::make_shared<PrinterHeaters>(config);
        }

    }
}

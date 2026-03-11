/***************************************************************************** 
 * @Author       : Louisa
 * @Date         : 2024-11-22 15:58:51
 * @LastEditors  : Ben
 * @LastEditTime : 2025-03-23 16:56:31
 * @Description  : Kinematic input shaper to minimize motion vibrations in XY plane
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "input_shaper.h"
#include "shaper_defs.h"
#include "gcode.h"
#include "kinematics/kinematics.h"
#include "c_helper.h"

namespace elegoo 
{
    namespace extras 
    {
        std::string cmd_SET_INPUT_SHAPER_help = "Set cartesian parameters for input shaper";

        InputShaperParams::InputShaperParams(std::string axis, std::shared_ptr<ConfigWrapper> config)
        {
            this->axis = axis;
            this->shapers = {};
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            std::vector<InputShaperCfg> cfg = get_input_shapers();
            SPDLOG_DEBUG("__func__:{} #2",__func__);
            for(auto it = cfg.begin(); it != cfg.end(); ++it)
            {
                this->shapers[it->name] = it->init_func;
                SPDLOG_DEBUG("__func__:{} #2",__func__);
            }
            SPDLOG_DEBUG("__func__:{} #2",__func__);
            std::string shaper_type = config->get("shaper_type", "mzv");
            SPDLOG_DEBUG("__func__:{} #2",__func__);
            this->shaper_type = config->get("shaper_type_" + axis, shaper_type);
            auto it = this->shapers.find(this->shaper_type);
            if(it == this->shapers.end())
            {
                SPDLOG_ERROR("Unsupported shaper type: " + this->shaper_type);
                throw elegoo::common::CommandError("Unsupported shaper type: " + this->shaper_type);
            }
            SPDLOG_DEBUG("__func__:{} #3",__func__);
            this->damping_ratio = config->getdouble("damping_ratio_" + axis,DEFAULT_DAMPING_RATIO,0.,1.);
            this->shaper_freq = config->getdouble("shaper_freq_" + axis,0.,0.);
            SPDLOG_DEBUG("__func__:{} #4",__func__);
        }

        InputShaperParams::~InputShaperParams()
        {
            SPDLOG_DEBUG("~InputShaperParams");
        }

        void InputShaperParams::update(std::shared_ptr<GCodeCommand> gcmd)
        {
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            std::string axis = this->axis;
            std::transform(axis.begin(), axis.end(), axis.begin(), ::toupper);
            this->damping_ratio = gcmd->get_double("DAMPING_RATIO_" + axis,this->damping_ratio,0.,1.);
            this->shaper_freq = gcmd->get_double("SHAPER_FREQ_" + axis,this->shaper_freq,0.);
            std::string shaper_type = gcmd->get("SHAPER_TYPE","NONE");
            if("NONE" == shaper_type)
            {
                shaper_type = gcmd->get("SHAPER_TYPE_" + axis,this->shaper_type);
            }
            std::transform(shaper_type.begin(), shaper_type.end(), shaper_type.begin(), ::tolower);
            auto it = this->shapers.find(shaper_type);
            if(it == this->shapers.end())
            {
                SPDLOG_ERROR("Unsupported shaper type:" + shaper_type);
                throw elegoo::common::CommandError("Unsupported shaper type:" + shaper_type);
            }
            this->shaper_type = shaper_type;
        }

        std::tuple<int32_t, std::vector<double>, std::vector<double>> InputShaperParams::get_shaper()
        {
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            std::vector<double> A = {};
            std::vector<double> T = {};
            if(!this->shaper_freq)
            {
                std::pair<std::vector<double>, std::vector<double>> shaperPair = get_none_shaper();
                A = shaperPair.first;
                T = shaperPair.second;
            }
            else
            {
                std::pair<std::vector<double>, std::vector<double>> shaperPair = this->shapers[this->shaper_type](this->shaper_freq,this->damping_ratio);
                A = shaperPair.first;
                T = shaperPair.second;
            }
            return {A.size(),A,T};
        }

        std::map<std::string,std::string> InputShaperParams::get_status()
        {
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            std::ostringstream oss_ratio;
            oss_ratio << std::fixed << std::setprecision(3) << this->damping_ratio;
            std::string damping_ratio = oss_ratio.str();
            std::ostringstream oss_freq;
            oss_freq << std::fixed << std::setprecision(6) << this->shaper_freq;
            std::string shaper_freq = oss_freq.str();
            std::map<std::string,std::string> status = {
                {"shaper_type",this->shaper_type},
                {"shaper_freq",shaper_freq},
                {"damping_ratio",damping_ratio},
            };
            SPDLOG_DEBUG("__func__:{},shaper_type:{},shaper_freq:{},damping_ratio:{}",__func__,this->shaper_type,shaper_freq,damping_ratio);

            return status;
        }


        AxisInputShaper::AxisInputShaper(std::string axis, std::shared_ptr<ConfigWrapper> config)
        {
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            this->axis = axis;
            this->params = std::make_shared<InputShaperParams>(axis,config);
            std::tuple<int32_t, std::vector<double>, std::vector<double>> shaperTuple = this->params->get_shaper();
            this->n = std::get<0>(shaperTuple);
            this->A = std::get<1>(shaperTuple);
            this->T = std::get<2>(shaperTuple);
            this->saved = {};
        }

        AxisInputShaper::~AxisInputShaper()
        {
            SPDLOG_DEBUG("~AxisInputShaper");
        }

        std::string AxisInputShaper::get_name()
        {
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            return "shaper_" + this->axis;
        }

        std::tuple<int32_t, std::vector<double>, std::vector<double>> AxisInputShaper::get_shaper()
        {
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            return {this->n,this->A,this->T};
        }

        void AxisInputShaper::update(std::shared_ptr<GCodeCommand> gcmd)
        {
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            this->params->update(gcmd);
            std::tuple<int32_t, std::vector<double>, std::vector<double>> shaperTuple = this->params->get_shaper();
            this->n = std::get<0>(shaperTuple);
            this->A = std::get<1>(shaperTuple);
            this->T = std::get<2>(shaperTuple);
        }

        #define ARRAY_SIZE 5 // need ARRAY_SIZE >= this->n 
        bool AxisInputShaper::set_shaper_kinematics(std::shared_ptr<stepper_kinematics> sk)
        {
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            double arrayA[ARRAY_SIZE] = {0.};
            double arrayT[ARRAY_SIZE] = {0.};
            if(ARRAY_SIZE < this->n || this->axis.size() != 1)
            {
                SPDLOG_ERROR("error!shaper ARRAY_SIZE:5 > {}",this->n);
                return false;
            }
            for(auto ii = 0; ii < this->n; ++ii)
            {
                arrayA[ii] = this->A[ii];
                arrayT[ii] = this->T[ii];
            }
            bool success = input_shaper_set_shaper_params(sk.get(),*this->axis.c_str(),this->n,arrayA,arrayT) == 0;
            if(!success)
            {
                disable_shaping();
                input_shaper_set_shaper_params(sk.get(),*this->axis.c_str(),this->n,arrayA,arrayT);
            }

            return success;
        }

        void AxisInputShaper::disable_shaping()
        {
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            bool isNone = std::get<0>(this->saved) == 0 && std::get<1>(this->saved).empty() && std::get<2>(this->saved).empty();
            if(isNone && this->n)
            {
                this->saved = {this->n,this->A,this->T};
            }
            std::pair<std::vector<double>, std::vector<double>> shaperPair = get_none_shaper();
            this->A = shaperPair.first;
            this->T = shaperPair.second;
            this->n = this->A.size();
        }

        void AxisInputShaper::enable_shaping()
        {
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            bool isNone = std::get<0>(this->saved) == 0 && std::get<1>(this->saved).empty() && std::get<2>(this->saved).empty();
            if(isNone)
            {
                //Input shaper was not disabled
                return;
            }
            this->n = std::get<0>(this->saved);
            this->A = std::get<1>(this->saved);
            this->T = std::get<2>(this->saved);
            std::get<0>(this->saved) = 0;
            std::get<1>(this->saved).clear();
            std::get<2>(this->saved).clear();
        }

        void AxisInputShaper::report(std::shared_ptr<GCodeCommand> gcmd)
        {
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            std::ostringstream info;
            std::map<std::string,std::string> status = this->params->get_status();
            int32_t size = status.size();
            for(auto items : status)
            {
                info << items.first << "_" << this->axis << ":" << items.second;
                if(--size > 0)
                {
                    info << " ";
                }
            }
            gcmd->respond_info(info.str(),true);
        }

        std::map<std::string,std::string> AxisInputShaper::get_shaping_params()
        {
            return this->params->get_status();
        }

        InputShaper::InputShaper(std::shared_ptr<ConfigWrapper> config)
        {
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            this->printer = config->get_printer();

            elegoo::common::SignalManager::get_instance().register_signal(
                "elegoo:connect",
                std::function<void()>([this](){
                    SPDLOG_DEBUG("InputShaper connect~~~~~~~~~~~~~~~~~");
                    connect();
                    SPDLOG_DEBUG("InputShaper connect~~~~~~~~~~~~~~~~~ success!");
                })
            );

            SPDLOG_DEBUG("__func__:{} #2",__func__);
            this->toolhead = nullptr;
            this->shapers.push_back(std::make_shared<AxisInputShaper>("x", config));
            this->shapers.push_back(std::make_shared<AxisInputShaper>("y", config));
            this->input_shaper_stepper_kinematics = {};
            this->orig_stepper_kinematics = {};

            SPDLOG_DEBUG("__func__:{} #3",__func__);
            //Register gcode commands
            std::shared_ptr<GCodeDispatch> gcode = any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));
            gcode->register_command("SET_INPUT_SHAPER", 
                [this](std::shared_ptr<GCodeCommand> gcmd){ 
                    cmd_SET_INPUT_SHAPER(gcmd); 
                },
                false,cmd_SET_INPUT_SHAPER_help);
            SPDLOG_DEBUG("__func__:{} #4",__func__);
        }

        InputShaper::~InputShaper()
        {
            SPDLOG_DEBUG("~InputShaper");
        }

        std::vector<std::shared_ptr<AxisInputShaper>> InputShaper::get_shapers()
        {
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            return this->shapers;
        }

        void InputShaper::connect()
        {
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            this->toolhead = any_cast<std::shared_ptr<ToolHead>>(this->printer->lookup_object("toolhead"));
            //Configure initial values
            this->_update_input_shaping();
        }

        std::shared_ptr<stepper_kinematics> InputShaper::_get_input_shaper_stepper_kinematics(std::shared_ptr<MCU_stepper> stepper)
        {
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            std::shared_ptr<stepper_kinematics> sk = stepper->get_stepper_kinematics();
            auto it = std::find(this->orig_stepper_kinematics.begin(), this->orig_stepper_kinematics.end(), sk);
            if(it != this->orig_stepper_kinematics.end())
            {
                return nullptr;
            }
            it = std::find(this->input_shaper_stepper_kinematics.begin(), this->input_shaper_stepper_kinematics.end(), sk);
            if(it != this->input_shaper_stepper_kinematics.end())
            {
                return sk;
            }
            
            SPDLOG_DEBUG("__func__:{} #2",__func__);
            this->orig_stepper_kinematics.emplace_back(sk);
            std::shared_ptr<stepper_kinematics> is_sk = std::shared_ptr<stepper_kinematics>(
                input_shaper_alloc(),
                free
            );

            SPDLOG_DEBUG("__func__:{} #3",__func__);
            stepper->set_stepper_kinematics(is_sk);
            int32_t res = input_shaper_set_sk(is_sk.get(),sk.get());
            if(res < 0)
            {
                stepper->set_stepper_kinematics(sk);
                return nullptr;
            }
            this->input_shaper_stepper_kinematics.emplace_back(is_sk);

            SPDLOG_DEBUG("__func__:{} #4",__func__);
            return is_sk;
        }

        void InputShaper::_update_input_shaping()
        {
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            this->toolhead->flush_step_generation();
            std::shared_ptr<Kinematics> kin = this->toolhead->get_kinematic();
            std::vector<std::shared_ptr<AxisInputShaper>> failed_shapers = {};

            for(auto stepper : kin->get_steppers())
            {
                if(!stepper->get_trapq())
                {
                    continue;
                }

                std::shared_ptr<stepper_kinematics> is_sk = _get_input_shaper_stepper_kinematics(stepper);
                if(!is_sk)
                {
                    continue;
                }

                double old_delay = input_shaper_get_step_generation_window(is_sk.get());
                for(auto shaper : this->shapers)
                {
                    auto it = std::find(failed_shapers.begin(), failed_shapers.end(), shaper);
                    if(it != failed_shapers.end())
                    {
                        continue;
                    }
                    if(!shaper->set_shaper_kinematics(is_sk))
                    {
                        failed_shapers.emplace_back(shaper);
                    }
                }
                double new_delay = input_shaper_get_step_generation_window(is_sk.get());
                if(old_delay != new_delay)
                {
                    this->toolhead->note_step_generation_scan_time(new_delay,old_delay);
                }
            }

            SPDLOG_DEBUG("__func__:{} #2",__func__);
            if(!failed_shapers.empty())
            {
                std::ostringstream err;
                for(auto shaper : failed_shapers)
                {
                    err << ", " << shaper->get_name();
                }
                throw elegoo::common::CommandError("Failed to configure shaper(s) " + err.str() + " with given parameters");
            }
            SPDLOG_DEBUG("__func__:{} #3",__func__);
        }
    
        void InputShaper::disable_shaping()
        {
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            for(auto shaper : this->shapers)
            {
                shaper->disable_shaping();
            }
            _update_input_shaping();
        }

        void InputShaper::enable_shaping()
        {
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            for(auto shaper : this->shapers)
            {
                shaper->enable_shaping();
            }
            _update_input_shaping();
        }

        void InputShaper::cmd_SET_INPUT_SHAPER(std::shared_ptr<GCodeCommand> gcmd)
        {
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            if(!gcmd->get_command_parameters().empty())
            {
                for(auto shaper : this->shapers)
                {
                    shaper->update(gcmd);
                }
                _update_input_shaping();
            }
            for(auto shaper : this->shapers)
            {
                shaper->report(gcmd);
            }
        }

        std::shared_ptr<InputShaper> input_shaper_load_config(std::shared_ptr<ConfigWrapper> config)
        {
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            std::shared_ptr<InputShaper> inputShaper = std::make_shared<InputShaper>(config);
            return inputShaper;
        }
    }
}

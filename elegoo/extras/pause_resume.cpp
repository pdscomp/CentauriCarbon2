/*****************************************************************************
 * @Author       : Ben
 * @Date         : 2024-11-27 12:04:25
 * @LastEditors  : loping
 * @LastEditTime : 2025-09-29 16:57:55
 * @Description  : Pause/Resume functionality with position capture/restore
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "pause_resume.h"
#include "printer.h"
#include "virtual_sdcard.h"
#include "gcode_macro.h"
#include "controller_fan.h"
#include "heater_fan.h"
#include "cavity_fan.h"
#include "kinematics_factory.h"
#include "mmu.h"
namespace elegoo {
namespace extras {
PauseResume::PauseResume(std::shared_ptr<ConfigWrapper> config)
{
    SPDLOG_DEBUG("PauseResume init!");
    printer = config->get_printer();
    reactor = printer->get_reactor();
    gcode = any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));
    recover_velocity = config->getdouble("recover_velocity", 50);
    is_paused = false;
    sd_paused = false;
    is_goto_waste_box = true;
    pause_command_sent =false;
    //gcode_macro
    gcode_move = any_cast<std::shared_ptr<GCodeMove>>(printer->load_object(config, "gcode_move"));
    gcode_macro = any_cast<std::shared_ptr<PrinterGCodeMacro>>(printer->load_object(config, "gcode_macro"));
    after_pause_gcode_macro1 = gcode_macro->load_template(config, "after_pause_gcode_macro1", "");
    after_pause_gcode_macro2 = gcode_macro->load_template(config, "after_pause_gcode_macro2", "");
    before_resume_gcode_macro1 = gcode_macro->load_template(config, "before_resume_gcode_macro1", "");
    before_resume_gcode_macro2 = gcode_macro->load_template(config, "before_resume_gcode_macro2", "");
    before_resume_gcode_macro3 = gcode_macro->load_template(config, "before_resume_gcode_macro3", "");
    cancel_print_gcode_macro1 = gcode_macro->load_template(config, "cancel_print_gcode_macro1", "");
    cancel_print_gcode_macro2 = gcode_macro->load_template(config, "cancel_print_gcode_macro2", "");
    //fan
    this->fan = any_cast<std::shared_ptr<PrinterFan>>(printer->load_object(config, "fan"));
    this->generic_box_fan = any_cast<std::shared_ptr<PrinterFanCavity>>(printer->load_object(config, "cavity_fan"));
    this->generic_fan1 = any_cast<std::shared_ptr<PrinterFanGeneric>>(printer->load_object(config, "fan_generic fan1"));
    //signal
    elegoo::common::SignalManager::get_instance().register_signal(
        "elegoo:connect",
        std::function<void()>([this](){
            SPDLOG_DEBUG("PauseResume connect~~~~~~~~~~~~~~~~~");
            handle_connect();
            SPDLOG_DEBUG("PauseResume connect~~~~~~~~~~~~~~~~~ success!");
        })
    );
    elegoo::common::SignalManager::get_instance().register_signal(
        "elegoo:ready",
        std::function<void()>([this](){
            handle_ready();
        })
    );

    elegoo::common::SignalManager::get_instance().register_signal(
        "elegoo:cover_off",
        std::function<void(bool)>([this](bool state){
            is_cover_drop = state;
        })
    );
    elegoo::common::SignalManager::get_instance().register_signal(
        "verify_heater:fault",
        std::function<void(std::string)>([this](std::string heater_name){
            is_verify_heater_fault = true;
        })
    );
    elegoo::common::SignalManager::get_instance().register_signal(
        "verify_heater:normal",
        std::function<void(std::string)>([this](std::string heater_name){
            is_verify_heater_fault = false;
        })
    );

    elegoo::common::SignalManager::get_instance().register_signal(
        "adc_temp:fault",
        std::function<void(std::string)>([this](std::string heater_name){
            is_adc_temp_fault = true;
        })
    );
    elegoo::common::SignalManager::get_instance().register_signal(
        "adc_temp:normal",
        std::function<void(std::string)>([this](std::string heater_name){
            is_adc_temp_fault = false;
        })
    );

    //command
    gcode->register_command("PAUSE",
        [this](std::shared_ptr<GCodeCommand> gcmd){
            cmd_PAUSE(gcmd);
        },
        false,
        "Pauses the current print");

    gcode->register_command("RESUME",
        [this](std::shared_ptr<GCodeCommand> gcmd){
            cmd_RESUME(gcmd);
        },
        false,
        "Resumes the print from a pause");

    gcode->register_command("CLEAR_PAUSE",
        [this](std::shared_ptr<GCodeCommand> gcmd){
            cmd_CLEAR_PAUSE(gcmd);
        },
        false,
        "Clears the current paused state without resuming the print");

    gcode->register_command("CANCEL_PRINT",
        [this](std::shared_ptr<GCodeCommand> gcmd){
            cmd_CANCEL_PRINT(gcmd);
        },
        false,
        "Cancel the current print");

    std::shared_ptr<WebHooks> webhooks =
        any_cast<std::shared_ptr<WebHooks>>(printer->lookup_object("webhooks"));
    webhooks->register_endpoint("pause_resume/cancel",
        [this](std::shared_ptr<WebRequest> web_request){
        handle_cancel_request(web_request);
    });
    webhooks->register_endpoint("pause_resume/pause",
        [this](std::shared_ptr<WebRequest> web_request){
        handle_pause_request(web_request);
    });
    webhooks->register_endpoint("pause_resume/resume",
        [this](std::shared_ptr<WebRequest> web_request){
        handle_resume_request(web_request);
    });
    SPDLOG_DEBUG("PauseResume init success!!");
}

PauseResume::~PauseResume()
{

}

void PauseResume::handle_connect()
{
    //heater
    this->pheaters = any_cast<std::shared_ptr<PrinterHeaters>>(printer->lookup_object("heaters"));
    this->heaters = pheaters->get_heaters();
    //accel
    this->toolhead = any_cast<std::shared_ptr<ToolHead>>(printer->lookup_object("toolhead"));
    //VirtualSD
    v_sd = any_cast<std::shared_ptr<VirtualSD>>(printer->lookup_object("virtual_sdcard"));
}

void PauseResume::handle_ready()
{
    canvas = any_cast<std::shared_ptr<Canvas>>(printer->lookup_object("canvas_dev",std::shared_ptr<Canvas>()));
}

json PauseResume::get_status(double eventtime)
{
    json status;
    status["is_paused"] = is_paused;
    return status;
}

bool PauseResume::is_sd_active()
{
    return v_sd != nullptr && v_sd->is_active();
}

void PauseResume::save_state(std::string state_name)
{
    // if(this->state.contains(state_name))
    {
        //heater
        if(heaters.find("extruder") != heaters.end())
        {
            std::cout<<"extruder" <<std::endl;
            if(0 != pheaters->get_heaters()["extruder"]->get_temp(0).second)
            {
                this->state[state_name]["T"] = pheaters->get_heaters()["extruder"]->get_temp(0).second;
            }
            else
            {
                this->state[state_name]["T"] = pheaters->get_heaters()["extruder"]->get_temp(0).first;
            }
        }
        if(heaters.find("heater_bed") != heaters.end())
        {
            std::cout<<"heater_bed" <<std::endl;
            if(0 != pheaters->get_heaters()["heater_bed"]->get_temp(0).second)
            {
                this->state[state_name]["B"] = pheaters->get_heaters()["heater_bed"]->get_temp(0).second;
            }
            else
            {
                this->state[state_name]["B"] = pheaters->get_heaters()["heater_bed"]->get_temp(0).first;
            }
        }
        //accel
        this->state[state_name]["max_accel"] = toolhead->get_status(0.)["max_accel"];
        //speed
        this->state[state_name]["fan_speed"] = this->fan->get_status(0.)["speed"];
        this->state[state_name]["generic_box_fan_speed"] = this->generic_box_fan->get_status(0.)["speed"];
        this->state[state_name]["generic_fan1_speed"] = this->generic_fan1->get_status(0.)["speed"];
        SPDLOG_INFO("__func__:{} #1 state_name:{} fan_speed:{} box_fan_speed:{} {}",__func__,state_name,state[state_name]["fan_speed"].get<double>(),state[state_name]["generic_box_fan_speed"].get<double>(),state[state_name]["generic_fan1_speed"].get<double>());
    }
}

json PauseResume::get_pause_state()
{
    return state;
}

void PauseResume::resume_state(std::string state_name)
{

}

void PauseResume::power_outage_resume_pause()
{
    send_pause_command();
    gcode->run_script_from_command("POWER_OUTAGE_RESUME_PAUSE NAME=PAUSE_STATE");
    is_paused = true;
    SPDLOG_INFO("__func__:{} #1 is_paused:{}",__func__,is_paused);
    SPDLOG_INFO("__func__:{} __LINE__:{}___ ok", __func__, __LINE__);
}

void PauseResume::send_pause_command()
{
    if (!pause_command_sent)
    {
        if (is_sd_active())
        {
            SPDLOG_INFO("send_pause_command");
            sd_paused = true;
            v_sd->do_pause();
        }
        else
        {
            sd_paused = false;
            gcode->respond_info("action:paused");
        }
        pause_command_sent = true;  // 标记暂停命令已经发送
    }
}

void PauseResume::cmd_PAUSE(std::shared_ptr<GCodeCommand> gcmd)
{
    SPDLOG_INFO("{} : {}___", __FUNCTION__, __LINE__);
    SPDLOG_INFO("__func__:{} #1 is_paused:{}",__func__,is_paused);
    if (is_paused)
    {
        gcmd->respond_info("Print already paused", true);
        return;
    }

    //暂停打印中
    json res;
    res["command"] = "M2202";
    res["result"] = "2501";
    gcmd->respond_feedback(res);
    SPDLOG_INFO("__func__:{} #1 res:{}",__func__,res.dump());
    if(true == is_goto_waste_box)
    {
        is_goto_waste_box = gcmd->get_int("GOTO_WASTE_BOX",1,0,1);
    }
    SPDLOG_INFO("{} is_goto_waste_box:{}",__func__,is_goto_waste_box);
    //执行暂停动作,等待清空缓存
    // gcode->run_script_from_command("M400");
    //运动停止
    send_pause_command();
    // 判断Z轴是否已经归零
    json kin_status = toolhead->get_kinematic()->get_status(get_monotonic());
    bool X_homed = (kin_status["homed_axes"].get<std::string>().find('x') != std::string::npos);
    bool Y_homed = (kin_status["homed_axes"].get<std::string>().find('y') != std::string::npos);
    bool Z_homed = (kin_status["homed_axes"].get<std::string>().find('z') != std::string::npos);
    if(X_homed && Y_homed && Z_homed)
    {
        //记录状态1
        gcode->run_script_from_command("SAVE_GCODE_STATE NAME=PAUSE_STATE_1");
        save_state("PAUSE_STATE_1");
        //执行自定义宏1（设置加速度、抬升Z同时E回抽）
        after_pause_gcode_macro1->run_gcode_from_command();
        //记录状态2
        gcode->run_script_from_command("SAVE_GCODE_STATE NAME=PAUSE_STATE_2");
        save_state("PAUSE_STATE_2");
        if(is_goto_waste_box)
        {
            SPDLOG_INFO("{} is_goto_waste_box:{}",__func__,is_goto_waste_box);
            //执行自定义宏2（回到废料桶位置）
            after_pause_gcode_macro2->run_gcode_from_command();
            //记录状态3
            gcode->run_script_from_command("SAVE_GCODE_STATE NAME=PAUSE_STATE_3");
            save_state("PAUSE_STATE_3");
        }
        //等待清空缓存
        gcode->run_script_from_command("M400");
    }
    gcode_move->clear_gcode_speed();
    //真实暂停
    gcode->run_script_from_command("M25");
    //设置暂停状态为true
    is_paused = true;
    SPDLOG_INFO("__func__:{} #1 is_paused:{}",__func__,is_paused);
    //暂停流程完成
    res["command"] = "M2202";
    res["result"] = "2502";
    gcmd->respond_feedback(res);
    SPDLOG_INFO("__func__:{} #1 res:{}",__func__,res.dump());
    SPDLOG_INFO("{} : {}___ ok", __FUNCTION__, __LINE__);
}

void PauseResume::send_resume_command()
{
    if (sd_paused)
    {
        v_sd->do_resume();  // 恢复打印
        sd_paused = false;   // 标记为已恢复
    }
    else
    {
        gcode->respond_info("action:resumed");  // 向 GCode 发送恢复信息
    }
    pause_command_sent = false;  // 重置暂停命令发送状态
}

bool PauseResume::abnormal_check()
{
    return is_cover_drop || is_adc_temp_fault || is_verify_heater_fault;
}

void PauseResume::cmd_RESUME(std::shared_ptr<GCodeCommand> gcmd)
{
    SPDLOG_INFO("{} : {}___", __FUNCTION__, __LINE__);
    if (!is_paused)
    {
        gcmd->respond_info("Print is not paused, resume aborted", true);
        return;
    }

    //恢复打印中
    json res;
    res["command"] = "M2202";
    res["result"] = "2401";
    SPDLOG_INFO("{} res.dump:{}",__func__,res.dump());
    gcmd->respond_feedback(res);

    if(abnormal_check())
    {
        reactor->pause(get_monotonic() + 1.);
        res["command"] = "M2202";
        res["result"] = "2403";
        SPDLOG_INFO("{} res.dump:{}",__func__,res.dump());
        gcmd->respond_feedback(res);
        return;
    }

    // 判断Z轴是否已经归零
    json kin_status = toolhead->get_kinematic()->get_status(get_monotonic());
    bool X_homed = (kin_status["homed_axes"].get<std::string>().find('x') != std::string::npos);
    bool Y_homed = (kin_status["homed_axes"].get<std::string>().find('y') != std::string::npos);
    bool Z_homed = (kin_status["homed_axes"].get<std::string>().find('z') != std::string::npos);
    if(X_homed && Y_homed && Z_homed)
    {
        //
        json gcode_state = gcode_move->get_gcode_state();
        //G90
        gcode->run_script_from_command("G90");
        if(is_goto_waste_box)
        {
            SPDLOG_INFO("{} is_goto_waste_box:{}",__func__,is_goto_waste_box);
            //Z轴移动到状态3位置
            if(gcode_state.contains("PAUSE_STATE_3") && gcode_state["PAUSE_STATE_3"].contains("last_position"))
            {
                double Z_pos = gcode_state["PAUSE_STATE_3"]["last_position"][2].get<double>();
                gcode->run_script_from_command("G1 Z" + std::to_string(Z_pos) + " F600");
                SPDLOG_DEBUG("__func__:{} #1 Z_pos:{}",__func__,Z_pos);
            }
        }
        //设置温度
        double T_temp = 0.,B_temp = 0.;
        double T_cur_temp = 0.,B_cur_temp = 0.;
        if(heaters.find("extruder") != heaters.end())
        {
            SPDLOG_DEBUG("__func__:{} #1 heaters.find('extruder')");
            T_cur_temp = pheaters->get_heaters()["extruder"]->get_temp(0).first;
            T_temp = this->state["PAUSE_STATE_1"]["T"].get<double>();
            if(T_cur_temp < T_temp - 30)
            {
                gcode->run_script_from_command("M109 S" + std::to_string(T_temp));
            }
            else
            {
                gcode->run_script_from_command("M104 S" + std::to_string(T_temp));
            }
        }
        if(heaters.find("heater_bed") != heaters.end())
        {
            SPDLOG_DEBUG("__func__:{} #1 heaters.find('heater_bed')");
            B_cur_temp = pheaters->get_heaters()["heater_bed"]->get_temp(0).first;
            B_temp = this->state["PAUSE_STATE_1"]["B"].get<double>();
            if(B_cur_temp < B_temp - 5)
            {
                gcode->run_script_from_command("M190 S" + std::to_string(B_temp));
            }
            else
            {
                gcode->run_script_from_command("M140 S" + std::to_string(B_temp));
            }
        }
        //执行自定义宏1（设置加速度）
        before_resume_gcode_macro1->run_gcode_from_command();
        
        gcode->run_script_from_command("CANVAS_SWITCH_FILAMENT");
        if(canvas && canvas->get_canvas_protocol()->get_canvas_status().host_status.error_code)
        {
            res["command"] = "M2202";
            res["result"] = "2403";
            SPDLOG_INFO("{} res.dump:{}",__func__,res.dump());
            gcmd->respond_feedback(res);
            return;
        }

        //如果当前温度大于180,才执行挤出动作,自定义宏2(吐料擦料动作)包含挤出动作
        if(T_temp >= 180)
        {
            // if(is_goto_waste_box)
            {
                SPDLOG_INFO("{} is_goto_waste_box:{}",__func__,is_goto_waste_box);
                before_resume_gcode_macro2->run_gcode_from_command();
            }
        }
        //设置风扇速度
        std::string fan_speed = "M106 S" + std::to_string(this->state["PAUSE_STATE_1"]["fan_speed"].get<double>()*255.);
        SPDLOG_INFO("__func__:{} #1 fan_speed:{}",__func__,fan_speed);
        gcode->run_script_from_command(fan_speed);
        fan_speed = "M106 P2 S" + std::to_string(this->state["PAUSE_STATE_1"]["generic_fan1_speed"].get<double>()*255.);
        SPDLOG_INFO("__func__:{} #1 fan1_speed:{}",__func__,fan_speed);
        gcode->run_script_from_command(fan_speed);
        fan_speed = "SET_CAVITY_FAN SPEED=" + std::to_string(this->state["PAUSE_STATE_1"]["generic_box_fan_speed"].get<double>()*255.);
        SPDLOG_INFO("__func__:{} #1 box_fan_speed:{}",__func__,fan_speed);
        gcode->run_script_from_command(fan_speed);
        //快速移动到参数2位置
        if(gcode_state.contains("PAUSE_STATE_2") && gcode_state["PAUSE_STATE_2"].contains("last_position"))
        {
            double X_pos = gcode_state["PAUSE_STATE_2"]["last_position"][0].get<double>();
            double Y_pos = gcode_state["PAUSE_STATE_2"]["last_position"][1].get<double>();
            gcode->run_script_from_command("G1 X" + std::to_string(X_pos) + " Y" + std::to_string(Y_pos) + " F20000");
            SPDLOG_DEBUG("__func__:{} #1 X_pos:{} Y_pos:{}",__func__,X_pos,Y_pos);
        }
        //快速移动到参数3位置
        if(gcode_state.contains("PAUSE_STATE_1") && gcode_state["PAUSE_STATE_1"].contains("last_position"))
        {
            double X_pos = gcode_state["PAUSE_STATE_1"]["last_position"][0].get<double>();
            double Y_pos = gcode_state["PAUSE_STATE_1"]["last_position"][1].get<double>();
            double Z_pos = gcode_state["PAUSE_STATE_1"]["last_position"][2].get<double>();
            gcode->run_script_from_command("G1 X" + std::to_string(X_pos) + " Y" + std::to_string(Y_pos) + " Z" + std::to_string(Z_pos) + " F600");
            SPDLOG_DEBUG("__func__:{} #1 X_pos:{} Y_pos:{} Z_pos:{}",__func__,X_pos,Y_pos,Z_pos);
        }
        //如果当前温度大于180,才执行挤出动作,自定义宏3包含挤出动作
        if(T_temp >= 180)
        {
            // if(is_goto_waste_box)
            {
                SPDLOG_INFO("{} is_goto_waste_box:{}",__func__,is_goto_waste_box);
                before_resume_gcode_macro3->run_gcode_from_command();
            }
        }
        //恢复打印加速度
        double max_accel = this->state["PAUSE_STATE_1"]["max_accel"].get<double>();
        gcode->run_script_from_command("M204 S" + std::to_string(max_accel));
        //恢复打印速度并快速移动到参数1位置
        double velocity = gcmd->get_double("VELOCITY", recover_velocity);
        std::stringstream script;
        script << "RESTORE_GCODE_STATE NAME=PAUSE_STATE_1 MOVE=1 MOVE_SPEED=" << velocity;
        gcode->run_script_from_command(script.str());
        is_goto_waste_box = true;
    }
    send_resume_command();
    gcode_move->clean_gcode_state();
    //设置暂停状态为false
    is_paused = false;
    SPDLOG_INFO("__func__:{} #1 is_paused:{}",__func__,is_paused);
    //恢复流程完成
    res["command"] = "M2202";
    res["result"] = "2402";
    gcmd->respond_feedback(res);
    SPDLOG_INFO("__func__:{} #1 res:{}",__func__,res.dump());
    SPDLOG_INFO("{} : {}___ ok", __FUNCTION__, __LINE__);
}

void PauseResume::cmd_CLEAR_PAUSE(std::shared_ptr<GCodeCommand> gcmd)
{
    is_paused = pause_command_sent = false;
    SPDLOG_INFO("__func__:{} #1 is_paused:{}",__func__,is_paused);
}

void PauseResume::cmd_CANCEL_PRINT(std::shared_ptr<GCodeCommand> gcmd)
{
    //停止打印中
    elegoo::common::SignalManager::get_instance().emit_signal("gcode:CANCEL_PRINT_START");
    json res;
    res["command"] = "M2202";
    res["result"] = "2503";
    gcmd->respond_feedback(res);
    SPDLOG_INFO("__func__:{} #1 res:{}",__func__,res.dump());
    
    if (is_sd_active() || sd_paused)
    {
        SPDLOG_DEBUG("__func__:{} #1 ",__func__);
        v_sd->do_cancel();
    }
    else
    {
        SPDLOG_DEBUG("__func__:{} #1 ",__func__);
        gcode->respond_info("action:cancel");
    }
    elegoo::common::SignalManager::get_instance().emit_signal("gcode:CANCEL_PRINT_DONE");

    // 等待缓存清空
    gcode->run_script_from_command("M400");
    // 判断是否已经归零
    json kin_status = toolhead->get_kinematic()->get_status(get_monotonic());
    bool X_homed = (kin_status["homed_axes"].get<std::string>().find('x') != std::string::npos);
    bool Y_homed = (kin_status["homed_axes"].get<std::string>().find('y') != std::string::npos);
    bool Z_homed = (kin_status["homed_axes"].get<std::string>().find('z') != std::string::npos);
    if(Z_homed)
    {
        double Z_pos = toolhead->get_status(0.)["position"][2].get<double>();
        if(Z_pos < 75)
        {
            SPDLOG_INFO("__func__:{} #1 Z_pos:{}",__func__,Z_pos);
            gcode->run_script_from_command("G90");
            gcode->run_script_from_command("G1 Z80 F1200");
        }
        else
        {
            SPDLOG_INFO("__func__:{} #2 Z_pos:{}",__func__,Z_pos);
            gcode->run_script_from_command("MANUAL_MOVE Z=5 V=600");
            gcode->run_script_from_command("G90");
        }
    }
    if(X_homed && Y_homed)
    {
        cancel_print_gcode_macro1->run_gcode_from_command();
    }
    //执行自定义宏
    cancel_print_gcode_macro2->run_gcode_from_command();
    //清空标志位
    cmd_CLEAR_PAUSE(gcmd);
    //停止打印中
    res["command"] = "M2202";
    res["result"] = "2504";
    gcmd->respond_feedback(res);
    SPDLOG_INFO("{} : {}___ ok", __FUNCTION__, __LINE__);
}

void PauseResume::handle_cancel_request(std::shared_ptr<WebRequest> web_request)
{
    SPDLOG_DEBUG("__func__:{} #1 ",__func__);
    gcode->run_script("CANCEL_PRINT");
}

void PauseResume::handle_pause_request(std::shared_ptr<WebRequest> web_request)
{
    gcode->run_script("PAUSE");
}

void PauseResume::handle_resume_request(std::shared_ptr<WebRequest> web_request)
{
    gcode->run_script("RESUME");
}

std::shared_ptr<PauseResume> pause_resume_load_config(
        std::shared_ptr<ConfigWrapper> config) {
    return std::make_shared<PauseResume>(config);
}

}
}
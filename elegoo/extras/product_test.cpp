/*****************************************************************************
 * @Author       : loping
 * @Date         : 2025-06-10 10:30:46
 * @LastEditors  : loping
 * @LastEditTime : 2025-08-09 18:26:55
 * @Description  :
 * @Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "product_test.h"
#include "controller_fan.h"
#include "heater_fan.h"
#include "extruder.h"
#include "heater_bed.h"
#include "resonance_tester.h"
#include "bed_mesh.h"
#include "pid_calibrate.h"
#include "gcode.h"
#include "json.h"
#include "buttons.h"
#include "filament_switch_sensor.h"
#include "load_cell.h"


namespace elegoo
{
    namespace extras
    {

        #undef SPDLOG_DEBUG
        #define SPDLOG_DEBUG SPDLOG_INFO

ProductTest::ProductTest(std::shared_ptr<ConfigWrapper> config)
{
    SPDLOG_DEBUG("__func__:{} #1",__func__);
    printer = config->get_printer();
    reactor = printer->get_reactor();
    lis2dw = std::static_pointer_cast<Lis2dw>(any_cast<std::shared_ptr<AccelChip>>(printer->lookup_object("lis2dw",std::shared_ptr<AccelChip>())));
    SPDLOG_DEBUG("__func__:{} #1",__func__);
    // buttons = any_cast<std::shared_ptr<PrinterButtons>>(printer->load_object(config, "buttons"));
    // std::string motor_reset_pin = config->get("motor_reset_pin");
    // buttons->register_buttons({motor_reset_pin}, [this](double eventtime, bool state) {
    //     return motor_reset_button_handler(eventtime, state);
    // });
    SPDLOG_DEBUG("__func__:{} #1",__func__);
    // std::string filament_runout_pin = config->get("filament_runout_pin");
    // buttons->register_buttons({filament_runout_pin}, [this](double eventtime, bool state) {
    //     return filament_runout_button_handler(eventtime, state);
    // });

    switch_sensor = any_cast<std::shared_ptr<SwitchSensor>>(printer->lookup_object("filament_switch_sensor filament_sensor",std::shared_ptr<SwitchSensor>()));

    SPDLOG_DEBUG("__func__:{} #1",__func__);
    pheaters = any_cast<std::shared_ptr<PrinterHeaters>>(printer->lookup_object("heaters",std::shared_ptr<PrinterHeaters>()));
    heaters = pheaters->get_heaters();
    controller_fan = any_cast<std::shared_ptr<ControllerFan>>(printer->lookup_object("controller_fan board_cooling_fan",std::shared_ptr<ControllerFan>()));
    if(!controller_fan)
        SPDLOG_DEBUG("__func__:{} #1",__func__);
    heater_fan = any_cast<std::shared_ptr<PrinterHeaterFan>>(printer->lookup_object("heater_fan heatbreak_cooling_fan",std::shared_ptr<PrinterHeaterFan>()));
    if(!heater_fan)
        SPDLOG_DEBUG("__func__:{} #1",__func__);
    fan = any_cast<std::shared_ptr<PrinterFan>>(printer->lookup_object("fan", std::shared_ptr<PrinterFan>()));
    if(!fan)
        SPDLOG_DEBUG("__func__:{} #1",__func__);
    loadcell = any_cast<std::shared_ptr<LoadCell>>(printer->lookup_object("load_cell",std::shared_ptr<LoadCell>()));
    bed_mesh = any_cast<std::shared_ptr<BedMesh>>(printer->lookup_object("bed_mesh",std::shared_ptr<BedMesh>()));
    if(!bed_mesh)
        SPDLOG_DEBUG("__func__:{} #1",__func__);
    resonance_tester = any_cast<std::shared_ptr<ResonanceTester>>(printer->lookup_object("resonance_tester",std::shared_ptr<ResonanceTester>()));
    if(!resonance_tester)
        SPDLOG_DEBUG("__func__:{} #1",__func__);
    name = config->get_name();
    product_test_ready = false;
    product_test_start = false;
    shutdown_status = false;
    motor_reset_pin_value = false;
    filament_runout_pin_value = false;

    gcode = any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));
    canvas = any_cast<std::shared_ptr<Canvas>>(printer->lookup_object("canvas_dev"));
    servo = any_cast<std::shared_ptr<PrinterServo>>(printer->lookup_object("servo printer_servo"));
    encoder_sensor = any_cast<std::shared_ptr<EncoderSensor>>(printer->lookup_object("z_filament_motion_sensor encoder_sensor",std::shared_ptr<EncoderSensor>()));

    test_timer = reactor->register_timer(
        [this](double eventtime){
            return callback(eventtime);
        },
        _NEVER, "product test");

    elegoo::common::SignalManager::get_instance().register_signal(
        "elegoo:ready",
        std::function<void()>(
            [this](){
                reactor->update_timer(test_timer, _NOW);
                shutdown_status = false;
            }
        )
    );

    elegoo::common::SignalManager::get_instance().register_signal(
        "elegoo:shutdown",
        std::function<void()>(
            [this](){
                handle_shutdown();
            }
        )
    );

    // gcode->register_command(
    //         "PRODUCT_TEST"
    //         ,[this](std::shared_ptr<GCodeCommand> gcmd){
    //             cmd_PRODUCT_TEST(gcmd);
    //         }
    //         ,false
    //         ,"Product Test");

    gcode->register_command(
            "TURN_ON_OUTPUT_PIN"
            ,[this](std::shared_ptr<GCodeCommand> gcmd){
                cmd_TURN_ON_OUTPUT_PIN(gcmd);
            }
            ,false
            ,"TURN ON OUTPUT PIN");

    gcode->register_command(
            "TURN_OFF_OUTPUT_PIN"
            ,[this](std::shared_ptr<GCodeCommand> gcmd){
                cmd_TURN_OFF_OUTPUT_PIN(gcmd);
            }
            ,false
            ,"TURN OFF OUTPUT PIN");

    gcode->register_command(
            "TURN_ON_MOTOR"
            ,[this](std::shared_ptr<GCodeCommand> gcmd){
                cmd_TURN_ON_MOTOR(gcmd);
            }
            ,false
            ,"TURN ON MOTOR");

    gcode->register_command(
            "TURN_ON_LOAD_CELL"
            ,[this](std::shared_ptr<GCodeCommand> gcmd){
                cmd_TURN_ON_LOAD_CELL(gcmd);
            }
            ,false
            ,"TURN ON LOAD CELL");

    gcode->register_command(
            "FILA_IN"
            ,[this](std::shared_ptr<GCodeCommand> gcmd){
                cmd_FILA_IN(gcmd);
            }
            ,false
            ,"Filament In");

    gcode->register_command(
            "FILA_OUT"
            ,[this](std::shared_ptr<GCodeCommand> gcmd){
                cmd_FILA_OUT(gcmd);
            }
            ,false
            ,"Filament Out");
    gcode->register_command(
            "BEEP"
            ,[this](std::shared_ptr<GCodeCommand> gcmd){
                cmd_BEEP(gcmd);
            }
            ,false
            ,"Beep");
}

ProductTest::~ProductTest()
{

}

void ProductTest::handle_shutdown()
{
    SPDLOG_INFO("ProductTest handle_shutdown");
    shutdown_status = true;
}
void ProductTest::cmd_TURN_ON_MOTOR(std::shared_ptr<GCodeCommand> gcmd)
{
    std::string asix = gcmd->get("ASIX", "");
    std::string value = gcmd->get("VALUE", "");
    // double value = gcmd->get_int("VALUE", 20,0,100);
    if(asix.empty())
    {
        SPDLOG_INFO("cmd_TURN_ON_MOTOR asix:{} value:{}",asix,value);
        gcode->run_script_from_command("G1 X1000 Y1200 Z1100 E1100 f600");
    }
    else if(asix == "X")
    {
        SPDLOG_INFO("cmd_TURN_ON_MOTOR asix:{} value:{}",asix,value);
        gcode->run_script_from_command("G1 X" + value + " f600");
    }
    else if(asix == "Y")
    {
        SPDLOG_INFO("cmd_TURN_ON_MOTOR asix:{} value:{}",asix,value);
        gcode->run_script_from_command("G1 Y" + value + " f600");
    }
    else if(asix == "Z")
    {
        SPDLOG_INFO("cmd_TURN_ON_MOTOR asix:{} value:{}",asix,value);
        gcode->run_script_from_command("G1 Z" + value + " f600");
    }
    else if(asix == "E")
    {
        SPDLOG_INFO("cmd_TURN_ON_MOTOR asix:{} value:{}",asix,value);
        gcode->run_script_from_command("G1 E" + value + " f300");
    }
    else
    {
        SPDLOG_INFO("cmd_TURN_ON_MOTOR asix:{} value:{}",asix,value);
        gcode->run_script_from_command("G1 x11000 Y12000 Z11000 E11000 f600");
    }
    SPDLOG_INFO("cmd_TURN_ON_MOTOR __OK");
}
void ProductTest::cmd_TURN_ON_LOAD_CELL(std::shared_ptr<GCodeCommand> gcmd)
{
    gcode->run_script_from_command("LOAD_CELL_DIAGNOSTIC LOAD_CELL=load_cell_probe");
}
void ProductTest::cmd_TURN_ON_OUTPUT_PIN(std::shared_ptr<GCodeCommand> gcmd)
{
    double value = gcmd->get_double("VALUE", 0.5,0.,1.);
    gcode->run_script_from_command("SET_PIN PIN=led_pin VALUE=" + std::to_string(value));
    // gcode->run_script_from_command("SET_PIN PIN=heart_nozzle_pin VALUE=" + std::to_string(value));
    // gcode->run_script_from_command("SET_PIN PIN=heart_bed_pin VALUE=" + std::to_string(value));
    gcode->run_script_from_command("SET_PIN PIN=control_fan_pin VALUE=" + std::to_string(value));
    gcode->run_script_from_command("SET_PIN PIN=fan1_pin VALUE=" + std::to_string(value));
    gcode->run_script_from_command("SET_PIN PIN=box_fan_pin VALUE=" + std::to_string(value));
    gcode->run_script_from_command("SET_PIN PIN=heart_fan_pin VALUE=" + std::to_string(value));
    gcode->run_script_from_command("SET_PIN PIN=fan_pin VALUE=" + std::to_string(value));
    // gcode->run_script_from_command("SET_PIN PIN=filament_runout_pin VALUE=" + std::to_string(value));
    // gcode->run_script_from_command("SET_PIN PIN=motor_reset_pin VALUE=" + std::to_string(value));
}
void ProductTest::cmd_TURN_OFF_OUTPUT_PIN(std::shared_ptr<GCodeCommand> gcmd)
{
    double value = 0.;
    gcode->run_script_from_command("SET_PIN PIN=led_pin VALUE=" + std::to_string(value));
    // gcode->run_script_from_command("SET_PIN PIN=heart_nozzle_pin VALUE=" + std::to_string(value));
    // gcode->run_script_from_command("SET_PIN PIN=heart_bed_pin VALUE=" + std::to_string(value));
    gcode->run_script_from_command("SET_PIN PIN=control_fan_pin VALUE=" + std::to_string(value));
    gcode->run_script_from_command("SET_PIN PIN=fan1_pin VALUE=" + std::to_string(value));
    gcode->run_script_from_command("SET_PIN PIN=box_fan_pin VALUE=" + std::to_string(value));
    gcode->run_script_from_command("SET_PIN PIN=heart_fan_pin VALUE=" + std::to_string(value));
    gcode->run_script_from_command("SET_PIN PIN=fan_pin VALUE=" + std::to_string(value));
    // gcode->run_script_from_command("SET_PIN PIN=filament_runout_pin VALUE=" + std::to_string(value));
    // gcode->run_script_from_command("SET_PIN PIN=motor_reset_pin VALUE=" + std::to_string(value));
}

void ProductTest::cmd_FILA_IN(std::shared_ptr<GCodeCommand> gcmd)
{
    int32_t id = gcmd->get_int("ID",0,0,3);

    FeederMotor filament_control = {100, 100, 100};

    canvas->get_canvas_protocol()->feeder_filament_control(id, filament_control);
}

void ProductTest::cmd_FILA_OUT(std::shared_ptr<GCodeCommand> gcmd)
{
    int32_t id = gcmd->get_int("ID",0,0,3);

    FeederMotor filament_control = {-100, 100, 100};

    canvas->get_canvas_protocol()->feeder_filament_control(id, filament_control);
}

void ProductTest::cmd_BEEP(std::shared_ptr<GCodeCommand> gcmd)
{
    canvas->get_canvas_protocol()->beeper_control(1, 1000);
}

double ProductTest::callback(double eventtime)
{
    if(!product_test_start)
    {
        gcode->run_script_from_command("TURN_ON_OUTPUT_PIN");
        reactor->pause(get_monotonic() + 0.1);
        gcode->run_script_from_command("TURN_ON_LOAD_CELL");
        reactor->pause(get_monotonic() + 0.1);
        product_test_ready = true;
        product_test_start = true;
        SPDLOG_INFO("product_test_start:{}",product_test_start);
    }
    double heater_bed_temp = 0.;
    for(auto heater : heaters)
    {
        if("heater_bed" == heater.first)
            heater_bed_temp = heater.second->get_status(0.)["last_temp"].get<double>();
    }
    auto nozzle_heart = any_cast<std::shared_ptr<PrinterExtruder>>(printer->lookup_object("extruder"))->get_heater();

    std::array<FeederLed, 4> leds;

    for (auto &led : leds) {
        led.enable_control = true;
        led.red  = FeederLedState::Blink1Hz;
        led.blue = FeederLedState::Breathe;
    }

    // canvas_dev->feeders_leds_control(leds);
    canvas->get_canvas_protocol()->feeders_leds_control(leds);

    json res;
    res["command"]="product_test";
    res["product_ready"] = product_test_ready;
    res["heart_nozzle_temp"] = nozzle_heart->get_status(0.)["last_temp"].get<double>();
    res["heater_bed_temp"] = heater_bed_temp;
    res["model_fan_rpm"] = fan->get_status(0.)["rpm"].get<double>();
    res["controller_fan_rpm"] = controller_fan->get_status(0.)["rpm"].get<double>();
    res["heater_fan_rpm"] = heater_fan->get_status(0.)["rpm"].get<double>();
    res["filament_runout_pin_value"] = switch_sensor->get_status(0.)["filament_detected"].get<bool>();
    res["motor_reset_pin_value"] = motor_reset_pin_value;
    res["lis2dw_id"] = lis2dw->get_lis2dw_id();
    res["loadcell_std_value"] = loadcell->get_diagnostic_std_value();
    res["product_shutdown"] = shutdown_status;

    json canvas_status = canvas->get_status_product_test(0.);
    res["canvas_dev"] = canvas_status;

    json servo_status = servo->get_status(0.);
    res["servo_status"] = servo_status;

    json filament_status = encoder_sensor->get_status(0.);
    res["filament_status"] = filament_status;

    gcode->respond_feedback(res);
    SPDLOG_INFO("res.dump:{}",res.dump());

    return eventtime + 1.0;
}

double ProductTest::motor_reset_button_handler(double eventtime, bool state)
{
    SPDLOG_DEBUG("motor_reset_button_handler state = {}", state);
    motor_reset_pin_value = state;
    return 0.0;
}

double ProductTest::filament_runout_button_handler(double eventtime, bool state)
{
    SPDLOG_DEBUG("filament_runout_button_handler state = {}", state);
    filament_runout_pin_value = state;
    return 0.0;
}

void ProductTest::product_test_feedback(std::string command,std::string result)
{
    json res;
    res["command"] = command;
    if(!result.empty())
        res["result"] = result;
    SPDLOG_INFO("__func__:{} #1 res:{}",__func__,res.dump());
    gcode->respond_feedback(res);
}

std::shared_ptr<ProductTest> zproduct_test_load_config(std::shared_ptr<ConfigWrapper> config)
{
    SPDLOG_INFO("__func__:{} #1",__func__);
    std::shared_ptr<ProductTest> product_test = std::make_shared<ProductTest>(config);
    return product_test;
}

    }
}

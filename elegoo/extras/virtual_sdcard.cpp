/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2024-11-27 14:59:55
 * @LastEditors  : loping
 * @LastEditTime : 2025-09-27 17:38:16
 * @Description  : Virtual sdcard support (print files directly from a host
 * g-code file)
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "virtual_sdcard.h"
#include "logger.h"
#include "exception_handler.h"
#include "utilities.h"
#include "input_shaper.h"
#include "pause_resume.h"
#include "fan.h"
#include "filament_switch_sensor.h"
#include "mmu.h"
#include "cavity_fan.h"

namespace elegoo
{
    namespace extras
    {

        const std::string DEFAULT_ERROR_GCODE = R"(
{% if 'heaters' in printer %}
   TURN_OFF_HEATERS
{% endif %}
)";

        const std::string cmd_SDCARD_RESET_FILE_help =
            "Clears a loaded SD File. Stops the print if necessary";
        const std::string cmd_SDCARD_PRINT_FILE_help =
            "Loads a SD file and starts the print.  May include files in "
            "subdirectories.";
        std::string expanduser(const std::string &path)
        {
            if (path.empty() || path[0] != '~')
            {
                return path;
            }

            const char *home_env = std::getenv("HOME");
            if (home_env == nullptr)
            {
                throw std::runtime_error("HOME environment variable is not set.");
            }

            std::string home_dir = home_env;

            if (path == "~")
            {
                return home_dir;
            }
            else
            {
                return home_dir + path.substr(1);
            }
        }

        std::string normpath(const std::string &path)
        {
            std::vector<std::string> parts;
            std::string part;
            bool in_part = false;

            for (char c : path)
            {
                if (c == '/')
                {
                    if (in_part)
                    {
                        parts.push_back(part);
                        part.clear();
                        in_part = false;
                    }
                }
                else
                {
                    part += c;
                    in_part = true;
                }
            }

            if (in_part)
            {
                parts.push_back(part);
            }

            std::vector<std::string> result;
            for (const auto &p : parts)
            {
                if (p == "..")
                {
                    if (!result.empty() && result.back() != "..")
                    {
                        result.pop_back();
                    }
                    else
                    {
                        result.push_back(p);
                    }
                }
                else if (p != "." && !p.empty())
                {
                    result.push_back(p);
                }
            }

            std::string normalized_path;
            if (!path.empty() && path[0] == '/')
            {
                normalized_path = "/";
            }

            for (const auto &p : result)
            {
                if (!normalized_path.empty() && normalized_path.back() != '/')
                {
                    normalized_path += "/";
                }
                normalized_path += p;
            }

            return normalized_path;
        }

        VirtualSD::VirtualSD(std::shared_ptr<ConfigWrapper> config)
        {
            SPDLOG_INFO("VirtualSD init!");
            printer = config->get_printer();
            this->config = config;
            power_outage_file_name = "/opt/usr/record/power_outage.json";

            elegoo::common::SignalManager::get_instance().register_signal(
                "elegoo:ready",
                std::function<void()>([this]() {
                    handle_ready();
                })
            );

            elegoo::common::SignalManager::get_instance().register_signal(
                "elegoo:shutdown",
                std::function<void()>([this]() {
                    handle_shutdown();
                })
            );

            elegoo::common::SignalManager::get_instance().register_signal(
                "por:power_off",
                std::function<void()>([this]() { 
                    handle_power_outage(); 
                })
            );

            auto sd = config->get("path");
            power_outage_enable = config->getint("power_outage_enable", 1);
            std::string expanded_path = expanduser(sd);
            std::string normalized_path = normpath(expanded_path);

            sdcard_dirname = normalized_path;
            file_position = 0;
            file_size = 0;

            print_stats = any_cast<std::shared_ptr<PrintStats>>(printer->load_object(config, "print_stats"));

            reactor = printer->get_reactor();
            must_pause_work = false;
            cmd_from_sd = false;
            is_over_work = true;
            next_file_position = 0;
            // work_timer = nullptr;

            auto gcode_macro = any_cast<std::shared_ptr<PrinterGCodeMacro>>(printer->load_object(config, "gcode_macro"));
            print_start_gcode = gcode_macro->load_template(config, "print_start_gcode", "");
            move_to_waste_box = gcode_macro->load_template(config, "move_to_waste_box", "");
            extrude_filament = gcode_macro->load_template(config, "extrude_filament", "");
            on_error_gcode = gcode_macro->load_template(config, "on_error_gcode", DEFAULT_ERROR_GCODE);
            gcode = any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));

            std::vector<std::pair<std::string, std::function<void(std::shared_ptr<GCodeCommand>)>>> cmd_fuc = {
                {"M20", std::bind(&VirtualSD::cmd_M20, this, std::placeholders::_1)},
                {"M21", std::bind(&VirtualSD::cmd_M21, this, std::placeholders::_1)},
                {"M23", std::bind(&VirtualSD::cmd_M23, this, std::placeholders::_1)},
                {"M24", std::bind(&VirtualSD::cmd_M24, this, std::placeholders::_1)},
                {"M25", std::bind(&VirtualSD::cmd_M25, this, std::placeholders::_1)},
                {"M26", std::bind(&VirtualSD::cmd_M26, this, std::placeholders::_1)},
                {"M27", std::bind(&VirtualSD::cmd_M27, this, std::placeholders::_1)},
            };

            for (const auto &cmd : cmd_fuc)
            {
                gcode->register_command(cmd.first, cmd.second);
            }

            for (const auto &cmd : {"M28", "M29", "M30"})
            {
                gcode->register_command(cmd, [this](std::shared_ptr<GCodeCommand> gcmd)
                                        { this->cmd_error(gcmd); });
            }

            gcode->register_command("SDCARD_RESET_FILE", [this](std::shared_ptr<GCodeCommand> gcmd)
                                    { this->cmd_SDCARD_RESET_FILE(gcmd); }, false, cmd_SDCARD_RESET_FILE_help);

            gcode->register_command("RESET_PRINT_TIME", [this](std::shared_ptr<GCodeCommand> gcmd)
                { 
                    this->cmd_RESET_PRINT_TIME(gcmd);
                },
                false,
                "RESET PRINT TIME"
            );

            gcode->register_command("SDCARD_PRINT_FILE", [this](std::shared_ptr<GCodeCommand> gcmd) 
                { 
                    this->cmd_SDCARD_PRINT_FILE(gcmd); 
                }, 
                false, 
                cmd_SDCARD_PRINT_FILE_help
            );

            gcode->register_command("POWER_OUTAGE_RESUME", [this](std::shared_ptr<GCodeCommand> gcmd)
                                    { this->cmd_POWER_OUTAGE_RESUME(gcmd); }, false, "Resume printing after power outage");

            gcode->register_command("SET_POWER_OUTAGE_ENABLE", [this](std::shared_ptr<GCodeCommand> gcmd)
                                    { this->cmd_SET_POWER_OUTAGE_ENABLE(gcmd); }, false);
            SPDLOG_INFO("VirtualSD init success!!");
        }

        VirtualSD::~VirtualSD() {}

        void VirtualSD::handle_ready() 
        {
            std::ifstream file(power_outage_file_name);
            if (file.is_open()) {
                file >> power_outage_context;
                file.close();
                SPDLOG_INFO(power_outage_context.dump(4));
            }
            else {
                SPDLOG_WARN("File does not exist or failed to open");
                return;
            }

            // 判断GCODE文件是否存在
            std::string filename = power_outage_context["file_name"].get<std::string>();
            std::string full_path = sdcard_dirname + "/" + filename;
            struct stat file_stat;
            if (stat(full_path.c_str(), &file_stat) == 0) {
                // 设置GCODE文件
                gcode->run_script_from_command("M23 " + power_outage_context["file_name"].get<std::string>());
                print_stats->set_print_duration_before_power_loss(power_outage_context["print_duration"].get<double>());
            } else {
                SPDLOG_ERROR("power_outage_resume but file not found: {}", full_path);
            }
           
            if(power_outage_context["enable_power_outage"].get<int>() == 0)
            {
                if (std::remove(power_outage_file_name.c_str()) == 0)
                {
                    SPDLOG_INFO("File successfully deleted.");
                }
                else
                {
                    SPDLOG_WARN("File deletion failed. {}",strerror(errno));
                }
            }        
        }


        void VirtualSD::handle_shutdown()
        {
            handle_power_outage();
            if (work_timer != nullptr)
            {
                must_pause_work = true;
                try
                {
                    uint64_t readpos = std::max(file_position - 1024, static_cast<uint64_t>(0));
                    uint64_t readcount = file_position - readpos;
#if OP_LIBC
                    fseeko64(current_file, readpos, SEEK_SET);
                    std::string data(readcount + 128, '\0');
                    char *buf = (char *)data.data();
                    size_t count = fread(buf, 1, readcount + 128, current_file);
                    data.resize(count);
#else
                    current_file.seekg(readpos);
                    std::string data(readcount + 128, '\0');
                    current_file.read(&data[0], readcount + 128);
                    data.resize(current_file.gcount());
#endif

                    SPDLOG_INFO("Virtual sdcard (" + std::to_string(readpos) + "): " +
                                std::to_string(readcount) + "\nUpcoming (" +
                                std::to_string(file_position) + "): " +
                                std::to_string(readcount));
                }
                catch (const std::exception &e)
                {
                    SPDLOG_ERROR("virtual_sdcard shutdown read: " + std::string(e.what()));
                    return;
                }
            }
            SPDLOG_INFO("{} : {}___ ok", __FUNCTION__, __LINE__);
        }

        void VirtualSD::handle_power_outage()
        {
            SPDLOG_INFO("start saving print process data.");
            std::shared_ptr<PauseResume> pause_resume = 
                any_cast<std::shared_ptr<PauseResume>>(printer->lookup_object("pause_resume"));
            std::shared_ptr<PrinterHeaters> pheaters =
                any_cast<std::shared_ptr<PrinterHeaters>>(printer->lookup_object("heaters"));
            std::shared_ptr<ToolHead> toolhead =
                any_cast<std::shared_ptr<ToolHead>>(printer->lookup_object("toolhead"));
            std::shared_ptr<GCodeMove> gcode_move =
                any_cast<std::shared_ptr<GCodeMove>>(printer->lookup_object("gcode_move", std::shared_ptr<GCodeMove>()));
            std::shared_ptr<InputShaper> input_shaper =
                any_cast<std::shared_ptr<InputShaper>>(printer->lookup_object("input_shaper", std::shared_ptr<InputShaper>()));
            std::shared_ptr<PrinterFan> fan =
                any_cast<std::shared_ptr<PrinterFan>>(printer->lookup_object("fan", std::shared_ptr<PrinterFan>()));
            std::shared_ptr<BedMesh> bed_mesh =
                any_cast<std::shared_ptr<BedMesh>>(this->printer->lookup_object("bed_mesh", std::shared_ptr<BedMesh>()));
            std::shared_ptr<PrinterFanCavity> box_fan =
                any_cast<std::shared_ptr<PrinterFanCavity>>(printer->lookup_object("cavity_fan", std::shared_ptr<PrinterFanCavity>()));
            std::shared_ptr<PrinterFanGeneric> fan1 =
                any_cast<std::shared_ptr<PrinterFanGeneric>>(printer->lookup_object("fan_generic fan1", std::shared_ptr<PrinterFanGeneric>()));
            std::shared_ptr<SwitchSensor> switch_sensor =
                any_cast<std::shared_ptr<SwitchSensor>>(printer->lookup_object("filament_switch_sensor filament_sensor", std::shared_ptr<SwitchSensor>()));
            std::shared_ptr<Canvas> canvas = 
                any_cast<std::shared_ptr<Canvas>>(printer->lookup_object("canvas_dev",std::shared_ptr<Canvas>()));

            json pause_state = pause_resume->get_pause_state();
            json print_state_json = print_stats->get_status(get_monotonic());
            json status;

            // if(print_state_json["info"]["current_layer"] < 2 || is_over_work) 
            if(is_over_work) 
            {
                SPDLOG_INFO("{} No data needs to be saved before the actual printing.",__func__);
                return;
            }
            else
            {
                SPDLOG_INFO("{} Save new data during actual printing.",__func__);
            }

            status["file_name"] = current_file_name;
            status["file_path"] = sdcard_dirname;
            status["progress"] = progress();
            status["file_size"] = file_size;
            status["file_position"] = file_position;
            status["print_duration"] = print_state_json["print_duration"];
            // status["total_layer"] = print_state_json["info"]["total_layer"];
            status["current_layer"] = print_state_json["info"]["current_layer"];
            status["enable_power_outage"] = power_outage_enable;


            if(must_pause_work) 
            {
                json gcode_move_status = gcode_move->get_gcode_state();
                if(!gcode_move_status.contains("PAUSE_STATE_1")) 
                {
                    json move_status = gcode_move->get_status(get_monotonic());
                    status["absolute_extrude"] = move_status["absolute_extrude"];
                    status["gcode_position"] = move_status["gcode_position"];
                    status["kinematic_position_z"] = move_status["gcode_position"][2];
                    if(0 != pheaters->get_heaters()["extruder"]->get_temp(0).second)
                    {
                        status["T"] = pheaters->get_heaters()["extruder"]->get_temp(0).second;
                    }
                    else
                    {
                        status["T"] = pheaters->get_heaters()["extruder"]->get_temp(0).first;
                    }
                    if(0 != pheaters->get_heaters()["heater_bed"]->get_temp(0).second)
                    {
                        status["B"] = pheaters->get_heaters()["heater_bed"]->get_temp(0).second;
                    }
                    else
                    {
                        status["B"] = pheaters->get_heaters()["heater_bed"]->get_temp(0).first;
                    }
                    status["max_accel"] = toolhead->get_status(get_monotonic())["max_accel"];
                    status["model_fan_speed"] = fan->get_status(get_monotonic())["speed"].get<double>() * 255;
                    status["fan1_fan_speed"] = fan1->get_status(get_monotonic())["speed"].get<double>() * 255;
                    status["box_fan_speed"] = box_fan->get_status(get_monotonic())["speed"].get<double>() * 255;
                    status["is_pause"] = false;
                }
                else if(!gcode_move_status.contains("PAUSE_STATE_2")) 
                {
                    status["absolute_extrude"] = gcode_move_status["PAUSE_STATE_1"]["absolute_extrude"];
                    status["gcode_position"] = gcode_move_status["PAUSE_STATE_1"]["gcode_position"];
                    status["kinematic_position_z"] = gcode_move_status["PAUSE_STATE_1"]["gcode_position"][2];
                    status["T"] = pause_state["PAUSE_STATE_1"]["T"];
                    status["B"] = pause_state["PAUSE_STATE_1"]["B"];
                    status["max_accel"] = pause_state["PAUSE_STATE_1"]["max_accel"];
                    status["model_fan_speed"] = pause_state["PAUSE_STATE_1"]["fan_speed"].get<double>() * 255;
                    status["fan1_fan_speed"] = pause_state["PAUSE_STATE_1"]["generic_fan1_speed"].get<double>() * 255;
                    status["box_fan_speed"] = pause_state["PAUSE_STATE_1"]["generic_box_fan_speed"].get<double>() * 255;
                    status["is_pause"] = true;
                }
                else if(!gcode_move_status.contains("PAUSE_STATE_3")) 
                {         
                    status["absolute_extrude"] = gcode_move_status["PAUSE_STATE_1"]["absolute_extrude"];               
                    status["gcode_position"] = gcode_move_status["PAUSE_STATE_1"]["gcode_position"];
                    status["kinematic_position_z"] = gcode_move_status["PAUSE_STATE_2"]["gcode_position"][2];
                    status["T"] = pause_state["PAUSE_STATE_1"]["T"];
                    status["B"] = pause_state["PAUSE_STATE_1"]["B"];
                    status["max_accel"] = pause_state["PAUSE_STATE_1"]["max_accel"];
                    status["model_fan_speed"] = pause_state["PAUSE_STATE_1"]["fan_speed"].get<double>() * 255;
                    status["fan1_fan_speed"] = pause_state["PAUSE_STATE_1"]["generic_fan1_speed"].get<double>() * 255;
                    status["box_fan_speed"] = pause_state["PAUSE_STATE_1"]["generic_box_fan_speed"].get<double>() * 255;
                    status["is_pause"] = true;
                }
                else
                {
                    status["absolute_extrude"] = gcode_move_status["PAUSE_STATE_1"]["absolute_extrude"];               
                    status["gcode_position"] = gcode_move_status["PAUSE_STATE_1"]["gcode_position"];
                    status["kinematic_position_z"] = gcode_move_status["PAUSE_STATE_3"]["gcode_position"][2];
                    status["T"] = pause_state["PAUSE_STATE_1"]["T"];
                    status["B"] = pause_state["PAUSE_STATE_1"]["B"];
                    status["max_accel"] = pause_state["PAUSE_STATE_1"]["max_accel"];
                    status["model_fan_speed"] = pause_state["PAUSE_STATE_1"]["fan_speed"].get<double>() * 255;
                    status["fan1_fan_speed"] = pause_state["PAUSE_STATE_1"]["generic_fan1_speed"].get<double>() * 255;
                    status["box_fan_speed"] = pause_state["PAUSE_STATE_1"]["generic_box_fan_speed"].get<double>() * 255;
                    status["is_pause"] = true;
                }
            }
            else
            {
                json gcode_move_status = gcode_move->get_status(get_monotonic());
                status["absolute_extrude"] = gcode_move_status["absolute_extrude"];
                status["gcode_position"] = gcode_move_status["gcode_position"];
                status["kinematic_position_z"] = gcode_move_status["gcode_position"][2];
                if(0 != pheaters->get_heaters()["extruder"]->get_temp(0).second)
                {
                    status["T"] = pheaters->get_heaters()["extruder"]->get_temp(0).second;
                }
                else
                {
                    status["T"] = pheaters->get_heaters()["extruder"]->get_temp(0).first;
                }
                if(0 != pheaters->get_heaters()["heater_bed"]->get_temp(0).second)
                {
                    status["B"] = pheaters->get_heaters()["heater_bed"]->get_temp(0).second;
                }
                else
                {
                    status["B"] = pheaters->get_heaters()["heater_bed"]->get_temp(0).first;
                }
                status["max_accel"] = toolhead->get_status(get_monotonic())["max_accel"];
                status["model_fan_speed"] = fan->get_status(get_monotonic())["speed"].get<double>() * 255;
                status["fan1_fan_speed"] = fan1->get_status(get_monotonic())["speed"].get<double>() * 255;
                status["box_fan_speed"] = box_fan->get_status(get_monotonic())["speed"].get<double>() * 255;
                status["is_pause"] = false;
            }
            // 保存速度模式
            status["speed_factor"] = gcode_move->get_status(get_monotonic())["speed_factor"];
            SPDLOG_INFO("speed_factor {}",gcode_move->get_status(get_monotonic())["speed_factor"].get<double>());

            std::shared_ptr<DummyExtruder> extruder = toolhead->get_extruder();
            json extruder_status = extruder->get_status(0);
            status["pressure_advance"] = extruder_status["pressure_advance"];
            status["smooth_time"] = extruder_status["smooth_time"];
            

            if (input_shaper)
            {
                std::map<std::string,std::string> shaper_params = 
                    input_shaper->get_shapers().at(0)->get_shaping_params();
                status["shaper_type_x"] = shaper_params["shaper_type"];
                status["shaper_freq_x"] = shaper_params["shaper_freq"];
                status["damping_ratio_x"] = shaper_params["damping_ratio"];
                status["shaper_type_y"] = shaper_params["shaper_type"];
                status["shaper_freq_y"] = shaper_params["shaper_freq"];
                status["damping_ratio_y"] = shaper_params["damping_ratio"];
            }

            if(bed_mesh)
            {
                status["profile_name"] = bed_mesh->get_status(0)["profile_name"];
                status["execute_calirate_from_slicer"] = bed_mesh->get_execute_calirate_from_slicer();
            }

            if(switch_sensor)
            {
                bool filament_detected = switch_sensor->get_status(0)["filament_detected"].get<bool>();
                if(filament_detected) 
                {
                    status["filament_remaining"] = 0;
                }
                else
                {
                    status["filament_remaining"] = switch_sensor->get_status(0)["filament_remaining"];
                }             
            }

            if(canvas && canvas->get_connect_state())
            {
                status["canvas_power_outage_status"] = canvas->get_canvas_power_outage_status(0.);
            }

            if(access(power_outage_file_name.c_str(),F_OK) == 0)
            {
                SPDLOG_INFO("exist power_outage_file_name");
            }
            else
            {
                SPDLOG_INFO("not exist power_outage_file_name");
            }
            int fd = open(power_outage_file_name.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd == -1)
            {
                SPDLOG_ERROR("Failed to open file. {}", power_outage_file_name);
                return;
            }

            std::string json_str = status.dump(4).c_str();
            ssize_t bytes_written = write(fd, json_str.c_str(), json_str.size());
            if (fsync(fd) == -1)
            {
                perror("Failed to sync file to disk.");
                close(fd);
                return;
            }
            
            if (bytes_written == -1)
            {
                SPDLOG_ERROR("Failed to write to file.");
                close(fd);
                return;
            }
            
            close(fd);
            SPDLOG_INFO("JSON data has been written to power_outage.json");

            return;
        }

        void VirtualSD::canvas_power_outage_resume()
        {
            std::shared_ptr<Canvas> canvas = 
                any_cast<std::shared_ptr<Canvas>>(printer->lookup_object("canvas_dev",std::shared_ptr<Canvas>()));
            if(power_outage_context.contains("canvas_power_outage_status") && canvas)
            {
                std::string T_str = {};
                std::string channel_str = {};
                std::string cmd_str = {};
                json canvas_dev_json = power_outage_context["canvas_power_outage_status"];
                int printing_used_canvas = canvas_dev_json["printing_used_canvas"].get<int>();
                SPDLOG_INFO("{} printing_used_canvas:{}",__func__,printing_used_canvas);
                if(printing_used_canvas && canvas->get_connect_state())
                {
                    if(canvas_dev_json.contains("color_table_size") && canvas_dev_json["color_table_size"].get<size_t>() > 0)
                    {
                        size_t color_table_size = canvas_dev_json["color_table_size"].get<size_t>();
                        SPDLOG_INFO("{} color_table_size:{}",__func__,color_table_size);
                        for(size_t ii = 0; ii < color_table_size; ++ii)
                        {
                            if(canvas_dev_json.contains("color_table" + std::to_string(ii)))
                            {
                                if(T_str.empty())
                                {
                                    T_str = canvas_dev_json["color_table" + std::to_string(ii)]["T"].get<std::string>();
                                }
                                else
                                {
                                    T_str = T_str + "," + canvas_dev_json["color_table" + std::to_string(ii)]["T"].get<std::string>();
                                }
                                
                                if(channel_str.empty())
                                {
                                    channel_str = canvas_dev_json["color_table" + std::to_string(ii)]["channel"].get<std::string>();
                                }
                                else
                                {
                                    channel_str = channel_str + "," + canvas_dev_json["color_table" + std::to_string(ii)]["channel"].get<std::string>();
                                }
                            }
                        }
                        cmd_str = "CANVAS_SET_COLOR_TABLE T=" + T_str + " CHANNEL=" + channel_str;
                        SPDLOG_INFO("{} cmd_str:{}",__func__,cmd_str);
                        gcode->run_script_from_command("CANVAS_SET_COLOR_TABLE T=" + T_str + " CHANNEL=" + channel_str);
                    }
                    if(canvas_dev_json.contains("switch_filment_T") && canvas_dev_json["switch_filment_T"].get<int32_t>() >= 0)
                    {
                        cmd_str = "CANVAS_SWITCH_FILAMENT T=" + std::to_string(canvas_dev_json["switch_filment_T"].get<int32_t>()) + " SELECT=0\nM400";
                        SPDLOG_INFO("{} cmd_str:{}",__func__,cmd_str);
                        gcode->run_script_from_command(cmd_str);
                    }
                }
                else
                {
                    SPDLOG_INFO("no connect canvas!");
                }
            }
        }

        std::pair<bool, std::string> VirtualSD::stats(double eventtime)
        {
            if (work_timer == nullptr)
            {
                return {false, ""};
            }
            return {true, "sd_pos=" + std::to_string(file_position)};
        }

        bool is_valid_extension(const std::string &ext)
        {
            static const std::set<std::string> valid_exts = {"gcode", "g", "gco"};
            return valid_exts.find(ext) != valid_exts.end();
        }

        std::vector<std::pair<std::string, uint64_t>> get_file_list_recursive(
            const std::string &root)
        {
            std::vector<std::pair<std::string, uint64_t>> flist;

            DIR *dir = opendir(root.c_str());
            if (dir == nullptr)
            {
                SPDLOG_ERROR("virtual_sdcard get_file_list");
                return {};
            }

            struct dirent *entry;
            while ((entry = readdir(dir)) != nullptr)
            {
                std::string name = entry->d_name;
                if (name == "." || name == "..")
                    continue;

                std::string full_path = root + "/" + name;
                struct stat file_stat;
                if (stat(full_path.c_str(), &file_stat) == -1)
                {
                    SPDLOG_ERROR("virtual_sdcard get_file_list");
                    continue;
                }

                if (S_ISDIR(file_stat.st_mode))
                {
                    auto sub_flist = get_file_list_recursive(full_path);
                    flist.insert(flist.end(), sub_flist.begin(), sub_flist.end());
                }
                else if (S_ISREG(file_stat.st_mode))
                {
                    std::string ext = name.substr(name.find_last_of(".") + 1);
                    if (is_valid_extension(ext))
                    {
                        std::string r_path = full_path.substr(root.length() + 1);
                        uint64_t size = file_stat.st_size;
                        flist.emplace_back(r_path, size);
                    }
                }
            }

            closedir(dir);

            std::sort(flist.begin(), flist.end(),
                      [](const std::pair<std::string, uint64_t> &a,
                         const std::pair<std::string, uint64_t> &b)
                      {
                          return a.first < b.first;
                      });

            return flist;
        }

        std::vector<std::pair<std::string, uint64_t>> get_file_list_flat(
            const std::string &dir)
        {
            std::vector<std::pair<std::string, uint64_t>> flist;
            SPDLOG_INFO("{} : {}___dir:{}", __FUNCTION__, __LINE__, dir);

            DIR *dp = opendir(dir.c_str());
            if (dp == nullptr)
            {
                SPDLOG_ERROR("virtual_sdcard get_file_list");
                return {};
            }

            struct dirent *entry;
            while ((entry = readdir(dp)) != nullptr)
            {
                std::string name = entry->d_name;
                if (name[0] == '.')
                    continue;

                std::string full_path = dir + "/" + name;
                struct stat file_stat;
                if (stat(full_path.c_str(), &file_stat) == -1)
                {
                    SPDLOG_ERROR("virtual_sdcard get_file_list");
                    continue;
                }

                if (S_ISREG(file_stat.st_mode))
                {
                    std::string ext = name.substr(name.find_last_of(".") + 1);
                    if (is_valid_extension(ext))
                    {
                        uint64_t size = file_stat.st_size;
                        flist.emplace_back(name, size);
                    }
                }
            }

            closedir(dp);

            std::sort(flist.begin(), flist.end(),
                      [](const std::pair<std::string, uint64_t> &a,
                         const std::pair<std::string, uint64_t> &b)
                      {
                          return a.first < b.first;
                      });

            return flist;
        }

        std::vector<std::pair<std::string, uint64_t>> VirtualSD::get_file_list(
            bool check_subdirs)
        {
            if (check_subdirs)
            {
                return get_file_list_recursive(sdcard_dirname);
            }
            else
            {
                return get_file_list_flat(sdcard_dirname);
            }
        }

        json VirtualSD::get_status()
        {
            return {
                {"file_path", file_path()},
                {"progress", progress()},
                {"is_active", is_active()},
                {"file_position", file_position},
                {"file_size", file_size},
                {"power_outage", file_exists(power_outage_file_name)}};
        }

        std::string VirtualSD::file_path()
        {
            if (current_file)
            {
                return current_file_name;
            }
            return "";
        }

        double VirtualSD::progress()
        {
            if (file_size)
            {
                return double(file_position) / file_size;
            }
            return 0.0;
        }

        bool VirtualSD::is_active() { return work_timer != nullptr; }

        void VirtualSD::do_pause()
        {
            SPDLOG_INFO("do_pause");
            if (work_timer != nullptr)
            {
                must_pause_work = true;
                while (work_timer != nullptr && cmd_from_sd)
                {
                    reactor->pause(get_monotonic() + .001);
                }
            }
        }

        void VirtualSD::do_resume()
        {
            SPDLOG_INFO("do_resume");
            is_over_work = false;
            if (work_timer != nullptr)
            {
                throw elegoo::common::CommandError("SD busy");
            }
            must_pause_work = false;
            work_timer = reactor->register_timer(
                [this](double eventtime)
                { return work_handler(eventtime); }, reactor->NOW, "virtual_sdcard");
        }

        void VirtualSD::do_cancel()
        {
            SPDLOG_INFO("do_cancel");
            is_over_work = true;
            if (current_file)
            {
                if (work_timer != nullptr)
                {
                    must_pause_work = true;
                    while (work_timer != nullptr && cmd_from_sd)
                    {
                        reactor->pause(get_monotonic() + .001);
                    }
                }
#if OP_LIBC
                fclose(current_file);
                current_file = NULL;
#else
                current_file.close();
                current_file.clear();
#endif
                print_stats->note_cancel();
            }
            file_position = file_size = 0;
        }

        void VirtualSD::cmd_error(std::shared_ptr<GCodeCommand> gcmd)
        {
            throw elegoo::common::CommandError("SD write not supported");
        }

        void VirtualSD::_reset_file()
        {
            if (current_file)
            {

                SPDLOG_INFO("_reset_file");
                do_pause();
#if OP_LIBC
                fclose(current_file);
                current_file = NULL;
#else
                current_file.close();
                current_file.clear();
#endif
            }
            file_position = file_size = 0;
            print_stats->reset();
            elegoo::common::SignalManager::get_instance().emit_signal("virtual_sdcard:reset_file");
        }

        void VirtualSD::cmd_RESET_PRINT_TIME(std::shared_ptr<GCodeCommand> gcmd)
        {
            gcode->run_script_from_command("M2202 GCODE_ACTION_REPORT=2410");
        }

        void VirtualSD::cmd_SDCARD_RESET_FILE(std::shared_ptr<GCodeCommand> gcmd)
        {
            SPDLOG_INFO("cmd_SDCARD_RESET_FILE");
            if (cmd_from_sd)
                throw elegoo::common::CommandError("SDCARD_RESET_FILE cannot be run from the sdcard");
            _reset_file();
        }

        void VirtualSD::cmd_SDCARD_PRINT_FILE(std::shared_ptr<GCodeCommand> gcmd)
        {
            if (work_timer)
                throw elegoo::common::CommandError("SD busy");
            std::string filename = gcmd->get("FILENAME");
            // bool slice_cfg_model = gcmd->get_int("SLICE_CFG_MODEL",1,0,1);
            // SPDLOG_INFO("{} slice_cfg_model:{} filename={}",__func__,slice_cfg_model, filename);
            if (!filename.empty() && filename[0] == '/')
                filename = filename.substr(1);
            if (filename.empty())
            {
                SPDLOG_WARN("print filename is null , refuse to start print");
                return;
            }
            
            std::string full_path = sdcard_dirname + "/" + filename;
            struct stat file_stat;
            if (stat(full_path.c_str(), &file_stat) == -1) {
                SPDLOG_ERROR("File not found: {}", full_path);
                return;
            }

            _reset_file();
            _load_file(gcmd, filename, true);
            // 增加打印发起后的起始打印gcode
            // if(0 == slice_cfg_model)
            // {
                print_start_gcode->run_gcode_from_command();
            // }
            
            do_resume();
        }

        void VirtualSD::cmd_POWER_OUTAGE_RESUME(std::shared_ptr<GCodeCommand> gcmd)
        {
            int is_resume = gcmd->get_int("RESUME", 0);
            bool is_power_outage_null = power_outage_context.is_null();
            if (!is_power_outage_null)
            {
            }
            else
            {
                SPDLOG_ERROR("Could not open the file for reading.");
            }
            
            if (is_resume && (!is_power_outage_null))
            {   
                json res;
                res["command"] = "M2202";
                res["result"] = "2405";
                gcmd->respond_feedback(res);

                //G90
                gcode->run_script_from_command("G90");
                gcode->run_script_from_command("BED_MESH_CLEAR");
                gcode->run_script_from_command("BED_MESH_CALIBRATE_SET EXECUTE_CALIBRATE_FROM_SLICER=" + std::to_string(power_outage_context["execute_calirate_from_slicer"].get<int>()));
                gcode->run_script_from_command("SET_STEPPER_ENABLE STEPPER=stepper_z");
                // 设置GCODE文件
                SPDLOG_INFO("Step 0: configure GCODE file");
                gcode->run_script_from_command("M23 " + power_outage_context["file_name"].get<std::string>());
                gcode->run_script_from_command("M73 L" + std::to_string(power_outage_context["current_layer"].get<double>()));
                gcode->run_script_from_command("M26 S" + std::to_string(power_outage_context["file_position"].get<uint64_t>()));
                print_stats->set_print_duration_before_power_loss(power_outage_context["print_duration"].get<double>());
                // 床网加热
                SPDLOG_INFO("Step 1: heated bed.");
                gcode->run_script_from_command("M140 S" + std::to_string(power_outage_context["B"].get<double>()));
                // // 喷嘴加热至：断电前温度-80
                std::shared_ptr<PrinterHeaters> pheaters =
                    any_cast<std::shared_ptr<PrinterHeaters>>(printer->lookup_object("heaters"));
                double cur_temp = pheaters->get_heaters()["extruder"]->get_temp(0).first;
                SPDLOG_INFO("Step 2: Extruder heating. (-80),  current temp:{} T-80:{}",cur_temp,power_outage_context["T"].get<double>() - 80);
                if(cur_temp < (power_outage_context["T"].get<double>() - 80)) {
                    // gcode->run_script_from_command("M109 S" + std::to_string(power_outage_context["T"].get<double>() - 80));
                }
                // 移动Z轴到: 断电前位置+5
                SPDLOG_INFO("Step 3: move Z(+5).");
                std::vector<double> position = power_outage_context["gcode_position"].get<std::vector<double>>();
                double kinematic_position_z = power_outage_context["kinematic_position_z"].get<double>();
                gcode->run_script_from_command("SET_KINEMATIC_POSITION Z=" + std::to_string(kinematic_position_z));
                if(kinematic_position_z + 10. >= 256.)
                {
                    gcode->run_script_from_command("G1 Z" + std::to_string(kinematic_position_z + 1.) + " F200\nM400");
                }
                else
                {
                    gcode->run_script_from_command("G1 Z" + std::to_string(kinematic_position_z + 10.) + " F200\nM400");
                }
                // 回零
                SPDLOG_INFO("Step 4: homing.");
                // 移动至废料盒位置
                SPDLOG_INFO("Step 5: move to the waste box.");
                move_to_waste_box->run_gcode_from_command();
                // gcode->run_script_from_command("G28 X Y\nG1 X52 Y245 F20000\nG1 Y264 F3000\nM400");
                // 喷嘴加热至：断电前温度
                gcode->run_script_from_command("M400");
                SPDLOG_INFO("Step 6: extruder heating.");
                gcode->run_script_from_command("M109 S" + std::to_string(power_outage_context["T"].get<double>()));
                gcode->run_script_from_command("M190 S" + std::to_string(power_outage_context["B"].get<double>()));
                // 多色
                canvas_power_outage_resume();
                // 吐料
                SPDLOG_INFO("Step 7: extrude filament(100).");
                extrude_filament->run_gcode_from_command();
                // gcode->run_script_from_command("M83\nG1 E100 F300\nM400");
                // 开启风扇
                SPDLOG_INFO("Step 8: turn on the fan.");
                gcode->run_script_from_command("M106 S" + std::to_string(power_outage_context["model_fan_speed"].get<double>()));
                gcode->run_script_from_command("M106 P2 S" + std::to_string(power_outage_context["fan1_fan_speed"].get<double>()));
                gcode->run_script_from_command("SET_CAVITY_FAN SPEED=" + std::to_string(power_outage_context["box_fan_speed"].get<double>()));
                // 设置压力提前
                if (power_outage_context.contains("pressure_advance") && power_outage_context.contains("smooth_time"))
                {
                    SPDLOG_INFO("Step 9: set pressure advance.");
                    std::string set_pressure_advance = "SET_PRESSURE_ADVANCE ADVANCE=" +
                                                       std::to_string(power_outage_context["pressure_advance"].get<double>()) + " SMOOTH_TIME=" +
                                                       std::to_string(power_outage_context["smooth_time"].get<double>());
                    gcode->run_script_from_command(set_pressure_advance);
                }
                // 设置共振补偿
                gcode->run_script_from_command("M400");
                if (power_outage_context.contains("shaper_type_x") && power_outage_context.contains("shaper_freq_x") &&
                    power_outage_context.contains("damping_ratio_x") && power_outage_context.contains("shaper_type_y") &&
                    power_outage_context.contains("shaper_freq_y") && power_outage_context.contains("damping_ratio_y"))
                {
                    SPDLOG_INFO("Step 10: set input shaper.");
                    std::string set_input_shaper = "SET_INPUT_SHAPER"
                                                   " SHAPER_TYPE_X=" +
                                                   power_outage_context["shaper_type_x"].get<std::string>() +
                                                   " SHAPER_FREQ_X=" + power_outage_context["shaper_freq_x"].get<std::string>() +
                                                   " DAMPING_RATIO_X=" + power_outage_context["damping_ratio_x"].get<std::string>() +
                                                   " SHAPER_TYPE_Y=" + power_outage_context["shaper_type_y"].get<std::string>() +
                                                   " SHAPER_FREQ_Y=" + power_outage_context["shaper_freq_y"].get<std::string>() +
                                                   " DAMPING_RATIO_Y=" + power_outage_context["damping_ratio_y"].get<std::string>();
                    gcode->run_script_from_command(set_input_shaper);
                    gcode->run_script_from_command("M400");
                }
                // 设置加速度
                SPDLOG_INFO("Step 11: set accel.");
                gcode->run_script_from_command("M204 S" + std::to_string(power_outage_context["max_accel"].get<double>()));
                // 移动X,Y到断电前位置
                std::string profile_name = power_outage_context["profile_name"].get<std::string>();
                // if(power_outage_context["is_pause"].get<bool>())
                // {
                    // 加载网床数据                                   
                    SPDLOG_INFO("Step 13: load bed mesh");                
                    if(!profile_name.empty())
                    {
                        gcode->run_script_from_command("BED_MESH_PROFILE LOAD=" + profile_name);
                    }   
                // }    
                SPDLOG_INFO("Step 12: move X Y to the position before power outage");
                gcode->run_script_from_command("G1 X" + std::to_string(position[0]) +
                                               " Y" + std::to_string(position[1]) + " F20000");
                gcode->run_script_from_command("M400");                            
                // 移动Z到断电前位置
                SPDLOG_INFO("Step 14: move z to the position before power outage");
                gcode->run_script_from_command("G1 Z" + std::to_string(position[2]));
                gcode->run_script_from_command("M400");
                // 设置挤出头运动模式：绝对\相对
                SPDLOG_INFO("Step 17: set extruder movement mode");
                if (power_outage_context["absolute_extrude"].get<bool>())
                {
                    gcode->run_script_from_command("M82");
                }
                else
                {
                    gcode->run_script_from_command("M83");
                }
                // 设置速度模式
                if (power_outage_context.contains("speed_factor"))
                {
                    double speed_factor = power_outage_context["speed_factor"].get<double>() * 100.;
                    SPDLOG_INFO("speed_factor {}",speed_factor);
                    gcode->run_script_from_command("M220 S" + std::to_string(speed_factor));
                }
                // 设置挤出机当前位置
                SPDLOG_INFO("Step 18: set extruder position");
                gcode->run_script_from_command("G92 E" + std::to_string(position[3]));
                gcode->run_script_from_command("M400");
                // 开始工作
                SPDLOG_INFO("Step 19: start printing");
                gcode->run_script_from_command("M24");

                res["result"] = "2406";
                gcmd->respond_feedback(res);

                // std::shared_ptr<SwitchSensor> switch_sensor =
                //     any_cast<std::shared_ptr<SwitchSensor>>(printer->lookup_object("filament_switch_sensor filament_sensor", std::shared_ptr<SwitchSensor>()));
                // if(switch_sensor && switch_sensor->get_status(0)["filament_detected"].get<bool>() == false)
                // {
                //     switch_sensor->runout_helper->trigger_runout_event(-power_outage_context["filament_remaining"].get<double>());
                // }
            }
            else
            {
                print_stats->set_print_duration_before_power_loss(0);
            }

            if (std::remove(power_outage_file_name.c_str()) == 0)
            {
                SPDLOG_INFO("File successfully deleted.");
            }
            else
            {
                SPDLOG_WARN("File deletion failed. {}", strerror(errno));
            }
            power_outage_context = nullptr;
        }

        void VirtualSD::cmd_SET_POWER_OUTAGE_ENABLE(std::shared_ptr<GCodeCommand> gcmd)
        {
            power_outage_enable = gcmd->get_int("ENABLE", 1);

            // std::string cfgname = config->get_name();
            // std::shared_ptr<PrinterConfig> configfile =
            //     any_cast<std::shared_ptr<PrinterConfig>>(printer->lookup_object("configfile"));
            // configfile->set(cfgname, "power_outage_enable", std::to_string(power_outage_enable));
            // configfile->cmd_SAVE_CONFIG(gcode->create_gcode_command("SAVE_CONFIG", "SAVE_CONFIG", std::map<std::string, std::string>()));
        }

        void VirtualSD::cmd_M20(std::shared_ptr<GCodeCommand> gcmd)
        {
            auto files = get_file_list();
            gcmd->respond_raw("Begin file list");
            for (const auto &file : files)
            {
                gcmd->respond_raw(file.first + " " + std::to_string(file.second));
            }
            gcmd->respond_raw("End file list");
        }

        void VirtualSD::cmd_M21(std::shared_ptr<GCodeCommand> gcmd)
        {
            gcmd->respond_raw("SD card ok");
        }

        void VirtualSD::cmd_M23(std::shared_ptr<GCodeCommand> gcmd)
        {
            if (work_timer)
            {
                throw elegoo::common::CommandError("SD busy");
            }
            _reset_file();
            auto filename = gcmd->get_raw_command_parameters();
            filename = common::strip(filename);

            if (!filename.empty() && filename[0] == '/')
            {
                filename = filename.substr(1);
            }
            _load_file(gcmd, filename);
        }

        std::string to_lower(const std::string &str)
        {
            std::string lower_str;
            std::transform(str.begin(), str.end(), std::back_inserter(lower_str),
                           ::tolower);
            return lower_str;
        }

        void VirtualSD::_load_file(std::shared_ptr<GCodeCommand> gcmd,
                                   const std::string &filename, bool check_subdirs)
        {
            std::vector<std::pair<std::string, uint64_t>> files = get_file_list(check_subdirs);
            std::vector<std::string> flist;
            std::string fname = filename;
            fname = sdcard_dirname + "/" + fname;
            SPDLOG_INFO("fname {}", fname);
            try
            {
#if OP_LIBC
                FILE *fp = fopen(fname.c_str(), "rb");
                if (!fp)
                    throw elegoo::common::CommandError("Unable to open file");
                fseeko64(fp, 0, SEEK_END);
                uint64_t fsize = ftello64(fp);
                fseeko64(fp, 0, SEEK_SET);
#else
                std::ifstream f(fname, std::ios::binary);
                if (!f.is_open())
                {
                    throw elegoo::common::CommandError("Unable to open file");
                }

                f.seekg(0, std::ios::end);
                uint64_t fsize = f.tellg();
                f.seekg(0);
#endif
                SPDLOG_INFO("VirtualSD File opened: {}, Size:{}", fname, fsize);
                gcmd->respond_raw("File opened: " + filename + " Size: " + std::to_string(fsize));
                gcmd->respond_raw("File selected");
#if OP_LIBC
                current_file = fp;
#else
                current_file = std::move(f);
#endif

                file_position = 0;
                file_size = fsize;
                current_file_name = filename;
                this->print_stats->set_current_file(current_file_name);
                
            }
            catch (const std::exception &e)
            {
                SPDLOG_ERROR("virtual_sdcard file open");
                throw elegoo::common::CommandError("Unable to open file: " + std::string(e.what()));
            }
        }

        void VirtualSD::cmd_M24(std::shared_ptr<GCodeCommand> gcmd)
        {
            do_resume();
        }

        void VirtualSD::cmd_M25(std::shared_ptr<GCodeCommand> gcmd)
        {
            SPDLOG_INFO("cmd_M25");
            do_pause();
        }

        void VirtualSD::cmd_M26(std::shared_ptr<GCodeCommand> gcmd)
        {
            if (work_timer)
            {
                throw elegoo::common::CommandError("SD busy");
            }

            int pos = gcmd->get_int("S", INT_NONE, 0);
            file_position = pos;
        }

        void VirtualSD::cmd_M27(std::shared_ptr<GCodeCommand> gcmd)
        {
            if (!current_file)
            {
                gcmd->respond_raw("Not SD printing.");
                return;
            }
            gcmd->respond_raw("SD printing byte " + std::to_string(file_position) + "/" +
                              std::to_string(file_size));
        }

        int VirtualSD::get_file_position()
        {
            return next_file_position;
        }

        void VirtualSD::set_file_position(uint64_t pos)
        {
            next_file_position = pos;
        }

        bool VirtualSD::file_exists(const std::string &filename)
        {
            // if(access(power_outage_file_name.c_str(),F_OK))
            // {
            //     SPDLOG_INFO("exist power_outage_file_name");
            // }
            // else
            // {
            //     SPDLOG_INFO("not exist power_outage_file_name");
            // }
            std::ifstream file(filename);
            return file.good();
        }

        bool VirtualSD::is_cmd_from_sd() { return cmd_from_sd; }

        double VirtualSD::work_handler(double eventtime)
        {
            SPDLOG_INFO("Starting SD card print (position " + std::to_string(file_position) + ")");
            reactor->unregister_timer(work_timer);
#if OP_LIBC
            if (fseeko64(current_file, file_position, SEEK_SET) == -1)
            {
                SPDLOG_ERROR("virtual_sdcard seek failed: " + std::string(strerror(errno)));
                work_timer.reset();
                return reactor->NEVER;
            }
#else
            if (!current_file.seekg(file_position, std::ios::beg))
            {
                SPDLOG_ERROR("virtual_sdcard seek");
                work_timer.reset();
                return reactor->NEVER;
            }
#endif

            print_stats->note_start();
            auto gcode_mutex = gcode->get_mutex();

            std::string partial_input = "";
            std::vector<std::string> lines;
            std::string error_message = "";
            char buffer[8192];
            size_t rcount = 0;
            while (!must_pause_work)
            {
                if (lines.empty())
                {
#if OP_LIBC

                    rcount = fread(buffer, 1, sizeof(buffer), current_file);
                    if (rcount == 0)
                    {
                        // 完成
                        if (feof(current_file))
                        {
                            fclose(current_file);
                            current_file = NULL;
                            SPDLOG_INFO("Finished SD card print");
                            gcode->respond_raw("Done printing file");
                            break;
                        }
                        // 错误
                        if (ferror(current_file))
                        {
                            fclose(current_file);
                            current_file = NULL;
                            error_message = "virtual_sdcard read failed: " + std::string(strerror(errno));
                            SPDLOG_ERROR(error_message);
                            break;
                        }
                    }

                    std::string data(buffer, rcount);
#else
                    current_file.read(buffer, sizeof(buffer));
                    // 读取错误
                    if (current_file.bad())
                    {
                        current_file.close();
                        current_file = std::ifstream();
                        SPDLOG_ERROR("virtual_sdcard read bad");
                        error_message = "virtual_sdcard read bad";
                        break;
                    }

                    std::streamsize bytesRead = current_file.gcount();
                    if (bytesRead == 0)
                    {
                        current_file.close();
                        current_file = std::ifstream();
                        SPDLOG_INFO("Finished SD card print");
                        gcode->respond_raw("Done printing file");
                        break;
                    }

                    std::string data(buffer, bytesRead);
#endif
                    lines = common::split(data, "\n");
                    if (!lines.empty())
                    {
                        lines.front() = partial_input + lines.front();
                        partial_input = lines.back();
                        lines.pop_back();
                    }
                    std::reverse(lines.begin(), lines.end());
                    memset(buffer, 0, sizeof(buffer));
                    reactor->pause(reactor->NOW);
                    continue;
                }

                if (gcode_mutex->test())
                {
                    reactor->pause(get_monotonic() + 0.100);
                    continue;
                }

                cmd_from_sd = true;
                std::string line = lines.back();
                lines.pop_back();
                uint64_t next_file_position = file_position + line.size() + 1;
                this->next_file_position = next_file_position;
                try
                {
                    gcode->run_script(line);
                }
                catch (const elegoo::common::CommandError &e)
                {
                    error_message = e.what();
                    SPDLOG_INFO("error_message {}", error_message);
                    try
                    {
                        on_error_gcode->run_gcode_from_command();
                    }
                    catch (const std::exception &e)
                    {
                        SPDLOG_ERROR("virtual_sdcard on_error");
                    }
                    break;
                }
                catch (...)
                {
                    SPDLOG_ERROR("virtual_sdcard dispatch");
                    break;
                }

                cmd_from_sd = false;
                file_position = next_file_position;
                if (this->next_file_position != next_file_position)
                {
#if OP_LIBC
                    if (fseeko64(current_file, file_position, SEEK_SET) == -1)
                    {
                        SPDLOG_ERROR("virtual_sdcard seek failed: " + std::string(strerror(errno)));
                        work_timer.reset();
                        return reactor->NEVER;
                    }
#else
                    if (!current_file.seekg(file_position, std::ios::beg))
                    {
                        SPDLOG_ERROR("virtual_sdcard seek");
                        work_timer.reset();
                        return reactor->NEVER;
                    }
#endif
                    lines.clear();
                    partial_input = "";
                }
            }

            SPDLOG_INFO("Exiting SD card print (position " + std::to_string(file_position) + ")");
            work_timer.reset();
            cmd_from_sd = false;
#if OP_LIBC

            if (!error_message.empty())
                print_stats->note_error(error_message);
            else if (current_file != NULL)
                print_stats->note_pause();
            else
                print_stats->note_complete();
#else
            if (!error_message.empty())
            {
                print_stats->note_error(error_message);
            }
            else if (current_file.is_open())
            {
                print_stats->note_pause();
            } 
            else
            {
                SPDLOG_INFO("print_stats->note_complete");
                // if(canvas_dev && canvas_dev->is_canvas_dev_connect())
                // {
                //     canvas_dev->must_pause_work();
                // }
                is_over_work = true;
                print_stats->note_complete();
            }
            

#endif
            return reactor->NEVER;
        }

        std::shared_ptr<VirtualSD> virtual_sdcard_load_config(std::shared_ptr<ConfigWrapper> config)
        {
            return std::make_shared<VirtualSD>(config);
        }

    }
}
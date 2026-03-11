/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2024-11-15 11:45:02
 * @LastEditors  : coconut
 * @LastEditTime : 2025-02-25 18:26:21
 * @Description  : Perform Z Homing at specific XY coordinates
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "safe_z_home.h"
#include "utilities.h"
namespace elegoo
{
  namespace extras
  {

    SafeZHoming::SafeZHoming(std::shared_ptr<ConfigWrapper> config)
        : config(config)
    {
      printer = config->get_printer();
      std::vector<double> xy_pos = config->getdoublelist("home_xy_position", {}, ',');
      home_x_pos = xy_pos[0];
      home_y_pos = xy_pos[1];

      z_hop = config->getdouble("z_hop", 0.0);
      z_hop_speed = config->getdouble("z_hop_speed", 15.0, DOUBLE_NONE, DOUBLE_NONE, 0);
      auto zconfig = config->getsection("stepper_z");
      max_z = zconfig->getdouble("position_max", DOUBLE_INVALID, DOUBLE_NONE,
                                 DOUBLE_NONE, DOUBLE_NONE, DOUBLE_NONE, false);
      speed = config->getdouble("speed", 50, DOUBLE_NONE, DOUBLE_NONE, 0);
      move_to_previous = config->getboolean("move_to_previous", BoolValue::BOOL_FALSE);
      any_cast<std::shared_ptr<PrinterHoming>>(printer->load_object(config, "homing"));
      gcode = any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));
      prev_G28 = gcode->register_command("G28", nullptr);
      gcode->register_command("G28", [this](std::shared_ptr<GCodeCommand> cmd)
                              { this->cmd_G28(cmd); });

      if (config->has_section("homing_override"))
        throw elegoo::common::CommandError("homing_override and safe_z_homing cannot be used simultaneously");
    }

    void SafeZHoming::cmd_G28(std::shared_ptr<GCodeCommand> gcmd)
    {
      auto toolhead = any_cast<std::shared_ptr<ToolHead>>(printer->lookup_object("toolhead"));

      // Perform Z Hop if necessary
      if (z_hop != 0.)
      {
        double curtime = 0.0;
        curtime = get_monotonic();
        json kin_status = toolhead->get_kinematic()->get_status(curtime);
        auto pos = toolhead->get_position();

        if (kin_status["homed_axes"].get<std::string>().find('z') == std::string::npos)
        {
          // Always perform the z_hop if the Z axis is not homed
          pos[2] = 0;
          std::vector<double> coord = {DOUBLE_NONE, DOUBLE_NONE, z_hop};
          toolhead->set_position(pos, {2});
          toolhead->manual_move(coord, z_hop_speed);
        }
        else if (pos[2] < z_hop)
        {
          //  If the Z axis is homed, and below z_hop, lift it to z_hop
          std::vector<double> coord = {DOUBLE_NONE, DOUBLE_NONE, z_hop};
          toolhead->manual_move(coord, z_hop_speed);
        }
      }

      // Determine which axes we need to home
      bool need_x = gcmd->get("X", "None") != "None";
      bool need_y = gcmd->get("Y", "None") != "None";
      bool need_z = gcmd->get("Z", "None") != "None";
      SPDLOG_INFO("need {} {} {}", need_x, need_y, need_z);

      if (!need_x && !need_y && !need_z)
      {
        need_x = need_y = need_z = true;
      }

      // Home XY axes if necessary
      std::map<std::string, std::string> new_params;
      if (need_x)
      {
        new_params["X"] = "0";
      }
      if (need_y)
      {
        new_params["Y"] = "0";
      }

      if (!new_params.empty())
      {
        std::shared_ptr<GCodeCommand> g28_gcmd = gcode->create_gcode_command("G28", "G28", new_params);
        this->prev_G28(g28_gcmd);
      }
      // Home Z axis if necessary
      if (need_z)
      {
        // Throw an error if X or Y are not homed
        double curtime = get_monotonic();
        json kin_status = toolhead->get_kinematic()->get_status(curtime);
        if ((!kin_status["homed_axes"].get<std::string>().find('x') == std::string::npos) ||
            (!kin_status["homed_axes"].get<std::string>().find('y') == std::string::npos))
          throw elegoo::common::CommandError("Must home X and Y axes first");

        // Move to safe XY homing position
        auto prevpos = toolhead->get_position();
        std::vector<double> coord = {home_x_pos, home_y_pos};
        toolhead->manual_move(coord, speed);
        // Home Z
        std::map<std::string, std::string> params = {{"Z", "0"}};
        std::shared_ptr<GCodeCommand> g28_gcmd = gcode->create_gcode_command("G28", "G28", params);
        this->prev_G28(g28_gcmd);
        // Perform Z Hop again for pressure-based probes
        if (z_hop != 0.0)
        {
          auto pos = toolhead->get_position();
          if (pos[2] < z_hop)
          {
            std::vector<double> coord = {DOUBLE_NONE, DOUBLE_NONE, z_hop};
            toolhead->manual_move(coord, z_hop_speed);
          }
        }

        if (move_to_previous)
        {
          std::vector<double> coord = {prevpos[0], prevpos[1]};
          toolhead->manual_move(coord, speed);
        }
      }
    }

    std::shared_ptr<SafeZHoming> safe_z_home_load_config(
        std::shared_ptr<ConfigWrapper> config)
    {
      return std::make_shared<SafeZHoming>(config);
    }

  }
}
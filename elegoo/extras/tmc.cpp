/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2024-11-20 15:16:18
 * @LastEditors  : Jack
 * @LastEditTime : 2024-11-26 14:47:18
 * @Description  : Common helper code for TMC stepper drivers
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "tmc.h"
#include "logger.h"
#include "utilities.h"
namespace elegoo
{
  namespace extras
  {

    // ######################################################################
    // # Field helpers
    // ######################################################################
    FieldHelper::FieldHelper(
        std::map<std::string, std::map<std::string, uint32_t>> all_fields,
        std::vector<std::string> signed_fields,
        std::map<std::string, std::function<std::string(int)>>
            field_formatters,
        std::vector<std::pair<std::string, int64_t>> registers)
    {
      SPDLOG_DEBUG("FieldHelper init!");
      this->all_fields = all_fields;

      for (int i = 0; i < signed_fields.size(); i++)
      {
        this->signed_fields[signed_fields[i]] = 1;
      }

      this->field_formatters = field_formatters;
      this->registers = registers;

      for (const auto &af : all_fields)
      {
        for (const auto &f : af.second)
        {
          this->field_to_register[f.first] = af.first;
        }
      }
      SPDLOG_DEBUG("FieldHelper init success!!");
    }

    FieldHelper::~FieldHelper() {}

    std::string FieldHelper::lookup_register(std::string field_name,
                                             std::string default_value)
    {
      if (this->field_to_register.find(field_name) ==
          this->field_to_register.end())
      {
        return default_value;
      }
      else
      {
        return field_to_register.at(field_name);
      }
    }

    int64_t FieldHelper::get_field(std::string field_name, uint32_t reg_value,
                                   std::string reg_name)
    {
      using namespace elegoo::common;
      if (reg_name == "")
      {
        reg_name = field_to_register.at(field_name);
      }

      if (reg_value == 0)
      {
        auto it = std::find_if(registers.begin(), registers.end(),
                               [&reg_name](const std::pair<std::string, int> &p)
                               {
                                 return p.first == reg_name;
                               });

        reg_value = (it == registers.end()) ? 0 : it->second;
      }

      uint32_t mask = all_fields.at(reg_name).at(field_name);
      int64_t field_value = (reg_value & mask) >> elegoo::common::ffs(mask);

      if ((signed_fields.find(field_name) != signed_fields.end()) &&
          (((reg_value & mask) << 1) > mask))
      {
        field_value -= (1 << elegoo::common::bit_length(field_value));
      }
      return field_value;
    }

    int64_t FieldHelper::set_field(std::string field_name, int64_t field_value,
                                   uint32_t reg_value, std::string reg_name)
    {

      // SPDLOG_INFO("{} : field_name:{} field_value {}", __FUNCTION__, field_name, field_value);
      // printf("%s: %s %lld\n", __FUNCTION__, field_name.c_str(), field_value);

      if (reg_name == "" && field_to_register.find(field_name) != field_to_register.end())
      {
        reg_name = field_to_register.at(field_name);
      }
      auto it = std::find_if(registers.begin(), registers.end(),
                             [&reg_name](const std::pair<std::string, int> &p)
                             {
                               return p.first == reg_name;
                             });

      if (reg_value == 0)
      {
        reg_value = (it == registers.end()) ? 0 : it->second;
      }
      uint32_t mask = all_fields[reg_name][field_name];
      int64_t new_value = (reg_value & ~mask) |
                          ((field_value << elegoo::common::ffs(mask)) & mask);
      if (it == registers.end())
      {
        registers.push_back({reg_name, new_value});
      }
      else
      {
        it->second = new_value;
      }
      return new_value;
    }

    int64_t FieldHelper::set_config_field(std::shared_ptr<ConfigWrapper> config,
                                          std::string field_name, int default_val)
    {
      std::string field_upper_str = field_name;
      std::transform(field_upper_str.begin(), field_upper_str.end(),
                     field_upper_str.begin(), ::toupper);

      std::string config_name = "driver_" + field_upper_str;
      std::string reg_name = field_to_register.at(field_name);
      uint32_t mask = all_fields.at(reg_name).at(field_name);
      int64_t maxval = mask >> elegoo::common::ffs(mask);
      int64_t val;
      if (maxval == 1)
      {
        val = config->getboolean(config_name, static_cast<BoolValue>(default_val));
      }
      else if (signed_fields.find(field_name) != signed_fields.end())
      {
        val = config->getint(config_name, default_val, -(maxval / 2 + 1), maxval / 2);
      }
      else
      {
        val = config->getint(config_name, default_val, 0, maxval);
      }

      // SPDLOG_INFO("{} : field_name {} config_name {} field_value {} default_val {}", __FUNCTION__, field_name, config_name, val, default_val);
      // printf("%s: %s %s %lld %lld\n", __FUNCTION__, field_name.c_str(), config_name.c_str(), val, default_val);
      return set_field(field_name, val);
    }

    std::string FieldHelper::pretty_format(const std::string &reg_name,
                                           int reg_value)
    {
      std::map<std::string, uint32_t> reg_fields;
      if (all_fields.find(reg_name) != all_fields.end())
      {
        reg_fields = all_fields.at(reg_name);
      }

      std::vector<std::pair<uint32_t, std::string>> sorted_fields;
      for (const auto &reg_field : reg_fields)
      {
        sorted_fields.emplace_back(reg_field.second, reg_field.first);
      }
      std::sort(sorted_fields.begin(), sorted_fields.end());

      std::vector<std::string> fields;
      for (const auto &field : sorted_fields)
      {
        int field_value = this->get_field(field.second, reg_value, reg_name);
        std::string sval = field_formatters.count(field.second)
                               ? field_formatters.at(field.second)(field_value)
                               : std::to_string(field_value);
        if (!sval.empty() && sval != "0")
        {
          fields.push_back(" " + field.second + "=" + sval);
        }
      }

      std::ostringstream oss;
      oss << std::setw(11) << std::left << (reg_name + ":") << std::hex
          << std::setw(8) << std::setfill('0') << reg_value << std::dec
          << std::setw(0) << std::setfill(' ') << std::flush;
      for (const auto &field : fields)
      {
        oss << field;
      }
      return oss.str();
    }

    std::map<std::string, int64_t> FieldHelper::get_reg_fields(
        const std::string &reg_name, int reg_value)
    {
      auto reg_fields = all_fields.find(reg_name);
      if (reg_fields == all_fields.end())
      {
        return {};
      }

      std::map<std::string, int64_t> result;
      for (const auto &gf : reg_fields->second)
      {
        result[gf.first] = get_field(gf.first, reg_value, reg_name);
      }
      return result;
    }

    // ######################################################################
    // # Periodic error checking
    // ######################################################################
    TMCErrorCheck::TMCErrorCheck(std::shared_ptr<ConfigWrapper> config,
                                 std::shared_ptr<MCU_TMC_Base> mcu_tmc,
                                 std::function<void(double)> fun)
    {
      SPDLOG_DEBUG("TMCErrorCheck init!");
      printer = config->get_printer();

      std::istringstream iss(config->get_name());
      std::vector<std::string> name_parts;
      std::string part;
      while (iss >> part)
      {
        name_parts.push_back(part);
      }

      std::ostringstream oss;
      for (size_t i = 1; i < name_parts.size(); ++i)
      {
        if (i > 1)
        {
          oss << " ";
        }
        oss << name_parts[i];
      }
      stepper_name = oss.str();

      SPDLOG_INFO("stepper_name {}", stepper_name);

      this->mcu_tmc = mcu_tmc;
      this->fields = mcu_tmc->get_fields();
      this->check_timer = nullptr;
      this->last_drv_status = 0;
      this->last_drv_fields.clear();
      this->init_register = fun;
      std::string reg_name = this->fields->lookup_register("drv_err");
      if (!reg_name.empty())
      {
        gstat_reg_info.first = reg_name;
        gstat_reg_info.second = {0, 0xffffffff, 0xffffffff, 0};
      }

      clear_gstat = true;
      irun_field = "irun";
      reg_name = "DRV_STATUS";
      uint32_t mask = 0, err_mask = 0, cs_actual_mask = 0;

      if (name_parts[0] == "tmc2130")
      {
        clear_gstat = false;
        cs_actual_mask = this->fields->all_fields.at(reg_name).at("cs_actual");
      }
      else if (name_parts[0] == "tmc2660")
      {
        irun_field = "cs";
        reg_name = "READRSP@RDSEL2";
        cs_actual_mask = this->fields->all_fields.at(reg_name).at("se");
      }

      std::vector<std::string> err_fields = {"ot", "s2ga", "s2gb", "s2vsa",
                                             "s2vsb"};
      std::vector<std::string> warn_fields = {"otpw", "t120", "t143", "t150",
                                              "t157"};

      for (const auto &f : err_fields)
      {
        if (this->fields->all_fields[reg_name].find(f) !=
            this->fields->all_fields[reg_name].end())
        {
          mask |= this->fields->all_fields[reg_name][f];
          err_mask |= this->fields->all_fields[reg_name][f];
        }
      }
      for (const auto &f : warn_fields)
      {
        if (this->fields->all_fields[reg_name].find(f) !=
            this->fields->all_fields[reg_name].end())
        {
          mask |= this->fields->all_fields[reg_name][f];
        }
      }

      drv_status_reg_info.first = reg_name;
      drv_status_reg_info.second = {0, mask, err_mask, cs_actual_mask};

      adc_temp = 0;
      adc_temp_reg = this->fields->lookup_register("adc_temp");
      if (!adc_temp_reg.empty())
      {
        auto pheaters = any_cast<std::shared_ptr<PrinterHeaters>>(
            printer->load_object(config, "heaters"));

        pheaters->register_monitor(config);
      }
      SPDLOG_DEBUG("TMCErrorCheck init success!!");
    }

    TMCErrorCheck::~TMCErrorCheck() {}

    uint32_t TMCErrorCheck::_query_register(
        const std::pair<std::string, std::vector<uint32_t>> &reg_info,
        bool try_clear)
    {
      auto last_value = reg_info.second[0];
      auto mask = reg_info.second[1];
      auto err_mask = reg_info.second[2];
      auto cs_actual_mask = reg_info.second[3];
      auto reg_name = reg_info.first;

      uint32_t cleared_flags = 0;
      uint32_t val = 0;
      std::string fmt;
      int count = 0;
      int irun = 0;

      while (1)
      {
        try
        {
          val = this->mcu_tmc->get_register(reg_name);
        }
        catch (const elegoo::common::CommandError &e)
        {
          count += 1;
          if (count < 3 &&
              std::string(e.what()).find("Unable to read tmc uart") ==
                  std::string::npos)
          {
            auto reactor = this->printer->get_reactor();
            reactor->pause(get_monotonic() + 0.050);
            continue;
          }
          throw;
        }

        if (val & mask != last_value & mask)
        {
          fmt = this->fields->pretty_format(reg_name, val);
          SPDLOG_ERROR("TMC {} reports {}", stepper_name, fmt);
        }

        last_value = val;

        if (!(val & err_mask))
        {
          if (!cs_actual_mask || val & cs_actual_mask)
          {
            break;
          }
          irun = this->fields->get_field(this->irun_field);
          if ((check_timer == nullptr) || (irun < 4))
          {
            break;
          }
          if ((this->irun_field == "irun") && !(fields->get_field("ihold")))
          {
            break;
          }
        }

        count += 1;
        if (count >= 3)
        {
          fmt = this->fields->pretty_format(reg_name, val);
          throw elegoo::common::CommandError("TMC " + stepper_name + " reports error: " + fmt);
        }

        if (try_clear && (val & err_mask))
        {
          try_clear = false;
          cleared_flags |= (val & err_mask);
          mcu_tmc->set_register(reg_name, val & err_mask);
        }
      }
      return cleared_flags;
    }

    void TMCErrorCheck::_query_temperature()
    {
      try
      {
        adc_temp = this->mcu_tmc->get_register(this->adc_temp_reg);
      }
      catch (const elegoo::common::CommandError &e)
      {
        adc_temp = 0;
        return;
      }
    }

    double TMCErrorCheck::_do_periodic_check(double eventtime)
    {
      try
      {
        _query_register(this->drv_status_reg_info);
        if (!gstat_reg_info.first.empty())
        {
          int cleared_flags = 0;
          cleared_flags = _query_register(gstat_reg_info, true);

          if (cleared_flags && stepper_name =="extruder")
          {
            auto reset_mask = this->fields->all_fields["GSTAT"]["reset"];
            if (cleared_flags & reset_mask)
            {
                init_register(DOUBLE_NONE);
                SPDLOG_WARN("Detected TMC extruder reset, reinitializing settings.");
            }
          }
        }
        if (!adc_temp_reg.empty())
        {
          _query_temperature();
        }
      }
      catch (const elegoo::common::CommandError &e)
      {
        printer->invoke_shutdown(e.what());
        return printer->get_reactor()->NEVER;
      }
      return eventtime + 1;
    }

    void TMCErrorCheck::stop_checks()
    {
      if (check_timer == nullptr)
      {
        return;
      }
      printer->get_reactor()->unregister_timer(check_timer);
      check_timer = nullptr;
    }

    bool TMCErrorCheck::start_checks()
    {
      if (!check_timer)
      {
        stop_checks();
      }
      int cleared_flags = 0;
      _query_register(drv_status_reg_info);
      if (!gstat_reg_info.first.empty())
      {
        cleared_flags = _query_register(gstat_reg_info, clear_gstat);
      }

      auto reactor = this->printer->get_reactor();
      double curtime = 0.0;
      curtime = get_monotonic();
      this->check_timer = reactor->register_timer(
          [this](double eventtime)
          { return _do_periodic_check(eventtime); },
          curtime + 1., std::string("tmc_error_check ") + stepper_name);

      if (cleared_flags)
      {
        auto reset_mask = this->fields->all_fields["GSTAT"]["reset"];
        if (cleared_flags & reset_mask)
        {
          return true;
        }
      }
      return false;
    }

    void TMCErrorCheck::get_status(
        std::map<std::string, std::map<std::string, int>> &drv_status,
        std::map<std::string, double> &temperature)
    {
      std::map<std::string, int> status;

      if (check_timer == nullptr)
      {
        status.clear();
        drv_status["drv_status"] = status;
        temperature["temperature"] = 0;
      }

      double temp = 0;
      if (adc_temp != 0)
      {
        temp = std::round((adc_temp - 2038) / 7.7 * 100.0) / 100.0;
      }

      uint32_t last_value = drv_status_reg_info.second[0];
      std::string reg_name = drv_status_reg_info.first;

      if (last_value != last_drv_status)
      {
        last_drv_status = last_value;
        std::map<std::string, int64_t> fields =
            this->fields->get_reg_fields(reg_name, last_value);
        last_drv_fields.clear();
        for (const auto &field : fields)
        {
          if (field.second)
          {
            last_drv_fields[field.first] = field.second;
          }
        }
      }
      drv_status["drv_status"] = last_drv_fields;
      temperature["temperature"] = temp;
    }

    // ######################################################################
    // # G-Code command helpers
    // ######################################################################
    const double MAX_CURRENT = 2.0;
    TMCCurrentHelper::TMCCurrentHelper(std::shared_ptr<ConfigWrapper> config,
                                       std::shared_ptr<MCU_TMC_Base> mcu_tmc) : mcu_tmc(mcu_tmc)
    {
      printer = config->get_printer();
      name = elegoo::common::split(config->get_name()).back();
      fields = mcu_tmc->get_fields();
      double run_current = config->getdouble("run_current", DOUBLE_INVALID, DOUBLE_NONE, MAX_CURRENT, 0);
      double hold_current = config->getdouble("hold_current", MAX_CURRENT, DOUBLE_NONE, MAX_CURRENT, 0);
      req_hold_current = hold_current;
      sense_resistor = config->getdouble("sense_resistor", 0.110, DOUBLE_NONE, DOUBLE_NONE, 0);
      double vsense, irun, ihold;
      std::tie(vsense, irun, ihold) = calc_current(run_current, hold_current);
      fields->set_field("vsense", vsense);
      fields->set_field("ihold", ihold);
      fields->set_field("irun", irun);
    }

    TMCCurrentHelper::~TMCCurrentHelper()
    {
    }

    std::tuple<double, double, double, double> TMCCurrentHelper::get_current()
    {
      int irun = fields->get_field("irun");
      int ihold = fields->get_field("ihold");
      bool vsense = fields->get_field("vsense") != 0;

      // 计算运行电流和保持电流
      double run_current = calc_current_from_bits(irun, vsense);
      double hold_current = calc_current_from_bits(ihold, vsense);

      // 返回电流值和常量
      return std::make_tuple(run_current, hold_current, req_hold_current, MAX_CURRENT);
    }

    void TMCCurrentHelper::set_current(double run_current, double hold_current, double print_time)
    {
      req_hold_current = hold_current;

      std::tuple<bool, int, int> value = calc_current(run_current, hold_current);

      if (std::get<0>(value) != fields->get_field("vsense"))
      {
        int val = fields->set_field("vsense", std::get<0>(value));
        mcu_tmc->set_register("CHOPCONF", val, print_time);
      }

      fields->set_field("ihold", std::get<2>(value));
      int val = fields->set_field("irun", std::get<1>(value));
      mcu_tmc->set_register("IHOLD_IRUN", val, print_time);
    }

    int TMCCurrentHelper::calc_current_bits(double current, bool vsense)
    {
      double adjusted_sense_resistor = sense_resistor + 0.020;
      double vref = vsense ? 0.18 : 0.32;

      // 计算 cs 值
      int cs = static_cast<int>(32.0 * adjusted_sense_resistor * current * std::sqrt(2.0) / vref + 0.5) - 1;

      // 返回限定范围 [0, 31] 的值
      return std::max(0, std::min(31, cs));
    }

    double TMCCurrentHelper::calc_current_from_bits(int cs, bool vsense)
    {
      double adjusted_sense_resistor = sense_resistor + 0.020;
      double vref = vsense ? 0.18 : 0.32;

      // 计算实际电流
      return (cs + 1) * vref / (32.0 * adjusted_sense_resistor * std::sqrt(2.0));
    }

    std::tuple<bool, int, int> TMCCurrentHelper::calc_current(double run_current, double hold_current)
    {
      bool vsense = true;
      int irun = calc_current_bits(run_current, true);
      if (irun == 31)
      {
        double cur = calc_current_from_bits(irun, true);
        if (cur < run_current)
        {
          int irun2 = calc_current_bits(run_current, false);
          double cur2 = calc_current_from_bits(irun2, false);
          if (std::abs(run_current - cur2) < std::abs(run_current - cur))
          {
            vsense = false;
            irun = irun2;
          }
        }
      }
      int ihold = calc_current_bits(std::min(hold_current, run_current), vsense);
      return std::make_tuple(vsense, irun, ihold);
    }

    static const std::string cmd_INIT_TMC_help =
        "Initialize TMC stepper driver registers";
    static const std::string cmd_SET_TMC_FIELD_help =
        "Set a register field of a TMC driver";
    static const std::string cmd_SET_TMC_CURRENT_help =
        "Set the current of a TMC driver";
    static const std::string cmd_DUMP_TMC_help =
        "Read and display TMC stepper driver registers";

    TMCCommandHelper::TMCCommandHelper(
        std::shared_ptr<ConfigWrapper> config,
        std::shared_ptr<MCU_TMC_Base> mcu_tmc,
        std::shared_ptr<TMCCurrentHelper> current_helper)
    {
      SPDLOG_DEBUG("TMCCommandHelper init!");
      printer = config->get_printer();

      std::vector<std::string> parts =
          elegoo::common::split(config->get_name());
      parts.erase(parts.begin());
      stepper_name = elegoo::common::join(parts, " ");
      name = elegoo::common::split(config->get_name()).back();

      this->mcu_tmc = mcu_tmc;
      this->current_helper = current_helper;
      this->echeck_helper = std::make_shared<TMCErrorCheck>(config, mcu_tmc,
        [this](double print_time) { 
          this->_init_registers(print_time); 
        });
      this->fields = mcu_tmc->get_fields();
      this->toff = INT_NONE;
      this->mcu_phase_offset = INT_NONE;
      this->stepper_enable = any_cast<std::shared_ptr<PrinterStepperEnable>>(
          printer->load_object(config, "stepper_enable"));

      elegoo::common::SignalManager::get_instance().register_signal(
          "stepper:sync_mcu_position",
          std::function<void(std::shared_ptr<MCU_stepper>)>([this](std::shared_ptr<MCU_stepper> stepper)
                                                            { _handle_sync_mcu_pos(stepper); }));

      elegoo::common::SignalManager::get_instance().register_signal(
          "stepper:set_sdir_inverted",
          std::function<void(std::shared_ptr<MCU_stepper>)>([this](std::shared_ptr<MCU_stepper> stepper)
                                                            { _handle_sync_mcu_pos(stepper); }));

      elegoo::common::SignalManager::get_instance().register_signal(
          "elegoo:mcu_identify",
          std::function<void()>([this]()
                                {
        SPDLOG_DEBUG("MCU_TMC_uart mcu_identify~~~~~~~~~~~~~~~~~");
          _handle_mcu_identify();
          SPDLOG_DEBUG("MCU_TMC_uart mcu_identify~~~~~~~~~~~~~~~~~ success!"); }));

      elegoo::common::SignalManager::get_instance().register_signal(
          "elegoo:connect",
          std::function<void()>([this]()
                                {
        SPDLOG_DEBUG("MCU_TMC_uart connect~~~~~~~~~~~~~~~~~");
          _handle_connect();
          SPDLOG_DEBUG("MCU_TMC_uart connect~~~~~~~~~~~~~~~~~ success!"); }));

      tmc::TMCMicrostepHelper(config, mcu_tmc);

      auto gcode =
          any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));
      gcode->register_mux_command(
          "SET_TMC_FIELD", "STEPPER", name,
          [this](std::shared_ptr<GCodeCommand> gcmd)
          { cmd_SET_TMC_FIELD(gcmd); },
          cmd_SET_TMC_FIELD_help);
      gcode->register_mux_command(
          "INIT_TMC", "STEPPER", name,
          [this](std::shared_ptr<GCodeCommand> gcmd)
          { cmd_INIT_TMC(gcmd); },
          cmd_INIT_TMC_help);
      gcode->register_mux_command(
          "SET_TMC_CURRENT", "STEPPER", name,
          [this](std::shared_ptr<GCodeCommand> gcmd)
          { cmd_SET_TMC_CURRENT(gcmd); },
          cmd_SET_TMC_CURRENT_help);
      SPDLOG_DEBUG("TMCCommandHelper init success!!");
    }

    TMCCommandHelper::~TMCCommandHelper()
    {
      SPDLOG_DEBUG("~TMCCommandHelper()");
    }

    void TMCCommandHelper::_init_registers(double print_time)
    {
      for (auto f : fields->registers)
      {
        std::string reg_name = f.first;
        auto val = f.second;
        // SPDLOG_DEBUG("_init_registers {} {}",reg_name,val);
        mcu_tmc->set_register(reg_name, val, print_time);
      }
    }

    void TMCCommandHelper::cmd_SET_TMC_FIELD(std::shared_ptr<GCodeCommand> gcmd)
    {
      std::string field_name, tmp;
      tmp = gcmd->get("FIELD");
      for (char c : tmp)
      {
        field_name += std::tolower(c);
      }

      auto reg_name = fields->lookup_register(field_name);
      if (reg_name == "")
      {
        throw elegoo::common::CommandError("Unknown field name " + field_name);
      }
      int value = gcmd->get_int("VALUE");
      float velocity = gcmd->get_double("VELOCITY", DOUBLE_NONE, 0);

      using namespace elegoo::common;
      if ((!value) == (!elegoo::common::are_equal(velocity, 0.0)))
      {
        throw CommandError("Specify either VALUE or VELOCITY");
      }

      if (elegoo::common::are_equal(velocity, 0.0))
      {
        if (!mcu_tmc->get_tmc_frequency())
        {
          throw CommandError("VELOCITY parameter not supported by this driver");
        }
        value = tmc::TMCtstepHelper(mcu_tmc, velocity, stepper);
      }
      auto reg_val = fields->set_field(field_name, value);
      auto print_time =
          any_cast<std::shared_ptr<ToolHead>>(printer->lookup_object("toolhead"))
              ->get_last_move_time();
      mcu_tmc->set_register(reg_name, reg_val, print_time);
    }

    void TMCCommandHelper::cmd_INIT_TMC(std::shared_ptr<GCodeCommand> gcmd)
    {
      SPDLOG_DEBUG("INIT_TMC {}", name);
      auto print_time =
          any_cast<std::shared_ptr<ToolHead>>(printer->lookup_object("toolhead"))
              ->get_last_move_time();
      _init_registers(print_time);
    }

    void TMCCommandHelper::cmd_SET_TMC_CURRENT(std::shared_ptr<GCodeCommand> gcmd)
    {
      std::shared_ptr<TMCCurrentHelper> ch = this->current_helper;

      double prev_cur, prev_hold_cur, req_hold_cur, max_cur;
      std::tie(prev_cur, prev_hold_cur, req_hold_cur, max_cur) = ch->get_current();
      auto run_current = gcmd->get_double("CURRENT", DOUBLE_NONE, 0, max_cur);
      auto hold_current = gcmd->get_double("HOLDCURRENT", DOUBLE_NONE, DOUBLE_NONE, max_cur, 0);

      using namespace elegoo::common;
      if (!std::isnan(run_current) ||
          !std::isnan(hold_current))
      {
        if (std::isnan(run_current))
        {
          run_current = prev_cur;
        }
        if (std::isnan(hold_current))
        {
          hold_current = req_hold_cur;
        }

        auto toolhead = any_cast<std::shared_ptr<ToolHead>>(printer->lookup_object("toolhead"));
        auto print_time = toolhead->get_last_move_time();

        ch->set_current(run_current, hold_current, print_time);
        std::tie(prev_cur, prev_hold_cur, req_hold_cur, max_cur) = ch->get_current();
      }

      if (std::isnan(prev_hold_cur))
      {
        gcmd->respond_info("Run Current: " + std::to_string(prev_cur) + "A", true);
      }
      else
      {
        gcmd->respond_info("Run Current:" + std::to_string(prev_cur) + " A Hold Current: " + std::to_string(prev_hold_cur) + "A", true);
      }
    }

    int64_t TMCCommandHelper::_get_phases()
    {
      return (256 >> fields->get_field("mres")) * 4;
    }

    std::pair<int64_t, int64_t> TMCCommandHelper::get_phase_offset()
    {
      return {mcu_phase_offset, _get_phases()};
    }

    int64_t TMCCommandHelper::_query_phase()
    {
      std::string field_name = "mscnt";
      if (fields->lookup_register(field_name).empty())
      {
        field_name = "mstep";
      }

      uint32_t reg = mcu_tmc->get_register(fields->lookup_register(field_name));
      return fields->get_field(field_name, reg);
    }

    void TMCCommandHelper::_handle_sync_mcu_pos(
        std::shared_ptr<MCU_stepper> stepper)
    {
      if (stepper->get_name() != stepper_name)
      {
        return;
      }
      int64_t driver_phase = 0, phases = 0, phase = 0, moff = 0;
      try
      {
        driver_phase = _query_phase();
      }
      catch (const elegoo::common::CommandError &e)
      {
        // logging.info("Unable to obtain tmc %s phase", self.stepper_name);
        mcu_phase_offset = INT_NONE;
        std::shared_ptr<EnableTracking> enable_line =
            stepper_enable->lookup_enable(stepper_name);
        if (enable_line->is_motor_enabled())
        {
          throw;
        }
        return;
      }
      if (!stepper->get_dir_inverted().first)
      {
        driver_phase = 1023 - driver_phase;
      }
      phases = _get_phases();
      phase = int(float(driver_phase) / 1024 * phases + 0.5) % phases;
      moff = (phase - stepper->get_mcu_position()) % phases;

      if ((mcu_phase_offset != INT_NONE) && mcu_phase_offset != moff)
      {
        SPDLOG_WARN("Stepper {} phase change (was {} now {})", stepper_name,
                    mcu_phase_offset, moff);
      }
      mcu_phase_offset = moff;
      SPDLOG_DEBUG("{} : {}___ ok", __FUNCTION__, __LINE__);
    }

    // void TMCCommandHelper::_handle_sync_mcu_pos() {}

    json TMCCommandHelper::_do_enable(double print_time)
    {
      try
      {
        if (toff != INT_NONE)
        {
          fields->set_field("toff", toff);
        }
        _init_registers();
        SPDLOG_DEBUG("__func__:{},__LINE__:{}", __func__, __LINE__);
        auto did_reset = echeck_helper->start_checks();
        SPDLOG_DEBUG("__func__:{},__LINE__:{}", __func__, __LINE__);
        if (did_reset)
        {
          mcu_phase_offset = INT_NONE;
        }
        if (mcu_phase_offset != INT_NONE)
        {
          return json::object();
        }
        auto gcode = any_cast<std::shared_ptr<GCodeDispatch>>(
            printer->lookup_object("gcode"));
        gcode->get_mutex()->lock();

        if (mcu_phase_offset != INT_NONE)
        {
          gcode->get_mutex()->unlock();
          return json::object();
        }
        SPDLOG_DEBUG("Pausing toolhead to calculate {} phase offset",
                     stepper_name);
        any_cast<std::shared_ptr<ToolHead>>(printer->lookup_object("toolhead"))
            ->wait_moves();
        _handle_sync_mcu_pos(stepper);
        gcode->get_mutex()->unlock();
      }
      catch (const elegoo::common::CommandError &e)
      {
        SPDLOG_ERROR("Pausing toolhead to calculate {} phase offset",
                     stepper_name);
        // printer->invoke_shutdown(e.what());
      }
      return json::object();
    }

    json TMCCommandHelper::_do_disable(double print_time)
    {
      try
      {
        if (toff != INT_NONE)
        {
          int val = fields->set_field("toff", 0);
          auto reg_name = fields->lookup_register("toff");
          mcu_tmc->set_register(reg_name, val, print_time);
        }
        echeck_helper->stop_checks();
      }
      catch (const elegoo::common::CommandError &e)
      {
        SPDLOG_ERROR("!!!!!!!!!!!!!!!!!!!!!");
        // printer->invoke_shutdown(e.what());
      }
      return json::object();
    }

    void TMCCommandHelper::_handle_mcu_identify()
    {
      SPDLOG_DEBUG("__func__:{}", __func__);
      auto force_move = any_cast<std::shared_ptr<ForceMove>>(
          printer->lookup_object("force_move"));

      SPDLOG_DEBUG("stepper_name:{}", stepper_name);
      stepper = force_move->lookup_stepper(stepper_name);
      SPDLOG_DEBUG("__func__:{}", __func__);
      stepper->setup_default_pulse_duration(0.000000100, true);
      SPDLOG_DEBUG("{} : {}___ ok", __FUNCTION__, __LINE__);
    }

    void TMCCommandHelper::_handle_stepper_enable(double print_time,
                                                  bool is_enable)
    {
      // std::function<void(double)> cb;
      if (is_enable)
      {
        SPDLOG_DEBUG("__func__:{}", __func__);
        printer->get_reactor()->register_callback([this, print_time](double ev)
                                                  { return _do_enable(print_time); });
        // cb = [this](double print_time) { return _do_enable(print_time); };
      }
      else
      {
        SPDLOG_DEBUG("__func__:{}", __func__);
        printer->get_reactor()->register_callback([this, print_time](double ev)
                                                  { return _do_disable(print_time); });
        // cb = [this](double print_time) { return _do_disable(print_time); };
      }
      // printer->get_reactor()->register_callback(cb);
    }

    void TMCCommandHelper::_handle_connect()
    {
      auto retval = stepper->get_pulse_duration();
      if (retval.second)
      {
        fields->set_field("dedge", 1);
      }
      SPDLOG_DEBUG("retval.first:{},retval.second:{} stepper_name:{}", retval.first, retval.second, stepper_name);
      std::shared_ptr<elegoo::extras::EnableTracking> enable_line =
          stepper_enable->lookup_enable(stepper_name);

      SPDLOG_DEBUG("retval.first:{},retval.second:{}", retval.first, retval.second);
      enable_line->register_state_callback([this](double print_time, bool is_enable)
                                           { _handle_stepper_enable(print_time, is_enable); });

      SPDLOG_DEBUG("retval.first:{},retval.second:{}", retval.first, retval.second);
      if (!enable_line->has_dedicated_enable())
      {
        toff = fields->get_field("toff");
        fields->set_field("toff", 0);
        SPDLOG_DEBUG("Enabling TMC virtual enable for '{}'", stepper_name);
      }
      SPDLOG_DEBUG("retval.first:{},retval.second:{}", retval.first, retval.second);
      try
      {
        _init_registers();
      }
      catch (const elegoo::common::CommandError &e)
      {
        SPDLOG_ERROR("{}", e.what());
      }
      SPDLOG_DEBUG("retval.first:{},retval.second:{}", retval.first, retval.second);
    }

    json TMCCommandHelper::get_status(double eventtime)
    {
      int cpos = 0;
      // std::pair<int, int> current;

      if (stepper != nullptr && mcu_phase_offset != -1)
      {
        cpos = stepper->mcu_to_commanded_position(mcu_phase_offset);
      }

      std::tuple<double, double, double, double> current = current_helper->get_current();
      json res;

      res["mcu_phase_offset"] = mcu_phase_offset;
      res["phase_offset_position"] = cpos;
      res["run_current"] = std::get<0>(current);
      res["hold_current"] = std::get<1>(current);

      std::map<std::string, std::map<std::string, int>> drv_status;
      std::map<std::string, double> temperature;
      if (echeck_helper)
      {
        echeck_helper->get_status(drv_status, temperature);
        for (const auto &f : drv_status)
        {
          for (auto &s : f.second)
          {
            res[s.first] = s.second;
          }
        }
      }

      return res;
    }

    void TMCCommandHelper::setup_register_dump(
        std::vector<std::string> read_registers,
        std::function<std::pair<std::string, int64_t>(std::string, int64_t)>
            read_translate)
    {
      this->read_registers = read_registers;
      this->read_translate = read_translate;

      auto gcode =
          any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));
      gcode->register_mux_command(
          "DUMP_TMC", "STEPPER", name,
          [this](std::shared_ptr<GCodeCommand> gcmd)
          { cmd_DUMP_TMC(gcmd); },
          cmd_DUMP_TMC_help);
    }

    void TMCCommandHelper::cmd_DUMP_TMC(std::shared_ptr<GCodeCommand> gcmd)
    {
      SPDLOG_DEBUG("DUMP_TMC {}", name);
      std::string reg_name = gcmd->get("REGISTER", "");

      if (!reg_name.empty())
      {
        std::string reg_name_upper = reg_name;
        std::transform(reg_name_upper.begin(), reg_name_upper.end(),
                       reg_name_upper.begin(),
                       [](unsigned char c)
                       { return std::toupper(c); });

        auto it = std::find_if(fields->registers.begin(), fields->registers.end(),
                               [&reg_name](const std::pair<std::string, int> &p)
                               {
                                 return p.first == reg_name;
                               });

        if ((it != fields->registers.end()) &&
            (std::find(read_registers.begin(), read_registers.end(), reg_name) ==
             read_registers.end()))
        {
          gcmd->respond_info(fields->pretty_format(reg_name, it->second), true);
        }
        else if (std::find(read_registers.begin(), read_registers.end(),
                           reg_name) != read_registers.end())
        {
          it->second = mcu_tmc->get_register(reg_name);
          if (read_translate != nullptr)
          {
            auto retval = read_translate(reg_name, it->second);
            reg_name = retval.first;
            it->second = retval.second;
          }
          gcmd->respond_info(fields->pretty_format(reg_name, it->second), true);
        }
        else
        {
          throw elegoo::common::CommandError("Unknown register name " + (reg_name));
        }
      }
      else
      {
        gcmd->respond_info("========== Write-only registers ==========", true);
        for (const auto &f : fields->registers)
        {
          if (std::find(read_registers.begin(), read_registers.end(), f.first) ==
              read_registers.end())
          {
            gcmd->respond_info(fields->pretty_format(f.first, f.second), true);
          }
        }
        gcmd->respond_info("========== Queried registers ==========", true);
        for (const auto &reg_name : read_registers)
        {
          auto val = mcu_tmc->get_register(reg_name);
          if (read_translate)
          {
            auto retval = read_translate(reg_name, val);
            val = retval.second;
          }
          gcmd->respond_info(fields->pretty_format(reg_name, val), true);
        }
      }
    }

    // ######################################################################
    // # TMC virtual pins
    // ######################################################################

    TMCVirtualPinHelper::TMCVirtualPinHelper(
        std::shared_ptr<ConfigWrapper> config,
        std::shared_ptr<MCU_TMC_Base> mcu_tmc)
    {
      SPDLOG_DEBUG("TMCVirtualPinHelper init!");
      this->printer = config->get_printer();
      this->config = config;
      this->mcu_tmc = mcu_tmc;
      this->fields = mcu_tmc->get_fields();

      // Initialize diag pin if available
      // init();
      SPDLOG_DEBUG("TMCVirtualPinHelper init success!");
    }

    TMCVirtualPinHelper::~TMCVirtualPinHelper()
    {
      SPDLOG_DEBUG("~TMCVirtualPinHelper()");
    }

    void TMCVirtualPinHelper::init()
    {
      if (this->fields->lookup_register("diag0_stall") != "")
      {
        if (!config->get("diag0_pin", "").empty())
        {
          this->diag_pin = config->get("diag0_pin");
          this->diag_pin_field = "diag0_stall";
        }
        else
        {
          this->diag_pin = config->get("diag1_pin", "");
          this->diag_pin_field = "diag1_stall";
        }
      }
      else
      {
        this->diag_pin = config->get("diag_pin", "");
        this->diag_pin_field.clear();
      }

      mcu_endstop = nullptr;
      en_pwm = false;
      pwmthrs = coolthrs = thigh = 0;

      std::vector<std::string> name_parts;
      std::stringstream ss(config->get_name());
      std::string item;
      while (std::getline(ss, item, ' '))
      {
        name_parts.push_back(item);
      }

      auto ppins = any_cast<std::shared_ptr<PrinterPins>>(printer->lookup_object("pins"));
      std::string param = name_parts[0] + "_" + name_parts.back();
      ppins->register_chip(param, shared_from_this());
    }

    std::shared_ptr<MCU_pins> TMCVirtualPinHelper::setup_pin(const std::string &pin_type, std::shared_ptr<PinParams> pin_params)
    {

      auto ppins = any_cast<std::shared_ptr<PrinterPins>>(printer->lookup_object("pins"));
      if ((pin_type != "endstop") || *pin_params->pin != "virtual_endstop")
      {
        throw elegoo::common::PinsError("tmc virtual endstop only useful as endstop");
      }
      if (pin_params->invert || pin_params->pullup)
      {
        throw elegoo::common::PinsError("Can not pullup/invert tmc virtual pin");
      }
      if (diag_pin.empty())
      {
        throw elegoo::common::PinsError("tmc virtual endstop requires diag pin config");
      }

      elegoo::common::SignalManager::get_instance().register_signal(
          "homing:homing_move_begin",
          std::function<void(std::shared_ptr<HomingMove>)>(
              [this](std::shared_ptr<HomingMove> hmove)
              {
                handle_homing_move_begin(hmove);
              }));

      elegoo::common::SignalManager::get_instance().register_signal(
          "homing:homing_move_end",
          std::function<void(std::shared_ptr<HomingMove>)>(
              [this](std::shared_ptr<HomingMove> hmove)
              {
                handle_homing_move_end(hmove);
              }));

      this->mcu_endstop = std::static_pointer_cast<MCU_endstop>(
          ppins->setup_pin("endstop", diag_pin));
      return this->mcu_endstop;
    }

    void TMCVirtualPinHelper::handle_homing_move_begin(
        std::shared_ptr<HomingMove> hmove)
    {
      auto mcu_endstops = hmove->get_mcu_endstops();
      if ((std::find(mcu_endstops.begin(), mcu_endstops.end(), mcu_endstop) ==
           mcu_endstops.end()))
      {
        return;
      }

      // int64_t sgthrs = fields->get_field("sgthrs");
      // SPDLOG_INFO("TMCVirtualPinHelper::handle_homing_move_begin sgthrs {}", sgthrs);

      // Enable/disable stealthchop
      pwmthrs = fields->get_field("tpwmthrs");
      auto reg = fields->lookup_register("en_pwm_mode");
      int val = 0;
      if (reg.empty())
      {
        // TMC2209 stallguard4
        // On "stallguard4" drivers, "stealthchop" must be enabled
        en_pwm = !(fields->get_field("en_spreadcycle"));
        int tp_val = fields->set_field("tpwmthrs", 0);
        mcu_tmc->set_register("TPWMTHRS", tp_val);
        val = fields->set_field("en_spreadcycle", 0);
      }
      else
      {
        // On earlier drivers, "stealthchop" must be disabled
        en_pwm = fields->get_field("en_pwm_mode");
        fields->set_field("en_pwm_mode", 0);
        val = fields->set_field(diag_pin_field, 1);
      }
      mcu_tmc->set_register("GCONF", val);
      // Enable tcoolthrs (if not already)
      coolthrs = fields->get_field("tcoolthrs");
      if (coolthrs == 0)
      {
        int tc_val = fields->set_field("tcoolthrs", 0xfffff);
        mcu_tmc->set_register("TCOOLTHRS", tc_val);
      }
      // Disable thigh
      reg = fields->lookup_register("thigh");
      if (!reg.empty())
      {
        thigh = fields->get_field("thigh");
        int th_val = fields->set_field("thigh", 0);
        mcu_tmc->set_register(reg, th_val);
      }
    }

    void TMCVirtualPinHelper::handle_homing_move_end(
        std::shared_ptr<HomingMove> hmove)
    {
      auto mcu_endstops = hmove->get_mcu_endstops();
      if ((std::find(mcu_endstops.begin(), mcu_endstops.end(), mcu_endstop) ==
           mcu_endstops.end()))
      {
        return;
      }
      // int64_t sgthrs = fields->get_field("sgthrs");
      // int64_t sg_result = fields->get_field("sg_result");
      // SPDLOG_INFO("TMCVirtualPinHelper::handle_homing_move_end sgthrs {} sg_result {}", sgthrs, sg_result);
      // Restore stealthchop/spreadcycle
      int val = 0;
      auto reg = fields->lookup_register("en_pwm_mode");
      if (reg.empty())
      {
        // TMC2209
        int tp_val = fields->set_field("tpwmthrs", pwmthrs);
        mcu_tmc->set_register("TPWMTHRS", tp_val);
        val = fields->set_field("en_spreadcycle", !en_pwm);
      }
      else
      {
        fields->set_field("en_pwm_mode", en_pwm);
        val = fields->set_field(diag_pin_field, 0);
      }
      mcu_tmc->set_register("GCONF", val);
      // Restore tcoolthrs
      int tc_val = fields->set_field("tcoolthrs", coolthrs);
      mcu_tmc->set_register("TCOOLTHRS", tc_val);
      // Restore thigh
      reg = fields->lookup_register("thigh");
      if (!reg.empty())
      {
        int th_val = fields->set_field("thigh", thigh);
        mcu_tmc->set_register(reg, th_val);
      }
    }

    // ######################################################################
    // # Config reading helpers
    // ######################################################################
    namespace tmc
    {
      void TMCWaveTableHelper(std::shared_ptr<ConfigWrapper> config,
                              std::shared_ptr<MCU_TMC_Base> mcu_tmc)
      {
        std::shared_ptr<FieldHelper> filed = mcu_tmc->get_fields();
        filed->set_config_field(config, "mslut0", 0xAAAAB554);
        filed->set_config_field(config, "mslut1", 0x4A9554AA);
        filed->set_config_field(config, "mslut2", 0x24492929);
        filed->set_config_field(config, "mslut3", 0x10104222);
        filed->set_config_field(config, "mslut4", 0xFBFFFFFF);
        filed->set_config_field(config, "mslut5", 0xB5BB777D);
        filed->set_config_field(config, "mslut6", 0x49295556);
        filed->set_config_field(config, "mslut7", 0x00404222);
        filed->set_config_field(config, "w0", 2);
        filed->set_config_field(config, "w1", 1);
        filed->set_config_field(config, "w2", 1);
        filed->set_config_field(config, "w3", 1);
        filed->set_config_field(config, "x1", 128);
        filed->set_config_field(config, "x2", 255);
        filed->set_config_field(config, "x3", 255);
        filed->set_config_field(config, "start_sin", 0);
        filed->set_config_field(config, "start_sin90", 247);
      }

      void TMCMicrostepHelper(std::shared_ptr<ConfigWrapper> config,
                              std::shared_ptr<MCU_TMC_Base> mcu_tmc)
      {
        auto fields = mcu_tmc->get_fields();

        std::istringstream iss(config->get_name());
        std::vector<std::string> parts;
        std::string word;
        std::string stepper_name;

        while (iss >> word)
        {
          parts.push_back(word);
        }
        if (parts.size() > 1)
        {
          stepper_name = "";
          for (size_t i = 1; i < parts.size(); ++i)
          {
            if (i > 1)
              stepper_name += " ";
            stepper_name += parts[i];
          }
        }

        if (!config->has_section(stepper_name))
        {
          SPDLOG_ERROR("Could not find config section!!!!!!!!!!!!");
          // throw elegoo::common::CommandError("Could not find config section [" +
          //                    stepper_name +
          //                   "] required by tmc driver");
        }

        std::shared_ptr<ConfigWrapper> sconfig = config->getsection(stepper_name);
        std::map<int, int> steps;
        steps[256] = 0;
        steps[128] = 1;
        steps[64] = 2;
        steps[32] = 3;
        steps[16] = 4;
        steps[8] = 5;
        steps[4] = 6;
        steps[2] = 7;
        steps[1] = 8;
        int mres = sconfig->getint("microsteps", 16);
        fields->set_field("mres", steps[mres]);
        fields->set_field("intpol",
                          config->getboolean("interpolate", BoolValue::BOOL_TRUE));
      }

      int TMCtstepHelper(std::shared_ptr<MCU_TMC_Base> mcu_tmc, double velocity,
                         std::shared_ptr<MCU_stepper> pstepper,
                         std::shared_ptr<ConfigWrapper> config)
      {
        if (velocity < 0.000001)
        {
          return 0xfffff;
        }
        double step_dist;
        if (pstepper)
        {
          step_dist = pstepper->get_step_dist();
        }
        else if (config)
        {
          std::istringstream iss(config->get_name());
          std::vector<std::string> parts;
          std::string word;
          std::string stepper_name;

          while (iss >> word)
          {
            parts.push_back(word);
          }
          if (parts.size() > 1)
          {
            stepper_name = "";
            for (size_t i = 1; i < parts.size(); ++i)
            {
              if (i > 1)
                stepper_name += " ";
              stepper_name += parts[i];
            }
          }
          std::shared_ptr<ConfigWrapper> sconfig = config->getsection(stepper_name);
          auto rotation = parse_step_distance(sconfig);
          step_dist = rotation.first / rotation.second;
        }
        auto mres = mcu_tmc->get_fields()->get_field("mres");
        double step_dist_256 = step_dist / (1 << mres);
        auto tmc_freq = mcu_tmc->get_tmc_frequency();
        int threshold = int(tmc_freq * step_dist_256 / velocity + .5);

        return ((threshold < 0xfffff) ? threshold : 0xfffff) > 0
                   ? ((threshold < 0xfffff) ? threshold : 0xfffff)
                   : 0;
      }

      void TMCStealthchopHelper(std::shared_ptr<ConfigWrapper> config,
                                std::shared_ptr<MCU_TMC_Base> mcu_tmc)
      {
        auto fields = mcu_tmc->get_fields();
        bool en_pwm_mode = false;

        float velocity =
            config->getdouble("stealthchop_threshold", DOUBLE_NONE, 0);
        int tpwmthrs = 0xfffff;

        if (!std::isnan(velocity))
        {
          en_pwm_mode = true;
          tpwmthrs = TMCtstepHelper(mcu_tmc, velocity, nullptr, config);
        }
        fields->set_field("tpwmthrs", tpwmthrs);

        auto reg = fields->lookup_register("en_pwm_mode");
        if (!reg.empty())
        {
          fields->set_field("en_pwm_mode", en_pwm_mode);
        }
        else
        {
          fields->set_field("en_spreadcycle", !en_pwm_mode);
        }
      }

      void TMCVcoolthrsHelper(std::shared_ptr<ConfigWrapper> config,
                              std::shared_ptr<MCU_TMC_Base> mcu_tmc)
      {
        auto fields = mcu_tmc->get_fields();
        double velocity = config->getdouble("coolstep_threshold", DOUBLE_NONE, 0);
        int tcoolthrs = 0;

        if (!std::isnan(velocity))
        {
          tcoolthrs = TMCtstepHelper(mcu_tmc, velocity, nullptr, config);
        }
        fields->set_field("tcoolthrs", tcoolthrs);
      }

      void TMCVhighHelper(std::shared_ptr<ConfigWrapper> config,
                          std::shared_ptr<MCU_TMC_Base> mcu_tmc)
      {
        auto fields = mcu_tmc->get_fields();
        double velocity =
            config->getdouble("high_velocity_threshold", DOUBLE_NONE, 0);
        int thigh = 0;

        if (!std::isnan(velocity))
        {
          thigh = TMCtstepHelper(mcu_tmc, velocity, nullptr, config);
        }
        fields->set_field("thigh", thigh);
      }

    }

  }
}
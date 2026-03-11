/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2024-11-20 15:16:18
 * @LastEditors  : Jack
 * @LastEditTime : 2024-11-22 15:15:30
 * @Description  : Support for scaling ADC values based on measured VREF and
 *VSSA
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "adc_scaled.h"
#include "logger.h"
#include "query_adc.h"
namespace elegoo {
namespace extras {

const float SAMPLE_TIME = 0.001;
const int SAMPLE_COUNT = 8;
const float REPORT_TIME = 0.300;

MCU_scaled_adc::MCU_scaled_adc(PrinterADCScaled* main,
                               std::shared_ptr<PinParams> pin_params)
    : _main(main), _last_state(0.0, 0.0), _callback(nullptr) {
SPDLOG_INFO("MCU_scaled_adc init!");
  auto void_ptr = _main->get_mcu()->setup_pin("adc", pin_params);
  _mcu_adc = std::static_pointer_cast<MCU_adc>(void_ptr);

  auto query_adc = any_cast<std::shared_ptr<QueryADC>>(
      _main->get_printer()->lookup_object("query_adc"));

  std::string qname = _main->get_name() + ":" + *pin_params->pin;

  query_adc->register_adc(qname, _mcu_adc);
SPDLOG_INFO("MCU_scaled_adc init success!!");
}

void MCU_scaled_adc::setup_adc_callback(
    float report_time, std::function<void(float, float)> callback) {
  this->_callback = callback;
  if (this->_mcu_adc) {
    _mcu_adc->setup_adc_callback(report_time,
                                 [this](float read_time, float read_value) {
                                   this->handle_callback(read_time, read_value);
                                 });
  }
}

std::pair<float, float> MCU_scaled_adc::get_last_value() const {
  return this->_last_state;
}

void MCU_scaled_adc::handle_callback(float read_time, float read_value) {
  float max_adc = this->_main->get_last_vref().second;
  float min_adc = this->_main->get_last_vssa().second;
  float scaled_val = (read_value - min_adc) / (max_adc - min_adc);
  _last_state = {scaled_val, read_time};
  if (this->_callback) {
    _callback(read_time, scaled_val);
  }
}

PrinterADCScaled::PrinterADCScaled(std::shared_ptr<ConfigWrapper>& config)
    : printer(config->get_printer()),
      name(config->get_name().substr(config->get_name().find(' ') + 1)),
      last_vref(0.0, 0.0),
      last_vssa(0.0, 0.0) {
SPDLOG_INFO("PrinterADCScaled init!");
  double smooth_time =
      config->getdouble("smooth_time", 2, DOUBLE_NONE,
                        DOUBLE_NONE, 0);
  this->inv_smooth_time = 1.0 / smooth_time;

  mcu_vref =
      _config_pin(config, "vref", [this](float read_time, float read_value) {
        this->vref_callback(read_time, read_value);
      });
  mcu_vssa =
      _config_pin(config, "vssa", [this](float read_time, float read_value) {
        this->vssa_callback(read_time, read_value);
      });

  if (mcu_vref->get_mcu() != mcu_vssa->get_mcu()) {
    throw std::runtime_error("vref and vssa must be on same mcu");
  }
  this->mcu = mcu_vref->get_mcu();
  auto ppins =
      any_cast<std::shared_ptr<PrinterPins>>(printer->lookup_object("pins"));
  //ppins->register_chip(name, std::static_pointer_cast<ChipBase>(shared_from_this()));
SPDLOG_INFO("PrinterADCScaled init success!");
}

std::shared_ptr<MCU_pins> PrinterADCScaled::setup_pin(
    const std::string& pin_type, std::shared_ptr<PinParams> pin_params) {
  if (pin_type != "adc") {
    throw std::runtime_error("adc_scaled only supports adc pins");
  }
  return std::make_shared<MCU_scaled_adc>(this, pin_params);
}

std::shared_ptr<MCU_adc> PrinterADCScaled::_config_pin(
    std::shared_ptr<ConfigWrapper>& config, const std::string& name,
    std::function<void(float, float)> callback) {
  std::string param = name + "_pin";
  // std::string pin_name = config->get(param);
  std::string pin_name = "digital_out";

  auto ppins =
      any_cast<std::shared_ptr<PrinterPins>>(printer->lookup_object("pins"));
  auto mcu_adc_ptr = ppins->setup_pin("adc", pin_name);

  std::shared_ptr<MCU_adc> mcu_adc =
      std::static_pointer_cast<MCU_adc>(mcu_adc_ptr);

  mcu_adc->setup_adc_callback(REPORT_TIME, callback);
  mcu_adc->setup_adc_sample(SAMPLE_TIME, SAMPLE_COUNT);

  auto query_adc = any_cast<std::shared_ptr<QueryADC>>(
      config->get_printer()->load_object(config, "query_adc cmd"));

  query_adc->register_adc(name + ":" + pin_name, mcu_adc);
  return mcu_adc;
}

std::pair<float, float> PrinterADCScaled::calc_smooth(
    float read_time, float read_value, const std::pair<float, float>& last) {
  float last_time = last.first;
  float last_value = last.second;
  float time_diff = read_time - last_time;
  float value_diff = read_value - last_value;
  float adj_time = std::min(time_diff * inv_smooth_time, 1.0f);
  float smoothed_value = last_value + value_diff * adj_time;
  return {read_time, smoothed_value};
}

void PrinterADCScaled::vref_callback(float read_time, float read_value) {
  last_vref = calc_smooth(read_time, read_value, last_vref);
}

void PrinterADCScaled::vssa_callback(float read_time, float read_value) {
  last_vssa = calc_smooth(read_time, read_value, last_vssa);
}

const std::string& PrinterADCScaled::get_name() const { return name; }

std::shared_ptr<Printer> PrinterADCScaled::get_printer() const {
  return printer;
}

std::shared_ptr<MCU> PrinterADCScaled::get_mcu() const { return mcu; }

const std::pair<float, float>& PrinterADCScaled::get_last_vref() const {
  return last_vref;
}

const std::pair<float, float>& PrinterADCScaled::get_last_vssa() const {
  return last_vssa;
}


std::shared_ptr<PrinterADCScaled> adc_scaled_load_config_prefix(
    std::shared_ptr<ConfigWrapper> config) {
  return std::make_shared<PrinterADCScaled>(config);

}
}
}
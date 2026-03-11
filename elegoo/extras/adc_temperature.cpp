/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2024-11-14 17:51:15
 * @LastEditors  : loping
 * @LastEditTime : 2025-07-21 21:52:10
 * @Description  : Obtain temperature using linear interpolation of ADC values
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "adc_temperature.h"
#include "heaters.h"
#include "query_adc.h"

namespace elegoo {
namespace extras {

static constexpr double SAMPLE_TIME = 0.001f;
static constexpr int SAMPLE_COUNT = 8;
static constexpr double REPORT_TIME = 0.500f;
static constexpr int RANGE_CHECK_COUNT = 4;

PrinterADCtoTemperature::PrinterADCtoTemperature(
        std::shared_ptr<ConfigWrapper> config,
        std::shared_ptr<LinearBase> adc_convert)
        : adc_convert(adc_convert) ,name(config->get_name())
{
    SPDLOG_INFO("__func__:{} name:{}",__func__,name);
    auto ppins = any_cast<std::shared_ptr<PrinterPins>>(config->get_printer()->lookup_object("pins"));
    auto mcu_adc_ptr = ppins->setup_pin("adc", config->get("sensor_pin"));
    this->mcu_adc = std::static_pointer_cast<MCU_adc>(mcu_adc_ptr);

    is_adc_temp_fault = false;
    this->mcu_adc->setup_adc_callback(
        REPORT_TIME, std::bind(&PrinterADCtoTemperature::adc_callback, this,
                              std::placeholders::_1, std::placeholders::_2));
    this->diag_helper = std::make_shared<HelperTemperatureDiagnostics>(
        config, mcu_adc,
        std::bind(&LinearBase::calc_temp, adc_convert, std::placeholders::_1));
}

void PrinterADCtoTemperature::setup_callback(
    std::function<void(double, double)> callback) {
  temperature_callback = callback;
}

double PrinterADCtoTemperature::get_report_time_delta() const {
  return REPORT_TIME;
}

void PrinterADCtoTemperature::adc_callback(double read_time,
                                           double read_value) {
    if(read_value >= 0.995 || read_value <= 0.0001)
    {
        if(!this->is_adc_temp_fault)
        {
            SPDLOG_INFO("emit adc_temp:fault read_value:{} name:{}",read_value,name);
            this->is_adc_temp_fault = true;
            if("ztemperature_sensor box" != name)
              elegoo::common::SignalManager::get_instance().emit_signal("adc_temp:fault",this->name);
        }
    }
    else
    {
        if(this->is_adc_temp_fault)
        {
            SPDLOG_INFO("emit adc_temp:normal read_value:{} name:{}",read_value,name);
            this->is_adc_temp_fault = false;
            elegoo::common::SignalManager::get_instance().emit_signal("adc_temp:normal",this->name);
        }
    }
    double temp = adc_convert->calc_temp(read_value);
    if (temperature_callback) {
      temperature_callback(read_time + SAMPLE_COUNT * SAMPLE_TIME, temp);
    }
}

void PrinterADCtoTemperature::setup_minmax(double min_temp, double max_temp) {
  // SPDLOG_DEBUG("__func__:{},min_temp:{},max_temp:{}",__func__,min_temp,max_temp);
  std::vector<double> arange = {adc_convert->calc_adc(min_temp),
                                adc_convert->calc_adc(max_temp)};
  // SPDLOG_DEBUG("__func__:{},,arange[0]:{},arange[1]:{}",__func__,arange[0],arange[1]);
  std::sort(arange.begin(), arange.end());
  double min_adc = arange[0];
  double max_adc = arange[1];
  // SPDLOG_DEBUG("__func__:{},min_adc:{},max_adc:{}",__func__,min_adc,max_adc);
  mcu_adc->setup_adc_sample(SAMPLE_TIME, SAMPLE_COUNT, min_adc, max_adc,
                            RANGE_CHECK_COUNT);
  diag_helper->setup_diag_minmax(min_temp, max_temp, min_adc, max_adc);
}

HelperTemperatureDiagnostics::HelperTemperatureDiagnostics(
    std::shared_ptr<ConfigWrapper> config, std::shared_ptr<MCU_adc> mcu_adc,
    std::function<double(double)> calc_temp_cb)
    : printer(config->get_printer()),
      name(config->get_name()),
      mcu_adc(mcu_adc),
      calc_temp_cb(calc_temp_cb),
      min_temp(std::numeric_limits<double>::quiet_NaN()),
      max_temp(std::numeric_limits<double>::quiet_NaN()),
      min_adc(std::numeric_limits<double>::quiet_NaN()),
      max_adc(std::numeric_limits<double>::quiet_NaN()) 
{
  SPDLOG_INFO("name:{}",name);
  auto query_adc = any_cast<std::shared_ptr<QueryADC>>(printer->load_object(config, "query_adc"));
  query_adc->register_adc(name, mcu_adc);

  auto error_mcu = any_cast<std::shared_ptr<PrinterMCUError>>(printer->load_object(config, "error_mcu"));
  error_mcu->add_clarify("ADC out of range",
      std::bind(&HelperTemperatureDiagnostics::clarify_adc_range, this,
      std::placeholders::_1, std::placeholders::_2));
}

void HelperTemperatureDiagnostics::setup_diag_minmax(double min_temp,
                                                     double max_temp,
                                                     double min_adc,
                                                     double max_adc) {
  this->min_temp = min_temp;
  this->max_temp = max_temp;
  this->min_adc = min_adc;
  this->max_adc = max_adc;
}

std::string HelperTemperatureDiagnostics::clarify_adc_range(
    const std::string& msg, const std::map<std::string, std::string>& details) const {
  if (std::isnan(min_temp)) {
    return "";
  }
  std::pair<double, double> last_state = mcu_adc->get_last_value();
  double last_value = last_state.first;
  double last_read_time = last_state.second;

  if (!last_read_time) {
    return "";
  }
  if (last_value >= min_adc && last_value <= max_adc) {
    return "";
  }
  std::string tempstr = "?";
  try {
    double last_temp = calc_temp_cb(last_value);
    tempstr = std::to_string(last_temp).substr(0, 6);
  } catch (...) {
    std::cerr << "Error in calc_temp callback" << std::endl;
  }
  return std::string("Sensor '") + name + "' temperature " + tempstr +
         " not in range " + std::to_string(min_temp) + ":" +
         std::to_string(max_temp);
}

LinearInterpolate::LinearInterpolate(
    const std::vector<std::pair<double, double>>& samples) {
  double last_key = std::numeric_limits<double>::quiet_NaN();
  double last_value = std::numeric_limits<double>::quiet_NaN();
  double key = 0.0, value = 0.0;
  for (const auto& sample : samples) {
    key = sample.first;
    value = sample.second;
    if (!std::isnan(last_key)) {
      if (key <= last_key) {
        throw std::invalid_argument("duplicate value");
      }
      double gain = (value - last_value) / (key - last_key);
      double offset = last_value - last_key * gain;
      if (!keys.empty() && slopes.back() == std::make_pair(gain, offset)) {
        continue;
      }
      last_value = value;
      last_key = key;
      keys.push_back(key);
      slopes.push_back({gain, offset});
    } else {
      last_key = key;
      last_value = value;
    }
  }
  if (keys.empty()) {
    throw std::invalid_argument("need at least two samples");
  }
  keys.push_back(std::numeric_limits<double>::max());
  slopes.push_back(slopes.back());
}

double LinearInterpolate::interpolate(double key) const {
  auto it = std::upper_bound(keys.begin(), keys.end(), key);
  int pos = std::distance(keys.begin(), it) - 1;
  auto slope = slopes[pos];
  return key * slope.first + slope.second;
}

double LinearInterpolate::reverse_interpolate(double value) const {
  std::vector<double> values;
  for (size_t i = 0; i < keys.size(); ++i) {
    values.push_back(keys[i] * slopes[i].first + slopes[i].second);
  }
  if (values[0] < values[values.size() - 2]) {
    auto valid_it = std::find_if(values.begin(), values.end(),
                                 [value](double val) { return val >= value; });
    int pos = std::distance(values.begin(), valid_it);
    if (pos == values.size()) pos = values.size() - 1;
    return (value - slopes[pos].second) / slopes[pos].first;
  } else {
    auto valid_it = std::find_if(values.begin(), values.end(),
                                 [value](double val) { return val <= value; });
    int pos = std::distance(values.begin(), valid_it);
    if (pos == values.size()) pos = values.size() - 1;
    return (value - slopes[pos].second) / slopes[pos].first;
  }
}

LinearVoltage::LinearVoltage(std::shared_ptr<ConfigWrapper> config, const std::vector<std::pair<double, double>>& params) : LinearBase(config, params) {
    double adc_voltage = 0.0, voltage_offset = 0.0;
    adc_voltage = config->getdouble("adc_voltage", 5, DOUBLE_NONE, DOUBLE_NONE, 0);
    voltage_offset = config->getdouble("voltage_offset", 0);

  std::vector<std::pair<double, double>> samples;
  for (const auto& param : params) {
    double temp = param.first;
    double volt = param.second;
    double adc = (volt - voltage_offset) / adc_voltage;
    if (adc < 0.0 || adc > 1.0) {
      std::cerr << "Warning: Ignoring adc sample " << temp << "/" << volt
                << " in heater " << config->get_name() << std::endl;
      continue;
    }
    samples.push_back({adc, temp});
  }

  try {
    li = std::make_shared<LinearInterpolate>(samples);
  } catch (const std::invalid_argument& e) {
    throw std::invalid_argument("adc_temperature " + std::string(e.what()) +
                                " in heater " + config->get_name());
  }
}

double LinearVoltage::calc_temp(double adc) const {
  return li->interpolate(adc);
}

double LinearVoltage::calc_adc(double temp) const {
  return li->reverse_interpolate(temp);
}

CustomLinearVoltage::CustomLinearVoltage(std::shared_ptr<ConfigWrapper> config) : CustomLinearBase(config){
    name = config->get_name() + " " + std::string(config->get_name()).substr(strlen("heater"));
    for (int i = 1; i < 1000; ++i) {
        double t  = 0.0;
        t = config->getdouble(("temperature" + std::to_string(i)).c_str(), DOUBLE_NONE);
        if (std::isnan(t)) {
            break;
        }
        double v = config->getdouble(("voltage" + std::to_string(i)).c_str());
        params.emplace_back(t, v);
    }
}

std::shared_ptr<PrinterADCtoTemperature> CustomLinearVoltage::create(
    std::shared_ptr<ConfigWrapper> config) {
  std::shared_ptr<LinearVoltage> lv =
      std::make_shared<LinearVoltage>(config, params);
  return std::make_shared<PrinterADCtoTemperature>(config, lv);
}

std::shared_ptr<LinearBase> CustomLinearVoltage::get_liner(std::shared_ptr<ConfigWrapper> config) {
    return std::make_shared<LinearVoltage>(config, params);
}

LinearResistance::LinearResistance(std::shared_ptr<ConfigWrapper> config, const std::vector<std::pair<double, double>>& samples)  : LinearBase(config, samples){
    pullup = config->getdouble("pullup_resistor", 4700, DOUBLE_NONE, DOUBLE_NONE, 0);
    std::vector<std::pair<double, double>> reversed_samples;
    for (auto sample : samples) {
        std::swap(sample.first, sample.second);
        reversed_samples.push_back(sample);
    }

  try {
    li = std::make_shared<LinearInterpolate>(reversed_samples);
  } catch (const std::invalid_argument& e) {
    throw std::invalid_argument("adc_temperature " + std::string(e.what()) +
                                " in heater " + config->get_name());
  }
}
double LinearResistance::calc_temp(double adc) const {
  adc = std::max(0.00001, std::min(0.99999, adc));
  double r = pullup * adc / (1.0 - adc);
  return li->interpolate(r);
}

double LinearResistance::calc_adc(double temp) const {
  double r = li->reverse_interpolate(temp);
  return r / (pullup + r);
}

CustomLinearResistance::CustomLinearResistance(std::shared_ptr<ConfigWrapper> config) : CustomLinearBase(config) {
    name = config->get_name() + " " + std::string(config->get_name()).substr(strlen("heater"));
    for (int i = 1; i < 1000; ++i) {
        double t  = 0.0;
        t = config->getdouble(("temperature" + std::to_string(i)).c_str(), DOUBLE_NONE);
        if (std::isnan(t)) {
            break;
        }
        double r = config->getdouble(("resistance" + std::to_string(i)).c_str());
        params.emplace_back(t, r);
    }
}

std::shared_ptr<PrinterADCtoTemperature> CustomLinearResistance::create(
    std::shared_ptr<ConfigWrapper> config) {
  std::shared_ptr<LinearResistance> lr =
      std::make_shared<LinearResistance>(config, params);
  return std::make_shared<PrinterADCtoTemperature>(config, lr);
}

std::shared_ptr<LinearBase> CustomLinearResistance::get_liner(std::shared_ptr<ConfigWrapper> config) {
    return std::make_shared<LinearResistance>(config, params);
}

const std::vector<std::pair<double, double>> AD595 = {
    {0., .0027},   {10., .101},   {20., .200},   {25., .250},   {30., .300},
    {40., .401},   {50., .503},   {60., .605},   {80., .810},   {100., 1.015},
    {120., 1.219}, {140., 1.420}, {160., 1.620}, {180., 1.817}, {200., 2.015},
    {220., 2.213}, {240., 2.413}, {260., 2.614}, {280., 2.817}, {300., 3.022},
    {320., 3.227}, {340., 3.434}, {360., 3.641}, {380., 3.849}, {400., 4.057},
    {420., 4.266}, {440., 4.476}, {460., 4.686}, {480., 4.896}};

const std::vector<std::pair<double, double>> AD597 = {
    {0., 0.},      {10., .097},   {20., .196},   {25., .245},   {30., .295},
    {40., 0.395},  {50., 0.496},  {60., 0.598},  {80., 0.802},  {100., 1.005},
    {120., 1.207}, {140., 1.407}, {160., 1.605}, {180., 1.801}, {200., 1.997},
    {220., 2.194}, {240., 2.392}, {260., 2.592}, {280., 2.794}, {300., 2.996},
    {320., 3.201}, {340., 3.406}, {360., 3.611}, {380., 3.817}, {400., 4.024},
    {420., 4.232}, {440., 4.440}, {460., 4.649}, {480., 4.857}, {500., 5.066}};

const std::vector<std::pair<double, double>> AD8494 = {
    {-180, -0.714}, {-160, -0.658}, {-140, -0.594}, {-120, -0.523},
    {-100, -0.446}, {-80, -0.365},  {-60, -0.278},  {-40, -0.188},
    {-20, -0.095},  {0, 0.002},     {20, 0.1},      {25, 0.125},
    {40, 0.201},    {60, 0.303},    {80, 0.406},    {100, 0.511},
    {120, 0.617},   {140, 0.723},   {160, 0.829},   {180, 0.937},
    {200, 1.044},   {220, 1.151},   {240, 1.259},   {260, 1.366},
    {280, 1.473},   {300, 1.58},    {320, 1.687},   {340, 1.794},
    {360, 1.901},   {380, 2.008},   {400, 2.114},   {420, 2.221},
    {440, 2.328},   {460, 2.435},   {480, 2.542},   {500, 2.65},
    {520, 2.759},   {540, 2.868},   {560, 2.979},   {580, 3.09},
    {600, 3.203},   {620, 3.316},   {640, 3.431},   {660, 3.548},
    {680, 3.666},   {700, 3.786},   {720, 3.906},   {740, 4.029},
    {760, 4.152},   {780, 4.276},   {800, 4.401},   {820, 4.526},
    {840, 4.65},    {860, 4.774},   {880, 4.897},   {900, 5.018},
    {920, 5.138},   {940, 5.257},   {960, 5.374},   {980, 5.49},
    {1000, 5.606},  {1020, 5.72},   {1040, 5.833},  {1060, 5.946},
    {1080, 6.058},  {1100, 6.17},   {1120, 6.282},  {1140, 6.394},
    {1160, 6.505},  {1180, 6.616},  {1200, 6.727}};

const std::vector<std::pair<double, double>> AD8495 = {
    {-260, -0.786}, {-240, -0.774}, {-220, -0.751}, {-200, -0.719},
    {-180, -0.677}, {-160, -0.627}, {-140, -0.569}, {-120, -0.504},
    {-100, -0.432}, {-80, -0.355},  {-60, -0.272},  {-40, -0.184},
    {-20, -0.093},  {0, 0.003},     {20, 0.1},      {25, 0.125},
    {40, 0.2},      {60, 0.301},    {80, 0.402},    {100, 0.504},
    {120, 0.605},   {140, 0.705},   {160, 0.803},   {180, 0.901},
    {200, 0.999},   {220, 1.097},   {240, 1.196},   {260, 1.295},
    {280, 1.396},   {300, 1.497},   {320, 1.599},   {340, 1.701},
    {360, 1.803},   {380, 1.906},   {400, 2.01},    {420, 2.113},
    {440, 2.217},   {460, 2.321},   {480, 2.425},   {500, 2.529},
    {520, 2.634},   {540, 2.738},   {560, 2.843},   {580, 2.947},
    {600, 3.051},   {620, 3.155},   {640, 3.259},   {660, 3.362},
    {680, 3.465},   {700, 3.568},   {720, 3.67},    {740, 3.772},
    {760, 3.874},   {780, 3.975},   {800, 4.076},   {820, 4.176},
    {840, 4.275},   {860, 4.374},   {880, 4.473},   {900, 4.571},
    {920, 4.669},   {940, 4.766},   {960, 4.863},   {980, 4.959},
    {1000, 5.055},  {1020, 5.15},   {1040, 5.245},  {1060, 5.339},
    {1080, 5.432},  {1100, 5.525},  {1120, 5.617},  {1140, 5.709},
    {1160, 5.8},    {1180, 5.891},  {1200, 5.98},   {1220, 6.069},
    {1240, 6.158},  {1260, 6.245},  {1280, 6.332},  {1300, 6.418},
    {1320, 6.503},  {1340, 6.587},  {1360, 6.671},  {1380, 6.754}};

const std::vector<std::pair<double, double>> AD8496 = {
    {-180, -0.642}, {-160, -0.59}, {-140, -0.53}, {-120, -0.464},
    {-100, -0.392}, {-80, -0.315}, {-60, -0.235}, {-40, -0.15},
    {-20, -0.063},  {0, 0.027},    {20, 0.119},   {25, 0.142},
    {40, 0.213},    {60, 0.308},   {80, 0.405},   {100, 0.503},
    {120, 0.601},   {140, 0.701},  {160, 0.8},    {180, 0.9},
    {200, 1.001},   {220, 1.101},  {240, 1.201},  {260, 1.302},
    {280, 1.402},   {300, 1.502},  {320, 1.602},  {340, 1.702},
    {360, 1.801},   {380, 1.901},  {400, 2.001},  {420, 2.1},
    {440, 2.2},     {460, 2.3},    {480, 2.401},  {500, 2.502},
    {520, 2.603},   {540, 2.705},  {560, 2.808},  {580, 2.912},
    {600, 3.017},   {620, 3.124},  {640, 3.231},  {660, 3.34},
    {680, 3.451},   {700, 3.562},  {720, 3.675},  {740, 3.789},
    {760, 3.904},   {780, 4.02},   {800, 4.137},  {820, 4.254},
    {840, 4.37},    {860, 4.486},  {880, 4.6},    {900, 4.714},
    {920, 4.826},   {940, 4.937},  {960, 5.047},  {980, 5.155},
    {1000, 5.263},  {1020, 5.369}, {1040, 5.475}, {1060, 5.581},
    {1080, 5.686},  {1100, 5.79},  {1120, 5.895}, {1140, 5.999},
    {1160, 6.103},  {1180, 6.207}, {1200, 6.311}};

const std::vector<std::pair<double, double>> AD8497 = {
    {-260, -0.785}, {-240, -0.773}, {-220, -0.751}, {-200, -0.718},
    {-180, -0.676}, {-160, -0.626}, {-140, -0.568}, {-120, -0.503},
    {-100, -0.432}, {-80, -0.354},  {-60, -0.271},  {-40, -0.184},
    {-20, -0.092},  {0, 0.003},     {20, 0.101},    {25, 0.126},
    {40, 0.2},      {60, 0.301},    {80, 0.403},    {100, 0.505},
    {120, 0.605},   {140, 0.705},   {160, 0.804},   {180, 0.902},
    {200, 0.999},   {220, 1.097},   {240, 1.196},   {260, 1.296},
    {280, 1.396},   {300, 1.498},   {320, 1.599},   {340, 1.701},
    {360, 1.804},   {380, 1.907},   {400, 2.01},    {420, 2.114},
    {440, 2.218},   {460, 2.322},   {480, 2.426},   {500, 2.53},
    {520, 2.634},   {540, 2.739},   {560, 2.843},   {580, 2.948},
    {600, 3.052},   {620, 3.156},   {640, 3.259},   {660, 3.363},
    {680, 3.466},   {700, 3.569},   {720, 3.671},   {740, 3.773},
    {760, 3.874},   {780, 3.976},   {800, 4.076},   {820, 4.176},
    {840, 4.276},   {860, 4.375},   {880, 4.474},   {900, 4.572},
    {920, 4.67},    {940, 4.767},   {960, 4.863},   {980, 4.96},
    {1000, 5.055},  {1020, 5.151},  {1040, 5.245},  {1060, 5.339},
    {1080, 5.433},  {1100, 5.526},  {1120, 5.618},  {1140, 5.71},
    {1160, 5.801},  {1180, 5.891},  {1200, 5.981},  {1220, 6.07},
    {1240, 6.158},  {1260, 6.246},  {1280, 6.332},  {1300, 6.418},
    {1320, 6.503},  {1340, 6.588},  {1360, 6.671},  {1380, 6.754}};

std::vector<std::pair<double, double>> calc_pt100(double base = 100.0) {
  const double A = 3.9083e-3;
  const double B = -5.775e-7;
  std::vector<std::pair<double, double>> result;
  for (int t = 0; t <= 500; t += 10) {
    double R = base * (1.0 + A * t + B * t * t);
    result.emplace_back(t, R);
  }
  return result;
}

std::vector<std::pair<double, double>> calc_ina826_pt100() {
  const double pullup = 4400.0;
  const double gain = 10.0;
  const double Vcc = 5.0;
  std::vector<std::pair<double, double>> pt100 = calc_pt100();
  std::vector<std::pair<double, double>> result;
  for (const auto& pair : pt100) {
    double t = pair.first;
    double R = pair.second;
    double V = gain * Vcc * R / (pullup + R);
    result.emplace_back(t, V);
  }
  return result;
}

const std::vector<
    std::pair<std::string, std::vector<std::pair<double, double>>>>
    DefaultVoltageSensors = {{"AD595", AD595},
                             {"AD597", AD597},
                             {"AD8494", AD8494},
                             {"AD8495", AD8495},
                             {"AD8496", AD8496},
                             {"AD8497", AD8497},
                             {"PT100 INA826", calc_ina826_pt100()}};

const std::vector<
    std::pair<std::string, std::vector<std::pair<double, double>>>>
    DefaultResistanceSensors = {{"PT1000", calc_pt100(1000.0)}};

std::shared_ptr<void> adc_temperature_load_config(std::shared_ptr<ConfigWrapper> config) {
  std::shared_ptr<Printer> printer = config->get_printer();
  std::shared_ptr<PrinterHeaters> pheaters =
      any_cast<std::shared_ptr<PrinterHeaters>>(printer->load_object(config, "heaters"));

  for (const auto& sensor : DefaultVoltageSensors) {
    auto lv = std::make_shared<LinearVoltage>(config, sensor.second);
    std::shared_ptr<SensorFactory> factory =
        std::make_shared<SensorFactory>("PrinterADCtoTemperature", lv);
    pheaters->add_sensor_factory(sensor.first, factory);
  }

  for (const auto& sensor : DefaultResistanceSensors) {
    auto lr = std::make_shared<LinearResistance>(config, sensor.second);
    std::shared_ptr<SensorFactory> factory =
        std::make_shared<SensorFactory>("PrinterADCtoTemperature", lr);
    pheaters->add_sensor_factory(sensor.first, factory);
  }
  SPDLOG_INFO("-----------------adc_temperature_load_config success!----------------");
  return nullptr;  // Returning nullptr is expected for success in C++.
}

std::shared_ptr<void> adc_temperature_load_config_prefix(std::shared_ptr<ConfigWrapper> config) {
  std::shared_ptr<PrinterHeaters> pheaters =
      any_cast<std::shared_ptr<PrinterHeaters>>(config->get_printer()->load_object(config, "heaters"));

  std::shared_ptr<CustomLinearBase> custom_sensor;
  if (config->get("resistance1", "").empty()) {
    custom_sensor = std::make_shared<CustomLinearVoltage>(config);
  } else {
    custom_sensor = std::make_shared<CustomLinearResistance>(config);
  }

  auto liner = custom_sensor->get_liner(config);
  std::shared_ptr<SensorFactory> factory = 
      std::make_shared<SensorFactory>("PrinterADCtoTemperature", liner);
  pheaters->add_sensor_factory(custom_sensor->name, factory);

  SPDLOG_INFO("-----------------adc_temperature_load_config_prefix success!----------------");
  return nullptr;  // Returning nullptr is expected for success in C++.
}

}  
}
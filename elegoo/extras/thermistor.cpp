/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2025-02-18 16:42:39
 * @LastEditors  : Ben
 * @LastEditTime : 2025-07-12 12:32:55
 * @Description  : 
 * 
 * Copyright (c) 2025 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2024-11-23 15:46:56
 * @LastEditors  : Jack
 * @LastEditTime : 2024-11-26 15:45:06
 * @Description  :
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "thermistor.h"

namespace elegoo {
namespace extras {

const double KELVIN_TO_CELSIUS = -273.15;

Thermistor::Thermistor(const float pullup, const float inline_resistor)
    : LinearBase(pullup, inline_resistor) {
    this->pullup = pullup;
    this->inline_resistor = inline_resistor;
    this->c1 = this->c2 = this->c3 = 0.0;
}

Thermistor::~Thermistor() {}

void Thermistor::setup_coefficients(double t1, double r1, double t2, double r2,
                                    double t3, double r3,
                                    const std::string& name) {
    double inv_t1 = 1.0 / (t1 - KELVIN_TO_CELSIUS);
    double inv_t2 = 1.0 / (t2 - KELVIN_TO_CELSIUS);
    double inv_t3 = 1.0 / (t3 - KELVIN_TO_CELSIUS);

    double ln_r1 = std::log(r1);
    double ln_r2 = std::log(r2);
    double ln_r3 = std::log(r3);

    double ln3_r1 = std::pow(ln_r1, 3);
    double ln3_r2 = std::pow(ln_r2, 3);
    double ln3_r3 = std::pow(ln_r3, 3);

    double inv_t12 = inv_t1 - inv_t2;
    double inv_t13 = inv_t1 - inv_t3;

    double ln_r12 = ln_r1 - ln_r2;
    double ln_r13 = ln_r1 - ln_r3;

    double ln3_r12 = ln3_r1 - ln3_r2;
    double ln3_r13 = ln3_r1 - ln3_r3;

    c3 = (inv_t12 - inv_t13 * ln_r12 / ln_r13) /
        (ln3_r12 - ln3_r13 * ln_r12 / ln_r13);

    if (c3 <= 0.0) {
        double beta = ln_r13 / inv_t13;
        std::cerr << "Using thermistor beta " << beta << " in heater " << name
                << std::endl;
        setup_coefficients_beta(t1, r1, beta);
        return;
    }

    c2 = (inv_t12 - c3 * ln3_r12) / ln_r12;
    c1 = inv_t1 - c2 * ln_r1 - c3 * ln3_r1;
}

void Thermistor::setup_coefficients_beta(double t1, double r1, double beta) {
    double inv_t1 = 1.0 / (t1 - KELVIN_TO_CELSIUS);
    double ln_r1 = std::log(r1);

    c3 = 0.0;
    c2 = 1.0 / beta;
    c1 = inv_t1 - c2 * ln_r1;
    // SPDLOG_DEBUG("c3:{},c2:{},c1:{},beta:{},inv_t1:{},ln_r1:{}",c3,c2,c1,beta,inv_t1,ln_r1);
}

double Thermistor::calc_temp(double adc) const {
    // SPDLOG_DEBUG("adc:{}",adc);
    adc = std::max(0.00001, std::min(0.99999, adc));
    // SPDLOG_DEBUG("adc:{},pullup:{}",adc,pullup);
    double r = pullup * adc / (1.0 - adc);
    // SPDLOG_DEBUG("adc:{},pullup:{},r:{}",adc,pullup,r);
    double ln_r = std::log(r - inline_resistor);
    // SPDLOG_DEBUG("adc:{},pullup:{},r:{},inline_resistor:{},ln_r:{}",adc,pullup,r,inline_resistor,ln_r);
    double inv_t = c1 + c2 * ln_r + c3 * std::pow(ln_r, 3);
    // SPDLOG_DEBUG("c1:{},c2:{},c3:{},std::pow(ln_r, 3):{},inv_t:{},1.0 / inv_t:{},KELVIN_TO_CELSIUS:{}",c1,c2,c3,std::pow(ln_r, 3),inv_t,1.0 / inv_t,KELVIN_TO_CELSIUS);
    return 1.0 / inv_t + KELVIN_TO_CELSIUS;
}

double Thermistor::calc_adc(double temp) const {
    if (temp <= KELVIN_TO_CELSIUS) {
        return 1.0;
    }

    double inv_t = 1.0 / (temp - KELVIN_TO_CELSIUS);

    if (c3 != 0.0) {
        double y = (c1 - inv_t) / (2.0 * c3);
        double x = std::sqrt(std::pow(c2 / (3.0 * c3), 3) + std::pow(y, 2));
        double ln_r = std::pow(x - y, 1.0 / 3.0) - std::pow(x + y, 1.0 / 3.0);
        double r = std::exp(ln_r) + inline_resistor;
        return r / (pullup + r);
    } else {
        double ln_r = (inv_t - c1) / c2;
        double r = std::exp(ln_r) + inline_resistor;
        return r / (pullup + r);
    }
}

std::shared_ptr<Thermistor> PrinterThermistor(
    std::shared_ptr<ConfigWrapper> config,
    std::map<std::string, double> params) {
    double pullup = config->getdouble("pullup_resistor", 4700,
        DOUBLE_NONE, DOUBLE_NONE, 0);
    double inline_resistor = config->getdouble(
        "inline_resistor", 0, 0);

    auto thermistor = std::make_shared<Thermistor>(pullup, inline_resistor);

    if (params.find("beta") == params.end()) {
        auto name = config->get_name();
        thermistor->setup_coefficients(params["t1"], params["r1"], params["t2"],
                                    params["r2"], params["t3"], params["r3"],
                                    name);
    } else {
        thermistor->setup_coefficients_beta(params["t1"], params["r1"],
                                            params["beta"]);
    }
    // return std::make_shared<PrinterADCtoTemperature>(config, thermistor);
    return thermistor;
}

CustomThermistor::CustomThermistor(std::shared_ptr<ConfigWrapper> config) {
  name = extract_name(config->get_name());
    t1 = config->getdouble("temperature1", DOUBLE_INVALID, KELVIN_TO_CELSIUS);
    r1 = config->getdouble("resistance1", DOUBLE_INVALID, 0);
    beta = config->getdouble("beta", DOUBLE_NONE, DOUBLE_NONE, DOUBLE_NONE, 0);

    if (beta > 0.0) {
        SPDLOG_DEBUG("t1:{},r1:{},beta:{}",t1,r1,beta);
        params = {{"t1", t1}, {"r1", r1}, {"beta", beta}};
        return;
    }

    t2 = config->getdouble("temperature2", DOUBLE_INVALID, KELVIN_TO_CELSIUS);
    r2 = config->getdouble("resistance2", DOUBLE_INVALID, 0);
    t3 = config->getdouble("temperature3", DOUBLE_INVALID, KELVIN_TO_CELSIUS);
    r3 = config->getdouble("resistance3", DOUBLE_INVALID, 0);

    std::vector<std::pair<double, double>> samples = {
        {t1, r1}, {t2, r2}, {t3, r3}};
    std::sort(samples.begin(), samples.end());

    params = {{"t1", samples[0].first}, {"r1", samples[0].second},
                {"t2", samples[1].first}, {"r2", samples[1].second},
                {"t3", samples[2].first}, {"r3", samples[2].second}};
}

std::shared_ptr<PrinterADCtoTemperature> CustomThermistor::create(
    std::shared_ptr<ConfigWrapper> config) {
    // return PrinterThermistor(config, params);
    return {};
}

const std::string & CustomThermistor::get_name() {
    return name;
}

std::shared_ptr<void> thermistor_load_config_prefix(std::shared_ptr<ConfigWrapper> config) 
{
    auto custom_thermistor = std::make_shared<CustomThermistor>(config);
    std::shared_ptr<PrinterHeaters> pheaters = any_cast<std::shared_ptr<PrinterHeaters>>(config->get_printer()->load_object(config, "heaters"));
    auto lr = PrinterThermistor(config,custom_thermistor->params);
    std::shared_ptr<SensorFactory> factory = std::make_shared<SensorFactory>("PrinterADCtoTemperature", lr);
    SPDLOG_DEBUG("__func__:{},custom_thermistor->get_name:{}",__func__,custom_thermistor->get_name());
    pheaters->add_sensor_factory(custom_thermistor->get_name(), factory);
    return nullptr;
}

}  
}
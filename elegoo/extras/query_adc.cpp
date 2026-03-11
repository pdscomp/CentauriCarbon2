/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2024-11-04 21:56:50
 * @LastEditors  : Ben
 * @LastEditTime : 2025-02-18 14:57:45
 * @Description  : Utility for querying the current state of adc pins
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "query_adc.h"

namespace elegoo {
namespace extras {
const std::string cmd_QUERY_ADC_help = "Report the last value of an analog pin";

static std::string join(const std::vector<std::string>& vec, const std::string& delimiter) {
    std::ostringstream oss;
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i != 0) {
            oss << delimiter;
        }
        oss << vec[i];
    }
    return oss.str();
}

QueryADC::QueryADC(std::shared_ptr<ConfigWrapper> config) {
    printer = config->get_printer();
    adc.clear();

    auto gcode = any_cast<std::shared_ptr<GCodeDispatch>>(config->get_printer()->lookup_object("gcode"));
    gcode->register_command("QUERY_ADC", std::bind(&QueryADC::cmd_QUERY_ADC,
                                    this, std::placeholders::_1), true,
                                    cmd_QUERY_ADC_help);    
SPDLOG_INFO("QueryADC init success!");
}

QueryADC::~QueryADC() {

}

void QueryADC::register_adc(const std::string &name, std::shared_ptr<MCU_adc> mcu_adc) {
    adc[name] = mcu_adc;
}

void QueryADC::cmd_QUERY_ADC(std::shared_ptr<GCodeCommand> gcmd) {
SPDLOG_INFO("{} : {}___", __FUNCTION__, __LINE__);
    auto name = gcmd->get("NAME");
    if (adc.find(name) == adc.end()) {
        std::vector<std::string> objs;
        for (const auto& pair : adc) {
            objs.push_back("\"" + pair.first + "\"");
        }
        std::string msg = "Available ADC objects: " + join(objs, ", ");
        gcmd->respond_info(msg, true);
        return;        
    }

    auto retval = adc[name]->get_last_value();
    std::stringstream msg;
    msg << "ADC object \"" << name << "\" has value " << retval.first << " (timestamp " << retval.second << ")";

    float pullup = gcmd->get_double("PULLUP", DOUBLE_NONE, DOUBLE_NONE, DOUBLE_NONE, 0);
    if (pullup > 0.0) {
        float v = std::max(0.00001, std::min(0.99999, retval.first));
        float r = pullup * v / (1.0 - v);
        msg << "\n resistance " << r << " (with " << pullup << " pullup)";
    }

    gcmd->respond_info(msg.str(), true);

}


std::shared_ptr<QueryADC> query_adc_load_config(std::shared_ptr<ConfigWrapper> config) {
    return std::make_shared<QueryADC>(config);
}


}
}
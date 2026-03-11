/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2024-11-02 10:55:27
 * @LastEditors  : Jack
 * @LastEditTime : 2024-12-17 15:35:19
 * @Description  : Support for tracking canbus node ids
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "canbus_ids.h"
#include "configfile.h"

PrinterCANBus::PrinterCANBus(std::shared_ptr<ConfigWrapper> config) 
    : printer(config->get_printer()) 
{
SPDLOG_INFO("PrinterCANBus init success!");

}

int PrinterCANBus::add_uuid(std::shared_ptr<ConfigWrapper> config, 
    const std::string& canbus_uuid, const std::string& canbus_iface) 
{
    if (ids.find(canbus_uuid) != ids.end()) 
    {
        throw std::runtime_error("Duplicate canbus_uuid");
    }
    int new_id = ids.size() + NODEID_FIRST;
    ids[canbus_uuid] = new_id;
    return new_id;
}

int PrinterCANBus::get_nodeid(const std::string& canbus_uuid) 
{
    auto it = ids.find(canbus_uuid);
    if (it == ids.end()) 
    {
        throw std::runtime_error("Unknown canbus_uuid " + canbus_uuid);
    }
    return it->second;
}

std::shared_ptr<PrinterCANBus> canbus_ids_load_config(std::shared_ptr<ConfigWrapper> config) {
    return std::make_shared<PrinterCANBus>(config);
}
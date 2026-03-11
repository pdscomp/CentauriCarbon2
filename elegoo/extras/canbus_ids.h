/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2024-11-02 10:55:27
 * @LastEditors  : Jack
 * @LastEditTime : 2024-12-17 15:35:31
 * @Description  : Support for tracking canbus node ids
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#pragma once

#include <string>
#include <unordered_map>
#include <stdexcept>
#include <memory>
#include "printer.h"
#include "configfile.h"


const int NODEID_FIRST = 4;


class ConfigWrapper;

class PrinterCANBus {
public:
    PrinterCANBus(std::shared_ptr<ConfigWrapper> config);

    int add_uuid(std::shared_ptr<ConfigWrapper> config, const std::string& canbus_uuid, const std::string& canbus_iface);

    int get_nodeid(const std::string& canbus_uuid);

private:
    std::shared_ptr<Printer> printer;
    std::unordered_map<std::string, int> ids;
};


std::shared_ptr<PrinterCANBus> canbus_ids_load_config(std::shared_ptr<ConfigWrapper> config);


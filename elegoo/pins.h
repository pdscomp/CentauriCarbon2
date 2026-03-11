/***************************************************************************** 
 * @Author       : Louisa
 * @Date         : 2024-11-25 09:53:58
 * @LastEditors  : coconut
 * @LastEditTime : 2025-03-24 17:25:13
 * @Description  : Pin name handling
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#pragma once

#include <map>
#include <regex>
#include <string>
#include <stdexcept>
#include <memory>
//#include <optional>

#include "chipbase.h"

// class ChipBase
// {
// public:
//     int chip_type = 0;
// };



class PinResolver
{
public:
    explicit PinResolver(bool validate_aliases = true);

    void reserve_pin(const std::string& pin, const std::string& reserve_name);
    void alias_pin(const std::string &alias, std::string &pin);
    std::string update_command(const std::string &cmd);

// private:
    bool validate_aliases;
    std::map<std::string, std::string> reserved;
    std::map<std::string, std::string> aliases;
    std::map<std::string, std::string> active_pins;

    static const std::regex re_pin;
};

class PrinterPins
{
public:
    explicit PrinterPins();

    std::shared_ptr<PinParams> parse_pin(const std::string &pin_desc, bool can_invert = false, bool can_pullup = false);
    std::shared_ptr<PinParams> lookup_pin(const std::string &pin_desc, bool can_invert = false, bool can_pullup = false, std::string share_type = "");
    std::shared_ptr<MCU_pins> setup_pin(const std::string &pin_type, const std::string &pin_desc);
    void reset_pin_sharing(const std::shared_ptr<PinParams> &pin_params);
    std::shared_ptr<PinResolver> get_pin_resolver(const std::string &chip_name);
    // template <typename Func, typename... Args>
    // void register_chip(const std::string &chip_name,
    //                    Func &&factory, Args &&...args);
    void register_chip(const std::string &chip_name, const std::shared_ptr<ChipBase> &chip);
    template <typename T>
    std::shared_ptr<T> lookup_chip(const std::string &name, std::shared_ptr<T> default_object);
    // void register_chip(const std::string& chip_name, const std::shared_ptr<void>& chip);
    void allow_multi_use_pin(const std::string &pin_desc);

private:
    std::map<std::string, std::shared_ptr<ChipBase>> chips;
    std::map<std::string, std::shared_ptr<PinParams>> active_pins;
    std::map<std::string, std::shared_ptr<PinResolver>> pin_resolvers;
    std::map<std::string, bool> allow_multi_use_pins;
};

// template <typename Func, typename... Args>
// void PrinterPins::register_chip(const std::string &chip_name,
//                                 Func &&factory, Args &&...args)
// {
//     if (chips.find(chip_name) != chips.end())
//     {
//         // throw std::runtime_error("Factory with name '" + name + "' already exists.");
//     }

//     chips[chip_name] = factory(std::forward<decltype(args)>(args)...);
// }

template <typename T>
std::shared_ptr<T> PrinterPins::lookup_chip(const std::string &name, std::shared_ptr<T> default_object)
{
    auto it = chips.find(name);
    if (it != chips.end())
    {
        return std::static_pointer_cast<T>(it->second);
    }

    if (!default_object)
    {
        throw std::runtime_error("Object with name '" + name + "' not found");
    }

    return default_object;
}



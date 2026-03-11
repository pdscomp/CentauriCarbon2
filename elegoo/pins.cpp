/***************************************************************************** 
 * @Author       : Louisa
 * @Date         : 2024-11-25 09:53:56
 * @LastEditors  : coconut
 * @LastEditTime : 2025-03-24 17:25:10
 * @Description  : Pin name handling
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "pins.h"
#include <algorithm>
#include "configfile.h"
#include "printer.h"

// class PrinterPWMLED : public std::enable_shared_from_this<PrinterPWMLED>, public ChipBase
// {
// public:
//     std::shared_ptr<PrinterPWMLED> setup_pin(const std::string &, std::map<std::string, std::shared_ptr<void>>)
//     {
//         return shared_from_this();
//     };
// };

//运行时会触发异常，暂时屏蔽
// const std::regex PinResolver::re_pin(R"((?P<prefix>[ _]pin=)(?P<name>[^ ]*))");
const std::regex PinResolver::re_pin(R"(([ _]pin=)([^ ]*))");

PinResolver::PinResolver(bool validate_aliases)
    : validate_aliases(validate_aliases) 
{
    reserved = {};
    aliases = {};
    active_pins = {};
}

void PinResolver::reserve_pin(const std::string& pin, const std::string& reserve_name)
{
    auto it = reserved.find(pin);
    if (it != reserved.end() && it->second != reserve_name)
    {
        throw elegoo::common::PinsError("Pin " + pin + " reserved for " + it->second + " - can't reserve for " + reserve_name);
    }
    reserved[pin] = reserve_name;
}

void PinResolver::alias_pin(const std::string &alias, std::string &pin)
{
    auto it = aliases.find(alias);
    if (it != aliases.end() && it->second != pin)
    {
        throw elegoo::common::PinsError("Alias " + alias + " mapped to " + it->second + " - can't alias to " + pin);
    }
    if (pin.find_first_of("^~!:") != std::string::npos || std::remove_if(pin.begin(), pin.end(), ::isspace) != pin.end())
    {
        throw elegoo::common::PinsError("Invalid pin alias '" + pin + "'");
    }
    if (aliases.count(pin))
    {
        pin = aliases[pin];
    }
    aliases[alias] = pin;

    for (const auto &ex_alias : aliases) //[existing_alias, existing_pin]
    {
        if (ex_alias.second == alias)
        {
            aliases[ex_alias.first] = pin;
        }
    }
}

std::string PinResolver::update_command(const std::string &cmd)
{
    std::string result = cmd;
    std::smatch match;
    // std::regex re_pin(R"((?P<prefix>[ _]pin=)(?P<name>[^ ]*))"); // 定义正则表达式

    if (std::regex_search(result, match, re_pin))
    {
        std::string prefix = match.str(1); // 获取 prefix
        std::string name = match.str(2);   // 获取 name
        std::string pin_id = aliases.count(name) ? aliases[name] : name;

        // SPDLOG_INFO("__func__:{},prefix:{},name:{},pin_id:{},aliases.count(name):{}",__func__,prefix,name,pin_id,aliases.count(name));
       std::string existing_name;
       auto it = active_pins.find(pin_id);
        if (it != active_pins.end()) {
            existing_name = it->second;
        } else {
            active_pins[pin_id] = name; 
            existing_name = name;
        }

        if (name != existing_name && validate_aliases)
        {
            SPDLOG_ERROR("pin {} is an alias for {}",name,active_pins[pin_id]);
            throw elegoo::common::PinsError("pin " + name + " is an alias for " + active_pins[pin_id]);
        }
        if (reserved.count(pin_id))
        {
            SPDLOG_ERROR("pin {} is an reserved for {}",name,active_pins[pin_id]);
            throw elegoo::common::PinsError("pin " + name + " is reserved for " + reserved[pin_id]);
        }

        result.replace(match.position(0), match.length(0), prefix + pin_id);
    }

    return result;
}
PrinterPins::PrinterPins() {
    SPDLOG_INFO("create PrinterPins success!");
}

std::shared_ptr<PinParams> PrinterPins::parse_pin(const std::string &pin_desc, bool can_invert, bool can_pullup)
{
    std::string desc = pin_desc;
    std::remove_if(desc.begin(), desc.end(), ::isspace);
    int pullup = 0, invert = 0;
    if (can_pullup && ((desc[0] == '^' || desc[0] == '~') && !desc.empty()))
    {
        pullup = 1;
        if (desc[0] == '~') {
            pullup = -1;
        }
        desc = desc.substr(1);
    }
    if (can_invert && desc[0] == '!')
    {
        invert = 1;
        desc = desc.substr(1);
    }
    // SPDLOG_INFO("desc {}", desc);
    std::string chip_name, pin;
    if (desc.find(':') == std::string::npos) {
        chip_name = "mcu";
        pin = desc;
    } else {
        auto pos = desc.find(':');
        chip_name = desc.substr(0, pos);
        pin = desc.substr(pos + 1);
    }

    if (chips.find(chip_name) == chips.end()) {
        throw elegoo::common::PinsError("Unknown pin chip name '" + chip_name + "'");
    }

    std::vector<char> result;
    for (char c : "^~!:" ) {
        if (pin.find(c) != std::string::npos) {
            result.push_back(c);
        }
    }
    std::string filter_pin = elegoo::common::join(elegoo::common::split(pin),"");
    if(!result.empty() || filter_pin != pin){
        std::string format;
        if (can_pullup)
            format += "[^~] ";
        if (can_invert)
            format += "[!] ";
        throw elegoo::common::PinsError("Invalid pin description '" + pin_desc + "'\nFormat is: " + format + "[chip_name:] pin_name");
    }

    std::shared_ptr<PinParams> pin_params = std::make_shared<PinParams>();
    pin_params->chip = chips[chip_name];
    pin_params->chip_name = std::make_shared<std::string>(chip_name);
    pin_params->pin = std::make_shared<std::string>(pin);
    pin_params->invert = invert;
    pin_params->pullup = pullup;
    pin_params->share_type = std::make_shared<std::string>("");
    // pin_params->pin_class = std::make_shared<std::string>("");

    // SPDLOG_WARN("chips.size:{},chip_name:{},pin:{},invert:{},pullup:{}",chips.size(),*pin_params->chip_name,*pin_params->pin,pin_params->invert,pin_params->pullup);
    return pin_params;
}

std::shared_ptr<PinParams> PrinterPins::lookup_pin(const std::string &pin_desc, bool can_invert, bool can_pullup, std::string share_type)
{
    auto pin_params = parse_pin(pin_desc, can_invert, can_pullup);

    std::shared_ptr<std::string> pin = pin_params->pin;
    std::shared_ptr<std::string> chip_name = pin_params->chip_name;
    
    SPDLOG_DEBUG("pin:{},chip_name:{}",*pin,*chip_name);
    std::string share_name = *chip_name + ":" + *pin;

    // SPDLOG_DEBUG("active_pins.count(share_name:{}):{}",share_name,active_pins.count(share_name));
    if (active_pins.find(share_name) != active_pins.end())
    {
        auto share_params = active_pins[share_name];
        if (*(share_params->share_type) != share_type && *(share_params->share_type) != "ignore" && share_type != "ignore")
        {
            SPDLOG_ERROR("pin " + *pin + " used multiple times in config");
            SPDLOG_ERROR("share_type {} {}", *(share_params->share_type), share_type);

            throw  elegoo::common::PinsError("pin " + *pin + " used multiple times in config");
        }
        if (pin_params->invert != share_params->invert || pin_params->pullup != share_params->pullup)
        {
            SPDLOG_ERROR("Shared pin " + *pin + " must have same polarity");
            SPDLOG_ERROR("invert {} {} pullup {} {}", pin_params->invert, share_params->invert, pin_params->pullup, share_params->pullup);
            throw elegoo::common::PinsError("Shared pin " + *pin + " must have same polarity");
        }
        return share_params;
    }

    // SPDLOG_DEBUG("share_type:{},*pin_params->share_type:{}",share_type,*pin_params->share_type);
    *(pin_params->share_type) = share_type;
    active_pins[share_name] = pin_params;
    // SPDLOG_DEBUG("share_type:{},*pin_params->share_type:{}",share_type,*pin_params->share_type);
    return pin_params;
}

std::shared_ptr<MCU_pins> PrinterPins::setup_pin(const std::string &pin_type, const std::string &pin_desc)
{
    SPDLOG_DEBUG("setup_pin pin_type:{}, pin_desc:{}", pin_type, pin_desc);
    bool can_invert = (pin_type == "endstop" || pin_type == "digital_out" || pin_type == "pwm");
    bool can_pullup = (pin_type == "endstop");
    std::shared_ptr<PinParams> pin_params = lookup_pin(pin_desc, can_invert, can_pullup);
    return pin_params->chip->setup_pin(pin_type, pin_params);
}

void PrinterPins::reset_pin_sharing(const std::shared_ptr<PinParams> &pin_params)
{
    std::string share_name = *pin_params->chip_name + ":" + *pin_params->pin;
    active_pins.erase(share_name);
}

std::shared_ptr<PinResolver> PrinterPins::get_pin_resolver(const std::string &chip_name)
{
    if (pin_resolvers.find(chip_name) == pin_resolvers.end())
    {
        throw elegoo::common::PinsError("Unknown chip name '" + chip_name + "'");
    }
    return pin_resolvers[chip_name];
}

void PrinterPins::register_chip(const std::string &chip_name, const std::shared_ptr<ChipBase> &chip)
{
    SPDLOG_DEBUG("__func__:{},chip_name:{}",__func__,chip_name);
    if (chips.find(chip_name) != chips.end())
    {
        throw elegoo::common::PinsError("Duplicate chip name '" + chip_name + "'");
    }
    chips[chip_name] = chip;
    pin_resolvers[chip_name] = std::make_shared<PinResolver>();
}

void PrinterPins::allow_multi_use_pin(const std::string &pin_desc)
{
    auto pin_params = parse_pin(pin_desc);
    std::string share_name = *(std::static_pointer_cast<std::string>(pin_params->chip_name)) + ":" + *(std::static_pointer_cast<std::string>(pin_params->pin));
    allow_multi_use_pins[share_name] = true;
}

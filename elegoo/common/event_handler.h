/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2024-11-27 12:20:47
 * @LastEditors  : coconut
 * @LastEditTime : 2025-04-07 18:18:52
 * @Description  : event management module.
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include <iostream>
#include <functional>
#include <map>
#include <vector>
#include <memory>
#include <type_traits>
#include "common/logger.h"

namespace elegoo
{
namespace common
{
    
template <typename... Args>
class EventHandler {
public:
    void register_event(std::function<void(Args...)> slot) {
        slots.push_back(slot);
    }

    void event_handler(Args... args) {
        for (auto& slot : slots) {
            slot(args...);
        }
    }

private:
    std::vector<std::function<void(Args...)>> slots;
};


class SignalManager {
public:
    static SignalManager& get_instance();

    SignalManager(const SignalManager&) = delete;
    SignalManager& operator=(const SignalManager&) = delete;

    template <typename... Args>
    void register_signal(const std::string& signalName, std::function<void(Args...)> slot) {
        auto it = signals.find(signalName);
        if (it != signals.end()) {
            auto existingSignal = std::static_pointer_cast<EventHandler<Args...>>(it->second);
            existingSignal->register_event(slot);
        } else {
            auto newSignal = std::make_shared<EventHandler<Args...>>();
            newSignal->register_event(slot);
            signals[signalName] = newSignal;
        }
    }

    template <typename... Args>
    void emit_signal(const std::string& signalName, Args... args) {
        auto it = signals.find(signalName);
        if (it != signals.end()) {
            auto signal = std::static_pointer_cast<EventHandler<Args...>>(it->second);
            signal->event_handler(args...);
        } else {
            // SPDLOG_WARN("Signal not found: {}", signalName);
            // throw std::runtime_error("Signal not found: " + signalName);
        }
    }
    
    template <typename... Args>
    void try_emit_signal(const std::string& signalName, Args... args) {
        auto it = signals.find(signalName);
        if (it != signals.end()) {
            auto signal = std::static_pointer_cast<EventHandler<Args...>>(it->second);
            try
            {
                signal->event_handler(args...);
            }
            catch(const std::exception &e)
            {
                SPDLOG_ERROR("Exception during shutdown handler: " + std::string(e.what()));
            }
        } else {
            // SPDLOG_WARN("Signal not found: {}", signalName);
            // throw std::runtime_error("Signal not found: " + signalName);
        }
    }

    void reset() {
        signals.clear();
    }

private:
    SignalManager() = default; 
    std::map<std::string, std::shared_ptr<void>> signals;
};

} // namespace common
} // namespace elegoo
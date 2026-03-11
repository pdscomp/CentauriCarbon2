/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2024-11-27 12:20:58
 * @LastEditors  : Ben
 * @LastEditTime : 2024-11-30 12:18:02
 * @Description  : event management module.
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "event_handler.h"

namespace elegoo
{
namespace common
{
    
SignalManager& SignalManager::get_instance() {
    static SignalManager signal_manager;
    return signal_manager;
}

} // namespace common
} // namespace elegoo
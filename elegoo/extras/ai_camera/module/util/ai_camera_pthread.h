/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2024-12-04 10:28:07
 * @LastEditors  : Jack
 * @LastEditTime : 2024-12-10 15:09:41
 * @Description  : 
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/
#pragma once

#include <iostream>
#include <memory>
#include <functional>
#include <pthread.h>

namespace znp
{
    pthread_t CreateNewThread(const std::function<void()>& task, size_t stack_size = 1*1024*1024, std::string thread_name = "newthread");
} // namespace znp


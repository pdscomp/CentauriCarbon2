/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2024-12-04 10:35:26
 * @LastEditors  : Jack
 * @LastEditTime : 2024-12-10 15:04:29
 * @Description  : 
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "ai_camera_pthread.h"

namespace znp
{
extern "C" void* thread_adapter(void* arg) {
    auto callable = static_cast<std::function<void()>*>(arg);
    (*callable)();
    delete callable;
    return nullptr;
}

pthread_t CreateNewThread(const std::function<void()>& task, size_t stack_size, std::string thread_name) {
    pthread_t thread;
    pthread_attr_t attr;

    if (pthread_attr_init(&attr) != 0) {
        throw std::runtime_error("Failed to initialize thread attributes");
    }

    if (stack_size > 0) {
        if (pthread_attr_setstacksize(&attr, stack_size) != 0) {
            pthread_attr_destroy(&attr);
            throw std::runtime_error("Failed to set thread stack size");
        }
    }

    auto* task_ptr = new std::function<void()>(task);

    if (pthread_create(&thread, &attr, thread_adapter, task_ptr) != 0) {
        delete task_ptr;
        pthread_attr_destroy(&attr);
        throw std::runtime_error("Failed to create thread");
    }

    if (pthread_detach(thread) != 0) {
        perror("Failed to detach thread");
        return 1;
    }
    std::cout << "[NEW THREAD: " << thread_name << " ] Thread has finished and stack_size : " << stack_size << std::endl;
    return thread;
}

    
} // namespace znp
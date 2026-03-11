/***************************************************************************** 
 * @Author       : Gary
 * @Date         : 2024-10-25 22:26:39
 * @LastEditors  : Ben
 * @LastEditTime : 2025-04-17 21:24:27
 * @Description  : File descriptor and timer event helper
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#pragma once


#include <iostream>
#include <vector>
#include <functional>
#include <cmath>
#include <thread>
#include <mutex>
#include <queue>
#include <map>
#include <condition_variable>
#include <chrono>
#include <unistd.h>
#include "c_helper.h"
#include "json.h"
#include "co_routine.h"

const double _NEVER = 9999999999999999.0;
const double _NOW = 0.0;

class SelectReactor;
class ReactorGreenlet;

class ReactorTimer 
{
public:
    ReactorTimer(std::function<double(double)> callback, double waketime, const std::string& func);
    std::function<double(double)> callback;
    double waketime;
    std::string func;
};

class ReactorCompletion
{
public:
    ReactorCompletion(std::shared_ptr<SelectReactor> reactor);
    ~ReactorCompletion();

    bool test();
    void complete(json result);
    json wait(double waketime=_NEVER, const json& waketime_result=json::object());

private: 
    std::shared_ptr<SelectReactor> reactor;
    json result;
    std::vector<std::shared_ptr<ReactorGreenlet>> waiting;
};

class ReactorCallback
{
public:
    ReactorCallback(std::shared_ptr<SelectReactor> reactor, 
        std::function<json(double)> callback, double waketime);
    ~ReactorCallback();

    double invoke(double eventtime);
    std::shared_ptr<SelectReactor> reactor;
    std::shared_ptr<ReactorTimer> timer;
    std::function<json(double)> callback;
    std::shared_ptr<ReactorCompletion> completion;
};

class ReactorFileHandler
{
public:
    ReactorFileHandler(int fd, std::function<void(double)> read_callback,
        std::function<void(double)> write_callback);
    ~ReactorFileHandler();

    int fileno();
    std::function<void(double)> read_callback;
    std::function<void(double)> write_callback;
    int fd;
private:

};

class ReactorGreenlet
{
public:
    ReactorGreenlet(std::function<void()> run);
    ~ReactorGreenlet();

    void resume();
    stCoRoutine_t* get_co();
    std::shared_ptr<ReactorTimer> timer;
     
private:
    static void* coroutine(void* arg);
private:
    std::function<void()> run;
    stCoRoutine_t *co = nullptr;
};



class ReactorMutex
{
public:
    ReactorMutex(std::shared_ptr<SelectReactor> reactor,
        bool is_locked);
    ~ReactorMutex();

    bool test();
    void lock();
    void unlock();
private:
    std::shared_ptr<SelectReactor> reactor;
    bool is_locked;
    bool next_pending;
    std::queue<std::shared_ptr<ReactorGreenlet>> queue;
    std::mutex mtx;
};


class SelectReactor : public std::enable_shared_from_this<SelectReactor>
{
public:
    SelectReactor();
    virtual ~SelectReactor();
    int get_gc_stats();
    void update_timer(std::shared_ptr<ReactorTimer> timer_handler, 
        double waketime);



    std::shared_ptr<ReactorTimer> register_timer(
        std::function<double(double)> callback, double waketime = _NEVER, std::string func = "");
    void unregister_timer(std::shared_ptr<ReactorTimer> timer_handler);
    std::shared_ptr<ReactorCompletion> completion();
    std::shared_ptr<ReactorCompletion> register_callback(
        std::function<json(double)> callback, double waketime = 0);
    void register_async_callback(
        std::function<json(double)> callback, double waketime = 0);
    void async_complete(
        std::shared_ptr<ReactorCompletion> completion, const json& result
    );
    double pause(double waketime);
    std::shared_ptr<ReactorMutex> mutex(bool is_locked = false);
    virtual std::shared_ptr<ReactorFileHandler> register_fd(int fd, 
        std::function<void(double)> read_callback,
        std::function<void(double)> write_callback = {});
    virtual void unregister_fd(std::shared_ptr<ReactorFileHandler> file_handler);
    virtual void set_fd_wake(
        std::shared_ptr<ReactorFileHandler> file_handler,
        bool is_readable = true,
        bool is_writeable = false
    );
    void run();
    void end();
    void finalize();
    std::shared_ptr<ReactorGreenlet> getcurrent();

protected:
    double check_timers(double eventtime, bool busy);
    void got_pipe_signal(double eventtime);
    void setup_async_callbacks();
    double sys_pause(double waketime);
    void end_greenlet(std::shared_ptr<ReactorGreenlet> g_old);
    virtual void dispatch_loop();
    

protected:
    bool process;
    std::vector<std::shared_ptr<ReactorTimer>> timers;
    double next_timer;
    double buffer_time;
    int pipe_fds[2];
    std::queue<std::function<void()>> async_queue;
    std::vector<std::shared_ptr<ReactorCallback>> reactor_callbacks;
    std::vector<std::shared_ptr<ReactorCallback>> reactor_async_callbacks;
    std::vector<std::shared_ptr<ReactorFileHandler>> read_fds;
    std::vector<std::shared_ptr<ReactorFileHandler>> write_fds;
    std::shared_ptr<ReactorGreenlet> g_dispatch;
    std::vector<std::shared_ptr<ReactorGreenlet>> greenlets;
    std::vector<std::shared_ptr<ReactorGreenlet>> all_greenlets;
    std::mutex mtx;
public:
    const double NEVER = _NEVER;
    const double NOW = _NOW;
    std::map<std::string, double> interval;
};

class PollReactor : public SelectReactor
{
public:
    PollReactor();
    ~PollReactor();
    std::shared_ptr<ReactorFileHandler> register_fd(int fd, 
        std::function<void(double)> read_callback,
        std::function<void(double)> write_callback = {});
    void unregister_fd(std::shared_ptr<ReactorFileHandler> file_handler);
    void set_fd_wake(
        std::shared_ptr<ReactorFileHandler> file_handler,
        bool is_readable = true,
        bool is_writeable = false
    );

private:
    void dispatch_loop();

private:
    std::map<int, std::shared_ptr<ReactorFileHandler>> fds;
    std::vector<struct pollfd> poll_fds;
};


class EPollReactor : public SelectReactor
{
public:
    EPollReactor();
    ~EPollReactor();
    void register_fd();
    void unregister_fd();
    void set_fd_wake();

private:
    void dispatch_loop();
};


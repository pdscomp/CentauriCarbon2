/*****************************************************************************
 * @Author       : Ben
 * @Date         : 2024-11-28 20:15:20
 * @LastEditors  : coconut
 * @LastEditTime : 2025-04-07 12:11:13
 * @Description  : File descriptor and timer event helper
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "reactor.h"
#include <sys/select.h>
#include <poll.h>
#include <algorithm>
#include "utilities.h"
#include "logger.h"

ReactorTimer::ReactorTimer(std::function<double(double)> callback, double waketime, const std::string &func)
    : callback(callback), waketime(waketime), func(func)
{
}

ReactorCompletion::ReactorCompletion(
    std::shared_ptr<SelectReactor> reactor)
    : reactor(reactor), result(json::value_t::null)
{
}

ReactorCompletion::~ReactorCompletion()
{
    // SPDLOG_DEBUG("~ReactorCompletion()");
}

bool ReactorCompletion::test()
{
    return !result.is_null();
}

void ReactorCompletion::complete(json result)
{
    // SPDLOG_DEBUG("__func__:{},__LINE__:{}",__func__,__LINE__);
    this->result = result;
    for (std::shared_ptr<ReactorGreenlet> wait : waiting)
    {
        reactor->update_timer(wait->timer, _NOW);
    }
}

json ReactorCompletion::wait(double waketime, const json &waketime_result)
{

    if (result.is_null())
    {
        std::shared_ptr<ReactorGreenlet> wait = reactor->getcurrent();
        if (wait == nullptr)
        {
            SPDLOG_ERROR("!!!!!!!!!!!!!!!!!!!!!");
        }
        waiting.push_back(wait);
        reactor->pause(waketime);
        for (auto it = waiting.begin(); it != waiting.end();)
        {
            if (*it == wait)
            {
                it = waiting.erase(it);
                break;
            }
            else
            {
                ++it;
            }
        }
        if (result.is_null())
        {
            return waketime_result;
        }
    }

    return result;
}

ReactorCallback::ReactorCallback(std::shared_ptr<SelectReactor> reactor,
                                 std::function<json(double)> callback, double waketime)
    : reactor(reactor), callback(callback)
{
    this->timer = reactor->register_timer(
        [this](double eventtime)
        { return this->invoke(eventtime); }, waketime, "reactor_callback");
    this->completion = std::make_shared<ReactorCompletion>(reactor);
}

ReactorCallback::~ReactorCallback()
{
    // reactor->unregister_timer(timer);
    SPDLOG_DEBUG("~ReactorCallback()");
}

double ReactorCallback::invoke(double eventtime)
{
    reactor->unregister_timer(timer);
    json res = callback(eventtime);
    if (completion)
        completion->complete(res);
    return _NEVER;
}

ReactorFileHandler::ReactorFileHandler(int fd,
                                       std::function<void(double)> read_callback,
                                       std::function<void(double)> write_callback)
    : fd(fd), read_callback(read_callback),
      write_callback(write_callback)
{
}

ReactorFileHandler::~ReactorFileHandler()
{
}

int ReactorFileHandler::fileno()
{
    return fd;
}

ReactorGreenlet::ReactorGreenlet(std::function<void()> run)
{
    this->run = run;
    co_create(&co, NULL, coroutine, this);
}

ReactorGreenlet::~ReactorGreenlet()
{
    // 外部释放
    //  if (co)
    //  {
    //      // printf("co_delete %p\n", co);
    //      DeleteCo(co); // 在析构时释放协程资源
    //  }
}

void ReactorGreenlet::resume()
{
    if (co)
    {
        co_resume(co);
    }
}

stCoRoutine_t *ReactorGreenlet::get_co()
{
    return co;
}

void *ReactorGreenlet::coroutine(void *arg)
{
    // co_enable_hook_sys();
    // SPDLOG_DEBUG("__func__:{},__LINE__:{}",__func__,__LINE__);
    ReactorGreenlet *instance = static_cast<ReactorGreenlet *>(arg);
    instance->run();

    return nullptr;
}

ReactorMutex::ReactorMutex(std::shared_ptr<SelectReactor> reactor,
                           bool is_locked) : reactor(reactor), is_locked(is_locked)
{
    this->next_pending = false;
}

ReactorMutex::~ReactorMutex()
{
}

bool ReactorMutex::test()
{
    return is_locked;
}

void ReactorMutex::lock()
{
    if (!is_locked)
    {
        is_locked = true;
        return;
    }
    std::shared_ptr<ReactorGreenlet> g =
        reactor->getcurrent();
    if (g == nullptr)
    {
        SPDLOG_ERROR("!!!!!!!!!!!!!!!!!!!!");
    }

    mtx.lock();
    queue.push(g);
    mtx.unlock();
    while (1)
    {
        reactor->pause(_NEVER);
        mtx.lock();
        if (next_pending && queue.front() == g)
        {
            next_pending = false;
            queue.pop();
            mtx.unlock();
            return;
        }
        mtx.unlock();
    }
}

void ReactorMutex::unlock()
{
    mtx.lock();
    if (queue.empty())
    {
        is_locked = false;
        mtx.unlock();
        return;
    }

    next_pending = true;
    reactor->update_timer(queue.front()->timer, _NOW);
    mtx.unlock();
}

SelectReactor::SelectReactor()
{
    timers.reserve(64); // 避免扩容发生竞争，程序会在timers的回调中操作timers，可能存在风险
    process = false;
    next_timer = NEVER;
    buffer_time = NEVER;
    pipe_fds[0] = -1;
    pipe_fds[1] = -1;
    SPDLOG_DEBUG("create SelectReactor success!");
}

SelectReactor::~SelectReactor()
{
    SPDLOG_DEBUG("~SelectReactor()!");
}

int SelectReactor::get_gc_stats()
{
    // 暂时保留改接口，后期删除，C++不需要配置垃圾回收等级
    return 0;
}

void SelectReactor::update_timer(
    std::shared_ptr<ReactorTimer> timer_handler, double waketime)
{
    if (timer_handler)
    {
        timer_handler->waketime = waketime;
    }
    next_timer = std::min(next_timer, waketime);
}

std::shared_ptr<ReactorTimer> SelectReactor::register_timer(
    std::function<double(double)> callback, double waketime, std::string func)
{
    auto timer_handler = std::make_shared<ReactorTimer>(callback, waketime, func);
    timers.push_back(timer_handler);
    next_timer = std::min(next_timer, waketime);
#if TIMER_DEBUG
    interval[func] = 0.;
#endif
    return timer_handler;
}

void SelectReactor::unregister_timer(std::shared_ptr<ReactorTimer> timer_handler)
{
    timer_handler->waketime = NEVER;

    timers.erase(std::remove_if(timers.begin(), timers.end(),
                                [&timer_handler](const std::shared_ptr<ReactorTimer> &timer)
                                {
                                    return timer == timer_handler;
                                }),
                 timers.end());
}

std::shared_ptr<ReactorCompletion> SelectReactor::completion()
{
    return std::make_shared<ReactorCompletion>(shared_from_this());
}

std::shared_ptr<ReactorCompletion> SelectReactor::register_callback(
    std::function<json(double)> callback, double waketime)
{
    auto rcb = std::make_shared<ReactorCallback>(
        shared_from_this(), callback, waketime);
    reactor_callbacks.push_back(rcb);
    return rcb->completion;
}

void SelectReactor::register_async_callback(
    std::function<json(double)> callback, double waketime)
{
    std::lock_guard<std::mutex> lock(mtx);
    std::shared_ptr<SelectReactor> self = shared_from_this();
    async_queue.push([self, callback, waketime]()
                     {
        auto rcb = std::make_shared<ReactorCallback>(
            self, callback, waketime);
        self->reactor_async_callbacks.push_back(rcb); });

    const char signal = '.';
    if (write(pipe_fds[1], &signal, 1) == -1)
    {
        std::cerr << "Error writing to pipe: " << std::endl;
    }
}

void SelectReactor::async_complete(
    std::shared_ptr<ReactorCompletion> completion, const json &result)
{
    std::lock_guard<std::mutex> lock(mtx);
    async_queue.push([completion, result]()
                     {
                         completion->complete(result); // 任务执行时调用函数
                     });

    const char signal = '.';
    if (write(pipe_fds[1], &signal, 1) == -1)
    {
        std::cerr << "Error writing to pipe: " << std::endl;
    }
}

double SelectReactor::pause(double waketime)
{
    // SPDLOG_DEBUG("__func__:{},waketime:{}",__func__,waketime);
    std::shared_ptr<ReactorGreenlet> g = getcurrent();

    // 在非协程环境内调用getcurrent会返回nullptr
    if (g != g_dispatch || g == nullptr)
    {
        if (g == nullptr)
            SPDLOG_WARN("g == nullptr");
        if (g_dispatch == nullptr)
        {
            SPDLOG_WARN("sys_pause");
            return sys_pause(waketime);
        }
        // if(get_monotonic() > waketime)
        {
            buffer_time = waketime;
            // SPDLOG_DEBUG("__func__:{},waketime:{}",__func__,waketime);
            g_dispatch->resume();
            return buffer_time;
        }
    }

    // SPDLOG_DEBUG("__func__:{},all_greenlets.size:{},greenlets.size:{}",__func__,all_greenlets.size(),greenlets.size());
    std::shared_ptr<ReactorGreenlet> g_next;
    if (!greenlets.empty())
    {
        // SPDLOG_DEBUG("__func__:{},waketime:{}",__func__,waketime);
        g_next = greenlets.back();
        greenlets.pop_back();
    }
    else
    {
        SPDLOG_DEBUG("__func__:{},waketime:{}", __func__, waketime);
        g_next = std::make_shared<ReactorGreenlet>(
            [this]()
            { this->dispatch_loop(); });
        all_greenlets.push_back(g_next);
    }

    // g_next的父类设置为g的父类
    co_set_parent(g_next->get_co(), g->get_co());
    // SPDLOG_DEBUG("__func__:{},all_greenlets.size:{},greenlets.size:{}",__func__,all_greenlets.size(),greenlets.size());
    g->timer = register_timer(
        [g, this](double eventtime)
        {
            buffer_time = eventtime;
            g->resume(); 
            return buffer_time; }, waketime, "pause");

    next_timer = NOW;
    g_next->resume();

    return buffer_time;
}

std::shared_ptr<ReactorMutex> SelectReactor::mutex(bool is_locked)
{
    return std::make_shared<ReactorMutex>(shared_from_this(), is_locked);
}

std::shared_ptr<ReactorFileHandler> SelectReactor::register_fd(
    int fd,
    std::function<void(double)> read_callback,
    std::function<void(double)> write_callback)
{
    auto file_handler = std::make_shared<ReactorFileHandler>(
        fd, read_callback, write_callback);
    set_fd_wake(file_handler, true, false);
    return file_handler;
}

void SelectReactor::unregister_fd(
    std::shared_ptr<ReactorFileHandler> file_handler)
{
    auto read_it = std::find(read_fds.begin(), read_fds.end(), file_handler);
    if (read_it != read_fds.end())
    {
        read_fds.erase(read_it);
    }

    auto write_it = std::find(write_fds.begin(), write_fds.end(), file_handler);
    if (write_it != write_fds.end())
    {
        write_fds.erase(write_it);
    }
}

void SelectReactor::set_fd_wake(
    std::shared_ptr<ReactorFileHandler> file_handler,
    bool is_readable,
    bool is_writeable)
{
    auto read_it = std::find(read_fds.begin(), read_fds.end(), file_handler);
    if (read_it != read_fds.end())
    {
        if (!is_readable)
        {
            read_fds.erase(read_it);
        }
    }
    else if (is_readable)
    {
        read_fds.push_back(file_handler);
    }

    auto write_it = std::find(write_fds.begin(), write_fds.end(), file_handler);
    if (write_it != write_fds.end())
    {
        if (!is_writeable)
        {
            write_fds.erase(write_it);
        }
    }
    else if (is_writeable)
    {
        write_fds.push_back(file_handler);
    }
}

void SelectReactor::run()
{
    if (-1 == pipe_fds[0] && -1 == pipe_fds[1])
    {
        setup_async_callbacks();
    }
    process = true;
    // 创建协程并启动
    auto g_next = std::make_shared<ReactorGreenlet>(
        [this]()
        { this->dispatch_loop(); });
    all_greenlets.push_back(g_next);
    g_next->resume();
}

void SelectReactor::end()
{
    process = false;
}

void SelectReactor::finalize()
{
    g_dispatch.reset();
    greenlets.clear();

    for (size_t i = 0; i < all_greenlets.size(); ++i)
    {
        co_release(all_greenlets.at(i)->get_co());
    }
    all_greenlets.clear();

    if (pipe_fds[0] != -1 && pipe_fds[1] != -1)
    {
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        std::cout << "Pipe closed successfully" << std::endl;
        pipe_fds[0] = -1;
        pipe_fds[1] = -1;
    }
    else
    {
        std::cout << "Pipe is not open or already closed" << std::endl;
    }
}

double SelectReactor::check_timers(double eventtime, bool busy)
{
    if (eventtime < next_timer)
    {
        if (busy)
        {
            return 0;
        }
        return std::min(1.0, std::max(0.001, next_timer - eventtime));
    }

    next_timer = NEVER;
    std::shared_ptr<ReactorGreenlet> dispatch = g_dispatch;
    for (std::shared_ptr<ReactorTimer> t : timers)
    {
        if (!t)
        {
            continue;
        }

        double waketime = t->waketime;
        if (eventtime >= waketime)
        {
            t->waketime = NEVER;
#if TIMER_DEBUG
            double start_time = get_monotonic();
#endif
            t->waketime = waketime = t->callback(eventtime);
#if TIMER_DEBUG
            interval[t->func] = std::max(get_monotonic() - start_time, interval[t->func]);
            SPDLOG_INFO("{}: {}", t->func, interval[t->func]);
#endif
            if (dispatch != g_dispatch)
            {
                next_timer = std::min(next_timer, waketime);
                // SPDLOG_DEBUG("before end_greenlet");
                end_greenlet(dispatch);
                return 0.0;
            }
        }

        next_timer = std::min(next_timer, waketime);
    }
    return 0.0;
}

void SelectReactor::got_pipe_signal(double eventtime)
{
    // SPDLOG_DEBUG("got_pipe_signal");
    char buffer[4096];
    ssize_t bytes_read = read(pipe_fds[0], buffer, sizeof(buffer));
    if (bytes_read == -1)
    {
        perror("read failed");
    }
    std::lock_guard<std::mutex> lock(mtx);
    while (!async_queue.empty())
    {
        std::function<void()> func = async_queue.front();
        async_queue.pop();
        func();
    }
}

void SelectReactor::setup_async_callbacks()
{
    if (pipe(pipe_fds) == -1)
    {
        perror("pipe failed");
    }
    elegoo::common::set_nonblock(pipe_fds[0]);
    elegoo::common::set_nonblock(pipe_fds[1]);

    std::shared_ptr<ReactorFileHandler> fd_handle = register_fd(pipe_fds[0], [this](double eventtime)
                                                                { got_pipe_signal(eventtime); });
    SPDLOG_DEBUG("fd_handle->fd:{}", fd_handle->fd);
}

double SelectReactor::sys_pause(double waketime)
{
    double delay = waketime - get_monotonic();
    if (delay > 0)
    {
        sleep(delay);
    }
    return get_monotonic();
}

void SelectReactor::end_greenlet(std::shared_ptr<ReactorGreenlet> g_old)
{
    greenlets.push_back(g_old);
    unregister_timer(g_old->timer);
    g_old->timer = nullptr;
    buffer_time = NEVER;
    // SPDLOG_DEBUG("__func__:{}",__func__);
    g_dispatch->resume();
    // SPDLOG_DEBUG("__func__:{}",__func__);
    g_dispatch = g_old;
}

void SelectReactor::dispatch_loop()
{
    std::shared_ptr<ReactorGreenlet> t_dispatch;
    g_dispatch = t_dispatch = getcurrent();
    if (g_dispatch == nullptr)
    {
        SPDLOG_ERROR("!!!!!!!!!!!!!!!!");
    }
    bool busy = true;
    double eventtime = get_monotonic();
    double timeout = 0;
    int max_fd = 0;

    while (process)
    {
        timeout = check_timers(eventtime, busy);
        busy = false;

        fd_set read_set, write_set;
        FD_ZERO(&read_set);
        FD_ZERO(&write_set);

        for (const auto &handler : read_fds)
        {
            FD_SET(handler->fileno(), &read_set);
            if (handler->fileno() > max_fd)
            {
                max_fd = handler->fileno();
            }
        }

        for (const auto &handler : write_fds)
        {
            FD_SET(handler->fileno(), &write_set);
            if (handler->fileno() > max_fd)
            {
                max_fd = handler->fileno();
            }
        }

        struct timeval tv;
        tv.tv_sec = static_cast<long>(timeout);
        tv.tv_usec = static_cast<long>((timeout - tv.tv_sec) * 1e6);
        int res = select(max_fd + 1, &read_set, &write_set, nullptr, &tv);
        eventtime = get_monotonic();

        if (res > 0)
        {
            for (const auto &handler : read_fds)
            {
                if (FD_ISSET(handler->fileno(), &read_set))
                {
                    busy = true;
                    handler->read_callback(eventtime);
                    if (t_dispatch != g_dispatch)
                    {
                        SPDLOG_DEBUG("before end_greenlet");
                        end_greenlet(t_dispatch);
                        eventtime = get_monotonic();
                        break;
                    }
                }
            }

            for (const auto &handler : write_fds)
            {
                if (FD_ISSET(handler->fileno(), &write_set))
                {
                    busy = true;
                    handler->write_callback(eventtime);
                    if (t_dispatch != g_dispatch)
                    {
                        SPDLOG_DEBUG("before end_greenlet");
                        end_greenlet(t_dispatch);
                        eventtime = get_monotonic();
                        break;
                    }
                }
            }
        }
    }

    g_dispatch.reset();
}

std::shared_ptr<ReactorGreenlet> SelectReactor::getcurrent()
{
    // SPDLOG_DEBUG("__func__:{}",__func__);
    stCoRoutine_t *co = co_self();
    int index = -1;
    // printf("all_greenlets.size %d co %p\n", all_greenlets.size(), co);

    for (size_t i = 0; i < all_greenlets.size(); ++i)
    {
        if (all_greenlets[i]->get_co() == co)
        {
            index = i;
            break;
        }
    }

    if (index < 0)
    {
        std::cout << "error row 449" << std::endl;
        return nullptr;
    }

    return all_greenlets[index];
}

PollReactor::PollReactor() : SelectReactor()
{
    SPDLOG_DEBUG("create PollReactor success!");
}

PollReactor::~PollReactor()
{
}

std::shared_ptr<ReactorFileHandler> PollReactor::register_fd(
    int fd,
    std::function<void(double)> read_callback,
    std::function<void(double)> write_callback)
{
    auto file_handler = std::make_shared<ReactorFileHandler>(
        fd, read_callback, write_callback);
    fds[fd] = file_handler;

    // SPDLOG_DEBUG("__func__:{},fd:{}",__func__,fd);
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN | POLLHUP;
    poll_fds.push_back(pfd);

    return file_handler;
}

void PollReactor::unregister_fd(std::shared_ptr<ReactorFileHandler> file_handler)
{
    int fd = file_handler->fileno();

    auto it = std::remove_if(poll_fds.begin(), poll_fds.end(),
                             [fd](const pollfd &pfd)
                             { return pfd.fd == fd; });
    poll_fds.erase(it, poll_fds.end());

    fds.erase(fd);
}

void PollReactor::set_fd_wake(
    std::shared_ptr<ReactorFileHandler> file_handler,
    bool is_readable,
    bool is_writeable)
{
    short flags = POLLHUP;

    if (is_readable)
    {
        flags |= POLLIN;
    }

    if (is_writeable)
    {
        flags |= POLLOUT;
    }

    for (auto &pfd : poll_fds)
    {
        if (pfd.fd == file_handler->fileno())
        {
            pfd.events = flags;
            break;
        }
    }
}

void PollReactor::dispatch_loop()
{
    std::shared_ptr<ReactorGreenlet> t_dispatch;
    g_dispatch = t_dispatch = getcurrent();
    if (g_dispatch == nullptr)
    {
        SPDLOG_ERROR("!!!!!!!!!!!!!!!!!!!");
    }
    bool busy = true;
    double eventtime = get_monotonic();
    double timeout = 0;
    while (process)
    {
        timeout = check_timers(eventtime, busy);
        busy = false;
        int poll_timeout = static_cast<int>(std::ceil(timeout * 1000.0));
        int res = poll(poll_fds.data(), poll_fds.size(), poll_timeout);
        eventtime = get_monotonic();
        // SPDLOG_INFO("waitting task... poll_timeout: {},eventtime:{},res:{}", poll_timeout,eventtime,res);

        if (res > 0)
        {
            // SPDLOG_INFO("waitting task... poll_timeout: {},eventtime:{},res:{}", poll_timeout,eventtime,res);
            for (auto pfd : poll_fds)
            {
                busy = true;
                if (pfd.revents & (POLLIN | POLLHUP))
                {
                    auto handler = fds[pfd.fd];
                    // SPDLOG_DEBUG("pfd.revents:{} for read,pfd.fd:{}",pfd.revents,pfd.fd);
                    handler->read_callback(eventtime);
                    if (t_dispatch != g_dispatch)
                    {
                        SPDLOG_DEBUG("before end_greenlet");
                        end_greenlet(t_dispatch);
                        eventtime = get_monotonic();
                        break;
                    }
                }

                if (pfd.revents & POLLOUT)
                {
                    auto handler = fds[pfd.fd];
                    // SPDLOG_DEBUG("pfd.revents:{} for write,pfd.fd:{}",pfd.revents,pfd.fd);
                    handler->write_callback(eventtime);
                    if (t_dispatch != g_dispatch)
                    {
                        SPDLOG_DEBUG("before end_greenlet");
                        end_greenlet(t_dispatch);
                        eventtime = get_monotonic();
                        break;
                    }
                }
            }
        }
        else
        {
            // SPDLOG_INFO("waitting task... poll_timeout: {},eventtime:{},res:{}", poll_timeout,eventtime,res);
        }
    }

    g_dispatch.reset();
}

EPollReactor::EPollReactor() : SelectReactor()
{
}

EPollReactor::~EPollReactor()
{
}

void EPollReactor::register_fd()
{
}

void EPollReactor::unregister_fd()
{
}

void EPollReactor::set_fd_wake()
{
}

void EPollReactor::dispatch_loop()
{
}

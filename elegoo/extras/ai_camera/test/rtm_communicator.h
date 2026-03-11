#ifndef RTM_COMMUNICATOR_H
#define RTM_COMMUNICATOR_H

#include <string>
#include <functional>
#include <memory>
#include <vector>
#include "agora_rtc_api.h"  // 引入SDK头文件获取结构体定义


class RtmCommunicator {
public:
    // 回调函数类型定义
    using LoginSuccessCallback = std::function<void()>;
    using ErrorCallback = std::function<void(int32_t, const std::string&)>;
    using SendMsgCallback = std::function<void(int32_t, const std::string&)>;
    using RecvMsgCallback = std::function<void(const std::string&, const std::string&)>;
    using EventCallback = std::function<void(const std::string&, uint32_t, uint32_t)>;

    // 构造/析构函数
    explicit RtmCommunicator(const std::string& appid);
    ~RtmCommunicator();

    // 禁止拷贝构造和赋值（C++11特性）
    RtmCommunicator(const RtmCommunicator&) = delete;
    RtmCommunicator& operator=(const RtmCommunicator&) = delete;

    // 核心功能接口
    int Start(const std::string& user_id, const std::string& peer_id, const std::string& token);
    void Stop();
    int SendTextMessage(const std::string& peer_id, const std::string& text);
    int SendDataMessage(const std::string& peer_id, const void* data, size_t len, uint32_t msg_id = 0);
    int Refresh(const std::string& new_token);

    // 配置接口
    void SetLoginSuccessCallback(LoginSuccessCallback cb);
    void SetErrorCallback(ErrorCallback cb);
    void SetSendMsgCallback(SendMsgCallback cb);
    void SetRecvMsgCallback(RecvMsgCallback cb);
    void SetEventCallback(EventCallback cb);
    void SetServiceOption(const rtc_service_option_t& opt);
    void SetPid(pid_t pid);

    // 状态查询
    bool IsRunning() const;
    bool IsLoggedIn() const;

private:
    // Pimpl模式隐藏实现细节
    class Impl;
    std::unique_ptr<Impl> impl_;
};

#endif // RTM_COMMUNICATOR_H
    
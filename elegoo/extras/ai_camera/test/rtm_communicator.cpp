/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2025-08-08 10:45:24
 * @LastEditors  : Jack
 * @LastEditTime : 2025-08-29 17:43:19
 * @Description  : 
 * 
 * Copyright (c) 2025 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "rtm_communicator.h"
#include <cstring>
#include <chrono>
#include <algorithm>
#include <mutex>
#include <thread>
#include <cstdio>
#include <stdint.h>
#include <sstream>

// 常量定义
#define MSG_QUEUE_MAX 1024          // 消息队列最大长度
#define RESEND_TIMEOUT_MS 10000     // 消息重发超时时间(10秒)
#define MAX_RESEND_PER_ROUND 3      // 每轮最大重发数量
#define MSG_DATA_MAX_SIZE 1024      // 消息数据最大长度

// 消息结构体定义
struct MyMessage {
    uint32_t msg_id;                // 消息ID
    uint32_t msg_len;               // 消息长度
    char msg_data[MSG_DATA_MAX_SIZE]; // 消息数据
    std::string peer_id;            // 目标对端ID
};

// 消息缓存结构体
struct MessageCache {
    MyMessage msg;                  // 消息内容
    rtm_msg_state_e state;          // 消息状态
    uint64_t sent_ts;               // 发送时间戳(ms)

    // C++11构造函数初始化
    MessageCache() : state(rtm_msg_state_e::RTM_MSG_STATE_INIT), sent_ts(0) {
        msg.msg_id = 0;
        msg.msg_len = 0;
        std::memset(msg.msg_data, 0, MSG_DATA_MAX_SIZE);
    }
};

// 内部实现类
class RtmCommunicator::Impl {
public:
    // 允许外部类访问私有成员
    friend class RtmCommunicator;

    explicit Impl(const std::string& appid) 
        : appid_(appid), 
          next_msg_id_(1), 
          is_running_(false), 
          is_logged_in_(false),
          pid_(0) {
        // 初始化消息缓存
        msg_cache_.resize(MSG_QUEUE_MAX);
        // 初始化SDK回调结构体
        initCallbacks();
        // 设置静态实例指针
        instance_ = this;
    }

    ~Impl() {
        Stop(); // 确保资源释放
        if (instance_ == this) {
            instance_ = nullptr;
        }
    }

    // 启动RTM通信
    int Start(const std::string& user_id, const std::string& peer_id, const std::string& token) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (is_running_) return -1;

        // 保存动态参数
        user_id_ = user_id;
        default_peer_id_ = peer_id;
        token_ = token;

        // 初始化SDK
        if (InitSdk() != 0) return -2;

        // 登录RTM服务
        int ret = LoginRtm();
        printf("LoginRtm ret:%d\n", ret);
        if (ret != 0) {
            FiniSdk();
            return ret;
        }

        // 启动消息重发定时器
        // startResendTimer();

        is_running_ = true;
        // is_logged_in_ = true;
        return 0;
    }

    // 停止RTM通信
    void Stop() {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!is_running_) return;

        // 停止重发线程
        stopResendTimer();
        // 登出服务
        LogoutRtm();
        // 释放SDK资源
        FiniSdk();
        // 清空消息缓存
        clearMessageCache();

        is_running_ = false;
        is_logged_in_ = false;
    }

    // 发送文本消息
    int SendTextMessage(const std::string& peer_id, const std::string& text) {
        if (!IsLoggedIn()) return -1;
        if (text.empty()) return -2;

        MyMessage msg;
        msg.msg_id = getNextMsgId();
        msg.peer_id = peer_id.empty() ? default_peer_id_ : peer_id;
        msg.msg_len = std::min(text.size(), static_cast<size_t>(MSG_DATA_MAX_SIZE - 1));
        std::strncpy(msg.msg_data, text.c_str(), msg.msg_len);
        msg.msg_data[msg.msg_len] = '\0';
        printf("xxx[%d]:{%s}\n", msg.msg_len, msg.msg_data);

        return sendMessage(msg);
    }

    // 发送数据消息
    int SendDataMessage(const std::string& peer_id, const void* data, size_t len, uint32_t msg_id) {
        if (!IsLoggedIn()) return -1;
        if (!data || len == 0) return -2;
        if (len > MSG_DATA_MAX_SIZE) return -3;

        MyMessage msg;
        msg.msg_id = (msg_id == 0) ? getNextMsgId() : msg_id;
        msg.peer_id = peer_id.empty() ? default_peer_id_ : peer_id;
        msg.msg_len = len;
        std::memcpy(msg.msg_data, data, len);
        printf("xxx[%d]:{%s}\n", msg.msg_len, msg.msg_data);
        return sendMessage(msg);
    }

    // 刷新连接（使用新token重连）
    int Refresh(const std::string& new_token) {
        printf("Refresh! \n");
        // std::lock_guard<std::mutex> lock(mtx_);
        auto current_messages = msg_cache_;
        if (is_running_ && is_logged_in_) {
            printf("LogoutRtm! \n");
            // 登出当前连接
            LogoutRtm();
            is_logged_in_ = false;
        }
        // 更新token并重新登录
        token_ = new_token;
        printf("new_token [%s]! \n", new_token.c_str());
        int ret = LoginRtm();
        if (ret != 0) {
            is_logged_in_ = false;
            return ret;
        }

        // 恢复消息状态并触发重发
        msg_cache_ = current_messages;
        // is_logged_in_ = true;
        if (is_logged_in_) checkResend();
        return 0;
    }

    // 设置回调函数
    void SetLoginSuccessCallback(LoginSuccessCallback cb) { 
        std::lock_guard<std::mutex> lock(mtx_);
        login_success_cb_ = cb; 
    }

    void SetErrorCallback(ErrorCallback cb) { 
        std::lock_guard<std::mutex> lock(mtx_);
        error_cb_ = cb; 
    }

    void SetSendMsgCallback(SendMsgCallback cb) { 
        std::lock_guard<std::mutex> lock(mtx_);
        send_msg_cb_ = cb; 
    }

    void SetRecvMsgCallback(RecvMsgCallback cb) { 
        std::lock_guard<std::mutex> lock(mtx_);
        recv_msg_cb_ = cb; 
    }

    void SetEventCallback(EventCallback cb) { 
        std::lock_guard<std::mutex> lock(mtx_);
        event_cb_ = cb; 
    }

    // 配置设置
    void SetServiceOption(const rtc_service_option_t& opt) {
        std::lock_guard<std::mutex> lock(mtx_);
        service_opt_ = opt;
    }

    void SetPid(pid_t pid) {
        std::lock_guard<std::mutex> lock(mtx_);
        pid_ = pid;
    }

    // 状态查询
    bool IsRunning() const { 
        std::lock_guard<std::mutex> lock(mtx_);
        return is_running_; 
    }

    bool IsLoggedIn() const { 
        std::lock_guard<std::mutex> lock(mtx_);
        return is_logged_in_; 
    }

private:
    // 初始化SDK回调结构体
    void initCallbacks() {
        std::memset(&event_handler_, 0, sizeof(event_handler_));
        event_handler_.on_error = nullptr;

        std::memset(&rtm_handler_, 0, sizeof(rtm_handler_));
        rtm_handler_.on_rtm_data = &Impl::onRtmDataStatic;
        rtm_handler_.on_rtm_event = &Impl::onRtmEventStatic;
        rtm_handler_.on_send_rtm_data_result = &Impl::onSendResultStatic;
    }

    // 获取下一个消息ID
    uint32_t getNextMsgId() {
        std::lock_guard<std::mutex> lock(mtx_);
        return next_msg_id_++;
    }

    // 获取消息缓存槽
    MessageCache& getMessageCache(uint32_t msg_id) {
        size_t index = msg_id % MSG_QUEUE_MAX;
        return msg_cache_[index];
    }

    // 发送消息核心逻辑
    int sendMessage(const MyMessage& msg) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        // 检查消息队列是否已满
        // if (getPendingMessageCount() >= MSG_QUEUE_MAX) {
        //     if (error_cb_) {
        //         error_cb_(-100, "Message queue is full");
        //     }
        //     return -100;
        // }

        // 计算消息大小
        size_t msg_size = offsetof(MyMessage, msg_data) + msg.msg_len;

        // 调用SDK发送接口
        int ret = agora_rtc_send_rtm_data(
            msg.peer_id.c_str(),
            msg.msg_data,
            msg.msg_len,
            msg.msg_id
        );

        if (ret < 0) {
            if (error_cb_) {
                error_cb_(ret, "Send failed: " + std::string(agora_rtc_err_2_str(ret)));
            }
            return ret;
        }
        else {
            printf("send success:[%d] : %s\n", msg_size, msg.msg_data);
        }

        // 更新消息缓存
        auto& cache = getMessageCache(msg.msg_id);
        cache.msg = msg;
        cache.state = rtm_msg_state_e::RTM_MSG_STATE_RECEIVED;
        cache.sent_ts = getCurrentTimestamp();

        return 0;
    }

    // 检查并重新发送超时消息
    int checkResend() {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!is_logged_in_) return 0;

        int resend_count = 0;
        uint64_t now = getCurrentTimestamp();

        // 遍历消息缓存查找需要重发的消息
        for (auto& cache : msg_cache_) {
            if (resend_count >= MAX_RESEND_PER_ROUND) break;
            if (cache.state != rtm_msg_state_e::RTM_MSG_STATE_RECEIVED) continue;

            // 检查是否超时
            uint64_t time_diff = now - cache.sent_ts;
            if (time_diff >= RESEND_TIMEOUT_MS) {
                size_t msg_size = offsetof(MyMessage, msg_data) + cache.msg.msg_len;
                int ret = agora_rtc_send_rtm_data(
                    cache.msg.peer_id.c_str(),
                    &cache.msg,
                    msg_size,
                    cache.msg.msg_id
                );

                if (ret < 0) {
                    if (error_cb_) {
                        std::stringstream ss;
                        ss << "Resend failed (id=" << cache.msg.msg_id << ")";
                        error_cb_(ret, ss.str());
                    }
                } else {
                    cache.sent_ts = now;
                    resend_count++;
                }
            }
        }
        return resend_count;
    }

    // 启动重发定时器线程
    void startResendTimer() {
        resend_thread_ = std::thread(&Impl::resendLoop, this);
    }

    // 重发循环逻辑
    void resendLoop() {
        while (IsRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            std::lock_guard<std::mutex> lock(mtx_);
            if (is_logged_in_) {
                checkResend();
            }
        }
    }

    // 停止重发线程
    void stopResendTimer() {
        if (resend_thread_.joinable()) {
            resend_thread_.join();
        }
    }

    // 清空消息缓存
    void clearMessageCache() {
        for (auto& cache : msg_cache_) {
            cache = MessageCache();
        }
        next_msg_id_ = 1;
    }

    // 获取当前时间戳(ms)
    uint64_t getCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()
        );
        return static_cast<uint64_t>(ms.count());
    }

    // 获取待确认消息数量
    size_t getPendingMessageCount() {
        size_t count = 0;
        for (const auto& cache : msg_cache_) {
            if (cache.state != rtm_msg_state_e::RTM_MSG_STATE_UNREACHABLE) {
                count++;
            }
        }
        return count;
    }

    // SDK初始化
    int InitSdk() {
        rtc_service_option_t opt = service_opt_;
        // if (opt.log_cfg.log_path[0] == '\0') {
        //     opt.log_cfg.log_path = "./rtm_logs";
        // }
        // if (opt.log_cfg.log_level == 0) {
        //     opt.log_cfg.log_level = RTC_LOG_INFO;
        // }
        return agora_rtc_init(appid_.c_str(), &event_handler_, &opt);
    }

    // SDK释放
    void FiniSdk() {
        agora_rtc_fini();
    }

    // 登录RTM
    int LoginRtm() {
        return agora_rtc_login_rtm(user_id_.c_str(), token_.c_str(), &rtm_handler_);
    }

    // 登出RTM
    void LogoutRtm() {
        if (agora_rtc_logout_rtm() == 0) {
            printf("LogoutRtm success! \n");
        }
    }

    // SDK回调转发（静态函数）
    static void onRtmEventStatic(const char *user_id, rtm_event_type_e event_id, rtm_err_code_e event_code) {
        printf("<<<<<<<<<<<<<<<<<< user_id[%s] event id[%u], event code[%u] >>>>>>>>>>>>>>>>>>", user_id, event_id, event_code);
        if (instance_) {
            instance_->OnRtmEvent(user_id ? user_id : "", event_id, event_code);
        }
    }

    static void onRtmDataStatic(const char *user_id, const void *data, size_t data_len) {
        printf("<<<<<<<<<<<<<<<<<< user_id[%s] data[%s], event code[%u] >>>>>>>>>>>>>>>>>>", user_id, data, data_len);
        if (instance_ && user_id && data && data_len > 0) {
            instance_->OnReceiveMessage(user_id, data, data_len);
        }
    }

    static void onSendResultStatic(const char *peer_id, uint32_t msg_id, rtm_msg_state_e state) {
        printf("<<<<<<<<<<<<<<<<<< peer_id[%s] msg_id[%u], state[%u] >>>>>>>>>>>>>>>>>>", peer_id, msg_id, state);
        if (instance_ && peer_id) {
            instance_->OnSendResult(peer_id, msg_id, state);
        }
    }

    // 回调处理（成员函数）
    void OnSendResult(const std::string& peer_id, uint32_t msg_id, rtm_msg_state_e state) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto& cache = getMessageCache(msg_id);
        
        if (cache.state == rtm_msg_state_e::RTM_MSG_STATE_UNREACHABLE) return;
        if (cache.msg.peer_id != peer_id) return;

        // 更新消息状态并触发回调
        cache.state = (state == 0) ? rtm_msg_state_e::RTM_MSG_STATE_UNREACHABLE : rtm_msg_state_e::RTM_MSG_STATE_RECEIVED;
        
        if (send_msg_cb_) {
            std::stringstream ss;
            ss << msg_id;
            send_msg_cb_(state, ss.str());
        }
    }

    void OnReceiveMessage(const std::string& sender, const void* data, size_t len) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (recv_msg_cb_) {
            recv_msg_cb_(sender, std::string(static_cast<const char*>(data), len));
        }
    }

    void OnRtmEvent(const std::string& user_id, uint32_t event_id, uint32_t event_code) {
        printf("is_logged_in_: %d event_id:%d event_code:%d\n", is_logged_in_, event_id, event_code);
        std::lock_guard<std::mutex> lock(mtx_);
        
        // 处理登录状态事件
        if (event_id == 0) {
            is_logged_in_ = (event_code == 0);
            if (is_logged_in_ && login_success_cb_) {
                login_success_cb_();
            }
        }

        // 触发事件回调
        if (event_cb_) {
            event_cb_(user_id, event_id, event_code);
        }
    }

    // 成员变量
    std::string appid_;                  // 应用ID
    std::string user_id_;                // 当前用户ID
    std::string default_peer_id_;        // 默认对端ID
    std::string token_;                  // 令牌
    rtc_service_option_t service_opt_;   // 服务选项
    pid_t pid_;                          // 进程ID

    std::vector<MessageCache> msg_cache_;// 消息缓存
    uint32_t next_msg_id_;               // 下一个消息ID

    mutable std::mutex mtx_;             // 互斥锁
    bool is_running_;                    // 运行状态
    bool is_logged_in_;                  // 登录状态

    std::thread resend_thread_;          // 重发定时器线程

    // 回调函数
    LoginSuccessCallback login_success_cb_;
    ErrorCallback error_cb_;
    SendMsgCallback send_msg_cb_;
    RecvMsgCallback recv_msg_cb_;
    EventCallback event_cb_;

    // SDK回调结构体
    agora_rtc_event_handler_t event_handler_;
    agora_rtm_handler_t rtm_handler_;

    // 静态实例指针（用于回调转发）
    static Impl* instance_;
};

// 初始化静态实例指针
RtmCommunicator::Impl* RtmCommunicator::Impl::instance_ = nullptr;

// 公开接口实现
RtmCommunicator::RtmCommunicator(const std::string& appid) 
    : impl_(new Impl(appid)) {}  // C++11兼容的智能指针初始化

RtmCommunicator::~RtmCommunicator() = default;

int RtmCommunicator::Start(const std::string& user_id, const std::string& peer_id, const std::string& token) {
    return impl_->Start(user_id, peer_id, token);
}

void RtmCommunicator::Stop() {
    impl_->Stop();
}

int RtmCommunicator::SendTextMessage(const std::string& peer_id, const std::string& text) {
    return impl_->SendTextMessage(peer_id, text);
}

int RtmCommunicator::SendDataMessage(const std::string& peer_id, const void* data, size_t len, uint32_t msg_id) {
    return impl_->SendDataMessage(peer_id, data, len, msg_id);
}

int RtmCommunicator::Refresh(const std::string& new_token) {
    return impl_->Refresh(new_token);
}

void RtmCommunicator::SetLoginSuccessCallback(LoginSuccessCallback cb) {
    impl_->SetLoginSuccessCallback(cb);
}

void RtmCommunicator::SetErrorCallback(ErrorCallback cb) {
    impl_->SetErrorCallback(cb);
}

void RtmCommunicator::SetSendMsgCallback(SendMsgCallback cb) {
    impl_->SetSendMsgCallback(cb);
}

void RtmCommunicator::SetRecvMsgCallback(RecvMsgCallback cb) {
    impl_->SetRecvMsgCallback(cb);
}

void RtmCommunicator::SetEventCallback(EventCallback cb) {
    impl_->SetEventCallback(cb);
}

void RtmCommunicator::SetServiceOption(const rtc_service_option_t& opt) {
    impl_->SetServiceOption(opt);
}

void RtmCommunicator::SetPid(pid_t pid) {
    impl_->SetPid(pid);
}

bool RtmCommunicator::IsRunning() const {
    return impl_->IsRunning();
}

bool RtmCommunicator::IsLoggedIn() const {
    return impl_->IsLoggedIn();
}
    
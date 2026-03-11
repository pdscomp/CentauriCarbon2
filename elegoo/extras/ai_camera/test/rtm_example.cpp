/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2025-08-08 10:45:24
 * @LastEditors  : Jack
 * @LastEditTime : 2025-08-08 15:27:32
 * @Description  : 
 * 
 * Copyright (c) 2025 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "rtm_communicator.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <string.h>

// 自定义消息结构（示例）
struct my_message_t {
    uint32_t msg_id;
    uint32_t msg_len;
    char data[1024];
};

int main() {
    // 1. 创建RTM实例
    RtmCommunicator rtm("d035320941e34cd5bc4ec106eff05580");

    // 2. 配置服务选项
    rtc_service_option_t service_opt = { 0 };
    service_opt.area_code = AREA_CODE_GLOB;
    service_opt.log_cfg.log_path = "./";
    service_opt.log_cfg.log_level = RTC_LOG_INFO;
    snprintf(service_opt.license_value, sizeof(service_opt.license_value), "%s", "019836C773D07642BD402F0BE1578C9B");
    rtm.SetServiceOption(service_opt);
    // rtm.SetPid(getpid());

    // 3. 注册回调函数
    rtm.SetEventCallback([](const std::string& user_id, uint32_t event_id, uint32_t event_code) {
        std::cout << "RTM事件 - 用户: " << user_id 
                  << ", 事件ID: " << event_id 
                  << ", 事件代码: " << event_code << std::endl;
    });

    rtm.SetRecvMsgCallback([](const std::string& user_id, const std::string& data) {
        std::cout << "收到来自 " << user_id << " 的数据: " << data << std::endl;
    });

    rtm.SetSendMsgCallback([](int32_t code, const std::string& msg_id) {
        std::cout << "消息发送结果 - ID: " << msg_id 
                  << ", 状态码: " << code << std::endl;
    });

    rtm.SetLoginSuccessCallback([]() {
        std::cout << "RTM登录成功！" << std::endl;
    });

    rtm.SetErrorCallback([](int32_t code, const std::string& msg) {
        std::cerr << "错误 - 代码: " << code << ", 消息: " << msg << std::endl;
    });

    // 4. 启动RTM通信
    int ret = rtm.Start("5ABC0000000000002", 
                    "5",
                    "007eJxTYGBgzYz+c9cjZ5fxg6TCCz26BoX3duRazViXk3lvJ/PUF7MVGFIMjE2NjQwsTQxTjU2SU0yTkk1Skw0NzFLT0gxMTS0MzKunZjQEMjIcLeyazMjAxMAIhCC+IIOpo5OzARIwAgDMjyIW");
    if (ret != 0) {
        std::cerr << "启动RTM失败，错误码: " << ret << std::endl;
        return -1;
    }

    // 5. 等待登录成功
    int wait_count = 0;
    while (!rtm.IsLoggedIn() && wait_count < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        wait_count++;
    }

    // 6. 发送测试消息
    if (rtm.IsLoggedIn()) {
        std::cout << "发送文本消息..." << std::endl;
        std::string text = "Hello RTM!";
        rtm.SendTextMessage("5", text);
    }

    // 7. 运行一段时间后刷新连接
    std::this_thread::sleep_for(std::chrono::seconds(5));
    std::cout << "刷新连接，使用新token..." << std::endl;
    ret = rtm.Refresh("NEW_TOKEN_123456");
    if (ret != 0) {
        std::cerr << "刷新连接失败，错误码: " << ret << std::endl;
    }

    // 8. 继续运行一段时间
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // 9. 停止RTM通信
    std::cout << "停止RTM通信..." << std::endl;
    rtm.Stop();

    return 0;
}

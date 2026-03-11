/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2025-07-31 11:50:10
 * @LastEditors  : Jack
 * @LastEditTime : 2025-08-30 17:41:34
 * @Description  : 
 * 
 * Copyright (c) 2025 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#ifndef RTC_STREAMER_H
#define RTC_STREAMER_H

#include <functional>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include "agora_rtc_api.h"


#define DEFAULT_CHANNEL_NAME "hello_demo"
#define DEFAULT_CERTIFACTE_FILENAME "certificate.bin"
#define DEFAULT_SEND_AUDIO_FILENAME_PCM_16K "send_audio_16k_1ch.pcm"
#define DEFAULT_SEND_AUDIO_FILENAME_PCM_08K "send_audio_8k_1ch.pcm"
#define DEFAULT_SEND_VIDEO_FILENAME "send_video.h264"
#define DEFAULT_SEND_AUDIO_FILENAME "send_audio_16k_1ch.pcm"
#define DEFAULT_SEND_AUDIO_BASENAME "send_audio"
#define DEFAULT_SEND_VIDEO_BASENAME "send_video"
#define DEFAULT_RECV_AUDIO_BASENAME "recv_audio"
#define DEFAULT_RECV_VIDEO_BASENAME "recv_video"
#define DEFAULT_SEND_VIDEO_FRAME_RATE (20)
#define DEFAULT_BANDWIDTH_ESTIMATE_MIN_BITRATE (10 * 1000)
#define DEFAULT_BANDWIDTH_ESTIMATE_MAX_BITRATE (1000* 1000)
#define DEFAULT_BANDWIDTH_ESTIMATE_START_BITRATE (500000)
#define DEFAULT_SEND_AUDIO_FRAME_PERIOD_MS (20)
#define DEFAULT_PCM_SAMPLE_RATE (16000)
#define DEFAULT_PCM_CHANNEL_NUM (1)


class RtcStreamer {
public:
    using FrameCallback = std::function<bool(unsigned char*, int*)>;
    
    explicit RtcStreamer(FrameCallback frameCallback);
    
    ~RtcStreamer();
    
    int Start(const std::string& appid, 
              const std::string& license, 
              const std::string& token, 
              const std::string& channel, 
              int uid);
    
    void Stop();
    void Pause();
    void Resume();
    
    int Refresh(const std::string& newToken);

private:
    // 内部状态
    enum class State {
        IDLE,
        INITIALIZING,
        RUNNING,
        PAUSE,
        STOPPING
    };
    
    // 事件处理回调
    static void eventHandler(int conn_id, const char* event, const char* data);
    
    // 视频发送线程
    void videoSendThread();
    
    // 解析帧类型
    video_frame_type_e parseFrameType(const unsigned char* data, int length);
    
    // 成员变量
    FrameCallback frameCallback_;
    std::thread videoThread_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::atomic<State> state_{State::IDLE};
    std::atomic<bool> connected_{false};
    std::atomic<bool> stopRequested_{false};
    
    // RTC相关参数
    connection_id_t conn_id_{0};
    std::string appid_;
    std::string license_;
    std::string token_;
    std::string channel_;
    int uid_;
    agora_rtc_event_handler_t event_handler{0};

    // 视频参数
    const int video_frame_rate_ = 20; // fps
    const int video_width_ = 640;    // 视频宽度
    const int video_height_ = 360;    // 视频高度
};

#endif 





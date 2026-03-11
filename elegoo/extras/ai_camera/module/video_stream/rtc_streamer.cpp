/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2025-07-31 11:49:54
 * @LastEditors  : huangpeiyun
 * @LastEditTime : 2025-09-15 21:21:26
 * @Description  : 
 * 
 * Copyright (c) 2025 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "rtc_streamer.h"
#include <cstring>
#include <chrono>
#include <iostream>
#include "spdlog/spdlog.h"

static void __on_join_channel_success(connection_id_t conn_id, uint32_t uid, int elapsed)
{
//   g_app.b_connected_flag = true;
  connection_info_t conn_info = { 0 };
//   agora_rtc_get_connection_info(g_app.conn_id, &conn_info);
  printf("[conn-%u] Join the channel %s successfully, uid %u elapsed %d ms\n", conn_id,
       conn_info.channel_name, uid, elapsed);
}

static void __on_reconnecting(connection_id_t conn_id)
{
	// g_app.b_connected_flag = false;
	printf("[conn-%u] connection timeout, reconnecting\n", conn_id);
}

static void __on_connection_lost(connection_id_t conn_id)
{
//   g_app.b_connected_flag = false;
  printf("[conn-%u] Lost connection from the channel\n", conn_id);
}

static void __on_rejoin_channel_success(connection_id_t conn_id, uint32_t uid, int elapsed_ms)
{
//   g_app.b_connected_flag = true;
  printf("[conn-%u] Rejoin the channel successfully, uid %u elapsed %d ms\n", conn_id, uid, elapsed_ms);
}

static void __on_user_joined(connection_id_t conn_id, uint32_t uid, int elapsed_ms)
{
  printf("[conn-%u] Remote user \"%u\" has joined the channel, elapsed %d ms\n", conn_id, uid, elapsed_ms);
}

static void __on_user_offline(connection_id_t conn_id, uint32_t uid, int reason)
{
  printf("[conn-%u] Remote user \"%u\" has left the channel, reason %d\n", conn_id, uid, reason);
}

static void __on_user_joined_with_user_account(connection_id_t conn_id, const user_info_t *user, int elapsed_ms)
{
  printf("[conn-%u] Remote user %s - %u has joined the channel, elapsed %d ms\n", conn_id, user->user_account, user->uid, elapsed_ms);
}

static void __on_user_offline_with_user_account(connection_id_t conn_id, const user_info_t *user, int reason)
{
  printf("[conn-%u] Remote user %s - %u has left the channel, reason %d\n", conn_id, user->user_account, user->uid, reason);
}

static void __on_user_info_updated(connection_id_t conn_id, const user_info_t *user)
{
  printf("[conn-%u] Remote user info updated: %s - %u\n", conn_id, user->user_account, user->uid);
}

static void __on_user_mute_audio(connection_id_t conn_id, uint32_t uid, bool muted)
{
  printf("[conn-%u] audio: uid=%u muted=%d\n", conn_id, uid, muted);
}

static void __on_user_mute_video(connection_id_t conn_id, uint32_t uid, bool muted)
{
  printf("[conn-%u] video: uid=%u muted=%d\n", conn_id, uid, muted);
}

static void __on_error(connection_id_t conn_id, int code, const char *msg)
{
  if (code == ERR_VIDEO_SEND_OVER_BANDWIDTH_LIMIT) {
    printf("Not enough uplink bandwdith. Error msg \"%s\" \n", msg);
    return;
  }

  if (code == ERR_INVALID_APP_ID) {
    printf("Invalid App ID. Please double check. Error msg \"%s\" \n", msg);
  } else if (code == ERR_INVALID_CHANNEL_NAME) {
    printf("Invalid channel name for conn_id %u. Please double check. Error msg \"%s\" \n", conn_id,
         msg);
  } else if (code == ERR_INVALID_TOKEN || code == ERR_TOKEN_EXPIRED) {
    printf("Invalid token. Please double check. Error msg \"%s\" \n", msg);
  } else if (code == ERR_DYNAMIC_TOKEN_BUT_USE_STATIC_KEY) {
    printf("Dynamic token is enabled but is not provided. Error msg \"%s\" \n", msg);
  } else {
    printf("Error %d is captured. Error msg \"%s\" \n", code, msg);
  }

//   g_app.b_stop_flag = true;
}

static void __on_license_failed(connection_id_t conn_id, int reason)
{
  printf("License verified failed, reason: %d\n", reason);
//   g_app.b_stop_flag = true;
}

static void __on_audio_data(connection_id_t conn_id, const uint32_t uid, uint16_t sent_ts,
                            const void *data, size_t len, const audio_frame_info_t *info_ptr)
{
  printf("[conn-%u] on_audio_data, uid %u sent_ts %u data_type %d, len %zu\n", conn_id, uid, sent_ts,
       info_ptr->data_type, len);
  // write_file(g_app.audio_file_writer, info_ptr->data_type, data, len);
}

static void __on_mixed_audio_data(connection_id_t conn_id, const void *data, size_t len,
                                  const audio_frame_info_t *info_ptr)
{
  printf("[conn-%u] on_mixed_audio_data, data_type %d, len %zu\n", conn_id, info_ptr->data_type, len);
  // write_file(g_app.audio_file_writer, info_ptr->data_type, data, len);
}

static void __on_video_data(connection_id_t conn_id, const uint32_t uid, uint16_t sent_ts,
                            const void *data, size_t len, const video_frame_info_t *info_ptr)
{
  printf("[conn-%u] on_video_data: uid %u sent_ts %u data_type %d frame_type %d stream_type %d len %zu\n",
       conn_id, uid, sent_ts, info_ptr->data_type, info_ptr->frame_type, info_ptr->stream_type, len);
  // write_file(g_app.video_file_writer, info_ptr->data_type, data, len);
}

static void __on_target_bitrate_changed(connection_id_t conn_id, uint32_t target_bps)
{
  printf("[conn-%u] Bandwidth change detected. Please adjust encoder bitrate to %u kbps\n", conn_id,
       target_bps / 1000);
}

static void __on_key_frame_gen_req(connection_id_t conn_id, uint32_t uid,
                                   video_stream_type_e stream_type)
{
  printf("[conn-%u] Frame loss detected. Please notify the encoder to generate key frame immediately\n",
       conn_id);
}

static void __on_rtc_stats(connection_id_t conn_id, rtc_stats_t stats)
{
//   if (g_app.config.lan_accelerate) 
  {
    printf("[conn-%u] rtc_stats: lan_accelerate=%d\n", conn_id, stats.lan_accelerate_state);
  }
}

static void app_init_event_handler(agora_rtc_event_handler_t *event_handler) //, app_config_t *config)
{
	event_handler->on_join_channel_success = __on_join_channel_success;
	event_handler->on_reconnecting = __on_reconnecting,
	event_handler->on_connection_lost = __on_connection_lost;
	event_handler->on_rejoin_channel_success = __on_rejoin_channel_success;
	event_handler->on_user_joined = __on_user_joined;
	event_handler->on_user_offline = __on_user_offline;

    // if (config->uname) 
    {
        event_handler->on_user_joined_with_user_account = __on_user_joined_with_user_account;
        event_handler->on_user_offline_with_user_account = __on_user_offline_with_user_account;
        event_handler->on_user_info_updated = __on_user_info_updated;
    }
	
    event_handler->on_user_mute_audio = __on_user_mute_audio;
	  event_handler->on_user_mute_video = __on_user_mute_video;
    event_handler->on_target_bitrate_changed = __on_target_bitrate_changed;
    event_handler->on_key_frame_gen_req = __on_key_frame_gen_req;
    event_handler->on_video_data = __on_video_data;
    event_handler->on_error = __on_error;
    event_handler->on_license_validation_failure = __on_license_failed;
    event_handler->on_rtc_stats = __on_rtc_stats;

    // if (config->enable_audio_mixer) 
    // {
    //     event_handler->on_mixed_audio_data = __on_mixed_audio_data;
    // } else {
    //     event_handler->on_audio_data = __on_audio_data;
    // }
}

// 事件处理回调
void RtcStreamer::eventHandler(int conn_id, const char* event, const char* data) {
    // 这里简化处理，实际应用中需要处理各种事件
    if (strcmp(event, "onJoinSuccess") == 0) {
        std::cout << "Successfully joined channel" << std::endl;
    } else if (strcmp(event, "onError") == 0) {
        std::cerr << "RTC error: " << data << std::endl;
    } else if (strcmp(event, "onConnectionStateChanged") == 0) {
        // 处理连接状态变化
    }
}

// 构造函数
RtcStreamer::RtcStreamer(FrameCallback frameCallback)
    : frameCallback_(std::move(frameCallback)) {}

// 析构函数
RtcStreamer::~RtcStreamer() {
    Stop();
}

// 启动RTC推流
int RtcStreamer::Start(const std::string& appid, 
                      const std::string& license, 
                      const std::string& token, 
                      const std::string& channel, 
                      int uid) {
    
    // 检查是否已经在运行
    if (state_ != State::IDLE) {
        std::cerr << "RTC streamer is already running or initializing" << std::endl;
        return -1;
    }
    
    // 保存参数
    appid_ = appid;
    license_ = license;
    token_ = token;
    channel_ = channel;
    uid_ = uid;
    
    // 设置状态为初始化中
    state_ = State::INITIALIZING;
    
    // 1. 初始化SDK
    std::cout << "Initializing Agora RTC SDK..." << std::endl;
    
    rtc_service_option_t service_opt = {0};
    service_opt.area_code = AREA_CODE_GLOB; // 使用全球区域
    service_opt.log_cfg.log_level = RTC_LOG_INFO; // 日志级别
    snprintf(service_opt.license_value, sizeof(service_opt.license_value), "%s", license_.c_str());
    
    app_init_event_handler(&event_handler);

    int rval = agora_rtc_init(appid_.c_str(), &event_handler, &service_opt);
    if (rval < 0) {
        std::cerr << "Failed to initialize Agora SDK: " << agora_rtc_err_2_str(rval) << std::endl;
        state_ = State::IDLE;
        return rval;
    }
    
    // 2. 创建连接
    std::cout << "Creating RTC connection..." << std::endl;
    rval = agora_rtc_create_connection(&conn_id_);
    if (rval < 0) {
        std::cerr << "Failed to create connection: " << agora_rtc_err_2_str(rval) << std::endl;
        agora_rtc_fini();
        state_ = State::IDLE;
        return rval;
    }
    
    // 3. 设置带宽参数
    rval = agora_rtc_set_bwe_param(conn_id_, DEFAULT_BANDWIDTH_ESTIMATE_MIN_BITRATE,
                                  DEFAULT_BANDWIDTH_ESTIMATE_MAX_BITRATE,
                                  DEFAULT_BANDWIDTH_ESTIMATE_START_BITRATE);
    if (rval != 0) {
        std::cerr << "Failed to set bandwidth parameters: " << agora_rtc_err_2_str(rval) << std::endl;
    }
    
    // 4. 加入频道
    std::cout << "Joining channel: " << channel_ << std::endl;
    
    rtc_channel_options_t channel_options = {0};
    channel_options.auto_subscribe_audio = false;
    channel_options.auto_subscribe_video = false;
    channel_options.enable_audio_mixer = false;
    
    rval = agora_rtc_join_channel(conn_id_, channel_.c_str(), uid_, 
                                 const_cast<char*>(token_.c_str()), &channel_options);
    if (rval < 0) {
        std::cerr << "Failed to join channel: " << agora_rtc_err_2_str(rval) << std::endl;
        agora_rtc_destroy_connection(conn_id_);
        agora_rtc_fini();
        state_ = State::IDLE;
        return rval;
    }
    
    // 5. 启动视频发送线程
    stopRequested_ = false;
    connected_ = true;
    videoThread_ = std::thread([this]() { 
      pthread_setname_np(pthread_self(), "rtc_video_send");
      this->videoSendThread(); 
    });

    state_ = State::RUNNING;
    std::cout << "RTC streaming started successfully" << std::endl;
    return 0;
}

void RtcStreamer::Pause()
{
  if(state_ == State::RUNNING) {
    state_ = State::PAUSE;
  }
}

void RtcStreamer::Resume()
{
  if(state_ == State::PAUSE) {
    state_ = State::RUNNING;
  }
}

// 停止RTC推流
void RtcStreamer::Stop() {
    if (state_ == State::IDLE || state_ == State::STOPPING) {
        return;
    }
    
    state_ = State::STOPPING;
    stopRequested_ = true;
    
    if (videoThread_.joinable()) {
        videoThread_.join();
    }
    
    if (conn_id_ != 0) {
        agora_rtc_leave_channel(conn_id_);
    }
    
    if (conn_id_ != 0) {
        agora_rtc_destroy_connection(conn_id_);
        conn_id_ = 0;
    }
    
    agora_rtc_fini();
    
    state_ = State::IDLE;
    std::cout << "RTC streaming stopped" << std::endl;
}

int RtcStreamer::Refresh(const std::string& newToken) {
    if (state_ != State::RUNNING) {
        std::cerr << "Cannot refresh token - streamer is not running" << std::endl;
        return -1;
    }
    
    token_ = newToken;
    int rval = agora_rtc_renew_token(conn_id_, token_.c_str());
    
    if (rval < 0) {
        std::cerr << "Failed to refresh token: " << agora_rtc_err_2_str(rval) << std::endl;
    } else {
        std::cout << "Token refreshed successfully" << std::endl;
    }
    
    return rval;
}

video_frame_type_e RtcStreamer::parseFrameType(const unsigned char* data, int length) {
    if (length < 5) return VIDEO_FRAME_DELTA;
    
    int start = 0;
    while (start < length - 4) {
        if (data[start] == 0x00 && data[start+1] == 0x00 && 
            data[start+2] == 0x00 && data[start+3] == 0x01) {
            start += 4;
            break;
        }
        if (data[start] == 0x00 && data[start+1] == 0x00 && 
            data[start+2] == 0x01) {
            start += 3;
            break;
        }
        start++;
    }
    
    if (start >= length) return VIDEO_FRAME_DELTA;
    
    // 解析NAL单元类型
    int nal_type = data[start] & 0x1F;
    
    // IDR帧是关键帧
    if (nal_type == 5) {
        return VIDEO_FRAME_KEY;
    }
    
    // SPS/PPS也需要作为关键帧处理
    if (nal_type == 7 || nal_type == 8) {
        return VIDEO_FRAME_KEY;
    }
    
    return VIDEO_FRAME_DELTA;
}

// 视频发送线程
void RtcStreamer::videoSendThread() {
    const int frame_interval_us = 1000000 / video_frame_rate_; // 每帧间隔时间(微秒)
    
    unsigned char* frame_buffer = nullptr;
    int frame_length = 0;
    
    // 初始分配缓冲区
    frame_buffer = new unsigned char[1024 * 1024]; // 1MB初始缓冲区
    frame_length = 1024 * 1024;
    
    uint64_t start_ts = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    uint64_t frame_count = 0;
    
    // 视频帧信息结构体（根据新API要求）
    video_frame_info_t video_info;
    memset(&video_info, 0, sizeof(video_info));
    
    // 设置不变的视频参数
    video_info.data_type = VIDEO_DATA_TYPE_H264;
    video_info.stream_type = VIDEO_STREAM_HIGH;
    video_info.frame_rate = 25;
    video_info.rotation = VIDEO_ORIENTATION_0;

    
    while (!stopRequested_) {
        if (state_ == State::PAUSE) {
          std::this_thread::sleep_for(std::chrono::milliseconds(1000));
          continue;
        }
        auto frame_start_time = std::chrono::steady_clock::now();
        
        // 获取H264帧数据
        if (!frameCallback_(frame_buffer, &frame_length)) {
            std::cerr << "Failed to get frame data" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }
        
        if (frame_length <= 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        
        // 解析帧类型
        video_info.frame_type = parseFrameType(frame_buffer, frame_length);
        
        int rval = agora_rtc_send_video_data(conn_id_, 
                                            frame_buffer, 
                                            frame_length, 
                                            &video_info);
        
        if (rval < 0) {
            std::cerr << "Failed to send video data: " << agora_rtc_err_2_str(rval) << std::endl;
            
            // 如果连接断开，尝试重新连接
            if (rval == ERR_NOT_EXIST_CONNECTION)
            {
                std::cerr << "Connection disconnected, attempting to rejoin..." << std::endl;
                // 这里可以添加重连逻辑
            }
        }
        
        // 控制帧率
        auto frame_end_time = std::chrono::steady_clock::now();
        auto frame_duration = std::chrono::duration_cast<std::chrono::microseconds>(
            frame_end_time - frame_start_time).count();
        
        int sleep_time = frame_interval_us - static_cast<int>(frame_duration);
        if (sleep_time > 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(sleep_time));
        } else if (sleep_time < -10000) { // 超过10ms延迟
            std::cerr << "Frame processing delay: " << -sleep_time << "us" << std::endl;
        }
    }
    
    // 清理缓冲区
    if (frame_buffer) {
        delete[] frame_buffer;
    }
}
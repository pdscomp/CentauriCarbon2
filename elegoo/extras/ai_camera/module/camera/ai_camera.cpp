/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2024-11-05 11:50:31
 * @LastEditors  : Ben
 * @LastEditTime : 2025-09-09 20:59:22
 * @Description  :
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

 #include <random>
#include "ai_camera.h"
#include "uvc_camera_without_uart.h"
#include "camera_execption.h"
#include "spdlog/spdlog.h"
#include "hv/Event.h"
#include "video_info.h"
#include "comm_protocol.h"
#include "obico_client.h"
#include "video_encoder.h"

namespace znp {
template <typename CameraT>
AICamera<CameraT>::AICamera(hv::EventLoop *loop)
    : loop_(loop) {
    try {
        camera_ = std::make_shared<CameraT>();
    #if REAL_TIME_SAVE_VIDEO
        video_converter_ = std::make_shared<VideoFormatConvert>();
    #else
        time_lapse_video_ = std::make_shared<TimeLapseVideo>(TIME_LAPSE_PIC_PATH, TIME_LAPSE_VIDEO_PATH);
    #endif
      } catch (const CameraException &e) {
          SPDLOG_ERROR(e.what());
    }

    if (CommonTools::isWLAN0Connect()) {
        video_stream_ip_ = CommonTools::getWLAN0IPAddress();
    }
    pic_buf_ = new unsigned char[PIC_MAX_SIZE];
    video_streamer_ = std::make_shared<VideoStreamer>(
                [this](unsigned char *frame, int *size) {
                    if (this->camera_) {
                        return this->camera_->GetFrameOfMjpeg(frame, size);
                    }
              });

    rtc_streamer_ = std::make_shared<RtcStreamer>(
              [this](unsigned char *frame, int *size) {
                if (this->camera_) {
                  return this->camera_->GetFrameOfH264(frame, size);
                }
              });

}
template <typename CameraT>
AICamera<CameraT>::~AICamera() {
    SPDLOG_INFO("time_id: {}", time_id);
    loop_->killTimer(time_id);
    if (pic_buf_) {
        delete []pic_buf_;
    }
    if (camera_) {
        camera_.reset();
    }
    #if REAL_TIME_SAVE_VIDEO
        if (video_converter_) {
          video_converter_.reset();
        }
    #else
        if (time_lapse_video_) {
            time_lapse_video_.reset();
        }
    #endif
        if (video_streamer_) {
            video_streamer_.reset();
        }
}

template <typename CameraT>
void AICamera<CameraT>::Start()
{
    try
    {
        SPDLOG_INFO("AICamera 打开");
        if (this->loop_)
        {
            time_id = this->loop_->setInterval(
                timer_delay_ms_,
                std::bind(&AICamera::TimerHandle, this, std::placeholders::_1));
                SPDLOG_INFO("time_id: {}", time_id);
        }
        if (camera_) {
            camera_->Start();
        }
        if (video_streamer_) {
            video_streamer_->start();
        }
        #if 0   // rtc声网推流测试 -> token每24小时需更换
            std::string appid = "d035320941e34cd5bc4ec106eff05581";
            std::string license = "019836C773D07642BD402F0BE1578C9B";
            std::string token = "007eJxTYNi3oOr3mwSe6++cXlnVPvvY9GGNib1d+uP5k5pk5vE85ihSYEgxMDY1NjKwNDFMNTZJTjFNSjZJTTY0MEtNSzMwNbUw2LtoQkZDICPDe7FnnowMEAjiCzA4OjkbIAEjBgYAu/4j1w==";
            std::string channel = "ABC0000000000002";
            std::string agora_pid = "01975EDB67427D84908B8C5C628C81CF";
            json j_respond;

            SPDLOG_INFO("new appid: {} license: {} token: {} channel: {} agora_pid: {}", appid, license, token, channel, agora_pid);
            RtcMonitorStart(j_respond, appid, license, token, channel, agora_pid);
        #endif

    } catch (const CameraException &e) {
      SPDLOG_ERROR(e.what());
    }
}

template <typename CameraT>
void AICamera<CameraT>::Stop() {
    SPDLOG_INFO("AICamera stop");
    if (camera_) {
        camera_->Stop();
    }
}

template <typename CameraT>
void AICamera<CameraT>::TimerHandle(hv::TimerID id) {
    std::string wlan0_ip;
    if (CommonTools::isWLAN0Connect()) {
        wlan0_ip = CommonTools::getWLAN0IPAddress();
    }
    else {
        wlan0_ip.clear();
    }
    // SPDLOG_INFO("viode_stream_ip_: {}, wlan0_ip: {} ", video_stream_ip_, wlan0_ip);
    if (video_stream_ip_ != wlan0_ip) {
        video_stream_ip_ = wlan0_ip;
        if (this->video_streamer_ && video_stream_ip_.length() > 4) {
            this->video_streamer_->restart();
        }
    }
    else if (!this->video_streamer_->isAlive() && video_stream_ip_.length() > 4 ) {
        this->video_streamer_->restart();
    }

}

template <typename CameraT>
void AICamera<CameraT>::RegisterRespondFun(RespondMsg respond_msg)
{
  if (respond_msg != nullptr) {
    this->respond_msg_ = respond_msg;
  }
}

/*********************************************************** CmdHandle  *********************************************************************************/
template <typename CameraT>
void AICamera<CameraT>::CmdHandle(json &j_requset, json &j_respond)
{
    if (!j_requset.contains("method"))
    {
        JsonParseInvalidRequest(j_respond);
        return;
    }

    if (j_requset["method"] == "time_lapse")
    {
        if (!j_requset.contains("params")) {
            JsonParseInvalidRequest(j_respond);
        }
        else {
            TimeLapseControl(j_requset["params"], j_respond);
        }
    }
    else if (j_requset["method"] == "composite_video")
    {
        if (!j_requset.contains("params")) {
            JsonParseInvalidRequest(j_respond);
        }
        else {
            TimeLapseComposite(j_requset["params"], j_respond);
        }
    }
    else if (j_requset["method"] == "ai_detection")
    {
        AiDetection(j_requset, j_respond);
    }

    else if (j_requset["method"] == "video_monitor")
    {
        if (!j_requset.contains("params")) {
            JsonParseInvalidRequest(j_respond);
        }
        else {
            RtcMonitor(j_requset["params"], j_respond);
        }
    }

}

/*********************************************************** TimeLapse  *********************************************************************************/

template <typename CameraT>
void AICamera<CameraT>::TimeLapseControl(json &params, json &j_respond) {
    if (!this->camera_) {
        JspnParseAbnormal(j_respond);
        return;
    }

    if (!params.contains("status")) {
        JsonParseInvalidRequest(j_respond);
        return;
    }

    if (params["status"] == "start")
    {
        j_respond["status"] = "start";
        if (!params.contains("mode") || !params.contains("filename") || !params.contains("total_frames")) {
          JsonParseInvalidRequest(j_respond);
        }
        else
        {
            std::string module_name = params["filename"].get<std::string>();
            int total_frames = params["total_frames"].get<int>();

            std::string output_video = TIME_LAPSE_VIDEO_PATH + module_name + ".mp4";
            #if REAL_TIME_SAVE_VIDEO
                video_converter_->clearFile(output_video);

                if (total_frames > 30) {
                  TimeLapseStart(j_respond, module_name, total_frames);
                }
            #else
                if (params["mode"] == "begin") {
                    if (time_lapse_video_) {
                        TimeLapseStart(j_respond, module_name, total_frames, false);
                    }
                }
                if (params["mode"] == "continue") {
                    if (time_lapse_video_) {
                        TimeLapseStart(j_respond, module_name, total_frames, true);
                    }
                }
            #endif
        }
    }
    else if (params["status"] == "capture")
    {
        if (!params.contains("index")) {
            JsonParseInvalidRequest(j_respond);
        }
        int index = params["index"].get<int>();
        std::lock_guard<std::mutex> lock(this->jpeg_write_mutex_);
        TimeLapseSavePicture(j_respond, index);
    }
    else if (params["status"] == "stop")
    {
        TimeLapseStop(j_respond);
    }
    else
    {
        JsonParseInvalidRequest(j_respond);
        return;
    }
}

template <typename CameraT>
void AICamera<CameraT>::TimeLapseStart(json &j_respond, std::string module_name, int t_frame, bool is_continue) {
    if (this->camera_ && this->aiCameraInfo_.isTimelapsePhotographyActive_ == false)
    {
        this->aiCameraInfo_.isTimelapsePhotographyActive_ = true;
        std::string output_video = TIME_LAPSE_VIDEO_PATH + module_name;
        #if REAL_TIME_SAVE_VIDEO
            if (video_converter_) {
                video_converter_->createMP4(output_video, CAMERA_FARME_WIDTH, CAMERA_FARME_HIGHT, VIDEO_FPS, t_frame);
            }
        #else
            if (time_lapse_video_) {
                if (is_continue) {
                    time_lapse_video_->Continue(module_name, CAMERA_FARME_WIDTH, CAMERA_FARME_HIGHT, VIDEO_FPS, t_frame);
                }
                else {
                    time_lapse_video_->Start(module_name, CAMERA_FARME_WIDTH, CAMERA_FARME_HIGHT, VIDEO_FPS, t_frame);
                }
            }
        #endif
        MessageCmdSuccess(j_respond, "TimeLapseStart");
    }
    else {
        MessageCmdFailed(j_respond, "TimeLapseStart", "NO Camera or isTimelapsePhotographyActive_ is running!!!");
    }


}
template <typename CameraT>
void AICamera<CameraT>::TimeLapseSavePicture(json &j_respond, int index) {
    if (!this->camera_ || aiCameraInfo_.isTimelapsePhotographyActive_ == false) {
        MessageCmdFailed(j_respond, "Capture", "NO Camera or isTimelapsePhotographyActive_ is not Start!!!");
        return;
    }
    int length = 0;
    bool get_frame = false;
    do {
        get_frame = this->camera_->GetFrameOfMjpeg(this->pic_buf_, &length);
    }while (!get_frame);

    if (get_frame)
    {
        try {
            #if REAL_TIME_SAVE_VIDEO
              video_converter_->addSingleFrame(pic_buf_, length);
            #else
              time_lapse_video_->Capture(index, this->pic_buf_, length);
            #endif
            MessageCmdSuccess(j_respond, "Capture");
        } catch (const std::exception &e) {
            SPDLOG_ERROR("Error: {}", e.what());
            MessageCmdFailed(j_respond, "Capture", e.what());
        }
    } else {
          MessageCmdFailed(j_respond, "Capture", "GetFrameOfMjpeg failed!!!");
    }
}
template <typename CameraT>
void AICamera<CameraT>::TimeLapseStop(json &j_respond) {
    if (this->camera_ && aiCameraInfo_.isTimelapsePhotographyActive_) {
        this->aiCameraInfo_.isTimelapsePhotographyActive_ = false;
        #if REAL_TIME_SAVE_VIDEO
            if (video_converter_)
            {
              video_converter_->renameFile();
              video_converter_->close();
              MessageCmdSuccess(j_respond);
            }
            else {
              MessageCmdFailed(j_respond);
            }
        #else
            if (time_lapse_video_->Close()) {
                j_respond["result"] = "ok";
                j_respond["status"] = "TimeLapseStop";
                j_respond["video_name"] = time_lapse_video_->GetPicName();
                j_respond["video_path"] = time_lapse_video_->GetPicPath();
                j_respond["video_duration"] = time_lapse_video_->GetVideoDuration();
            }
            else {
                MessageCmdFailed(j_respond, "TimeLapseStop", "End halfway without saving the final result!!!");
            }
        #endif
    }
    else {
        MessageCmdFailed(j_respond, "TimeLapseStop", "NO Camera or isTimelapsePhotographyActive_ is not Start!!!");
    }
}

/*********************************************************** TimeLapseComposite  *********************************************************************************/
template <typename CameraT>
void AICamera<CameraT>::TimeLapseComposite(json &params, json &j_respond) {
    if (this->camera_)
    {
        if (!params.contains("video_name"))
        {
            JsonParseInvalidRequest(j_respond);
            return;
        }
        uint32_t video_size= 0;
        std::string video_dir = params["video_name"];
        std::string video_name = video_dir + ".mp4";
        json camera_status;
        std::string json_string;
        MessageCompositeVideoStart(camera_status, video_name);
        json_string = MessageParseString(camera_status);
        this->respond_msg_(json_string);
        SPDLOG_INFO("Composite video start");

        #if 0
            if (time_lapse_video_->Composite(video_dir, CAMERA_FARME_WIDTH, CAMERA_FARME_HIGHT, VIDEO_FPS) )
            {
            // time_lapse_video_->ClearPicture(video_dir);  // 清除缓存图片
            video_size = time_lapse_video_->GetVideoSize();
            std::string video_path = time_lapse_video_->GetVideoPath();
            MessageCompositeVideoFinish(camera_status, video_name, video_path, video_size);
            json_string = MessageParseString(camera_status);
            this->respond_msg_(json_string);
            SPDLOG_INFO("Composite video finish");
            MessageCmdSuccess(j_respond);
            }
            else
            {
            MessageCompositeVideoFailed(camera_status, video_name);
            json_string = MessageParseString(camera_status);
            this->respond_msg_(json_string);
            SPDLOG_INFO("Composite video failed");
            MessageCmdFailed(j_respond);
            }
        #else
                std::string pic_dir = TIME_LAPSE_PIC_PATH + video_dir;
                std::string pic_dir_link = "/tmp/pic_link";
                std::string cmd = "ln -s \'" + pic_dir + "\'" + " " + pic_dir_link;
                SPDLOG_INFO("cmd {}", cmd);
                std::system(cmd.c_str());

                std::string video_path_ = TIME_LAPSE_VIDEO_PATH + video_name;

                std::shared_ptr<ImageToVideoEncoder> encoder = std::make_shared<ImageToVideoEncoder>();
                encoder->set_progress_callback(
                        [this](double progress, bool& cancel) {
                            json camera_status;
                            std::string json_string;
                            MessageCompositeVideoProcess(camera_status, progress);
                            json_string = MessageParseString(camera_status);
                            this->respond_msg_(json_string);
                            SPDLOG_INFO("Composite video progress [{}] ", progress);
                        });
                if (encoder->encode_from_directory(pic_dir_link, video_path_, CAMERA_FARME_WIDTH, CAMERA_FARME_HIGHT, VIDEO_FPS)) {
                    SPDLOG_INFO("✅ Video saved to {}", video_path_);
                    video_size = CommonTools::getFileSize(video_path_);
                    MessageCompositeVideoFinish(camera_status, video_name, video_path_, video_size);
                    json_string = MessageParseString(camera_status);
                    this->respond_msg_(json_string);
                    SPDLOG_INFO("Composite video finish");
                    MessageCmdSuccess(j_respond, "Composite_video");

                } else {
                    SPDLOG_INFO("❌ Encoding failed.");
                    MessageCompositeVideoFailed(camera_status, video_name);
                    json_string = MessageParseString(camera_status);
                    this->respond_msg_(json_string);
                    SPDLOG_INFO("Composite video failed");
                    MessageCmdFailed(j_respond, "Composite_video", "Composite video failed");
                }

                encoder.reset();

                cmd = "rm -rf " + pic_dir_link;
                SPDLOG_INFO("cmd {}", cmd);
                std::system(cmd.c_str());

                cmd = "rm -rf \'" + pic_dir + "\'";
                SPDLOG_INFO("cmd {}", cmd);
                std::system(cmd.c_str());
            }
        #endif
    else
    {
        MessageCmdFailed(j_respond, "Composite_video", "NO Camera!!!");
    }
}

/*********************************************************** AiDetection  *********************************************************************************/
template <typename CameraT>
void AICamera<CameraT>::AiDetection(json &params, json &j_respond) {
    if (this->camera_)
    {
        std::string token = "elegoo_9YpRu2XaTb8MLZod3KfCVeHWq0rTxEbnYp5WgAjLtBDRUZmqVNXYuHf6OaZ3rwsUcHnPAytcqE4xKmUZp1o6dQsM8RJgv7NYtEl4m5yAX3LFWo2";
        std::string printer_id = "test00000001";
        std::string print_id = "camera_1";
        int length = 0;
        bool get_frame = false;
        do {
            get_frame = this->camera_->GetFrameOfMjpeg(this->pic_buf_, &length);
        }while (!get_frame);

        if(SuperDetectionTest(token, printer_id, print_id, pic_buf_, length, j_respond) == 0)
        {
            MessageCmdSuccess(j_respond, "AiDetection");
        }
        else
        {
            MessageCmdFailed(j_respond, "AiDetection", "SuperDetectionTest failed!!!");
        }
    }
    else
    {
        MessageCmdFailed(j_respond, "AiDetection", "NO Camera!!!");
    }
}


/*********************************************************** RtcMonitor  *********************************************************************************/
template <typename CameraT>
void AICamera<CameraT>::RtcMonitor(json &params, json &j_respond) {
    if (this->camera_ && this->rtc_streamer_)
    {
        std::string appid = "d035320941e34cd5bc4ec106eff05581";
        std::string license = "019836C773D07642BD402F0BE1578C9B";
        std::string token = "007eJxTYNi3oOr3mwSe6++cXlnVPvvY9GGNib1d+uP5k5pk5vE85ihSYEgxMDY1NjKwNDFMNTZJTjFNSjZJTTY0MEtNSzMwNbUw2LtoQkZDICPDe7FnnowMEAjiCzA4OjkbIAEjBgYAu/4j1w==";
        std::string channel = "ABC0000000000002";
        std::string agora_pid = "01975EDB67427D84908B8C5C628C81CF";
        int uid = 0;

        if (!params.contains("status"))
        {
            JsonParseInvalidRequest(j_respond);
            return;
        }

        if (params["status"] == "start")
        {
            if (!params.contains("appid") || !params.contains("license") || !params.contains("token") || !params.contains("channel") || !params.contains("uid"))
            {
                JsonParseInvalidRequest(j_respond);
                return;
            }
            else
            {
                this->camera_->StartH264Stream();
                sleep(1);
                SPDLOG_INFO("default: appid: {} license: {} token: {} channel: {} uid: {}", appid, license, token, channel, uid);

                appid = params["appid"];
                license = params["license"];
                token = params["token"];
                channel = params["channel"];
                uid = params["uid"];
                // agora_pid = params["agora_pid"];

                SPDLOG_INFO("new appid: {} license: {} token: {} channel: {} uid: {}", appid, license, token, channel, uid);

                RtcMonitorStart(j_respond, appid, license, token, channel, uid);
                MessageCmdSuccess(j_respond, "RtcMonitorStart");
            }
        }
        else if (params["status"] == "refresh")
        {
            if (!params.contains("token"))
            {
                JsonParseInvalidRequest(j_respond);
                return;
            }
            token = params["token"];
            RtcMonitorRefresh(j_respond, token);
            MessageCmdSuccess(j_respond, "RtcMonitorRefresh");
        }
        else if (params["status"] == "stop")
        {
            this->camera_->StopH264Stream();
            RtcMonitorStop(j_respond);
            MessageCmdSuccess(j_respond, "RtcMonitorStop");
        }
        else if (params["status"] == "pause")
        {
            RtcMonitorPause(j_respond);
            MessageCmdSuccess(j_respond, "RtcMonitorPause");
        }
        else if (params["status"] == "resume")
        {
            RtcMonitorResume(j_respond);
            MessageCmdSuccess(j_respond, "RtcMonitorResume");
        }

        else
        {
            JsonParseInvalidRequest(j_respond);
            return;
        }
    }
    else
    {
        MessageCmdFailed(j_respond, "RtcMonitor", "NO Camera!!!");
    }
}


template <typename CameraT>
void AICamera<CameraT>::RtcMonitorStart(json &j_respond,
                      const std::string& appid,
                      const std::string& license,
                      const std::string& token,
                      const std::string& channel,
                      int uid) {
    if (this->camera_ && rtc_streamer_)
    {
        if (rtc_streamer_->Start(appid, license, token, channel, uid))
        {
          MessageCmdFailed(j_respond, "RtcMonitorStart", "rtc_streamer_ start failed!!!");
        }
        else
        {
          MessageCmdSuccess(j_respond, "RtcMonitorStart");
        }
    }
    else
    {
        MessageCmdFailed(j_respond, "RtcMonitorStart", "camera or rtc_streamer_ not running!!!");
    }
}

template <typename CameraT>
void AICamera<CameraT>::RtcMonitorRefresh(json &j_respond, const std::string &new_token) {
    if (this->camera_ && rtc_streamer_)
    {
        if (rtc_streamer_->Refresh(new_token))
        {
            MessageCmdFailed(j_respond, "RtcMonitorRefresh", "rtc_streamer_ Refresh failed!!!");
        }
        else
        {
            MessageCmdSuccess(j_respond, "RtcMonitorRefresh");
        }
    }
    else
    {
        MessageCmdFailed(j_respond, "RtcMonitorRefresh", "camera or rtc_streamer_ not running!!!");
    }
}


template <typename CameraT>
void AICamera<CameraT>::RtcMonitorStop(json &j_respond) {
    if (this->camera_ && rtc_streamer_)
    {
        rtc_streamer_->Stop();
        MessageCmdSuccess(j_respond, "RtcMonitorStop");
    }
    else
    {
        MessageCmdFailed(j_respond, "RtcMonitorStop", "camera or rtc_streamer_ not running!!!");
    }
}

template <typename CameraT>
void AICamera<CameraT>::RtcMonitorPause(json &j_respond) {
    if (this->camera_ && rtc_streamer_)
    {
        rtc_streamer_->Pause();
        MessageCmdSuccess(j_respond, "RtcMonitorPause");
    }
    else
    {
        MessageCmdFailed(j_respond, "RtcMonitorPause", "camera or rtc_streamer_ not running!!!");
    }
}

template <typename CameraT>
void AICamera<CameraT>::RtcMonitorResume(json &j_respond) {
    if (this->camera_ && rtc_streamer_)
    {
        rtc_streamer_->Resume();
        MessageCmdSuccess(j_respond, "RtcMonitorResume");
    }
    else
    {
        MessageCmdFailed(j_respond, "RtcMonitorResume", "camera or rtc_streamer_ not running!!!");
    }
}

// 显式实例化
template class AICamera<UvcCamera>;
}
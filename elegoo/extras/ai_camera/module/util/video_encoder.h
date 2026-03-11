/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2025-08-28 11:59:08
 * @LastEditors  : Jack
 * @LastEditTime : 2025-08-28 15:25:01
 * @Description  : 
 * 
 * Copyright (c) 2025 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/
#pragma once

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

#include <string>
#include <vector>
#include <functional>

class ImageToVideoEncoder {
public:
    using ProgressCallback = std::function<void(double progress, bool& cancel)>;

    ImageToVideoEncoder();
    ~ImageToVideoEncoder();

    void set_progress_callback(ProgressCallback cb);
    bool encode_from_directory(
        const std::string& pic_dir_path,
        const std::string& video_path,
        int width,
        int height,
        int fps
    );
    void cancel();

private:
    bool load_and_encode_frame(const std::string& filename);
    bool encode_frame(void* frame);
    void cleanup();
    std::vector<std::string> collect_sorted_images(const std::string& dir_path);
    int extract_number_from_filename(const std::string& filename);

private:
    std::string output_file_;
    std::string temp_output_file_;
    int width_, height_, fps_;
    int total_frames_;
    struct FFPointers {
        AVFormatContext* fmt_ctx = nullptr;
        AVStream* stream = nullptr;
        AVCodecContext* codec_ctx = nullptr;
        const AVCodec* codec = nullptr;
        SwsContext* sws_ctx = nullptr;
        AVFrame* frame = nullptr;
        AVPacket* pkt = nullptr;
    } *ptrs_;
    ProgressCallback progress_callback_;
    bool running_;
    int frame_count_;
};

/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2025-08-28 11:58:55
 * @LastEditors  : Jack
 * @LastEditTime : 2025-08-28 17:15:35
 * @Description  : 
 * 
 * Copyright (c) 2025 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

// video_encoder.cpp
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

#include <iostream>
#include <vector>
#include <string>
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>

#include "video_encoder.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

ImageToVideoEncoder::ImageToVideoEncoder()
    : ptrs_(new FFPointers()), running_(true), frame_count_(0) {
    avformat_network_init();
}

ImageToVideoEncoder::~ImageToVideoEncoder() {
    cleanup();
    avformat_network_deinit();
    delete ptrs_;
}

void ImageToVideoEncoder::set_progress_callback(ProgressCallback cb) {
    progress_callback_ = cb;
}

void ImageToVideoEncoder::cancel() {
    running_ = false;
}

bool ImageToVideoEncoder::encode_from_directory(
    const std::string& pic_dir_path,
    const std::string& video_path,
    int width,
    int height,
    int fps
) {
    output_file_ = video_path;
    temp_output_file_ = video_path + ".temp";
    width_ = width;
    height_ = height;
    fps_ = fps;

    std::cout << "pic_dir_path: " << pic_dir_path << " video_path:" << video_path <<  std::endl;
    auto images = collect_sorted_images(pic_dir_path);
    if (images.empty()) {
        std::cerr << "No images found in directory: " << pic_dir_path << "\n";
        return false;
    }

    total_frames_ = images.size();

    const char* format_name = "mp4";
    AVFormatContext* fmt_ctx = nullptr;

    if (avformat_alloc_output_context2(&fmt_ctx, nullptr, format_name, temp_output_file_.c_str()) < 0) {
        std::cerr << "Failed to allocate output context." << std::endl;
        return false;
    }
    ptrs_->fmt_ctx = fmt_ctx;

    ptrs_->codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!ptrs_->codec) {
        std::cerr << "H.264 codec not found\n";
        return false;
    }

    ptrs_->stream = avformat_new_stream(ptrs_->fmt_ctx, ptrs_->codec);
    if (!ptrs_->stream) {
        std::cerr << "Could not create stream\n";
        return false;
    }
    ptrs_->stream->time_base = {1, fps_};

    ptrs_->codec_ctx = avcodec_alloc_context3(ptrs_->codec);
#if 0
    ptrs_->codec_ctx->width = width_;
    ptrs_->codec_ctx->height = height_;
    ptrs_->codec_ctx->time_base = {1, fps_};
    ptrs_->codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    ptrs_->codec_ctx->bit_rate = 2000000;
    ptrs_->codec_ctx->gop_size = 30;
    ptrs_->codec_ctx->max_b_frames = 3;
    printf("line:%d \n", __LINE__);
    av_opt_set(ptrs_->codec_ctx->priv_data, "preset", "medium", 0);
    av_opt_set(ptrs_->codec_ctx->priv_data, "tune", "film", 0);
#else
    ptrs_->codec_ctx->width = width_;
    ptrs_->codec_ctx->height = height_;
    ptrs_->codec_ctx->time_base = {1, fps_};
    ptrs_->codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    ptrs_->codec_ctx->bit_rate = 2000000;
    ptrs_->codec_ctx->gop_size = 30;
    ptrs_->codec_ctx->max_b_frames = 3;

    av_opt_set(ptrs_->codec_ctx->priv_data, "preset", "ultrafast", 0);
    av_opt_set(ptrs_->codec_ctx->priv_data, "tune", "stillimage", 0);
    av_opt_set(ptrs_->codec_ctx->priv_data, "profile", "baseline", 0);
    av_opt_set(ptrs_->codec_ctx->priv_data, "level", "3.0", 0);
    av_opt_set(ptrs_->codec_ctx->priv_data, "speed", "10", 0);
    av_opt_set(ptrs_->codec_ctx->priv_data, "keyint_min", "60", 0);
    av_opt_set(ptrs_->codec_ctx->priv_data, "sc_threshold", "0", 0);
#endif

    if (avcodec_open2(ptrs_->codec_ctx, ptrs_->codec, nullptr) < 0) {
        std::cerr << "Could not open codec\n";
        return false;
    }
    if (avcodec_parameters_from_context(ptrs_->stream->codecpar, ptrs_->codec_ctx) < 0) {
        std::cerr << "Could not copy codec parameters\n";
        return false;
    }

    if (!(ptrs_->fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&ptrs_->fmt_ctx->pb, temp_output_file_.c_str(), AVIO_FLAG_WRITE) < 0) {
            std::cerr << "Could not open output file\n";
            return false;
        }
    }
    if (avformat_write_header(ptrs_->fmt_ctx, nullptr) < 0) {
        std::cerr << "Error writing header\n";
        return false;
    }

    ptrs_->frame = av_frame_alloc();
    ptrs_->frame->width = width_;
    ptrs_->frame->height = height_;
    ptrs_->frame->format = AV_PIX_FMT_YUV420P;
    av_frame_get_buffer(ptrs_->frame, 32);

    ptrs_->sws_ctx = sws_getContext(
        width_, height_, AV_PIX_FMT_RGB24,
        width_, height_, AV_PIX_FMT_YUV420P,
        SWS_BICUBIC, nullptr, nullptr, nullptr);

    ptrs_->pkt = av_packet_alloc();

    for (int i = 0; i < total_frames_ && running_; ++i) {
        if (!load_and_encode_frame(images[i])) {
            break;
        }

        frame_count_++;
        double progress = (double)frame_count_ / total_frames_ * 100.0;

        if (progress_callback_) {
            progress_callback_(progress, running_);
        }
        printf("\rEncoding: %.1f%% (%d/%d)   ", progress, frame_count_, total_frames_);
        fflush(stdout);
    }
    if (!running_) {
        printf("\nEncoding cancelled.\n");
        std::remove(temp_output_file_.c_str());
        return false;
    } else {
        printf("\nEncoding completed.\n");
    }

    encode_frame(nullptr);
    av_write_trailer(ptrs_->fmt_ctx);

    if (std::rename(temp_output_file_.c_str(), output_file_.c_str()) != 0) {
        std::cerr << "Failed to rename temp file to final file.\n";
        std::remove(temp_output_file_.c_str());
        return false;
    }

    return true;
}

std::vector<std::string> ImageToVideoEncoder::collect_sorted_images(const std::string& dir_path) {
    std::vector<std::string> files;
    DIR* dir = opendir(dir_path.c_str());
    if (!dir) {
        std::cerr << "Cannot open directory: " << dir_path << "\n";
        return {};
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name.size() >= 5 && (name.substr(name.size()-4) == ".jpg" || name.substr(name.size()-5) == ".jpeg")) {
            files.push_back(dir_path + "/" + name);
        }
    }
    closedir(dir);

    std::sort(files.begin(), files.end(), [&](const std::string& a, const std::string& b) {
        return extract_number_from_filename(a) < extract_number_from_filename(b);
    });

    return files;
}

int ImageToVideoEncoder::extract_number_from_filename(const std::string& filename) {
    std::string base = filename;
    size_t dot_pos = base.rfind('.');
    if (dot_pos != std::string::npos) base = base.substr(0, dot_pos);
    size_t num_start = base.find_first_of("0123456789");
    if (num_start == std::string::npos) return -1;

    std::string num_str = base.substr(num_start);
    for (char c : num_str) {
        if (!std::isdigit(c)) return -1;
    }

    return std::stoi(num_str);
}

bool ImageToVideoEncoder::load_and_encode_frame(const std::string& filename) {
    int w, h, channels;
    uint8_t* rgb_data = stbi_load(filename.c_str(), &w, &h, &channels, 3);
    if (!rgb_data || w != width_ || h != height_) {
        std::cerr << "Failed to load image: " << filename << "\n";
        stbi_image_free(rgb_data);
        return false;
    }

    AVFrame* rgb_frame = av_frame_alloc();
    rgb_frame->width = width_;
    rgb_frame->height = height_;
    rgb_frame->format = AV_PIX_FMT_RGB24;
    av_frame_get_buffer(rgb_frame, 32);

    int stride = width_ * 3;
    for (int y = 0; y < height_; ++y) {
        memcpy(rgb_frame->data[0] + y * rgb_frame->linesize[0],
               rgb_data + y * stride, stride);
    }

    sws_scale(ptrs_->sws_ctx,
              (const uint8_t* const*)rgb_frame->data,
              rgb_frame->linesize, 0, height_,
              ptrs_->frame->data, ptrs_->frame->linesize);

    av_frame_free(&rgb_frame);
    stbi_image_free(rgb_data);

    ptrs_->frame->pts = frame_count_;

    return encode_frame(ptrs_->frame);
}

bool ImageToVideoEncoder::encode_frame(void* frame) {
    AVFrame* avf = static_cast<AVFrame*>(frame);

    if (avcodec_send_frame(ptrs_->codec_ctx, avf) < 0) {
        return false;
    }

    while (avcodec_receive_packet(ptrs_->codec_ctx, ptrs_->pkt) == 0) {
        ptrs_->pkt->stream_index = ptrs_->stream->index;
        av_packet_rescale_ts(ptrs_->pkt, ptrs_->codec_ctx->time_base, ptrs_->stream->time_base);
        if (av_interleaved_write_frame(ptrs_->fmt_ctx, ptrs_->pkt) < 0) {
            return false;
        }
        av_packet_unref(ptrs_->pkt);
    }
    return true;
}

void ImageToVideoEncoder::cleanup() {
    if (ptrs_->frame) {
        av_frame_free(&ptrs_->frame);
    }

    if (ptrs_->codec_ctx) {
        avcodec_send_frame(ptrs_->codec_ctx, nullptr);
        avcodec_free_context(&ptrs_->codec_ctx);
    }

    if (ptrs_->sws_ctx) {
        sws_freeContext(ptrs_->sws_ctx);
    }

    if (ptrs_->pkt) {
        av_packet_free(&ptrs_->pkt);
    }

    if (ptrs_->fmt_ctx) {
        if (ptrs_->fmt_ctx->pb) {
            avio_closep(&ptrs_->fmt_ctx->pb);
        }
        avformat_free_context(ptrs_->fmt_ctx);
    }
}
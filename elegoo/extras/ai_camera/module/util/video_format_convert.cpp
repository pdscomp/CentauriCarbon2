#include "video_format_convert.h"

namespace znp
{

// 错误处理回调函数
METHODDEF(void) jpeg_error_exit(j_common_ptr cinfo) {
    JpegErrorMgr* myerr = (JpegErrorMgr*)cinfo->err;
    (*cinfo->err->output_message)(cinfo);
    longjmp(myerr->setjmp_buffer, 1);
}


// 初始化视频文件（支持断电续打）
void VideoFormatConvert::createMP4(const std::string& file_name, int width, int height, int fps, int total_frame) {
    close();
    
    this->width = width;
    this->height = height;
    this->fps = fps;
    this->total_frames = total_frame;
    this->original_filename = file_name + ".mp4"; // 保存原始文件名
    this->filename = file_name + ".mp4";          // 当前使用的文件名
    next_pts = 0;
    frame_count = 0;
    is_completed = false;

    // 检查文件是否已存在
    file_exists = fileExists(filename);
    bool write_header = !file_exists;

    // 创建输出上下文
    if (avformat_alloc_output_context2(&fmt_ctx, nullptr, nullptr, filename.c_str()) < 0) {
        throw std::runtime_error("Failed to create output context");
    }

    // 查找并配置H.264编码器
    const AVCodec* encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!encoder) {
        throw std::runtime_error("H.264 encoder not found");
    }

    enc_ctx = avcodec_alloc_context3(encoder);
    if (!enc_ctx) {
        throw std::runtime_error("Failed to allocate encoder context");
    }

    // 编码器参数设置
    enc_ctx->codec_id = AV_CODEC_ID_H264;
    enc_ctx->width = width;
    enc_ctx->height = height;
    enc_ctx->time_base = {1, fps};
    enc_ctx->framerate = {fps, 1};
    enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    // enc_ctx->bit_rate = width * height * 3;
    enc_ctx->gop_size = 1;
    enc_ctx->max_b_frames = 0;
    
    // H.264高级参数
    // av_opt_set(enc_ctx->priv_data, "preset", "medium", 0);
    // av_opt_set(enc_ctx->priv_data, "tune", "stillimage", 0);
    // av_opt_set_int(enc_ctx->priv_data, "forced-idr", 1, 0);

    enc_ctx->bit_rate = width * height * 5;  // 增加比特率 (5 bpp)
    enc_ctx->rc_max_rate = width * height * 8; // 最大比特率
    enc_ctx->rc_buffer_size = width * height * 2; // 缓冲区大小

    // H.264 高级参数优化
    av_opt_set(enc_ctx->priv_data, "preset", "slow", 0); // 使用更慢但质量更高的预设
    av_opt_set(enc_ctx->priv_data, "tune", "film", 0); // 使用高质量调优
    av_opt_set_int(enc_ctx->priv_data, "crf", 18, 0); // 设置CRF值 (18-23是高质量范围)
    av_opt_set(enc_ctx->priv_data, "profile", "high", 0); // 使用High Profile
    av_opt_set(enc_ctx->priv_data, "level", "4.1", 0); // 设置编码级别

    // 提高关键帧质量
    av_opt_set_int(enc_ctx->priv_data, "aq-mode", 3, 0); // 自适应量化模式

    // 打开编码器
    if (avcodec_open2(enc_ctx, encoder, nullptr) < 0) {
        throw std::runtime_error("Failed to open encoder");
    }

    // 创建视频流
    stream = avformat_new_stream(fmt_ctx, nullptr);
    if (!stream) {
        throw std::runtime_error("Failed to create video stream");
    }

    if (avcodec_parameters_from_context(stream->codecpar, enc_ctx) < 0) {
        throw std::runtime_error("Failed to copy codec parameters");
    }

    // 打开输出文件
    int flags = file_exists ? AVIO_FLAG_READ_WRITE : AVIO_FLAG_WRITE;
    if (avio_open(&fmt_ctx->pb, filename.c_str(), flags) < 0) {
        throw std::runtime_error("Failed to open output file");
    }

    // 对于新文件写入文件头
    if (write_header) {
        if (avformat_write_header(fmt_ctx, nullptr) < 0) {
            throw std::runtime_error("Failed to write file header");
        }
    } else {
        avio_seek(fmt_ctx->pb, 0, SEEK_END);
        frame_count = recoverFrameCount();
        next_pts = frame_count;
    }

    // 初始化MJPEG解码器
    const AVCodec* decoder = avcodec_find_decoder(AV_CODEC_ID_MJPEG);
    if (!decoder) {
        throw std::runtime_error("MJPEG decoder not found");
    }

    dec_ctx = avcodec_alloc_context3(decoder);
    if (!dec_ctx) {
        throw std::runtime_error("Failed to allocate decoder context");
    }

    dec_ctx->flags |= AV_CODEC_FLAG_LOW_DELAY;
    dec_ctx->flags2 |= AV_CODEC_FLAG2_FAST;
    dec_ctx->thread_count = 1;
    
    if (avcodec_open2(dec_ctx, decoder, nullptr) < 0) {
        throw std::runtime_error("Failed to open MJPEG decoder");
    }

    // 初始化图像转换器
    sws_ctx = sws_getContext(
        width, height, AV_PIX_FMT_YUVJ420P,
        width, height, AV_PIX_FMT_YUV420P,
        SWS_LANCZOS, // SWS_BICUBIC, 
        nullptr, nullptr, nullptr
    );
    if (!sws_ctx) {
        throw std::runtime_error("Failed to create image scaler");
    }

    if (!persistent_jerr) {
        persistent_jerr = new JpegErrorMgr();
        persistent_cinfo = new jpeg_decompress_struct();
        
        persistent_cinfo->err = jpeg_std_error(&persistent_jerr->pub);
        persistent_jerr->pub.error_exit = jpeg_error_exit;
        
        jpeg_create_decompress(persistent_cinfo);
    }

}

bool VideoFormatConvert::decodeJPEG(uint8_t* data, int size, AVFrame** frame) {
    struct jpeg_decompress_struct cinfo;
    JpegErrorMgr jerr;
    
    if (size < 100) { // 最小 JPEG 文件大小
        return false;
    }

    if (data[0] != 0xFF || data[1] != 0xD8) {
        return false;
    }

    // 初始化 JPEG 解压缩对象
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = jpeg_error_exit;
    
    // 设置错误处理跳转点
    if (setjmp(jerr.setjmp_buffer)) {
        jpeg_destroy_decompress(&cinfo);
        return false;
    }
    
    jpeg_create_decompress(&cinfo);
    
    // 设置数据源
    jpeg_mem_src(&cinfo, data, size);
    
    // 读取 JPEG 头信息
    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
        jpeg_destroy_decompress(&cinfo);
        return false;
    }
    
    if (cinfo.image_width == 0 || cinfo.image_height == 0) {
        jpeg_abort_decompress(&cinfo);
        return false;
    }

    // 开始解压缩
    jpeg_start_decompress(&cinfo);
    
    // 检查输出色彩空间
    if (cinfo.out_color_space != JCS_YCbCr && cinfo.out_color_space != JCS_RGB) {
        jpeg_destroy_decompress(&cinfo);
        return false;
    }
    
    // 分配 AVFrame
    *frame = av_frame_alloc();
    if (!*frame) {
        jpeg_destroy_decompress(&cinfo);
        return false;
    }
    
    // 设置帧属性
    // (*frame)->format = (cinfo.out_color_space == JCS_YCbCr) ? 
    //                    AV_PIX_FMT_YUVJ420P : AV_PIX_FMT_RGB24;
    (*frame)->width = cinfo.output_width;
    (*frame)->height = cinfo.output_height;
    
    switch (cinfo.out_color_space) {
        case JCS_GRAYSCALE:
            // 灰度图像处理
            (*frame)->format = AV_PIX_FMT_GRAY8;
            break;
        case JCS_RGB:
            (*frame)->format = AV_PIX_FMT_RGB24;
            break;
        case JCS_YCbCr:
            (*frame)->format = AV_PIX_FMT_YUVJ420P;
            break;
        default:
            // 不支持的格式
            jpeg_destroy_decompress(&cinfo);
            av_frame_free(frame);
            return false;
    }

    if (av_frame_get_buffer(*frame, 32) < 0) {
        av_frame_free(frame);
        jpeg_destroy_decompress(&cinfo);
        return false;
    }
    
    // 逐行读取数据
    int row_stride = cinfo.output_width * cinfo.output_components;
    JSAMPARRAY buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, 
                                                 JPOOL_IMAGE, row_stride, 1);
    
    // 根据颜色空间处理数据
    if (cinfo.out_color_space == JCS_YCbCr) {
        // YUV420P 处理
        uint8_t* y_plane = (*frame)->data[0];
        uint8_t* u_plane = (*frame)->data[1];
        uint8_t* v_plane = (*frame)->data[2];
        int y_stride = (*frame)->linesize[0];
        int uv_stride = (*frame)->linesize[1];
        
        for (int y = 0; y < cinfo.output_height; y++) {
            jpeg_read_scanlines(&cinfo, buffer, 1);
            
            // 复制 Y 分量
            for (int x = 0; x < cinfo.output_width; x++) {
                y_plane[y * y_stride + x] = buffer[0][x * 3];
            }
            
            // 隔行处理 U、V 分量
            if (y % 2 == 0) {
                for (int x = 0; x < cinfo.output_width; x += 2) {
                    if (x/2 < cinfo.output_width/2) {
                        u_plane[(y/2) * uv_stride + x/2] = buffer[0][x * 3 + 1];
                        v_plane[(y/2) * uv_stride + x/2] = buffer[0][x * 3 + 2];
                    }
                }
            }
        }
    } else {
        // RGB 处理
        for (int y = 0; y < cinfo.output_height; y++) {
            jpeg_read_scanlines(&cinfo, buffer, 1);
            memcpy((*frame)->data[0] + y * (*frame)->linesize[0], 
                   buffer[0], 
                   cinfo.output_width * 3);
        }
    }
    
    // 完成解压缩
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    return true;
}

// 添加单帧MJPEG数据并转换为H.264 I帧
void VideoFormatConvert::addSingleFrame(uint8_t *mjpeg_data, int data_len) {
    if (is_completed) {
        throw std::runtime_error("Video already completed");
    }
    
    if (frame_count > total_frames) {
        throw std::runtime_error("Exceeded total frames limit");
    }

     // 使用 libjpeg 解码
    AVFrame* decoded_frame = nullptr;
    if (!decodeJPEG(mjpeg_data, data_len, &decoded_frame)) {
        fprintf(stderr, "Failed to decode JPEG frame with libjpeg\n");
        if (decoded_frame) av_frame_free(&decoded_frame);
        return;
    }
    
    // 检查解码帧尺寸
    if (decoded_frame->width != width || decoded_frame->height != height) {
        fprintf(stderr, "Decoded frame size (%dx%d) doesn't match video size (%dx%d)\n",
                decoded_frame->width, decoded_frame->height, width, height);
        av_frame_free(&decoded_frame);
        return;
    }
    
    // 如果格式不匹配，进行转换
    if (decoded_frame->format != AV_PIX_FMT_YUVJ420P) {
        SwsContext* convert_ctx = sws_getContext(
            width, height, (AVPixelFormat)decoded_frame->format,
            width, height, AV_PIX_FMT_YUVJ420P,
            SWS_BICUBIC, nullptr, nullptr, nullptr
        );
        
        if (!convert_ctx) {
            av_frame_free(&decoded_frame);
            fprintf(stderr, "Failed to create color conversion context\n");
            return;
        }
        
        AVFrame* converted_frame = av_frame_alloc();
        converted_frame->format = AV_PIX_FMT_YUVJ420P;
        converted_frame->width = width;
        converted_frame->height = height;
        
        if (av_frame_get_buffer(converted_frame, 32) < 0) {
            av_frame_free(&decoded_frame);
            av_frame_free(&converted_frame);
            sws_freeContext(convert_ctx);
            fprintf(stderr, "Failed to allocate converted frame\n");
            return;
        }
        
        sws_scale(convert_ctx, 
                  decoded_frame->data, decoded_frame->linesize, 
                  0, height,
                  converted_frame->data, converted_frame->linesize);
        
        av_frame_free(&decoded_frame);
        decoded_frame = converted_frame;
        sws_freeContext(convert_ctx);
    }

    // 准备编码帧
    AVFrame* enc_frame = av_frame_alloc();
    if (!enc_frame) {
        av_frame_free(&decoded_frame);
        throw std::runtime_error("Failed to allocate frame");
    }
    
    enc_frame->format = AV_PIX_FMT_YUV420P;
    enc_frame->width = width;
    enc_frame->height = height;
    enc_frame->pts = next_pts++;
    
    if (av_frame_get_buffer(enc_frame, 32) < 0) {
        av_frame_free(&decoded_frame);
        av_frame_free(&enc_frame);
        throw std::runtime_error("Could not allocate frame buffer");
    }

    // 转换像素格式（如果需要）
    if (decoded_frame->format != AV_PIX_FMT_YUV420P) {
        SwsContext* scale_ctx = sws_getContext(
            width, height, (AVPixelFormat)decoded_frame->format,
            width, height, AV_PIX_FMT_YUV420P,
            SWS_BICUBIC, nullptr, nullptr, nullptr
        );
        
        if (scale_ctx) {
            sws_scale(scale_ctx, 
                      decoded_frame->data, decoded_frame->linesize, 
                      0, height,
                      enc_frame->data, enc_frame->linesize);
            sws_freeContext(scale_ctx);
        } else {
            // 回退到简单复制
            for (int i = 0; i < 3; i++) {
                int src_stride = decoded_frame->linesize[i];
                int dst_stride = enc_frame->linesize[i];
                int height = (i == 0) ? enc_frame->height : enc_frame->height / 2;
                
                for (int y = 0; y < height; y++) {
                    memcpy(enc_frame->data[i] + y * dst_stride,
                           decoded_frame->data[i] + y * src_stride,
                           std::min(src_stride, dst_stride));
                }
            }
        }
    } else {
        // 直接复制数据
        for (int i = 0; i < 3; i++) {
            int linesize = std::min(decoded_frame->linesize[i], enc_frame->linesize[i]);
            int height = (i == 0) ? enc_frame->height : enc_frame->height / 2;
            
            for (int y = 0; y < height; y++) {
                memcpy(enc_frame->data[i] + y * enc_frame->linesize[i],
                       decoded_frame->data[i] + y * decoded_frame->linesize[i],
                       linesize);
            }
        }
    }
    
    av_frame_free(&decoded_frame);

    // 6. 编码为H.264 I帧
    AVPacket* enc_pkt = av_packet_alloc();
    if (!enc_pkt) {
        av_frame_free(&enc_frame);
        throw std::runtime_error("Failed to allocate packet");
    }
    
    if (avcodec_send_frame(enc_ctx, enc_frame) < 0) {
        av_frame_free(&enc_frame);
        av_packet_free(&enc_pkt);
        throw std::runtime_error("Error sending frame to encoder");
    }
    av_frame_free(&enc_frame);

    int ret = avcodec_receive_packet(enc_ctx, enc_pkt);
    if (ret < 0 && ret != AVERROR(EAGAIN)) {
        av_packet_free(&enc_pkt);
        throw std::runtime_error("Error during encoding");
    }

    if (ret >= 0) {
        // 7. 写入到MP4文件
        enc_pkt->stream_index = stream->index;
        av_packet_rescale_ts(enc_pkt, enc_ctx->time_base, stream->time_base);
        
        if (av_interleaved_write_frame(fmt_ctx, enc_pkt) < 0) {
            av_packet_free(&enc_pkt);
            throw std::runtime_error("Error writing video packet");
        }
    }
    av_packet_free(&enc_pkt);

    frame_count++;
    saveRecoveryState();
}

// 完成并关闭文件
void VideoFormatConvert::close() {
    if (is_completed) return;
    
    // 刷新编码器
    if (enc_ctx) {
        AVPacket* enc_pkt = av_packet_alloc();
        avcodec_send_frame(enc_ctx, nullptr);
        
        while (true) {
            int ret = avcodec_receive_packet(enc_ctx, enc_pkt);
            if (ret < 0) break;
            
            enc_pkt->stream_index = stream->index;
            av_packet_rescale_ts(enc_pkt, enc_ctx->time_base, stream->time_base);
            av_interleaved_write_frame(fmt_ctx, enc_pkt);
            av_packet_unref(enc_pkt);
        }
        av_packet_free(&enc_pkt);
    }

    // 写入文件尾
    if (fmt_ctx && !file_exists) {
        av_write_trailer(fmt_ctx);
    }
    
    // 释放资源
    if (sws_ctx) {
        sws_freeContext(sws_ctx);
        sws_ctx = nullptr;
    }
    if (dec_ctx) {
        avcodec_free_context(&dec_ctx);
        dec_ctx = nullptr;
    }
    if (enc_ctx) {
        avcodec_free_context(&enc_ctx);
        enc_ctx = nullptr;
    }
    if (fmt_ctx) {
        if (fmt_ctx->pb) avio_closep(&fmt_ctx->pb);
        avformat_free_context(fmt_ctx);
        fmt_ctx = nullptr;
    }
    
    stream = nullptr;
    
    // 清理恢复文件
    if (!recovery_file.empty() && fileExists(recovery_file)) {
        remove(recovery_file.c_str());
    }
    
    // 标记为已完成
    is_completed = true;
    if (persistent_cinfo) {
        jpeg_destroy_decompress(persistent_cinfo);
        delete persistent_cinfo;
        delete persistent_jerr;
        persistent_cinfo = nullptr;
        persistent_jerr = nullptr;
    }

}

int VideoFormatConvert::getFrameCount() const {
        return frame_count; 
}

// 获取当前文件名
const std::string& VideoFormatConvert::getCurrentFilename() const { 
    return filename; 
}

bool VideoFormatConvert::isCompleted() const { 
    return is_completed; 
}

// 重命名文件（必须调用此函数完成文件处理）
bool VideoFormatConvert::renameFile() {
    if (!is_completed) {
        // 如果尚未关闭，先关闭文件
        close();
    }
    
    if (filename.empty() || !fileExists(filename)) {
        return false;
    }
    
    // 使用时间戳作为后缀，确保唯一性
    time_t now = time(nullptr);
    struct tm *tm_now = localtime(&now);
    char time_buf[20];
    snprintf(time_buf, sizeof(time_buf), "%04d%02d%02d_%02d%02d%02d",
             1900 + tm_now->tm_year, tm_now->tm_mon + 1, tm_now->tm_mday,
             tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec);
    
    // 分解原始文件名
    size_t dot_pos = original_filename.find_last_of('.');
    std::string base_name, extension;
    
    if (dot_pos != std::string::npos) {
        base_name = original_filename.substr(0, dot_pos);
        extension = original_filename.substr(dot_pos);
    } else {
        base_name = original_filename;
        extension = "";
    }

    if (this->frame_count < 30) {
        this->clearFile(filename);  // 不满30帧,不生成视频文件
    }
    
    // 构造新文件名 - 使用C风格字符串操作
    char new_filename[256];
    snprintf(new_filename, sizeof(new_filename), "%s_%s%s", 
             base_name.c_str(), time_buf, extension.c_str());
    
    // 重命名文件
    if (rename(filename.c_str(), new_filename) == 0) {
        filename = new_filename; // 更新当前文件名
        return true;
    }
    return false;
}

void VideoFormatConvert::clearFile(const std::string& file_name)
{
    std::string recove_file = filename + ".recovery";
    if (fileExists(recove_file)) {
        remove(recove_file.c_str());
    }
    if (fileExists(file_name)) {
        remove(file_name.c_str());
    }
}

    // 保存恢复状态
void VideoFormatConvert::saveRecoveryState() {
    recovery_file = filename + ".recovery";
    std::ofstream out(recovery_file.c_str());
    if (out) {
        out << frame_count << "\n";
        out << next_pts << "\n";
    }
}

// 恢复帧计数
int VideoFormatConvert::recoverFrameCount() {
    recovery_file = filename + ".recovery";
    if (fileExists(recovery_file)) {
        std::ifstream in(recovery_file.c_str());
        if (in) {
            int count;
            in >> count;
            in >> next_pts;
            return count;
        }
    }
    return 0;
}

}
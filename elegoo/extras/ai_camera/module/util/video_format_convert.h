#ifndef __VIDEO_FORMAT_CONVERT_H__
#define __VIDEO_FORMAT_CONVERT_H__

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <jpeglib.h>
#include <jerror.h>
#include <setjmp.h>
#include <sys/stat.h> // 用于文件状态检查
#include <unistd.h>   // 用于文件操作
}

#include <string>
#include <stdexcept>
#include <vector>
#include <fstream>

namespace znp
{

// JPEG 错误处理结构
struct JpegErrorMgr {
    struct jpeg_error_mgr pub;
    jmp_buf setjmp_buffer;
};

class VideoFormatConvert {
public:
    VideoFormatConvert() 
        : fmt_ctx(nullptr), enc_ctx(nullptr), stream(nullptr), 
          dec_ctx(nullptr), sws_ctx(nullptr), frame_count(0),
          file_exists(false) {}
    
    ~VideoFormatConvert() {
        close();
    }
    void createMP4(const std::string& file_name, int width, int height, int fps, int total_frame);
    void addSingleFrame(uint8_t *mjpeg_data, int data_len);
    bool renameFile();
    void clearFile(const std::string& file_name);
    void close();
    int getFrameCount() const;
    const std::string& getCurrentFilename() const;
    bool isCompleted() const;

private:
    AVFormatContext* fmt_ctx;
    AVCodecContext* enc_ctx;
    AVStream* stream;
    AVCodecContext* dec_ctx;
    SwsContext* sws_ctx;

    // 重用解码结构
    struct jpeg_decompress_struct* persistent_cinfo = nullptr;
    JpegErrorMgr* persistent_jerr = nullptr;
    
    int width;
    int height;
    int fps;
    int total_frames;
    int frame_count;
    int64_t next_pts;
    std::string original_filename; // 原始文件名（无后缀）
    std::string filename;          // 当前使用的文件名
    bool file_exists;
    bool is_completed;             // 文件是否已完成
    std::string recovery_file;

    void saveRecoveryState();
    int recoverFrameCount();
    bool decodeJPEG(uint8_t* data, int size, AVFrame** frame);

    bool fileExists(const std::string& filename) {
        struct stat buffer;
        return (stat(filename.c_str(), &buffer) == 0);
    }
};

}
#endif
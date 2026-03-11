/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2024-10-31 21:52:39
 * @LastEditors  : Jack
 * @LastEditTime : 2025-09-02 20:58:55
 * @Description  : 
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "uvc_v4l2.h"
#include "ai_camera_pthread.h"
#include "camera_execption.h"
#include "spdlog/spdlog.h"
namespace znp {
const int BUFFER_COUNT = 2;

V4L2::V4L2(const char* device_name, int video_width, int video_hight,
           unsigned int video_format, unsigned int fps)
    : fd_(-1),
      video_width_(video_width),
      video_hight_(video_hight),
      video_format_(video_format),
      device_(device_name),
      fps_(fps),
      isAlive_(false) {
  SPDLOG_INFO("device name:{}, width:{}, hight:{}, fps:{}", device_, video_width_,
              video_hight_, fps_);
}

V4L2::~V4L2() { 
  if (IsOpen()) this->Stop(); 
}

void V4L2::Start() {
  if (this->Open()) {
    this->isAlive_ = true;
    CreateNewThread([this]() { GetFramesThread();},  2*1024*1024, "V4L2"+device_);
  }
  else {
    SPDLOG_ERROR("Open failed");
    throw CameraException(CameraException::ErrorCode::INITIALIZATION_ERROR,
                          "V4l2 Open failed!");
  }

}

void V4L2::Stop() {
  this->isAlive_ = false;
  this->Close();
}

bool V4L2::Open() {
  if (fd_ > 0) {
    spdlog::warn("Device is Running");
    return false;
  }
  if (!OpenDevice()) {
    SPDLOG_ERROR("OpenDevice failed");
    throw CameraException(CameraException::ErrorCode::INITIALIZATION_ERROR,
                          "V4l2 OpenDevice failed!");
  }
  
  // PrintSupportFormat(fd_);

  if (!SetFormat()) {
    SPDLOG_ERROR("SetFormat failed");
    throw CameraException(CameraException::ErrorCode::INITIALIZATION_ERROR,
                          "V4l2 SetFormat failed!");
  }
  if (!RequestBuffers()) {
    SPDLOG_ERROR("RequestBuffers failed");
    throw CameraException(CameraException::ErrorCode::INITIALIZATION_ERROR,
                          "V4l2 RequestBuffers failed!");
  }
  if (!MapBuffers()) {
    SPDLOG_ERROR("MapBuffers failed");
    throw CameraException(CameraException::ErrorCode::INITIALIZATION_ERROR,
                          "V4l2 MapBuffers failed!");
  }
  if (!StartStreaming()) {
    SPDLOG_ERROR("StartStreaming failed");
    throw CameraException(CameraException::ErrorCode::INITIALIZATION_ERROR,
                          "V4l2 EnqueueBuffers failed!");
  }

  if ((!this->isAlive_) && (this->fd_ > 0)) {
    this->isAlive_ = true;
  }
  return true;
}

void V4L2::Close() {
  if ((this->isAlive_) && (this->fd_ < 0)) {
    this->isAlive_ = false;
  }
  StopStreaming();
  UnmapBuffers();
  CloseDevice();
}

bool V4L2::IsOpen() const { return (this->isAlive_) && (this->fd_ > 0); }

void V4L2::RegisterVideoReceivedCallBack(V4l2CallBack callback) {
  this->v4l2_callback_ = callback;
}

bool V4L2::OpenDevice() {
  this->fd_ = ::open(this->device_.c_str(), O_RDWR | O_NONBLOCK);
  if (this->fd_ < 0) {
    SPDLOG_ERROR("Error opening video device: " + std::string(strerror(errno)));
    return false;
  } else {
    SPDLOG_INFO("fd = {}", fd_);
    return true;
  }
}
bool V4L2::CloseDevice() {
  if (this->fd_ > 0) {
    ::close(this->fd_);
    this->fd_ = -1;
    return true;
  } else {
    return false;
  }
}


void V4L2::PrintSupportFormat(int fd) {
    struct v4l2_fmtdesc format_desc;
    struct v4l2_frmsizeenum frame_size;
    struct v4l2_frmivalenum frame_interval;

    memset(&format_desc, 0, sizeof(format_desc));
    format_desc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    // 枚举所有支持的像素格式
    for (format_desc.index = 0; ; format_desc.index++) {
        if (ioctl(fd, VIDIOC_ENUM_FMT, &format_desc) == -1) {
            if (errno == EINVAL) break; // 没有更多的格式了
            perror("VIDIOC_ENUM_FMT");
            return;
        }

        printf("Pixel Format: %4.4s\n", (char *)&format_desc.pixelformat);

        // 对每个像素格式枚举所有支持的帧大小
        memset(&frame_size, 0, sizeof(frame_size));
        frame_size.pixel_format = format_desc.pixelformat;

        for (frame_size.index = 0; ; frame_size.index++) {
            if (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frame_size) == -1) {
                if (errno == EINVAL) break; // 没有更多的帧大小了
                perror("VIDIOC_ENUM_FRAMESIZES");
                return;
            }

            printf("  Resolution: %ux%u\n", frame_size.discrete.width, frame_size.discrete.height);

            // 对每个帧大小枚举其支持的帧间隔
            memset(&frame_interval, 0, sizeof(frame_interval));
            frame_interval.pixel_format = frame_size.pixel_format;
            frame_interval.width = frame_size.discrete.width;
            frame_interval.height = frame_size.discrete.height;

            for (frame_interval.index = 0; ; frame_interval.index++) {
                if (ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frame_interval) == -1) {
                    if (errno == EINVAL) break; // 没有更多的帧间隔了
                    perror("VIDIOC_ENUM_FRAMEINTERVALS");
                    return;
                }

                if (frame_interval.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
                    printf("    Frame Rate: %u/%u fps\n",
                           frame_interval.discrete.denominator,
                           frame_interval.discrete.numerator);
                } else if (frame_interval.type == V4L2_FRMIVAL_TYPE_CONTINUOUS ||
                           frame_interval.type == V4L2_FRMIVAL_TYPE_STEPWISE) {
                    // 连续或阶梯式帧间隔处理...
                    printf("    Continuous or stepwise frame intervals...\n");
                }
            }
        }
    }
}

bool V4L2::SetFormat() {
  if (this->fd_ < 0) {
    SPDLOG_ERROR("Fd error!");
    return false;
  }

  struct v4l2_format fmt;
  struct v4l2_streamparm stream_param;

  // 设置图像格式
  memset(&fmt, 0, sizeof(fmt));
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width       = video_width_;
  fmt.fmt.pix.height      = video_hight_;
  fmt.fmt.pix.pixelformat = video_format_;
  fmt.fmt.pix.field       = V4L2_FIELD_NONE;

  if (ioctl(this->fd_, VIDIOC_S_FMT, &fmt) < 0) {
    SPDLOG_ERROR("Error setting format: {}", std::string(strerror(errno)));
    return false;
  }

  memset(&stream_param, 0, sizeof(stream_param));
  stream_param.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  stream_param.parm.capture.timeperframe.numerator = 1;
  stream_param.parm.capture.timeperframe.denominator = fps_;

  if (ioctl(fd_, VIDIOC_S_PARM, &stream_param) < 0) {
      SPDLOG_ERROR("VIDIOC_S_PARM");
      return false;
  }
  SPDLOG_INFO("Set Format success!");
  return true;
}

bool V4L2::RequestBuffers() {
  if (this->fd_ < 0) {
    SPDLOG_ERROR("Fd error!");
    return false;
  }
  struct v4l2_requestbuffers req;
  memset(&req, 0, sizeof(req));
  req.count = BUFFER_COUNT;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;

  if (ioctl(this->fd_, VIDIOC_REQBUFS, &req) < 0) {
    SPDLOG_ERROR("Error requesting buffers: {}", std::string(strerror(errno)));
    return false;
  } else {
    return true;
  }
}
bool V4L2::MapBuffers() {
  if (this->fd_ < 0) {
    SPDLOG_ERROR("Fd error!");
    return false;
  }

  for (int i = 0; i < BUFFER_COUNT; ++i) {
    memset(&this->buf_, 0, sizeof(this->buf_));
    this->buf_.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    this->buf_.memory = V4L2_MEMORY_MMAP;
    this->buf_.index = i;

    if (ioctl(this->fd_, VIDIOC_QUERYBUF, &this->buf_) < 0) {
      SPDLOG_ERROR("Error querying buffer: {}", std::string(strerror(errno)));
      return false;
    }
    SPDLOG_INFO("querying buffer size: {}", this->buf_.length);
    this->buffers_[i] =
        (unsigned char*)mmap(NULL, this->buf_.length, PROT_READ | PROT_WRITE,
                             MAP_SHARED, this->fd_, this->buf_.m.offset);
    if (this->buffers_[i] == MAP_FAILED) {
      SPDLOG_ERROR("Error mapping buffer: {}", std::string(strerror(errno)));
      return false;
    }
    // 确保每个缓冲区都被排队
    if (ioctl(this->fd_, VIDIOC_QBUF, &this->buf_) < 0) {
        SPDLOG_ERROR("Error queuing buffer index {}: {}", i, std::string(strerror(errno)));
        return false;
    } else {
        SPDLOG_INFO("Buffer index {} queued successfully", i);
    }
  }
  return true;
}
bool V4L2::UnmapBuffers() {
  if (this->fd_ < 0) {
    SPDLOG_ERROR("Fd error!");
    return false;
  }

  for (int i = 0; i < BUFFER_COUNT; ++i) {
    if (this->buffers_[i] != MAP_FAILED) {
      munmap(this->buffers_[i], this->buf_.length);
    }
  }
  return true;
}
bool V4L2::EnqueueBuffers() {
  if (this->fd_ < 0) {
    SPDLOG_ERROR("Fd error!");
    return false;
  }
  for (int i = 0; i < BUFFER_COUNT; ++i) {
    memset(&this->buf_, 0, sizeof(this->buf_));
    this->buf_.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    this->buf_.memory = V4L2_MEMORY_MMAP;
    this->buf_.index = i;

    if (ioctl(this->fd_, VIDIOC_QBUF, &this->buf_) < 0) {
      SPDLOG_ERROR("Error queueing buffer: " + std::string(strerror(errno)));
      return false;
    } else {
      return true;
    }
  }
}

bool V4L2::StartStreaming() {
  if (this->fd_ < 0) {
    SPDLOG_ERROR("Fd error!");
    return false;
  }
  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(this->fd_, VIDIOC_STREAMON, &type) < 0) {
    SPDLOG_ERROR("Error starting stream: " + std::string(strerror(errno)));
    return false;
  } else {
    return true;
  }
}
bool V4L2::StopStreaming() {
  if (this->fd_ < 0) {
    SPDLOG_ERROR("Fd error!");
    return false;
  }
  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(this->fd_, VIDIOC_STREAMOFF, &type) < 0) {
    SPDLOG_ERROR("Error stop stream: " + std::string(strerror(errno)));
    return false;
  } else {
    return true;
  }
}

bool V4L2::ReleaseBuffers() {
  if (this->fd_ < 0) {
    SPDLOG_ERROR("Fd error!");
    return false;
  }
  struct v4l2_requestbuffers req;
  memset(&req, 0, sizeof(req));
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;

  if (ioctl(this->fd_, VIDIOC_REQBUFS, &req) < 0) {
    SPDLOG_ERROR("Error releaseBuffers: " + std::string(strerror(errno)));
    return false;
  } else {
    return true;
  }
}

void V4L2::GetFramesThread() {
  if ((this->fd_ < 0)) {
    SPDLOG_ERROR("Fd error!");
    return;
  }
  int ret = 0;
  struct v4l2_buffer buf;
  while (this->isAlive_) {
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    std::lock_guard<std::mutex> lock(this->video_ioctl_mutex_);
    {
      if (ioctl(this->fd_, VIDIOC_DQBUF, &buf) < 0) {
        // SPDLOG_ERROR("Error dequeuing: " + std::string(strerror(errno)));
        std::this_thread::sleep_for(std::chrono::milliseconds(20));  // 延时
        continue;
      }
      // SPDLOG_INFO("Frame size: {} bytes, index[{}] fps_: {}", buf.bytesused, buf.index, fps_);
      if (this->v4l2_callback_) {
        this->v4l2_callback_(this->buffers_[buf.index], buf.bytesused);
      }

      if (ioctl(this->fd_, VIDIOC_QBUF, &buf) < 0) {
        SPDLOG_ERROR("Error Requeue the buffers: " +
                     std::string(strerror(errno)));
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        continue;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1000/fps_));
  }
  SPDLOG_INFO("GetFramesThread Over!");
}

bool V4L2::GetOneFrame(uint8_t* pbuf, int* len) {
  if ((!pbuf) || (!len)) {
    SPDLOG_ERROR("param error!");
    return false;
  }
  if (!IsOpen()) {
    SPDLOG_WARN("camera is not ready!");
    return false;
  }
  struct v4l2_buffer buf;
  memset(&buf, 0, sizeof(buf));
  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_MMAP;
  std::lock_guard<std::mutex> lock(this->video_ioctl_mutex_);
  {
    if (ioctl(this->fd_, VIDIOC_DQBUF, &buf) < 0) {
      SPDLOG_ERROR("Error dequeuing: {}", std::string(strerror(errno)));
      return false;
    }
    SPDLOG_INFO("Frame size: {} bytes, index[{}]", buf.bytesused,
                buf.index);
    *len = buf.bytesused;
    memcpy(pbuf, this->buffers_[buf.index], buf.bytesused);

    if (ioctl(this->fd_, VIDIOC_QBUF, &buf) < 0) {
      SPDLOG_ERROR("Error Requeue the buffers: " +
                   std::string(strerror(errno)));
    }
  }

  return true;
}
}

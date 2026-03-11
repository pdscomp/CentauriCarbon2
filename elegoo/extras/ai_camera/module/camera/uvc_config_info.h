/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2024-11-05 11:50:31
 * @LastEditors  : huangpeiyun
 * @LastEditTime : 2025-08-29 17:40:56
 * @Description  : 
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#pragma once

#include <linux/videodev2.h>
#include <iostream>
#include <string>

#define CAMERA_FPS 20
#define VIDEO_FPS 30
#define CAMERA_UART_BOAD 115200
#define CAMERA_FARME_WIDTH 640
#define CAMERA_FARME_HIGHT 360
#define CAMERA_UART_TIMEOUT_MS 5*1000
#define CAMERA_UART_OFFLINE_MS 10*1000

#define CAMERA_UART_DEVICE "/dev/ttyS3"
#define CAMERA_JPEG_DEVICE "/dev/video0"
#define CAMERA_YUYV_DEVICE "/dev/video1"
#define CAMERA_H264_DEVICE "/dev/video2"

// 若使用rtc推流功能,需使用同时支持h264+mjpg摄像头,并且此处设置为1
#define ENABLE_BOTH_H264_MJPG 1

namespace znp {
struct UvcCameraInfo {
  std::string uart_device_name_{CAMERA_UART_DEVICE};
  int uart_device_baudrate_{CAMERA_UART_BOAD};

  std::string camera_device_h264_name_{CAMERA_H264_DEVICE};
  std::string camera_device_jpeg_name_{CAMERA_JPEG_DEVICE};
  unsigned int video_format_h264{V4L2_PIX_FMT_H264};
  unsigned int video_format_jpeg{V4L2_PIX_FMT_MPEG};
  unsigned int video_fps_h264{CAMERA_FPS};
  unsigned int video_fps_jpeg{CAMERA_FPS};
  int video_queue_frame_width_{CAMERA_FARME_WIDTH};
  int video_queue_frame_hight_{CAMERA_FARME_HIGHT};

  int uart_time_out_ms_{CAMERA_UART_TIMEOUT_MS};
  int uart_time_off_line_ms_{CAMERA_UART_OFFLINE_MS};
};

struct UvcCameraInfoV2 {
  std::string camera_device_name_{CAMERA_JPEG_DEVICE};
  unsigned int video_format{V4L2_PIX_FMT_MJPEG};
  unsigned int video_fps{CAMERA_FPS};
  int video_queue_frame_width_{CAMERA_FARME_WIDTH};
  int video_queue_frame_hight_{CAMERA_FARME_HIGHT};
};

struct UvcCameraInfoV21 {
  std::string camera_device_name_{CAMERA_H264_DEVICE};
  unsigned int video_format{V4L2_PIX_FMT_H264};
  unsigned int video_fps{CAMERA_FPS};
  int video_queue_frame_width_{CAMERA_FARME_WIDTH};
  int video_queue_frame_hight_{CAMERA_FARME_HIGHT};
};

}  // namespace znp

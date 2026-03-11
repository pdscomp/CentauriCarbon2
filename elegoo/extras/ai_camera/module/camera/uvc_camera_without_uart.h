/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2024-10-31 21:52:39
 * @LastEditors  : Jack
 * @LastEditTime : 2025-09-02 10:25:51
 * @Description  : 
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

 #pragma once

 #include <map>
 #include "uvc_config_info.h"
 #include "uvc_v4l2.h"
 #include "video_list.h"
 #include "store_time.h"
 namespace znp {
 
 class UvcCamera {

  public:
  UvcCamera();
  ~UvcCamera();

  void Start();
  void Stop();

  void StartMjpgStream(void);
  void StopMjpgStream(void);

  void StartH264Stream(void);
  void StopH264Stream(void);

  bool GetFrameOfMjpeg(unsigned char* pBuf, int* pLen);
  bool GetFrameOfH264(unsigned char* pBuf, int* pLen);
  bool VideoStreamMjpgInit(void);
  bool VideoStreamMjpgDeinit(void);
  bool VideoStreamH264Init(void);
  bool VideoStreamH264Deinit(void);

  private:

  bool IsDeviceSupportH264(void);

  bool IsMjpgStreamWorking(void);
  bool IsH264StreamWorking(void);

  void V4l2H264ReceivedCallBack(uint8_t *data, int len);
  void V4l2MjpgReceivedCallBack(uint8_t *data, int len);

  private:
    std::shared_ptr<znp::V4L2> v4l2_MJPG_;
    std::shared_ptr<znp::V4L2> v4l2_H264_;
    std::shared_ptr<VideoList> videoListMjpg_;
    std::shared_ptr<VideoList> videoListH264_;
    const UvcCameraInfoV2 uvc_camera_info_mjpg;
    const UvcCameraInfoV21 uvc_camera_info_h264;
 };
 
 }  // namespace znp
 
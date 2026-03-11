/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2025-07-21 10:24:07
 * @LastEditors  : Jack
 * @LastEditTime : 2025-08-28 18:18:48
 * @Description  :
 *
 * Copyright (c) 2025 by ELEGOO, All Rights Reserved.
 *****************************************************************************/
#pragma once

#include "json.h"

namespace znp {

std::string MessageParseString(json &jsonResponse);

void MessageCmdSuccess(json &jsonResponse, std::string status);

void MessageCmdFailed(json &jsonResponse, std::string status, std::string reason);

void JsonParseInvalidRequest(json &jsonResponse);

void JspnParseAbnormal(json &jsonResponse);

void MessageCmdNotFound(json &jsonResponse);

void MessageCameraInsert(json &jsonResponse);

void MessageCameraPullOut(json &jsonResponse);

void MessageCameraNotFound(json &jsonResponse);



void MessageCompositeVideoStart(json &json, const std::string &video_name);
void MessageCompositeVideoProcess(json &json, float process);
void MessageCompositeVideoFinish(json &json, const std::string &video_name,const std::string &video_path,  uint32_t video_size);
void MessageCompositeVideoFailed(json &json, const std::string &video_name);


}


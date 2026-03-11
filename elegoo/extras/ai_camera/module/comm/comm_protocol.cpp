/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2025-07-21 10:23:01
 * @LastEditors  : Jack
 * @LastEditTime : 2025-08-28 18:15:58
 * @Description  :
 *
 * Copyright (c) 2025 by ELEGOO, All Rights Reserved.
 *****************************************************************************/
#include "comm_protocol.h"

namespace znp {

std::string MessageParseString(json &jsonResponse)
{
    std::string jsonString = jsonResponse.dump();
    jsonString += "\x03";
    return jsonString;
}

void MessageCmdSuccess(json &jsonResponse, std::string status)
{
    jsonResponse["result"] = "ok";
    jsonResponse["status"] = status;
}

void MessageCmdFailed(json &jsonResponse, std::string status, std::string reason)
{
    jsonResponse["result"] = "fail";
    jsonResponse["status"] = status;
    jsonResponse["reason"] = reason;
}

void JsonParseInvalidRequest(json &jsonResponse) {
  jsonResponse["id"] = -1;
  jsonResponse["method"] = "status_report";
  jsonResponse["warning"]["message"] = "Invalid request";
}

void JspnParseAbnormal(json &jsonResponse) {
  jsonResponse["id"] = -1;
  jsonResponse["method"] = "status_report";
  jsonResponse["result"]["level"] = "error";
  jsonResponse["result"]["message"] = "Abnormal camera status!";
}

void MessageCmdNotFound(json &jsonResponse) {
  jsonResponse["id"] = -1;
  jsonResponse["method"] = "status_report";
  jsonResponse["result"]["level"] = "warning";
  jsonResponse["result"]["message"] = "Invalid request";
}

void MessageCameraInsert(json &jsonResponse) {
  jsonResponse["id"] = -1;
  jsonResponse["method"] = "status_report";
  jsonResponse["result"]["level"] = "warning";
  jsonResponse["result"]["message"] = "Camera device insertion";
}

void MessageCameraPullOut(json &jsonResponse) {
  jsonResponse["id"] = -1;
  jsonResponse["method"] = "status_report";
  jsonResponse["result"]["level"] = "warning";
  jsonResponse["result"]["message"] = "Camera device pull out";
}

void MessageCameraNotFound(json &jsonResponse) {
    jsonResponse["id"] = -1;
    jsonResponse["method"] = "status_report";
    jsonResponse["result"]["level"] = "error";
    jsonResponse["result"]["message"] = "No camera device detected!";
}


void MessageCompositeVideoStart(json &json, const std::string &video_name)
{
    json["id"] = -1;
    json["method"] = "status_report";
    json["result"]["level"] = "info";
    json["result"]["message"] = "Time-Lapse Composite Video Start";
    json["result"]["video_name"] = video_name;
}

void MessageCompositeVideoProcess(json &json, float process)
{
    json["id"] = -1;
    json["method"] = "status_report";
    json["result"]["level"] = "info";
    json["result"]["message"] = "Time-Lapse Composite Video Process";
    json["result"]["process"] = process;
}

void MessageCompositeVideoFinish(json &json, const std::string &video_name,const std::string &video_path,  uint32_t video_size)
{
    json["id"] = -1;
    json["method"] = "status_report";
    json["result"]["level"] = "info";
    json["result"]["message"] = "Time-Lapse Composite Video Finish";
    json["result"]["video_name"] = video_name;
    json["result"]["video_path"] = video_path;
    json["result"]["video_size"] = video_size;
}

void MessageCompositeVideoFailed(json &json, const std::string &video_name)
{
    json["id"] = -1;
    json["method"] = "status_report";
    json["result"]["level"] = "info";
    json["result"]["message"] = "Time-Lapse Composite Video Failed";
    json["result"]["video_name"] = video_name;
}





}

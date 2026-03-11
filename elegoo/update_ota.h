/*****************************************************************************
 * @Author       : Loping
 * @Date         : 2025-3-18 11:03:36
 * @LastEditors  : loping
 * @LastEditTime : 2025-05-23 17:55:25
 * @Description  : update ota
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#ifndef __UPDATE_OTA_H__
#define __UPDATE_OTA_H__

#include <iostream>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <functional>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include "printer.h"
#include "configfile.h"
#include "json.h"
#include "curl/curl.h"
#include "unzip.h"

typedef struct {
    int code;
    char message[1024];
    struct {
        char ver[32];
        char info[1024];
        char downloadUrl[1024];
        long long releaseTime;
        char fileMd5[33];
        long long fileSize;
    } url_info;
} revc_message_t;

typedef struct 
{
    std::string net_status;
    std::string cur_language;
    std::string cur_machineModel;
    std::string cur_sn;
    std::string cur_current_version;
    bool download_falg;
    uint8_t detect_falg;
    int32_t select_cnt;
    std::string url;
    std::string ota_pkg_name;
    // uint64_t ota_pkg_size;
    uint64_t ota_pkg_recv_size;
    double program;
    double recv_data_time;
    std::string signed_header;
    revc_message_t revc_message;
    bool reset_first;
    struct curl_slist *headers;
    double start_sign_time;
    std::mutex lock;
}ota_info_t;

typedef enum 
{
    FIRMWARE=0,
    ELEGOO,
    EEB001_GUI,
    AI_CAMERA,
    DSP,
    MCU_MAIN,
    MCU_TOOLHEAD,
    MCU_BEDMESH,
    E_VER_MAX,
}ota_ver_name_e;

typedef struct 
{
    // ota_ver_name_e e_ver_name;
    std::string ver_name;
    std::string ver_num;
    std::string ver_content;
}update_info_t;

class UpdateOta
{
public:
    UpdateOta(std::shared_ptr<ConfigWrapper> config);
    ~UpdateOta();
    void cmd_UPDATE_NET_STATUS(std::shared_ptr<GCodeCommand> gcmd);
    void cmd_UPDATE_CUR_LAN(std::shared_ptr<GCodeCommand> gcmd);
    void cmd_UPDATE_OTA(std::shared_ptr<GCodeCommand> gcmd);
    void cmd_UPDATE_UPAN(std::shared_ptr<GCodeCommand> gcmd);
    void cmd_RESTORE_FACTORY_SETTINGS(std::shared_ptr<GCodeCommand> gcmd);
    void update_ota_thread();
    void detect_ota_thread();
    void handle_ready();
    std::string popen_exec(std::string cmd);
    int32_t parse_sig_file();
    int32_t parse_swu_pkg(std::tuple<std::string,std::string,std::string> pkg_tuple);
    std::string get_zip_file();
    std::string skip_sig_obtain_zip_file(FILE *inputFile);
    std::tuple<std::string,std::string,std::string> extract_zip_file(std::string zipFileName);
    int32_t extract_one_zip_file(std::string name,unzFile& zipfile);
    std::pair<std::string,std::string> parse_json_file(std::string jsonName,unzFile& zipfile);
    int32_t unzip_file();
    void down_ota_feedback(std::string command,std::string result = "",double progress = -1.,bool flag = false);
    int32_t get_ota_info();
    void parse_firmware_ver_for_ini();
    void parse_firmware_ver_for_json();
private:
    char* url_Encode(const char* str);
    char* base64Encode(const unsigned char* buffer, size_t length);
    char* sign(const char* data, size_t dataLength, const char* privateKey, size_t privateKeyLength);
    char* readFileAndAddHeadTail(const char* filename, const char* head, const char* tail);
    char* replace_spaces_with_percent20(const char* str);
    int detect_ota_pkg();
    int get_revc_message(CURL *ota_curl);
    int32_t parse_revc_message(std::string readBuffer);
    int32_t write_update_info(std::string update_info,std::string file_name);
    int32_t parse_update_info(std::string update_info);
    int32_t json_parse_update_info(std::string update_info);
    void down_ota_pkg();
    void download_proc();
    int8_t reset_sign();
    int32_t get_sign();

private:
    std::shared_ptr<Printer> printer;
    std::shared_ptr<ReactorTimer> timer;
    std::shared_ptr<GCodeDispatch> gcode;
};

std::shared_ptr<UpdateOta> update_ota_load_config(std::shared_ptr<ConfigWrapper> config);

#endif

/*****************************************************************************
 * @Author       : Loping
 * @Date         : 2025-3-18 11:03:36
 * @LastEditors  : loping
 * @LastEditTime : 2025-05-29 16:03:27
 * @Description  : update ota
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include <fstream>

#include "update_ota.h"

#include "openssl/pem.h"
#include "openssl/rsa.h"
#include "openssl/err.h"
#include "openssl/evp.h"
#include "openssl/bio.h"
#include "openssl/md5.h"

// #undef SPDLOG_DEBUG
// #define SPDLOG_DEBUG SPDLOG_INFO

#define OTA_DETECT_WAIT_TIME (60)
#define SELECTT_TIME_OUT_SEC (2)
#define SELECTT_TIME_OUT_USEC (500000)
#define TIMER_DETECT_TIME (0.5)
#define OTA_PREFFIX "ota_"
#define IPK_PKG OTA_PREFFIX"elegoo.ipk"
#define SWU_PKG OTA_PREFFIX"tina-r528-evb1-ab.swu"
// #define SWU_PKG OTA_PREFFIX"ccpro_eeb001"
#define UPDATE_INFO_FILE "/opt/inst/ota/update_info"
#define FIRMWARE_VER_JSON "/opt/inst/firmware_version/versions.json"
#define FIRMWARE_VER_INI "/opt/inst/firmware_version/versions.ini"

static ota_info_t ota_info = {};
static update_info_t ver_update_info[E_VER_MAX] = {
    /*
{
    "firmware": {
        "version": "v1.0.1",
        "content_new": "\n1.\n2.\n",
        "content_modify": "\n1.\n2.\n"
    },
    "ai_camera": {
        "version": "v1.0.1",
        "content_new": "\n1.\n2.\n",
        "content_modify": "\n1.\n2.\n"
    },
    "elegoo": {
        "version": "v1.0.1",
        "content_new": "\n1.\n2.\n",
        "content_modify": "\n1.\n2.\n"
    },
    "eeb001_gui": {
        "version": "v1.0.1",
        "content_new": "\n1.\n2.\n",
        "content_modify": "\n1.\n2.\n"
    },
    "dsp": {
        "version": "v1.0.1",
        "content_new": "\n1.\n2.\n",
        "content_modify": "\n1.\n2.\n"
    },
    "mcu_main": {
        "version": "v1.0.1",
        "content_new": "\n1.\n2.\n",
        "content_modify": "\n1.\n2.\n"
    },
    "mcu_toolhead": {
        "version": "v1.0.1",
        "content_new": "\n1.\n2.\n",
        "content_modify": "\n1.\n2.\n"
    },
    "mcu_bedmesh": {
        "version": "v1.0.1",
        "content_new": "\n1.\n2.\n",
        "content_modify": "\n1.\n2.\n"
    }
}
    */
    {.ver_name = "firmware"},
    {.ver_name = "elegoo"},
    {.ver_name = "eeb001_gui"},
    {.ver_name = "ai_camera"},
    {.ver_name = "dsp"},
    {.ver_name = "mcu_main"},
    {.ver_name = "mcu_toolhead"},
    {.ver_name = "mcu_bedmesh"},
    };

typedef struct __attribute__((packed))
{
    uint32_t magic;
    uint8_t package_type;
    uint8_t machine_type;
    uint16_t machine_model;
    uint64_t filesize;
    uint8_t filename[128];
    uint64_t encrypt_offset;
    uint64_t encrypt_length;
    uint8_t aes_iv[16];
    uint64_t encrypt_filesize;
    uint8_t reserved[40];
    uint8_t sha256[32];
    uint8_t rsa2048[256];
} elegoo_sign_t;

UpdateOta::UpdateOta(std::shared_ptr<ConfigWrapper> config)
{
    SPDLOG_INFO("__func__:{} #1",__func__);
    get_ota_info();
    ota_info.download_falg = false;

    this->printer = config->get_printer();
    this->gcode = any_cast<std::shared_ptr<GCodeDispatch>>(this->printer->lookup_object("gcode"));
    this->gcode->register_command(
                "UPDATE_NET_STATUS"
                ,[this](std::shared_ptr<GCodeCommand> gcmd){
                    cmd_UPDATE_NET_STATUS(gcmd);
                }
                ,false
                ,"update net status");
    this->gcode->register_command(
                "UPDATE_CUR_LAN"
                ,[this](std::shared_ptr<GCodeCommand> gcmd){
                    cmd_UPDATE_CUR_LAN(gcmd);
                }
                ,false
                ,"update cur language");
    this->gcode->register_command(
                "UPDATE_OTA"
                ,[this](std::shared_ptr<GCodeCommand> gcmd){
                    cmd_UPDATE_OTA(gcmd);
                }
                ,false
                ,"update for ota");
    this->gcode->register_command(
                "UPDATE_UPAN"
                ,[this](std::shared_ptr<GCodeCommand> gcmd){
                    cmd_UPDATE_UPAN(gcmd);
                }
                ,false
                ,"update for upan");
    this->gcode->register_command(
                "RESTORE_FACTORY_SETTINGS"
                ,[this](std::shared_ptr<GCodeCommand> gcmd){
                    cmd_RESTORE_FACTORY_SETTINGS(gcmd);
                }
                ,false
                ,"Restore factory settings");
    
    elegoo::common::SignalManager::get_instance().register_signal(
        "elegoo:ready",
        std::function<void()>([this](){
            SPDLOG_DEBUG("UpdateOta ready~~~~~~~~~~~~~~~~~");
            handle_ready();
            SPDLOG_DEBUG("UpdateOta ready~~~~~~~~~~~~~~~~~ success!");
        })
    );
}

UpdateOta::~UpdateOta()
{
    SPDLOG_INFO("~UpdateOta");
    ota_info.detect_falg = 0;
    if(ota_info.headers)
        curl_slist_free_all(ota_info.headers);
    curl_global_cleanup();
}

void UpdateOta::handle_ready()
{
    SPDLOG_INFO("__func__:{} #1",__func__);
    curl_global_init(CURL_GLOBAL_DEFAULT);
    ota_info.reset_first = true;
    ota_info.headers = nullptr;
#if 1
    ota_info.revc_message.url_info.fileSize = 32*1024*1024;
#else
    ota_info.revc_message.url_info.fileSize = 0;
#endif
    ota_info.start_sign_time = get_monotonic();
    detect_ota_thread();
}

void UpdateOta::parse_firmware_ver_for_ini()
{ 
        std::shared_ptr<CSimpleIniA> fileconfig = std::make_shared<CSimpleIniA>();
        SI_Error rc = fileconfig->LoadFile(FIRMWARE_VER_INI);
        if (rc < 0) 
        {
            SPDLOG_ERROR("load INI file error,error code is {}",rc);
            return ;
        }
        const char* firmware_version = fileconfig->GetValue("firmware", "firmware_version", "");
        if (firmware_version != nullptr)
        {
            ota_info.cur_current_version = firmware_version;
        }
        else
        {
            ota_info.cur_current_version = "";
        }
        SPDLOG_INFO("__func__:{} #1 cur_current_version:{}", __func__,ota_info.cur_current_version);
        const char* machine_model = fileconfig->GetValue("machine", "machine_model", "");
        if (machine_model != nullptr)
        {
            ota_info.cur_machineModel = machine_model;
        }
        else
        {
            ota_info.cur_machineModel = "";
        }
        SPDLOG_INFO("__func__:{} #1 cur_machineModel:{}", __func__,ota_info.cur_machineModel);
}

void UpdateOta::parse_firmware_ver_for_json()
{
    std::string firmware_ver = FIRMWARE_VER_JSON;
    std::ifstream infile(firmware_ver,std::ios::binary);
    if (!infile.is_open())
    {
        SPDLOG_ERROR("Cannot open {} for reading",firmware_ver);
        ota_info.cur_current_version = "";
        ota_info.cur_machineModel = "";
    }
    else
    {
        json jsonData;
        infile >> jsonData;
        SPDLOG_INFO("jsonData:{}",jsonData.dump());
        infile.close();
        //machine Model
        if(jsonData.contains("machine") && jsonData["machine"].contains("model"))
        {
                ota_info.cur_machineModel = jsonData["machine"]["model"].get<std::string>();
        }
        else
        {
            ota_info.cur_machineModel = "";
        }
        SPDLOG_INFO("__func__:{} #1 cur_machineModel:{}", __func__,ota_info.cur_machineModel);
        //current version
        if(jsonData.contains("version")
            && jsonData["version"].is_array()
            && jsonData["version"].size() >= 8
            && jsonData["version"][7].contains("ota_version"))
        {
            ota_info.cur_current_version = jsonData["version"][7]["ota_version"].get<std::string>();
            SPDLOG_INFO("__func__:{} #1 version.size:{} cur_current_version:{}", __func__,jsonData["version"].size(),ota_info.cur_current_version);
        }
        else
        {
            ota_info.cur_current_version = "";
            SPDLOG_INFO("__func__:{} #1 version.size:{} cur_current_version:{}", __func__,jsonData["version"].size(),ota_info.cur_current_version);
        }
    }
}

int32_t UpdateOta::get_ota_info()
{
    if(access(FIRMWARE_VER_JSON,F_OK) == 0)
    {
        parse_firmware_ver_for_json();
    }
    else
    {
        parse_firmware_ver_for_ini();
    }
    
#if 1
    ota_info.cur_language = "zh";
#else
    ota_info.cur_language = "";
#endif
    ota_info.cur_sn = "147258";
// #if 0
//     ota_info.cur_machineModel = "ELegoo Centauri";
// #else
//     ota_info.cur_machineModel = "ELegoo Centauri Carbon";
// #endif
    // ota_info.cur_current_version = "01.01.15.03";
    SPDLOG_INFO("__func__:{} #1 cur_machineModel:{} cur_current_version:{}",__func__,ota_info.cur_machineModel,ota_info.cur_current_version);

    return 0;
}

void UpdateOta::cmd_UPDATE_NET_STATUS(std::shared_ptr<GCodeCommand> gcmd)
{
    ota_info.net_status = gcmd->get("STATUS","");
    if(ota_info.net_status.empty())
    {
        ota_info.net_status = "connected";
    }
    else if(ota_info.net_status != "connected")
    {
        ota_info.net_status = "disconnected";
    }
    SPDLOG_INFO("__func__:{} #1 __ok net_status:{}", __func__,ota_info.net_status);
}

void UpdateOta::cmd_UPDATE_CUR_LAN(std::shared_ptr<GCodeCommand> gcmd)
{
    ota_info.cur_language = gcmd->get("LAN","");
    if(ota_info.cur_language.empty())
    {
        ota_info.cur_language = "zh";
    }
    else if(ota_info.cur_language != "zh")
    {
        ota_info.cur_language = "en";
    }
    SPDLOG_INFO("__func__:{} #1 __ok cur_language:{}", __func__,ota_info.cur_language);
}

void UpdateOta::cmd_UPDATE_OTA(std::shared_ptr<GCodeCommand> gcmd)
{
    if(!ota_info.download_falg)
    {
        SPDLOG_INFO("__func__:{} #1 ", __func__);
        update_ota_thread();
    }
    else
    {
        SPDLOG_ERROR("cur is update ota ...");
        down_ota_feedback("M2202","2705",-1.,true);
        return;
    }
    SPDLOG_INFO("__func__:{} #1 __ok", __func__);
}

void UpdateOta::cmd_UPDATE_UPAN(std::shared_ptr<GCodeCommand> gcmd)
{
    SPDLOG_INFO("__func__:{} #1 ", __func__);
    system("update_ipk.sh upan;sleep 1;source /etc/profile;run_printer.sh start");
    SPDLOG_INFO("__func__:{} #1 __ok", __func__);
}

void UpdateOta::cmd_RESTORE_FACTORY_SETTINGS(std::shared_ptr<GCodeCommand> gcmd)
{
    SPDLOG_INFO("__func__:{} #1 ", __func__);
    std::string cmd = "rm /opt/usr/cfg/autosave.cfg -f;";
    //TODO:在执行命令前是否需要停止打印机的一些操作
    system(cmd.c_str());
    if(access("/opt/usr/cfg/autosave.cfg",F_OK) == 0)
    {
        json res;
        res["command"] = "RESTORE_FACTORY_SETTINGS";
        res["result"] = "failed";
        this->gcode->respond_feedback(res);
        SPDLOG_INFO("__func__:{} #1 file is exist! res:{}",__func__,res.dump());
    }
    else
    {
        json res;
        res["command"] = "RESTORE_FACTORY_SETTINGS";
        res["result"] = "success";
        this->gcode->respond_feedback(res);
        SPDLOG_INFO("__func__:{} #1 file is not exist! res:{}",__func__,res.dump());
    }
}

char* UpdateOta::url_Encode(const char* str)
{
    SPDLOG_DEBUG("__func__:{} #1 ",__func__);
    size_t len = strlen(str);
    size_t newLen = 3 * len + 1; // 加1用于'\0'
    char *encoded = (char *)malloc(newLen);
    if (!encoded) return NULL;

    const char *ptr = str;
    char *optr = encoded;

    while (*ptr)
    {
        if (isalnum((unsigned char)*ptr) ||
            *ptr == '-' || *ptr == '_' || *ptr == '.' ||
            *ptr == '~' || *ptr == '*' || *ptr == '/') {
            // 允许的字符直接复制
            *optr++ = *ptr;
        }
        else
        {
            // 其他字符进行编码
            sprintf(optr, "%%%02X", (unsigned char)*ptr);
            optr += 3;
        }
        ptr++;
    }

    *optr = '\0'; // 确保字符串以NULL结尾
    return encoded;
}

char* UpdateOta::base64Encode(const unsigned char* buffer, size_t length)
{
    SPDLOG_DEBUG("__func__:{} #1 ",__func__);
    BIO *bio, *b64;
    BUF_MEM *bufferPtr;

    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, buffer, length);
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &bufferPtr);
    BIO_set_close(bio, BIO_NOCLOSE);
    BIO_free_all(bio);

    // Allocate memory to hold the base64 encoded string
    char* encoded = (char*)malloc(bufferPtr->length + 1);
    strncpy(encoded, bufferPtr->data, bufferPtr->length);
    encoded[bufferPtr->length] = '\0';

    return encoded;
}

char* UpdateOta::sign(const char* data, size_t dataLength, const char* privateKey, size_t privateKeyLength)
{
    SPDLOG_DEBUG("__func__:{} #1 dataLength:{} {}",__func__,dataLength,privateKeyLength);
    // Decode the Base64 encoded private key
    BIO *bio = BIO_new_mem_buf(privateKey, privateKeyLength);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    if (bio == NULL)
    {
        ERR_print_errors_fp(stderr);
        elegoo::common::CommandError("bio == NULL");
    }

    // Read private key
    RSA *rsa = NULL;
    PEM_read_bio_RSAPrivateKey(bio, &rsa, NULL, NULL);
    BIO_free(bio);

    SPDLOG_DEBUG("__func__:{} #1 ",__func__);
    if (!rsa)
    {
        ERR_print_errors_fp(stderr);
        elegoo::common::CommandError("!rsa");
    }

    // Create a new EVP_PKEY structure and assign the RSA key to it
    EVP_PKEY *pkey = EVP_PKEY_new();
    EVP_PKEY_assign_RSA(pkey, rsa);

    // Create a new signature context
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (EVP_DigestSignInit(mdctx, nullptr, EVP_sha256(), nullptr, pkey) != 1)
    {
        EVP_MD_CTX_free(mdctx);
        EVP_PKEY_free(pkey);
        ERR_print_errors_fp(stderr);
        elegoo::common::CommandError("SignInit error");
    }

    // printf("dataLength:%d\n data:%s\n",dataLength,data);
    // Update with data
    if (EVP_DigestSignUpdate(mdctx, data, dataLength) != 1)
    {
        SPDLOG_DEBUG("__func__:{} #1 ",__func__);
        EVP_MD_CTX_free(mdctx);
        EVP_PKEY_free(pkey);
        ERR_print_errors_fp(stderr);
        elegoo::common::CommandError("SignUpdate error");
    }

    // Sign the data
    unsigned char signature[256] = {0};
    unsigned int signatureLength = 0;
    if(nullptr == mdctx || nullptr == pkey)
        elegoo::common::CommandError("nullptr == mdctx || nullptr == pkey");
    int ret = EVP_DigestSignFinal(mdctx, nullptr, &signatureLength);
    if (ret != 1)
    {
        EVP_MD_CTX_free(mdctx);
        EVP_PKEY_free(pkey);
        ERR_print_errors_fp(stderr);
        elegoo::common::CommandError("SignFinal error");
    }
    ret = EVP_DigestSignFinal(mdctx, signature, &signatureLength);
    if (ret != 1)
    {
        EVP_MD_CTX_free(mdctx);
        EVP_PKEY_free(pkey);
        ERR_print_errors_fp(stderr);
        elegoo::common::CommandError("SignFinal error");
    }

    EVP_MD_CTX_free(mdctx);
    EVP_PKEY_free(pkey);

    // Encode signature to Base64
    char* encodedSignature = base64Encode(signature, signatureLength);
    return encodedSignature;
}

char* UpdateOta::readFileAndAddHeadTail(const char* filename, const char* head, const char* tail)
{
    // SPDLOG_DEBUG("__func__:{} #1 filename:{}",__func__,filename);
    FILE *file = fopen(filename, "r");
    if (file == NULL)
    {
        SPDLOG_ERROR("open file failed!");
        elegoo::common::CommandError("open file failed!");
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    // SPDLOG_DEBUG("__func__:{} #1 fileSize:{},strlen(head):{},strlen(tail):{}",__func__,fileSize,strlen(head),strlen(tail));
    char* content = (char*)malloc(fileSize + strlen(head) + strlen(tail)); // +1 for null terminator
    if (content == NULL)
    {
        elegoo::common::CommandError("content == NULL");
        fclose(file);
        return NULL;
    }
    strcpy(content, head);

    fread(content + strlen(head), 1, fileSize, file);
    fclose(file);

    strcpy(content + strlen(head) + fileSize, tail);

    return content;
}

char* UpdateOta::replace_spaces_with_percent20(const char* str)
{
    SPDLOG_DEBUG("__func__:{} #1 ",__func__);
    int len = strlen(str);
    int new_len = len;
    const char* ptr;

    // 计算需要多少个%20来替换空格
    for (ptr = str; *ptr; ++ptr)
    {
        if (*ptr == ' ')
        {
            new_len += 2; // 每个空格将被替换为%20，长度增加2
        }
    }

    // 分配新的字符串空间
    char* new_str = (char*)malloc(new_len + 1); // +1 for null terminator
    if (new_str == NULL)
    {
        perror("Memory allocation failed");
        return NULL;
    }

    // 复制原始字符串到新的字符串，替换空格
    char* new_ptr = new_str;
    for (ptr = str; *ptr; ++ptr)
    {
        if (*ptr == ' ')
        {
            *new_ptr++ = '%';
            *new_ptr++ = '2';
            *new_ptr++ = '0';
        }
        else
        {
            *new_ptr++ = *ptr;
        }
    }
    *new_ptr = '\0'; // 添加字符串结束符

    return new_str;
}

size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
    int written = fwrite(ptr, size, nmemb, (FILE *)stream);
    if(written)
    {
        ota_info.recv_data_time = get_monotonic();
    }
    else
    {
        SPDLOG_DEBUG("__func__{} #1 size:{} nmemb:{} written*size:{} written:{}",__func__,size,nmemb,written*size,written);
    }
    ota_info.ota_pkg_recv_size += written * size;
    if(ota_info.ota_pkg_recv_size < ota_info.revc_message.url_info.fileSize)
    {
        ota_info.program = (double)ota_info.ota_pkg_recv_size / ota_info.revc_message.url_info.fileSize;
        SPDLOG_INFO("__func__{} #1 ota_pkg_recv_size:{} fileSize:{} program:{} recv_data_time:{}",__func__,ota_info.ota_pkg_recv_size,ota_info.revc_message.url_info.fileSize,ota_info.program,ota_info.recv_data_time);
    }
    else
    {
        ota_info.program = 1.;
        SPDLOG_INFO("__func__{} #1 ota_pkg_recv_size:{} fileSize:{} program:{} recv_data_time:{}",__func__,ota_info.ota_pkg_recv_size,ota_info.revc_message.url_info.fileSize,ota_info.program,ota_info.recv_data_time);
    }
    return written * size;
}

void UpdateOta::down_ota_feedback(std::string command,std::string result,double progress,bool flag)
{
    json res;
    res["command"] = command;
    if(!result.empty())
        res["result"] = result;
    if(progress >= 0. && progress <= 1.)
        res["progress"] = progress;
    SPDLOG_INFO("__func__:{} #1 res:{}",__func__,res.dump());
    if(gcode)
        gcode->respond_feedback(res);
    if(flag)
        ota_info.download_falg = false;
}

void UpdateOta::down_ota_pkg()
{
    down_ota_feedback("M2202","2701");

    if(2 != ota_info.detect_falg)
    {
        SPDLOG_ERROR("__func__:{} #1 downloadUrl is not ready",__func__);
        down_ota_feedback("M2202","2705",-1.,true);
        return;
    }
    std::string ota_pkg_url(ota_info.revc_message.url_info.downloadUrl);
    SPDLOG_INFO("__func__:{} #1 ota_pkg_url:{}",__func__,ota_pkg_url);
    if(ota_pkg_url.empty())
    {
        SPDLOG_ERROR("__func__:{} #1 ota_pkg_url.empty",__func__);
        ota_info.download_falg = false;
        return ;
    }

    std::vector<std::string> ver_url = elegoo::common::split(ota_pkg_url,"/");
    if(!ver_url.size())
    {
        SPDLOG_ERROR("__func__:{} #1 ota pkg format is error ",__func__);
        down_ota_feedback("M2202","2705",-1.,true);
        return;
    }
    
    std::string pkg_name = ver_url.back();
    std::string suffix = pkg_name.substr(pkg_name.size() - 4);
    SPDLOG_INFO("__func__:{} suffix:{} pkg_name:{}",__func__,suffix,pkg_name);
    if(suffix == ".sig")
    {
        suffix = pkg_name.substr(pkg_name.size() - 8);
        if(suffix != ".zip.sig")
        {
            SPDLOG_ERROR("__func__:{} #1 ota pkg format is error! suffix:{} pkg_name:{}",__func__,suffix,pkg_name);
            down_ota_feedback("M2202","2705",-1.,true);
            return;
        }
        ota_info.ota_pkg_name = OPT_USR "/" + pkg_name;
        SPDLOG_INFO("__func__:{} #1 ota_pkg_name:{}",__func__,ota_info.ota_pkg_name);
    }
    else if(suffix == ".ipk")
    {
        ota_info.ota_pkg_name = OPT_USR "/" IPK_PKG;
        SPDLOG_INFO("__func__:{} #1 ota_pkg_name:{}",__func__,ota_info.ota_pkg_name);
    }
    else if(suffix == ".swu")
    {
        ota_info.ota_pkg_name = OPT_USR "/" SWU_PKG;
        SPDLOG_INFO("__func__:{} #1 ota_pkg_name:{}",__func__,ota_info.ota_pkg_name);
    }
    else
    {
        SPDLOG_ERROR("__func__:{} #1 ota pkg format is error! suffix:{}",__func__,suffix);
        down_ota_feedback("M2202","2705",-1.,true);
        return;
    }

    FILE *fp = fopen(ota_info.ota_pkg_name.c_str(),"w");
    if(fp == nullptr)
    {
        SPDLOG_ERROR("__func__:{},__LINE__:{},fopen error\n",__func__,__LINE__);
        down_ota_feedback("M2202","2705",-1.,true);
        return;
    }

    reset_sign();
    CURL *curl = curl_easy_init();
    if(curl)
    {
        ota_info.recv_data_time = get_monotonic();
        SPDLOG_INFO("__func__:{} #1 ota_pkg_url:{}",__func__,ota_pkg_url);
        CURLcode res;
        // curl_easy_setopt(curl, CURLOPT_DNS_CACHE_TIMEOUT, 0);
        // curl_easy_setopt(curl, CURLOPT_FRESH_CONNECT, 1L);
        curl_easy_setopt(curl, CURLOPT_DNS_SERVERS, "114.114.114.114,8.8.8.8,8.8.4.4");
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15*60L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 100L);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 15L);
        res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, ota_info.headers);
        res = curl_easy_setopt(curl, CURLOPT_URL, ota_info.revc_message.url_info.downloadUrl);
        res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        // res = curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        res = curl_easy_perform(curl);
        if(res != CURLE_OK)
        {
            SPDLOG_ERROR("curl_easy_perform() failed: {}", curl_easy_strerror(res));
            curl_easy_cleanup(curl);
            fclose(fp);
            down_ota_feedback("M2202","2705",-1.,true);
            return;
        }
        curl_easy_cleanup(curl);
        system("touch /ota_download_over");
        SPDLOG_INFO("down ota file over!");
    }
    else
    {
        SPDLOG_ERROR("__func__:{} #1 curl error",__func__);
        fclose(fp);
        down_ota_feedback("M2202","2705",-1.,true);
        return;
    }

    fclose(fp);
    SPDLOG_INFO("__func__:{} __OK",__func__);
}

size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    char* buffer = (char*)userp;
    size_t realsize = size * nmemb;
    memset(buffer, 0, 4096);
    memcpy(buffer, (char*)contents, realsize < 4096 ? realsize:4096);
    // printf("\n*************************\n");
    // printf("realsize:%d vs 4096! buffer:%s",realsize, (char*)buffer);
    // printf("\n*************************\n");
    return realsize;
}

int32_t UpdateOta::get_sign()
{
    SPDLOG_DEBUG("__func__:{} #1 ",__func__);
    char url[256] = {0};
    char para[256] = {0};
    char signed_data[512];
    char signed_header[512] = {0};
    char key[2048] = {0};

    int ret;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    long long microseconds = tv.tv_sec * 1000000LL + tv.tv_usec;
    microseconds /= 1000;
    char *Model_n = replace_spaces_with_percent20(ota_info.cur_machineModel.c_str());
    snprintf(para, 256, "language=%s&machineModel=%s&sn=%s&timestamp=%lld&version=%s", ota_info.cur_language.c_str(), ota_info.cur_machineModel.c_str(), ota_info.cur_sn.c_str(), microseconds, ota_info.cur_current_version.c_str());
    SPDLOG_DEBUG("__func__:{} #1 cur_language:{} {} {} {} {}",__func__,ota_info.cur_language,ota_info.cur_machineModel,ota_info.cur_sn,microseconds,ota_info.cur_current_version);

    strncpy(para, url_Encode(para), 256);
#if 0
    snprintf(url, 256, "https://test.elegoo.com.cn/api/tp/ota/fdm/checkUpdate?language=%s&machineModel=%s&sn=%s&timestamp=%lld&version=%s", ota_info.cur_language.c_str(), Model_n, ota_info.cur_sn.c_str(), microseconds, ota_info.cur_current_version.c_str());
#else
    snprintf(url, 256, "https://test.elegoo.com/api/tp/ota/fdm/checkUpdate?language=%s&machineModel=%s&sn=%s&timestamp=%lld&version=%s", ota_info.cur_language.c_str(), Model_n, ota_info.cur_sn.c_str(), microseconds, ota_info.cur_current_version.c_str());
#endif
    // printf("\nurl_len:%d \nurl:%s\n",strlen(url),url);

    SPDLOG_DEBUG("otakey_dir:{}",OPT_INST "/otakey");
    strncpy(key, readFileAndAddHeadTail(OPT_INST "/otakey", "-----BEGIN RSA PRIVATE KEY-----\n", "\n-----END RSA PRIVATE KEY-----"), 2048);

    // printf("\npara_len:%d \npara:%s\n",strlen(para),para);
    // printf("key_len:%d \nkey:%s\n",strlen(key),key);
    SPDLOG_DEBUG("key_len:{} \nkey:{}",strlen(key),std::string(key));
    strncpy(signed_data, sign(para, strlen(para), key, strlen(key)), 512);
    if (signed_data == NULL)
    {
        perror("Error signed_data");
        return -1;
    }

    // printf("signed_data_len:%d \nsigned_data:%s\n",strlen(signed_data),signed_data);
    snprintf(signed_header, 512, "sign:%s", signed_data);
    // printf("signed_header_len:%d \nsigned_header:%s\n",strlen(signed_header),signed_header);

    // printf("\nURL:%s\n",url);
    // printf("\nHEADRES:%s\n",signed_header);
    ota_info.signed_header = signed_header;
    ota_info.url = url;
    SPDLOG_DEBUG("__func__:{} #1 url:{}",__func__,ota_info.url);
    SPDLOG_DEBUG("__func__:{} #1 signed_header:{}",__func__,ota_info.signed_header);
}

int8_t UpdateOta::reset_sign()
{
    if(ota_info.reset_first || get_monotonic() - ota_info.start_sign_time >= 2*OTA_DETECT_WAIT_TIME)
    {
        SPDLOG_DEBUG("__func__:{} #1 ota_info.start_sign_time:{}",__func__,ota_info.start_sign_time);
        std::lock_guard<std::mutex> lock(ota_info.lock);  // 自动管理锁的作用域
        ota_info.reset_first = false;
        if(ota_info.headers)
        {
            curl_slist_free_all(ota_info.headers);
            ota_info.headers = nullptr;
        }
        get_sign();
        ota_info.headers = curl_slist_append(ota_info.headers, ota_info.signed_header.c_str());
        ota_info.start_sign_time = get_monotonic();
    }

    return 0;
}

void UpdateOta::download_proc()
{
    //长时间接收不到数据，则失败退出
    double difftime = get_monotonic() - ota_info.recv_data_time;
    if(ota_info.program < 1. && difftime > 30)
    {
        SPDLOG_ERROR("__func__:{} #1 recv data nul for a long time! recv_data_time:{} difftime:{}",__func__,ota_info.recv_data_time,difftime);
        down_ota_feedback("M2202","2705",-1.,true);
        return ;
    }
    // 升级文件下载完成，进行升级处理
    if (access("/ota_download_over", F_OK) == 0)
    {
        SPDLOG_INFO("ota_download_over is exist");
        //清除下载完成标志
        system("rm /ota_download_over -rf");
        //反馈ota下载进度为1.
        down_ota_feedback("OTA_DOWNLOAD","",1.);
        //反馈ota解压中
        down_ota_feedback("M2202","2702");
        //md5sum 检测
        std::string md5sum_result = popen_exec("md5sum " + ota_info.ota_pkg_name);
        std::vector<std::string> ver_md5 = elegoo::common::split(md5sum_result," ");
        if(ver_md5.at(0) == std::string(ota_info.revc_message.url_info.fileMd5))// && ver_md5.size() == 2)
        {
            SPDLOG_INFO("__func__:{} #1 md5sum is right! ver_md5.size:{} md5sum_result:{} fileMd5:{}",__func__,ver_md5.size(),md5sum_result,std::string(ota_info.revc_message.url_info.fileMd5));
            // 解签名
            std::string suffix = ota_info.ota_pkg_name.substr(ota_info.ota_pkg_name.size() - 8);
            if(suffix == ".zip.sig")
            {
                if(parse_sig_file())
                {
                    SPDLOG_ERROR("__func__:{} #1 parse .zip.sig file faied",__func__);
                    down_ota_feedback("M2202","2705",-1.,true);
                    return ;
                }
            }
            //反馈ota更新中
            down_ota_feedback("M2202","2703");
            std::string update_ota_action = "update_ipk.sh ota "  + ota_info.ota_pkg_name + ";sync";
            SPDLOG_INFO("__func__:{} #1 update_ota_action:{}",__func__,update_ota_action);
            //执行升级动作
            system(update_ota_action.c_str());
            //反馈ota完成
            down_ota_feedback("M2202","2704",-1.,true);
            //清除overlay分区
            system("fw_setenv parts_clean rootfs_data");
            SPDLOG_INFO("parts_clean rootfs_data;sleep 1;reboot");
            //重启
            system("sleep 1;reboot");
        }
        else
        {
            SPDLOG_ERROR("__func__:{} #1 md5sum is error! ver_md5.size:{} md5sum_result:{} fileMd5:{}",__func__,ver_md5.size(),md5sum_result,std::string(ota_info.revc_message.url_info.fileMd5));
            down_ota_feedback("M2202","2705",-1.,true);
            return;
        }
    }
    else
    {
        //反馈ota下载进度
        down_ota_feedback("OTA_DOWNLOAD","", ota_info.program);
    }
}

int UpdateOta::detect_ota_pkg()
{
    SPDLOG_INFO("__func__:{} #1",__func__);
    CURL *ota_curl = nullptr;
    bool first_detect_flag = false;
    bool is_net_connected = false;
    double net_status_det_cnt = 0;
    timeval timeout;
    timeout.tv_sec = SELECTT_TIME_OUT_SEC;
    timeout.tv_usec = 0;
    
    while (ota_info.detect_falg)
    {
        fd_set readfds;
        FD_ZERO(&readfds);

        double monotonic = get_monotonic();
        int ret = select(0, &readfds, nullptr, nullptr, &timeout);
        // SPDLOG_INFO("__func__:{} #1 diff_monotonic:{}",__func__,get_monotonic() - monotonic);
    
        if (ret == 0) 
        {
            // 升级文件下载过程中
            if (ota_info.download_falg)
            {
                // 500ms 检测一次
                timeout.tv_sec = 0;
                timeout.tv_usec = SELECTT_TIME_OUT_USEC;
                download_proc();
                continue;
            }
            
            if(!is_net_connected)
            {
                // 2s 检测一次
                timeout.tv_sec = SELECTT_TIME_OUT_SEC;
                timeout.tv_usec = 0;
                
#if 1
                if(++net_status_det_cnt * SELECTT_TIME_OUT_SEC >= OTA_DETECT_WAIT_TIME)
                {
                    SPDLOG_DEBUG("__func__:{} #1 select_time_out:{}*net_status_det_cnt:{}",__func__,SELECTT_TIME_OUT_SEC,net_status_det_cnt);net_status_det_cnt = 0;
                    //网络状态同步
                    down_ota_feedback("M2202","2602");
                }

                if(ota_info.net_status == "")
                {
                    SPDLOG_DEBUG("net_status:{} is_net_connected:{}",ota_info.net_status,is_net_connected);
                    continue;
                }
                else if(ota_info.net_status == "connected")
                {
                    is_net_connected = true;
                    SPDLOG_INFO("net_status:{} is_net_connected:{}",ota_info.net_status,is_net_connected);
                }
                else
                {
                    SPDLOG_DEBUG("net_status:{} is_net_connected:{}",ota_info.net_status,is_net_connected);
                    continue;
                }
#else
                std::string result = popen_exec("wifid -t");
                if (result.find("NETWORK_CONNECTED") != std::string::npos)
                {
                    is_net_connected = true;
                    SPDLOG_INFO("result:{} is_net_connected:{}",result,is_net_connected);
                }
                else
                {
                    is_net_connected = false;
                    SPDLOG_DEBUG("result:{} is_net_connected:{}",result,is_net_connected);
                    continue;
                }
#endif
            }

            if(ota_curl)
            {
                // 2s 检测一次
                timeout.tv_sec = SELECTT_TIME_OUT_SEC;
                timeout.tv_usec = 0;
                
                if(first_detect_flag || ++ota_info.select_cnt * SELECTT_TIME_OUT_SEC >= OTA_DETECT_WAIT_TIME)
                {
                    first_detect_flag = false;
                    if(get_revc_message(ota_curl))
                    {
                        SPDLOG_ERROR("get_revc_message error!",__func__);
                        // is_net_connected = false;
                        ota_info.reset_first = true;
                        if(ota_curl)
                            curl_easy_cleanup(ota_curl);
                        ota_curl = nullptr;
                    }
                }
            }
            else
            {
                ota_curl = curl_easy_init();
                if(ota_curl)
                {
                    // is_net_connected = true;
                    first_detect_flag = true;
                }
                SPDLOG_INFO("first_detect_flag:{}",__func__,first_detect_flag);
            }
        }
    }
    if(ota_curl)
        curl_easy_cleanup(ota_curl);
    SPDLOG_INFO("__func__:{} #1 quit detect ota pkg!",__func__);
    
    return 0;
}

std::string UpdateOta::skip_sig_obtain_zip_file(FILE *inputFile)
{
    if(!inputFile)
    {
        return "";
    }
    //
    std::string zipFileName = ota_info.ota_pkg_name.substr(0,ota_info.ota_pkg_name.size() - 4);
    FILE *outputFile = fopen(zipFileName.c_str(), "wb");
    if (outputFile == NULL)
    {
        SPDLOG_ERROR("Cannot open {} for reading",zipFileName);
        fclose(inputFile);
        return "";
    }
    // 读取剩余内容并写入输出文件
    unsigned char fileBuffer[4096]; // 缓冲区大小
    size_t bytesRead;
    while ((bytesRead = fread(fileBuffer, 1, sizeof(fileBuffer), inputFile)) > 0)
    {
        fwrite(fileBuffer, 1, bytesRead, outputFile);
    }
    
    fclose(outputFile);
    return zipFileName;
}

std::string UpdateOta::get_zip_file()
{
    FILE *inputFile = fopen(ota_info.ota_pkg_name.c_str(), "rb");
    if (inputFile == NULL)
    {
        SPDLOG_ERROR("Cannot open {} for reading",ota_info.ota_pkg_name);
        return "";
    }
    // 读取签名
    elegoo_sign_t elegoo_sign = {};
    int32_t skipBytes = sizeof(elegoo_sign_t);
    if (fread(&elegoo_sign, 1, skipBytes, inputFile) != skipBytes)
    {
        SPDLOG_ERROR("Failed to read sig");
        fclose(inputFile);
        return "";
    }
    // 签名检查
    if(elegoo_sign.package_type != 4 
        || elegoo_sign.machine_type != 1
        || elegoo_sign.machine_model != 2)
    {
        SPDLOG_ERROR("Signature matching failed");
        fclose(inputFile);
        return "";
    }
    // 跳过签名获取zip文件
    std::string zipFileName = skip_sig_obtain_zip_file(inputFile);
    fclose(inputFile);
    return zipFileName;
}

int32_t UpdateOta::extract_one_zip_file(std::string extractFileName,unzFile& zipfile)
{
    if(!zipfile)
    {
        return -1;
    }
    //
    FILE *outputFile = fopen(extractFileName.c_str(), "wb");
    if (outputFile == NULL)
    {
        SPDLOG_ERROR("Cannot open {} for reading",extractFileName);
        return -1;
    }
    //
    char buffer[4096];
    int bytesRead;
    while ((bytesRead = unzReadCurrentFile(zipfile, buffer, sizeof(buffer))) > 0)
    {
        fwrite(buffer, 1, bytesRead, outputFile);
    }
    SPDLOG_INFO("__func__:{} extractFileName:{}",__func__,extractFileName);
    fclose(outputFile);
    return 0;
}

std::pair<std::string,std::string> UpdateOta::parse_json_file(std::string jsonName,unzFile& zipfile)
{
    if(!zipfile)
    {
        SPDLOG_ERROR("!zipfile");
        return {};
    }
    std::string pkgname = "";
    std::string pkghash = "";
    // 解压json文件成功
    if(access(jsonName.c_str(),F_OK) != 0)
    {
        SPDLOG_ERROR("{} is not exist",jsonName);
        return {};
    }
    // 解签名json文件
    std::string name_desig = jsonName.substr(0,jsonName.size() - 4);
    std::string dec_cmd = "decrypt " + jsonName + " " + name_desig + " /opt/inst/sigtools/public_key.pem /opt/inst/sigtools/aes_key.key";
    std::string dec_result = popen_exec(dec_cmd);
    SPDLOG_INFO("dec_cmd:{} \ndec_result:{}",dec_cmd,dec_result);
    popen_exec("rm " + jsonName + " -rf  2>/dev/null");
    // 解析json文件
    if(access(name_desig.c_str(),F_OK) == 0)
    {
        SPDLOG_INFO("name_desig:{}",name_desig);
        std::ifstream infile(name_desig,std::ios::binary);
        if (!infile.is_open())
        {
            SPDLOG_ERROR("Cannot open {} for reading",name_desig);
            return {};
        }

        json jsonData;
        try
        {
            infile >> jsonData;
            infile.close();
            popen_exec("rm " + name_desig + " -rf  2>/dev/null");
            SPDLOG_INFO("jsonData.dump:{}",jsonData.dump());
            if(jsonData["version"].get<std::string>() != std::string(ota_info.revc_message.url_info.ver))
            {
                SPDLOG_ERROR("version is error! {} {}",jsonData["version"].get<std::string>(),std::string(ota_info.revc_message.url_info.ver));
                // return {};
            }
            json packages = jsonData["packages"];
            for(auto ii = 0; ii < packages.size();++ii)
            {
                std::string package_name = packages[ii]["file"].get<std::string>();
                std::string suffix = package_name.substr(package_name.size() - 8);
                if(suffix != ".swu.sig")
                {
                    continue;
                }
                // 获取镜像文件名和哈希值
                pkgname = package_name;
                pkghash = packages[ii]["hash"].get<std::string>();
                SPDLOG_INFO("package_name:{} pkghash:{}",package_name,pkghash);
                break;
            }
        }
        catch(...)
        {
            SPDLOG_ERROR("{} convert to json failed",name_desig);
            infile.close();
            return {};
        }
    }
    else
    {
        SPDLOG_ERROR("{} is not exist",name_desig);
        return {};
    }
    
    return {pkgname,pkghash};
}

std::tuple<std::string,std::string,std::string> UpdateOta::extract_zip_file(std::string zipFileName)
{
    std::string basename = popen_exec("basename " + zipFileName);
    std::string fileDir = zipFileName.substr(0,zipFileName.size() - basename.size());
    unzFile zipfile = unzOpen(zipFileName.c_str());
    if(!zipfile)
    {
        SPDLOG_ERROR("{} is not supported zip",zipFileName);
        return {};
    }

    std::string pkgname = "";
    std::string pkghash = "";
    std::string swupkg = "";
    // 遍历 ZIP 文件中的每个文件
    char filename[256];
    if (unzGoToFirstFile(zipfile) == UNZ_OK) 
    {
        do {
            unz_file_info fileInfo;
            // 获取当前文件的信息
            if (unzGetCurrentFileInfo(zipfile, &fileInfo, filename, sizeof(filename), NULL, 0, NULL, 0) == UNZ_OK) 
            {
                // 如果是文件，则解压
                if (!(fileInfo.flag & 0x0001)) // 检查是否是文件夹（通过标志位判断）
                {
                    std::string fileName = std::string(filename);
                    // 打开当前文件
                    if (fileName == "ota-package-list.json.sig" && unzOpenCurrentFile(zipfile) == UNZ_OK)
                    {
                        SPDLOG_INFO("Extracting {} ...",filename);
                        // 解压json文件
                        std::string jsonName = fileDir + "/" + fileName;
                        if(extract_one_zip_file(jsonName,zipfile))
                        {
                            unzCloseCurrentFile(zipfile);
                            unzClose(zipfile);
                            return {};
                        }
                        unzCloseCurrentFile(zipfile);
                        //
                        if(access(jsonName.c_str(),F_OK) != 0)
                        {
                            SPDLOG_ERROR("{} is not exist",jsonName);
                            unzClose(zipfile);
                            return {};
                        }
                        // 解析json文件
                        std::pair<std::string,std::string> pkg_pair = parse_json_file(jsonName,zipfile);
                        pkgname = pkg_pair.first;
                        pkghash = pkg_pair.second;
                        if(pkgname.empty() || pkghash.empty())
                        {
                            SPDLOG_ERROR("parse json file is failed");
                            unzClose(zipfile);
                            return {};
                        }
                    }
                    else if (fileName.substr(fileName.size() - 8) == ".swu.sig" && unzOpenCurrentFile(zipfile) == UNZ_OK)
                    {
                        SPDLOG_INFO("Extracting {} ...",filename);
                        std::string extractFileName = fileDir + "/" + fileName;
                        //
                        if(extract_one_zip_file(extractFileName,zipfile))
                        {
                            unzCloseCurrentFile(zipfile);
                            unzClose(zipfile);
                            return {};
                        }
                        unzCloseCurrentFile(zipfile);
                        //
                        if(access(extractFileName.c_str(),F_OK) != 0)
                        {
                            SPDLOG_ERROR("{} is not exist",extractFileName);
                            unzClose(zipfile);
                            return {};
                        }
                        swupkg = extractFileName;
                        SPDLOG_INFO("swupkg:{}",swupkg);
                    }
                }
            }
        } while (unzGoToNextFile(zipfile) == UNZ_OK);
    }
    unzClose(zipfile);
    popen_exec("rm " + zipFileName + " -rf  2>/dev/null");

    return {swupkg, fileDir + "/" + pkgname, pkghash};
}

int32_t UpdateOta::parse_swu_pkg(std::tuple<std::string,std::string,std::string> pkg_tuple)
{
    std::string swupkg = std::get<0>(pkg_tuple);
    std::string pkgname = std::get<1>(pkg_tuple);
    std::string pkghash = std::get<2>(pkg_tuple);
    if(swupkg.empty() || pkgname.empty() || pkghash.empty())
    {
        SPDLOG_ERROR("get ota pkg error! {} {} {}",swupkg,pkgname,pkghash);
        return -1;
    }
    std::string hash_cmd = "openssl dgst -sha256 -r " + swupkg + " | awk '{print $1}' | tr -d '\n'";
    std::string package_hash = popen_exec(hash_cmd);
    SPDLOG_INFO("hash_cmd:{}",hash_cmd);
    SPDLOG_INFO("package_hash:{} {}",package_hash,package_hash.size());
    SPDLOG_INFO("pkghash:{} {}",pkghash,pkghash.size());
    if(package_hash != pkghash)
    {
        SPDLOG_ERROR("pkgname:{} hash is error! pkgname.size:{} hash {} {}",pkgname,pkgname.size(),package_hash,pkghash);
        return -1;
    }
    
    if(pkgname.substr(pkgname.size() - 8) != ".swu.sig")
    {
        SPDLOG_ERROR("{} is error",pkgname);
        return -1;
    }
    //
    std::string pkg_name_desig = pkgname.substr(0,pkgname.size() - 4);
    std::string dec_cmd = "decrypt " + pkgname + " " + pkg_name_desig + " /opt/inst/sigtools/public_key.pem /opt/inst/sigtools/aes_key.key";
    std::string dec_result = popen_exec(dec_cmd);
    SPDLOG_INFO("dec_cmd:{} \ndec_result:{}",dec_cmd,dec_result);
    popen_exec("rm " + pkgname + " -rf  2>/dev/null");
    if(access(pkg_name_desig.c_str(),F_OK) == 0)
    {
        std::string swu_name = "mv " + pkg_name_desig + " " + OPT_USR "/" SWU_PKG;
        system(swu_name.c_str());
        ota_info.ota_pkg_name = OPT_USR "/" SWU_PKG;
        SPDLOG_INFO("__func__:{} pkg_name_desig:{} ota_pkg_name:{}",__func__,pkg_name_desig,ota_info.ota_pkg_name);
    }
    else
    {
        SPDLOG_ERROR("{} is not exist",pkg_name_desig);
        return -1;
    }

    return 0;
}

int32_t UpdateOta::parse_sig_file()
{
    //开始不验签直接进行zip解压，后续通过解压的json进行验签
    //1. 获取zip文件
    std::string zipFileName = get_zip_file();
    //2.获取zip文件后删除原文件
    popen_exec("rm " + ota_info.ota_pkg_name + " -rf  2>/dev/null");
    //3.解压zip文件
    std::tuple<std::string,std::string,std::string> pkg_tuple = extract_zip_file(zipFileName);
    //4.解析升级包
    return parse_swu_pkg(pkg_tuple);
}

int32_t UpdateOta::unzip_file()
{
    unzFile zipfile = unzOpen(ota_info.ota_pkg_name.c_str());
    if(!zipfile)
    {
        SPDLOG_ERROR("{} is not supported zip",ota_info.ota_pkg_name);
        return -1;
    }

    unzClose(zipfile);

    return 0;
}

std::string UpdateOta::popen_exec(std::string cmd)
{
    FILE* pipe = popen(cmd.c_str(), "r");
    if (pipe) 
    {
        char buffer[128] = {};
        if(fgets(buffer, sizeof(buffer), pipe) == nullptr) 
        {
            SPDLOG_INFO("popen_exec {} buffer is nul",cmd);
        }
        else
        {
            SPDLOG_INFO("popen_exec {} buffer:{}",cmd,std::string(buffer));
        }
        pclose(pipe);
        return std::string(buffer);
    }
    else
    {
        SPDLOG_ERROR("popen {} failed",cmd);
    }
    return {};
}

int UpdateOta::get_revc_message(CURL *ota_curl)
{
    SPDLOG_DEBUG("__func__:{} #1 select_time_out:{}*select_cnt:{}",__func__,SELECTT_TIME_OUT_SEC,ota_info.select_cnt);
    ota_info.select_cnt = 0;
    if(ota_info.cur_language.empty())
    {
        return 0;
    }

    reset_sign();

    if(!ota_curl || !ota_info.headers)
    {
        SPDLOG_ERROR("!ota_curl || !ota_info.headers");
        return -1;
    }
    
    char readBuffer[4096] = {0};
    CURLcode res;
    // curl_easy_setopt(ota_curl, CURLOPT_DNS_CACHE_TIMEOUT, 0);
    // curl_easy_setopt(ota_curl, CURLOPT_FRESH_CONNECT, 1L);
    curl_easy_setopt(ota_curl, CURLOPT_DNS_SERVERS, "114.114.114.114,8.8.8.8,8.8.4.4");
    // curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15*60L);
    curl_easy_setopt(ota_curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(ota_curl, CURLOPT_LOW_SPEED_LIMIT, 100L);
    curl_easy_setopt(ota_curl, CURLOPT_LOW_SPEED_TIME, 15L);
    res = curl_easy_setopt(ota_curl, CURLOPT_HTTPHEADER, ota_info.headers);
    res = curl_easy_setopt(ota_curl, CURLOPT_URL, ota_info.url.c_str());
    res = curl_easy_setopt(ota_curl, CURLOPT_WRITEFUNCTION, write_callback);
    res = curl_easy_setopt(ota_curl, CURLOPT_WRITEDATA, readBuffer);
    res = curl_easy_setopt(ota_curl, CURLOPT_SSL_VERIFYPEER, 0L);
    res = curl_easy_setopt(ota_curl, CURLOPT_SSL_VERIFYHOST, 0L);
    // res = curl_easy_setopt(ota_curl, CURLOPT_VERBOSE, 1L);
    // printf("ota_info.headers:%s\n",ota_info.headers->data);
    res = curl_easy_perform(ota_curl);
    if(res == CURLE_COULDNT_RESOLVE_HOST)
    {
        SPDLOG_ERROR("curl_easy_perform() failed: {}",std::string(curl_easy_strerror(res)));
        return -1;
    }
    else if(res != CURLE_OK)
    {
        SPDLOG_ERROR("curl_easy_perform() failed: {}",std::string(curl_easy_strerror(res)));
        return -1;
    }

    return parse_revc_message(std::string(readBuffer));
}

int32_t UpdateOta::write_update_info(std::string update_info,std::string file_name)
{
    SPDLOG_INFO("__func__:{} #1 ",__func__);
    if(update_info.empty())
    {
        SPDLOG_ERROR("__func__:{},__LINE__:{},update_info.empty\n",__func__,__LINE__);
        return -1;
    }
    
    FILE *fp = fopen(file_name.c_str(),"w");
    if(fp == nullptr)
    {
        SPDLOG_ERROR("__func__:{},__LINE__:{},fopen {} error\n",__func__,__LINE__,file_name);
        return -1;
    }

    int32_t written = fwrite(update_info.c_str(), 1, update_info.size(), fp);
    SPDLOG_DEBUG("__func__:{} #1 written:{} size:{} update_info:{}",__func__,written,update_info.size(),update_info);

    if(written != update_info.size())
    {
        SPDLOG_ERROR("write update_info to {} failed",file_name);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    SPDLOG_INFO("write update_info to {} sucess",file_name);

    return 0;
}

int32_t UpdateOta::parse_update_info(std::string update_info)
{
    SPDLOG_INFO("__func__:{} #1 ",__func__);
    update_info = elegoo::common::strip(update_info,"\n");
    // SPDLOG_INFO("__func__:{} #1 update_info:\n{}",__func__,update_info);
    std::vector<std::string> ver_0 = elegoo::common::split(update_info,"[");
    // SPDLOG_INFO("__func__:{} #1 ver_0.size:{}",__func__,ver_0.size());
    for(auto info : ver_0)
    {
        if(info.empty())
        {
            continue;
        }
        // SPDLOG_INFO("__func__:{} #1 info:\n{}",__func__,info);
        std::vector<std::string> ver_1 = elegoo::common::split(info,"]");
        if(2 != ver_1.size())
        {
            SPDLOG_ERROR("parse update_info failed! ver_1.size:{} ver_1[0]:{}",ver_1.size(),ver_1[0]);
            continue;
        }

        std::vector<std::string> ver_2 = elegoo::common::split(ver_1[0]," ");
        if(2 != ver_2.size())
        {
            SPDLOG_ERROR("parse update_info failed! ver_2.size:{} ver_2[0]:{}",ver_2.size(),ver_2[0]);
            continue;
        }
        
        for(auto ii = 0; ii < E_VER_MAX; ++ii)
        {
            if(ver_2[0] == ver_update_info[ii].ver_name)
            {
                ver_update_info[ii].ver_num = ver_2[1];
                ver_update_info[ii].ver_content = ver_1[1];
                SPDLOG_INFO("__func__:{} #1 ver_name:{} ver_num:{} ver_content:\n{}",__func__,ver_update_info[ii].ver_name,ver_update_info[ii].ver_num,ver_update_info[ii].ver_content);
                break;
            }
        }
    }

    return 0;
}

int32_t UpdateOta::json_parse_update_info(std::string update_info)
{
    SPDLOG_INFO("__func__:{} #1 ",__func__);

    return 0;
}

int32_t UpdateOta::parse_revc_message(std::string readBuffer)
{
    SPDLOG_DEBUG("__func__:{} #1 readBuffer:{}",__func__,readBuffer);
    json read_json = json::parse(readBuffer);

    if (read_json.empty())
    {
        SPDLOG_ERROR("read_json is empty");
        ota_info.revc_message.code = -1;
        return -1;
    }

    json tmp = read_json["code"];
    if (read_json.contains("code") && tmp.is_number())
    {
        ota_info.revc_message.code = tmp.get<int>();
        SPDLOG_INFO("__func__:{} #1 code:{}",__func__,ota_info.revc_message.code);
    }
    else if (read_json.contains("code") && tmp.is_string())
    {
        ota_info.revc_message.code = std::stoi(tmp.get<std::string>());
        SPDLOG_INFO("__func__:{} #1 code:{}",__func__,ota_info.revc_message.code);
    }

    tmp = read_json["message"];
    if (read_json.contains("message") && tmp.is_string())
    {
        if(!tmp.empty() && std::string(ota_info.revc_message.message) != tmp)
        {
            memset(ota_info.revc_message.message,0,sizeof(ota_info.revc_message.message));
            strcpy(ota_info.revc_message.message, tmp.get<std::string>().c_str());
            SPDLOG_INFO("__func__:{} #1 message:{}",__func__,std::string(ota_info.revc_message.message));
        }
    }

    if (ota_info.revc_message.code != 200)
    {
        SPDLOG_ERROR("get download url failed! code:{}\n", ota_info.revc_message.code);
        return -1;
    }

    std::string tmp_str;
    json data = read_json["data"];
    if (!data.empty())
    {
        tmp = data["version"];
        if (data.contains("version") && tmp.is_string())
        {
            tmp_str = tmp.get<std::string>();
            if(!tmp_str.empty() && std::string(ota_info.revc_message.url_info.ver) != tmp_str)
            {
                memset(ota_info.revc_message.url_info.ver,0,sizeof(ota_info.revc_message.url_info.ver));
                strncpy(ota_info.revc_message.url_info.ver, tmp_str.c_str(), tmp_str.size());
                SPDLOG_INFO("__func__:{} #1 ver:{}",__func__,std::string(ota_info.revc_message.url_info.ver));
            }
        }

        tmp = data["info"];
        if (data.contains("info") && tmp.is_string())
        {
            tmp_str = tmp.get<std::string>();
            if(!tmp_str.empty() && std::string(ota_info.revc_message.url_info.info) != tmp_str)
            {
                SPDLOG_DEBUG("__func__:{} #1 info:{}",__func__,tmp_str);
                // parse_update_info(tmp_str);
                if(write_update_info(tmp_str,UPDATE_INFO_FILE))
                {
                    return -1;
                }
                else
                {
                    down_ota_feedback("M2202","2601");
                }
                memset(ota_info.revc_message.url_info.info,0,sizeof(ota_info.revc_message.url_info.info));
                strncpy(ota_info.revc_message.url_info.info, tmp_str.c_str(), tmp_str.size());
                SPDLOG_INFO("__func__:{} #1 info:\n{}",__func__,std::string(ota_info.revc_message.url_info.info));
            }
        }

        tmp = data["downloadUrl"];
        if (data.contains("downloadUrl") && tmp.is_string())
        {
            tmp_str = tmp.get<std::string>();
            if(!tmp_str.empty() && std::string(ota_info.revc_message.url_info.downloadUrl) != tmp_str)
            {
                memset(ota_info.revc_message.url_info.downloadUrl,0,sizeof(ota_info.revc_message.url_info.downloadUrl));
                strncpy(ota_info.revc_message.url_info.downloadUrl, tmp_str.c_str(), tmp_str.size());
                ota_info.detect_falg = 2;
                SPDLOG_INFO("__func__:{} #1 detect_falg:{} downloadUrl:{}",__func__,ota_info.detect_falg,std::string(ota_info.revc_message.url_info.downloadUrl));
            }
        }

        tmp = data["releaseTime"];
        if (data.contains("releaseTime") && tmp.is_number() && ota_info.revc_message.url_info.releaseTime != tmp.get<int64_t>())
        {
            ota_info.revc_message.url_info.releaseTime = tmp.get<int64_t>();
            SPDLOG_INFO("__func__:{} #1 releaseTime:{} tmp_str:{}",__func__,ota_info.revc_message.url_info.releaseTime,tmp_str);
        }

        tmp = data["fileSize"];
        if (data.contains("fileSize") && tmp.is_number() && ota_info.revc_message.url_info.fileSize != tmp.get<int32_t>())
        {
            ota_info.revc_message.url_info.fileSize = tmp.get<int32_t>();
            SPDLOG_INFO("__func__:{} #1 fileSize:{} tmp_str:{}",__func__,ota_info.revc_message.url_info.fileSize,tmp_str);
        }

        tmp = data["fileMd5"];
        if (data.contains("fileMd5") && tmp.is_string())
        {
            tmp_str = tmp.get<std::string>();
            if(!tmp_str.empty() && std::string(ota_info.revc_message.url_info.fileMd5) != tmp_str)
            {
                memset(ota_info.revc_message.url_info.fileMd5,0,sizeof(ota_info.revc_message.url_info.fileMd5));
                strncpy(ota_info.revc_message.url_info.fileMd5, tmp_str.c_str(), tmp_str.size());
                SPDLOG_INFO("__func__:{} #1 tmp_str.size:{} tmp_str:{} fileMd5:{}",__func__,tmp_str.size(),tmp_str,std::string(ota_info.revc_message.url_info.fileMd5));
            }
        }
    }

    return 0;
}

void UpdateOta::update_ota_thread()
{
    if(ota_info.headers)
    {
        SPDLOG_INFO("__func__:{} #1",__func__);
        ota_info.download_falg = true;
        ota_info.ota_pkg_recv_size = 0;
        ota_info.program = 0.;
        std::unique_ptr<std::thread> ota_thread(new std::thread(&UpdateOta::down_ota_pkg,this));
        ota_thread->detach();
    }
    else
    {
        SPDLOG_ERROR("__func__:{} #1 headers is nul",__func__);
        down_ota_feedback("M2202","2705",-1.,true);
    }
}

void UpdateOta::detect_ota_thread()
{
    SPDLOG_INFO("__func__:{} #1",__func__);
    ota_info.detect_falg = 1;
    std::unique_ptr<std::thread> detect_thread(new std::thread(&UpdateOta::detect_ota_pkg,this));
    detect_thread->detach();
}

std::shared_ptr<UpdateOta> update_ota_load_config(std::shared_ptr<ConfigWrapper> config)
{
    SPDLOG_INFO("__func__:{} #1",__func__);
    std::shared_ptr<UpdateOta> update_ota = std::make_shared<UpdateOta>(config);
    return update_ota;
}
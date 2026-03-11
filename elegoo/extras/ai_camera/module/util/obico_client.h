// obico_client.h
#pragma once

#include <string>
#include <vector>
#include "curl/curl.h"
#include "json.hpp"

using json = nlohmann::json;

// 内存数据结构
struct CurlReadData {
    const char* data;
    size_t size;
};

class HttpClient {
public:
    HttpClient(const std::string& token);
    ~HttpClient();

    // 设置SSL验证选项
    void SetSSLVerify(bool verify) { ssl_verify_ = verify; }
    void SetCACertPath(const std::string& path) { ca_cert_path_ = path; }

    json ExecuteRequest(const std::string& url,
                        const std::string& method,
                        const std::map<std::string, std::string>* params = nullptr,
                        const std::map<std::string, std::string>* form_data = nullptr,
                        const CurlReadData* file_data = nullptr);

private:
    struct curl_slist* GetHeaders();
    void SetQueryParams(void* curl, const std::map<std::string, std::string>& params);

    std::string auth_token_;
    bool ssl_verify_ = true;          // 默认启用SSL验证
    std::string ca_cert_path_;         // 自定义CA证书路径
};

// Super Detection API 封装类
class SuperDetectionAPI {
public:
    SuperDetectionAPI(const std::string& token, bool staging = false);
    
    // 设置SSL验证选项
    void SetSSLVerify(bool verify) { client_.SetSSLVerify(verify); }
    void SetCACertPath(const std::string& path) { client_.SetCACertPath(path); }
    
    json PredictFailure(const std::string& printer_id, const std::string& print_id, const unsigned char* pic_buf, int pic_length);

private:
    std::string base_url_;
    HttpClient client_;
};

// 检测结果数据结构
struct DetectionResult {
    double confidence;
    double x_center;
    double y_center;
    double width;
    double height;
};

struct TemporalStats {
    double ewm_mean;
    double rolling_mean_short;
    double rolling_mean_long;
    int prediction_num;
    int prediction_num_lifetime;
};

struct PredictionResult {
    double p;
    TemporalStats temporal_stats;
    std::vector<DetectionResult> detections;
};

// 解析API响应
PredictionResult ParsePredictionResponse(const json& json_data);

int SuperDetectionTest(const std::string& token, const std::string& printer_id, const std::string& print_id, const unsigned char* pic_buf, int pic_length, json &j_respond);
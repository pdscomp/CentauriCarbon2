// obico_client.cpp
#include "obico_client.h"
#include <iostream>
#include <stdexcept>
#include "spdlog/spdlog.h"

// 回调函数处理HTTP响应数据
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

HttpClient::HttpClient(const std::string& token) 
    : auth_token_("Token " + token) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

HttpClient::~HttpClient() {
    curl_global_cleanup();
}

// 执行HTTP请求
json HttpClient::ExecuteRequest(const std::string& url,
                    const std::string& method,
                    const std::map<std::string, std::string>* params,
                    const std::map<std::string, std::string>* form_data,
                    const CurlReadData* file_data) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize curl");
    }

    std::string response_string;
    long http_code = 0;
    json result;
    struct curl_httppost* formpost = nullptr;
    struct curl_httppost* lastptr = nullptr;

    // 设置基本选项
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, GetHeaders());
    
    // 设置SSL验证选项
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, ssl_verify_ ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, ssl_verify_ ? 2L : 0L);
    
    if (!ca_cert_path_.empty()) {
        curl_easy_setopt(curl, CURLOPT_CAINFO, ca_cert_path_.c_str());
    }

    // 设置请求方法
    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
    } else if (method == "PUT") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    } else if (method == "GET" && params) {
        SetQueryParams(curl, *params);
    }

    // 处理表单数据和文件上传 (使用旧版curl_formadd API)
    if ((method == "POST" || method == "PUT") && (form_data || file_data)) {
        // 创建表单
        if (form_data) {
            for (const auto& field : *form_data) {
                curl_formadd(&formpost, &lastptr,
                             CURLFORM_COPYNAME, field.first.c_str(),
                             CURLFORM_COPYCONTENTS, field.second.c_str(),
                             CURLFORM_END);
            }
        }

        // 添加内存中的图片数据
        if (file_data && file_data->data && file_data->size > 0) {
            curl_formadd(&formpost, &lastptr,
                         CURLFORM_COPYNAME, "img",
                         CURLFORM_BUFFER, "snapshot.jpg",
                         CURLFORM_BUFFERPTR, file_data->data,
                         CURLFORM_BUFFERLENGTH, file_data->size,
                         CURLFORM_CONTENTTYPE, "image/jpeg",
                         CURLFORM_END);
        }

        // 设置表单数据
        if (formpost) {
            curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
        }
    }

    // 执行请求
    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    // 清理资源
    if (formpost) {
        curl_formfree(formpost);
    }
    curl_easy_cleanup(curl);

    // 处理响应
    if (res != CURLE_OK) {
        throw std::runtime_error("Request failed: " + std::string(curl_easy_strerror(res)));
    }

    // 解析JSON或返回错误
    try {
        result = json::parse(response_string);
    } catch (...) {
        throw std::runtime_error("Invalid JSON response");
    }

    // 处理HTTP错误状态
    if (http_code >= 400) {
        std::string error = "HTTP error: " + std::to_string(http_code);
        if (result.contains("error")) {
            error += " - " + result["error"].get<std::string>();
        }
        throw std::runtime_error(error);
    }

    return result;
}

struct curl_slist* HttpClient::GetHeaders() {
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, ("Authorization: " + auth_token_).c_str());
    SPDLOG_INFO("headers->data:{}", headers->data);
    SPDLOG_INFO("headers->next->data:{}", headers->next->data);
    return headers;
}

void HttpClient::SetQueryParams(void* curl_ptr, const std::map<std::string, std::string>& params) {
    CURL* curl = static_cast<CURL*>(curl_ptr);
    
    // 获取当前设置的URL
    char* url_ptr = nullptr;
    curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &url_ptr);
    std::string url = url_ptr ? url_ptr : "";
    
    // 构建查询字符串
    std::string query;
    for (const auto& param : params) {
        if (!query.empty()) query += "&";
        char* escaped = curl_easy_escape(curl, param.second.c_str(), 0);
        if (escaped) {
            query += param.first + "=" + escaped;
            curl_free(escaped);
        } else {
            query += param.first + "=" + param.second;
        }
    }
    
    // 检查URL是否已有查询参数
    size_t pos = url.find('?');
    if (pos == std::string::npos) {
        url += "?" + query;
    } else {
        // 如果已有查询参数，则追加
        url += "&" + query;
    }
    SPDLOG_INFO("url:{}", url);
    // 设置新的URL
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
}

// SuperDetectionAPI 实现
SuperDetectionAPI::SuperDetectionAPI(const std::string& token, bool staging)
    : base_url_(staging ? "https://app-stg.obico.io" : "https://app.obico.io"),
      client_(token) {}

json SuperDetectionAPI::PredictFailure(const std::string& printer_id,
                   const std::string& print_id, const unsigned char* pic_buf, int pic_length) {
    // 准备内存数据
    CurlReadData img_data = {
        reinterpret_cast<const char*>(pic_buf),
        static_cast<size_t>(pic_length)
    };
    
    // 准备表单数据
    std::map<std::string, std::string> form_data = {
        {"printer_id", printer_id},
        {"print_id", print_id}
    };
    
    // 发送请求
    std::string endpoint = base_url_ + "/ent/partners/api/predict/";
    SPDLOG_INFO("endpoint:{}", endpoint);
    return client_.ExecuteRequest(endpoint, "POST", nullptr, &form_data, &img_data);
}

// 解析API响应
PredictionResult ParsePredictionResponse(const json& json_data) {
    PredictionResult result;
    std::string jsonString = json_data.dump();
    SPDLOG_INFO("jsonString:{}", jsonString);
    const auto& res = json_data["result"];
    
    result.p = res["p"].get<double>();
    
    const auto& ts = res["temporal_stats"];
    result.temporal_stats = {
        ts["ewm_mean"].get<double>(),
        ts["rolling_mean_short"].get<double>(),
        ts["rolling_mean_long"].get<double>(),
        ts["prediction_num"].get<int>(),
        ts["prediction_num_lifetime"].get<int>()
    };
    
    for (const auto& detection : res["detections"]) {
        double confidence = detection[0].get<double>();
        const auto& bbox = detection[1];
        result.detections.push_back({
            confidence,
            bbox[0].get<double>(),
            bbox[1].get<double>(),
            bbox[2].get<double>(),
            bbox[3].get<double>()
        });
    }
    
    return result;
}

int SuperDetectionTest(const std::string& token, const std::string& printer_id, const std::string& print_id, const unsigned char* pic_buf, int pic_length, json &j_respond) {
    try {
        SuperDetectionAPI api(token, true);
        api.SetSSLVerify(false);

        json response = api.PredictFailure(printer_id, print_id, pic_buf, pic_length);
        PredictionResult result = ParsePredictionResponse(response);

        j_respond["value"]["p"] = result.p;
        j_respond["value"]["ewm_mean"] = result.temporal_stats.ewm_mean;
        j_respond["value"]["rolling_mean_short"] = result.temporal_stats.rolling_mean_short;
        j_respond["value"]["rolling_mean_long"] = result.temporal_stats.rolling_mean_long;
        j_respond["value"]["prediction_num"] = result.temporal_stats.prediction_num;
        j_respond["value"]["prediction_num_lifetime"] = result.temporal_stats.prediction_num_lifetime;

        // 输出结果
        std::cout << "API Response:" << std::endl;
        std::cout << "  Failure probability (p): " << result.p << std::endl;
        std::cout << "  Temporal Stats:" << std::endl;
        std::cout << "    EWM mean: " << result.temporal_stats.ewm_mean << std::endl;
        std::cout << "    Short-term rolling mean: " << result.temporal_stats.rolling_mean_short << std::endl;
        std::cout << "    Long-term rolling mean: " << result.temporal_stats.rolling_mean_long << std::endl;
        std::cout << "    Predictions (current print): " << result.temporal_stats.prediction_num << std::endl;
        std::cout << "    Predictions (lifetime): " << result.temporal_stats.prediction_num_lifetime << std::endl;
        
        std::cout << "  Detections: " << result.detections.size() << std::endl;
        for (size_t i = 0; i < result.detections.size(); ++i) {
            const auto& det = result.detections[i];
            std::cout << "    Detection #" << i+1 << ":" << std::endl;
            std::cout << "      Confidence: " << det.confidence << std::endl;
            std::cout << "      Bounding box: [" 
                      << det.x_center << ", " << det.y_center << ", "
                      << det.width << ", " << det.height << "]" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}


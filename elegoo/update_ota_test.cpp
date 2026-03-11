#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include <ctype.h>

#include "openssl/pem.h"
#include "openssl/rsa.h"
#include "openssl/err.h"
#include "openssl/evp.h"
#include "openssl/bio.h"
#include "curl/curl.h"

#include "json.h"

typedef struct {
    int code;
    char message[1024];
    struct {
        char ver[32];
        char info[1024];
        char downloadUrl[1024];
        long long releaseTime;
        char md5[32];
    } url_info;
} revc_message_t;

char readBuffer[4096];
char signed_data[512];

char* url_Encode(const char* str) {
    size_t len = strlen(str);
    size_t newLen = 3 * len + 1; // 加1用于'\0'
    char *encoded = (char *)malloc(newLen);
    if (!encoded) return NULL;

    const char *ptr = str;
    char *optr = encoded;

    while (*ptr) {
        if (isalnum((unsigned char)*ptr) || 
            *ptr == '-' || *ptr == '_' || *ptr == '.' || 
            *ptr == '~' || *ptr == '*' || *ptr == '/') {
            // 允许的字符直接复制
            *optr++ = *ptr;
        } else {
            // 其他字符进行编码
            sprintf(optr, "%%%02X", (unsigned char)*ptr);
            optr += 3;
        }
        ptr++;
    }

    *optr = '\0'; // 确保字符串以NULL结尾
    return encoded; 
}

char* base64Encode(const unsigned char* buffer, size_t length) {
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

char* sign(const char* data, size_t dataLength, const char* privateKey, size_t privateKeyLength) {
    printf("\n__func__:%s #1 dataLength:%d %d\n",__func__,dataLength,privateKeyLength);
    // Decode the Base64 encoded private key
    BIO *bio = BIO_new_mem_buf(privateKey, privateKeyLength);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    if (bio == NULL) {
        printf("__func__:%s __LINE__:%d #1 \n",__func__,__LINE__);
        ERR_print_errors_fp(stderr);
        // exit(EXIT_FAILURE);
        return nullptr;
    }

    printf("__func__:%s __LINE__:%d #1 \n",__func__,__LINE__);
    // Read private key
    RSA *rsa = NULL;
    PEM_read_bio_RSAPrivateKey(bio, &rsa, NULL, NULL);
    BIO_free(bio);

    printf("__func__:%s __LINE__:%d #1 \n",__func__,__LINE__);
    if (!rsa) {
        printf("__func__:%s __LINE__:%d #1 \n",__func__,__LINE__);
        ERR_print_errors_fp(stderr);
        // exit(EXIT_FAILURE);
        return nullptr;
    }

    printf("__func__:%s __LINE__:%d #1 \n",__func__,__LINE__);
    // Create a new EVP_PKEY structure and assign the RSA key to it
    EVP_PKEY *pkey = EVP_PKEY_new();
    EVP_PKEY_assign_RSA(pkey, rsa);

    printf("__func__:%s __LINE__:%d #1 \n",__func__,__LINE__);
    // Create a new signature context
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (EVP_SignInit(mdctx, EVP_sha256()) != 1) {
        printf("__func__:%s __LINE__:%d #1 \n",__func__,__LINE__);
        EVP_MD_CTX_free(mdctx);
        EVP_PKEY_free(pkey);
        ERR_print_errors_fp(stderr);
        // exit(EXIT_FAILURE);
        return nullptr;
    }

    printf("__func__:%s __LINE__:%d #1 strlen(data):%d,strlen(privateKey):%d\n",__func__,__LINE__,strlen(data),strlen(privateKey));
    // Update with data
    if (EVP_SignUpdate(mdctx, data, dataLength) != 1) {
        printf("__func__:%s __LINE__:%d #1 \n",__func__,__LINE__);
        EVP_MD_CTX_free(mdctx);
        EVP_PKEY_free(pkey);
        ERR_print_errors_fp(stderr);
        // exit(EXIT_FAILURE);
        return nullptr;
    }

    // Sign the data
    unsigned char signature[256] = {0};
    unsigned int signatureLength = 0;
    printf("__func__:%s __LINE__:%d #1 \n",__func__,__LINE__);
    if (EVP_SignFinal(mdctx, signature, &signatureLength, pkey) != 1) {
        printf("__func__:%s __LINE__:%d #1 \n",__func__,__LINE__);
        EVP_MD_CTX_free(mdctx);
        printf("__func__:%s __LINE__:%d #1 \n",__func__,__LINE__);
        EVP_PKEY_free(pkey);
        printf("__func__:%s __LINE__:%d #1 \n",__func__,__LINE__);
        ERR_print_errors_fp(stderr);
        printf("__func__:%s __LINE__:%d #1 \n",__func__,__LINE__);
        // exit(EXIT_FAILURE);
        return nullptr;
    }

    printf("__func__:%s __LINE__:%d #1 \n",__func__,__LINE__);
    EVP_MD_CTX_free(mdctx);
    EVP_PKEY_free(pkey);

    printf("__func__:%s __LINE__:%d #1 \n",__func__,__LINE__);
    // Encode signature to Base64
    char* encodedSignature = base64Encode(signature, signatureLength);
    printf("__func__:%s __LINE__:%d #1 \n",__func__,__LINE__);
    return encodedSignature;
}

char* readFileAndAddHeadTail(const char* filename, const char* head, const char* tail) {
    printf("\n__func__:%s __LINE__:%d #1 filename:%s\n",__func__,__LINE__,filename);
    system("cat /otakey");
    printf("\n__func__:%s __LINE__:%d #1 filename:%s\n",__func__,__LINE__,filename);
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("open file failed");
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* content = (char*)malloc(fileSize + strlen(head) + strlen(tail)); // +1 for null terminator
    if (content == NULL) {
        fclose(file);
        return NULL;
    }
    strcpy(content, head);

    fread(content + strlen(head), 1, fileSize, file);
    fclose(file);

    strcpy(content + strlen(head) + fileSize, tail);

    return content;
}

char* replace_spaces_with_percent20(const char* str) {
    int len = strlen(str);
    int new_len = len;
    const char* ptr;

    // 计算需要多少个%20来替换空格
    for (ptr = str; *ptr; ++ptr) {
        if (*ptr == ' ') {
            new_len += 2; // 每个空格将被替换为%20，长度增加2
        }
    }

    // 分配新的字符串空间
    char* new_str = (char*)malloc(new_len + 1); // +1 for null terminator
    if (new_str == NULL) {
        perror("Memory allocation failed");
        return NULL;
    }

    // 复制原始字符串到新的字符串，替换空格
    char* new_ptr = new_str;
    for (ptr = str; *ptr; ++ptr) {
        if (*ptr == ' ') {
            *new_ptr++ = '%';
            *new_ptr++ = '2';
            *new_ptr++ = '0';
        } else {
            *new_ptr++ = *ptr;
        }
    }
    *new_ptr = '\0'; // 添加字符串结束符

    return new_str;
}

size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    memset(readBuffer, 0, 4096);
    memcpy(readBuffer, (char*)contents, realsize < 4096 ? realsize:4096);
    printf("\n*************************\n");
    printf("realsize:%d readBuffer:%s\n",realsize, (char*)readBuffer);
    printf("\n*************************\n");
    return realsize;
}

void get_down_url(CURL *curl,const char* url,struct curl_slist *headers,const char* signed_header)
{
    CURLcode res;
    curl_easy_setopt(curl, CURLOPT_URL, url);

    res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    if(res != CURLE_OK)
        fprintf(stderr, "curl_easy_setopt(CURLOPT_HTTPHEADER) failed: %s\n", curl_easy_strerror(res));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    res = curl_easy_perform(curl);

    if(res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    }
}

revc_message_t get_download_url()
{
    printf("__func__:%s __LINE__:%d #1 \n",__func__,__LINE__);
    revc_message_t r = {0};
    json read_json = json::parse(std::string(readBuffer));

    if (read_json.empty()) 
    {
        printf("read_json is empty");
        r.code = -1;
        return r;
    }

    json tmp = read_json["code"];
    if (tmp.is_number()) 
    {
        printf("__func__:%s __LINE__:%d #1 code:%d\n",__func__,__LINE__,tmp.get<int>());
        r.code = tmp.get<int>();
    } 
    else if (tmp.is_string()) 
    {
        printf("__func__:%s __LINE__:%d #1 code:%s\n",__func__,__LINE__,tmp.get<std::string>().c_str());
        r.code = std::stoi(tmp.get<std::string>());
    }

    tmp = read_json["message"];
    if (tmp.is_string()) 
    {
        printf("__func__:%s __LINE__:%d #1 message:%s\n",__func__,__LINE__,tmp.get<std::string>().c_str());
        strcpy(r.message, tmp.get<std::string>().c_str());
    }

    if (r.code != 200) 
    {
        ("get download url failed! code:%d\n", r.code);
        return r;
    }

    json data = read_json["data"];
    if (!data.empty()) 
    {
        tmp = data["version"];
        if (tmp.is_string()) 
        {
            printf("__func__:%s __LINE__:%d #1 version:%s\n",__func__,__LINE__,tmp.get<std::string>().c_str());
            strncpy(r.url_info.ver, tmp.get<std::string>().c_str(), strlen(tmp.get<std::string>().c_str()));
        }

        tmp = data["info"];
        if (tmp.is_string()) 
        {
            printf("__func__:%s __LINE__:%d #1 info:%s\n",__func__,__LINE__,tmp.get<std::string>().c_str());
            strncpy(r.url_info.info, tmp.get<std::string>().c_str(), strlen(tmp.get<std::string>().c_str()));
        }

        tmp = data["downloadUrl"];
        if (tmp.is_string()) 
        {
            printf("__func__:%s __LINE__:%d #1 downloadUrl:%s\n",__func__,__LINE__,tmp.get<std::string>().c_str());
            strncpy(r.url_info.downloadUrl, tmp.get<std::string>().c_str(), strlen(tmp.get<std::string>().c_str()));
        }

        tmp = data["releaseTime"];
        if (tmp.is_number()) 
        {
            printf("__func__:%s __LINE__:%d #1 releaseTime:%lld\n",__func__,__LINE__,tmp.get<int64_t>());
            r.url_info.releaseTime = tmp.get<int64_t>();
        }

        tmp = data["md5"];
        if (tmp.is_string()) 
        {
            printf("__func__:%s __LINE__:%d #1 md5:%s\n",__func__,__LINE__,tmp.get<std::string>().c_str());
            strncpy(r.url_info.md5, tmp.get<std::string>().c_str(), strlen(tmp.get<std::string>().c_str()));
        }
    }

    return r;
}

FILE *fp = nullptr;
char *filename[128] = {};
size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream) 
{
    printf("__func__:%s,__LINE__:%d,size:%d,nmemb:%d %d\n",__func__,__LINE__,size,nmemb,size*nmemb);
    int written = fwrite(ptr, size, nmemb, (FILE *)stream);
    printf("__func__:%s,__LINE__:%d,size:%d,nmemb:%d,written*size:%d,written:%d\n",__func__,__LINE__,size,nmemb,written*size,written);
    return written*size;

}

void down_pkg(revc_message_t revc_message,CURL *curl)
{
    CURLcode res;
    struct curl_slist *headers = NULL;

    printf("__func__:%s,__LINE__:%d\n",__func__,__LINE__);
    if((fp=fopen("/elegoo_ota.ipk","w")) == nullptr)
    {
        printf("__func__:%s,__LINE__:%d,fopen elegoo_ota.ipk failed\n",__func__,__LINE__);
        return ;
    }
    printf("__func__:%s,__LINE__:%d\n",__func__,__LINE__);
    if(curl) {
        printf("__func__:%s,__LINE__:%d\ndownloadUrl: %s\n",__func__,__LINE__,revc_message.url_info.downloadUrl);
        res = curl_easy_setopt(curl, CURLOPT_URL, revc_message.url_info.downloadUrl);
        printf("__func__:%s,__LINE__:%d res:%d\n",__func__,__LINE__,res);
        res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        printf("__func__:%s,__LINE__:%d res:%d\n",__func__,__LINE__,res);
        res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        printf("__func__:%s,__LINE__:%d res:%d\n",__func__,__LINE__,res);
        res = curl_easy_perform(curl);
        printf("__func__:%s,__LINE__:%d res:%d\n",__func__,__LINE__,res);
        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s res:%d\n", curl_easy_strerror(res),res);
        }
        fclose(fp);
    }

    printf("__func__:%s,__LINE__:%d\n",__func__,__LINE__);
}

int get_revc_message(const char* lang, const char* Model, const char* sn, const char* ver)
{
    char url[256];
    char para[256];
    char signed_header[512];
    char key[2048];

    int ret;
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    long long microseconds = tv.tv_sec * 1000000LL + tv.tv_usec;
    microseconds /= 1000;
    char *Model_n = replace_spaces_with_percent20(Model);
    snprintf(para, 256, "language=%s&machineModel=%s&sn=%s&timestamp=%lld&version=%s", lang, Model, sn, microseconds, ver);

    strncpy(para, url_Encode(para), 256);
    snprintf(url, 256, "https://test.elegoo.com/api/tp/ota/fdm/checkUpdate?language=%s&machineModel=%s&sn=%s&timestamp=%lld&version=%s", lang, Model_n, sn, microseconds, ver);

    strncpy(key, readFileAndAddHeadTail("/otakey", "-----BEGIN RSA PRIVATE KEY-----\n", "\n-----END RSA PRIVATE KEY-----"), 2048);
    // strncpy(key, readFileAndAddHeadTail("/otakey", "", ""), 2048);

    printf("\npara_len:%d \npara:%s\n",strlen(para),para);
    printf("key_len:%d \nkey:%s\n",strlen(key),key);
    char* ch = sign(para, strlen(para), key, strlen(key));
    if(!ch)
    {
        perror("sign is nullptr");
        return EXIT_FAILURE;
    }
    strncpy(signed_data, ch, 512);
    snprintf(signed_header, 512, "sign:%s", signed_data);
    printf("signed_data_len:%d \nsigned_data:%s\n",strlen(signed_data),signed_data);

    if (signed_data == NULL) {
        perror("Error signed_data");
        return EXIT_FAILURE;
    }

    printf("\nURL:%s\n",url);
    printf("\nHEADRES:%s\n",signed_header);

    CURL *curl;
    CURLcode res;
    struct curl_slist *headers = NULL;

    curl_global_init(CURL_GLOBAL_DEFAULT);

    curl = curl_easy_init();
    if(curl) {
        headers = curl_slist_append(headers, signed_header);

        get_down_url(curl,url,headers,signed_header);
        
        revc_message_t revc_message = {0};
        revc_message = get_download_url();

        printf("code: %d\n", revc_message.code);
        printf("message: %s\n", revc_message.message);
        if (revc_message.code == 200) {
            printf("version: %s\n", revc_message.url_info.ver);
            printf("info: %s\n", revc_message.url_info.info);
            printf("downloadUrl: %s\n", revc_message.url_info.downloadUrl);
            printf("releaseTime: %lld\n", revc_message.url_info.releaseTime);
            printf("md5: %s\n", revc_message.url_info.md5);
        }

        down_pkg(revc_message,curl);

        curl_slist_free_all(headers); 
        curl_easy_cleanup(curl); 
    }

    curl_global_cleanup();

    return 0;
}

const char language[]="zh";
const char machineModel[]="ELEGOO Mars 4";
const char sn[]="147258";
const char current_version[]="1.0.3";
// const char language[]="zh";
// const char machineModel[]="ELEGOO Jupiter 2";
// const char sn[]="147258";
// const char current_version[]="0.2.8";

int main() {
    int ret = get_revc_message(language, machineModel, sn, current_version);
    if (ret) {
        printf("get_revc_message failed!\n");
        return ret;
    }

    return 0;
}

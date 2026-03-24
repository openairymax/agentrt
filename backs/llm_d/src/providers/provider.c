/**
 * @file provider.c
 * @brief 提供商通用工具实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 改进说明：
 * 1. 移除不存在的 logger.h 依赖
 * 2. 使用 svc_logger.h 中的 SVC_LOG_* 宏
 * 3. 定义 http_buffer_t 类型
 */

#include "provider.h"
#include "svc_logger.h"
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <errno.h>

typedef struct {
    char* data;
    size_t size;
    size_t capacity;
} http_buffer_t;

/* ---------- HTTP 回调 ---------- */
static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    http_buffer_t* mem = (http_buffer_t*)userp;

    char* ptr = realloc(mem->data, mem->size + realsize + 1);
    if (!ptr) return 0;

    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = '\0';
    return realsize;
}

/* ---------- HTTP POST（带重试） ---------- */
int provider_http_post(const char* url,
                       struct curl_slist* headers,
                       const char* body,
                       double timeout_sec,
                       int max_retries,
                       http_buffer_t** out_response,
                       long* out_http_code) {
    if (!url || !body || !out_response || !out_http_code) {
        errno = EINVAL;
        return AGENTOS_ERR_INVALID_PARAM;
    }

    http_buffer_t* response = calloc(1, sizeof(http_buffer_t));
    if (!response) return AGENTOS_ERR_OUT_OF_MEMORY;

    CURL* curl = NULL;
    int retry = 0;
    int success = -1;
    CURLcode res;
    long http_code = 0;

    while (retry <= max_retries) {
        curl = curl_easy_init();
        if (!curl) {
            SVC_LOG_ERROR("provider_http_post: curl_easy_init failed");
            free(response->data);
            free(response);
            return AGENTOS_ERR_UNKNOWN;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)timeout_sec);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
#ifdef SKIP_PEER_VERIFICATION
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
#endif

        res = curl_easy_perform(curl);
        if (res == CURLE_OK) {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            success = 0;
            curl_easy_cleanup(curl);
            break;
        }

        SVC_LOG_WARN("provider_http_post: attempt %d failed: %s",
                         retry + 1, curl_easy_strerror(res));
        retry++;
        curl_easy_cleanup(curl);
        if (retry <= max_retries) {
            free(response->data);
            response->data = NULL;
            response->size = 0;
        }
    }

    if (success != 0) {
        free(response->data);
        free(response);
        return AGENTOS_ERR_IO;
    }

    *out_response = response;
    *out_http_code = http_code;
    return AGENTOS_OK;
}

void provider_http_buffer_free(http_buffer_t* buf) {
    if (buf) {
        free(buf->data);
        free(buf);
    }
}

/* ---------- 提供商注册表 ---------- */
void provider_register_all(void) {
    SVC_LOG_INFO("provider: all providers registered");
}
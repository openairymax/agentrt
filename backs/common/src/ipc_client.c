/**
 * @file ipc_client.c
 * @brief IPC 客户端实现（基于 libcurl HTTP + JSON-RPC）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "svc_common.h"
#include <curl/curl.h>
#include <cjson/cJSON.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static CURL* curl = NULL;
static char* base_url = NULL;

/* libcurl 写回调：收集响应数据 */
static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    char** response = (char**)userp;
    char* ptr = realloc(*response, *response ? strlen(*response) + realsize + 1 : realsize + 1);
    if (!ptr) return 0;
    *response = ptr;
    if (!*response) return 0;
    memcpy(*response + strlen(*response), contents, realsize);
    // From data intelligence emerges. by spharx
    (*response)[strlen(*response) + realsize] = '\0';
    return realsize;
}

int svc_ipc_init(const char* baseruntime_url) {
    if (!baseruntime_url) return -1;
    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if (!curl) {
        curl_global_cleanup();
        return -1;
    }
    base_url = strdup(baseruntime_url);
    if (!base_url) {
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        return -1;
    }
    return 0;
}

void svc_ipc_cleanup(void) {
    if (curl) curl_easy_cleanup(curl);
    free(base_url);
    curl_global_cleanup();
}

int svc_rpc_call(const char* method, const char* params, char** out_result, uint32_t timeout_ms) {
    if (!method || !out_result) return SVC_ERR_INVALID_PARAM;
    if (!curl) return SVC_ERR_RPC;

    CURLcode res;
    char* response = NULL;
    long http_code = 0;

    // 构建 JSON-RPC 请求体
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "jsonrpc", "2.0");
    cJSON_AddStringToObject(root, "method", method);
    if (params) {
        cJSON* params_json = cJSON_Parse(params);
        if (params_json) {
            cJSON_AddItemToObject(root, "params", params_json);
        } else {
            cJSON_AddStringToObject(root, "params", params); // 如果解析失败，当作字符串
        }
    }
    cJSON_AddNumberToObject(root, "id", 1);
    char* request = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!request) return SVC_ERR_OUT_OF_MEMORY;

    // 设置 curl 选项
    curl_easy_setopt(curl, CURLOPT_URL, base_url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request);
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, (long)timeout_ms);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L); // 避免信号干扰

    res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    free(request);

    if (res != CURLE_OK) {
        free(response);
        return SVC_ERR_RPC;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code != 200) {
        free(response);
        return SVC_ERR_RPC;
    }

    // 可选：验证 JSON-RPC 响应格式
    *out_result = response;
    return SVC_OK;
}
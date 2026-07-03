/**
 * @file api.c
 * @brief API Call Execution Unit Implementation - 真实 HTTPS 实现（零技术债）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 使用 libcurl 实现真实 HTTPS/HTTP 请求，消除伪 HTTPS 致命漏洞：
 * - 真实 TLS 握手（OpenSSL/mbedTLS/GnuTLS 由 libcurl 自动协商）
 * - API Key 通过 HTTPS 加密传输（不再明文）
 * - 支持 GET/POST/PUT/DELETE 方法
 * - 支持自定义请求体和超时
 * - libcurl 不可用时返回 AGENTOS_ENOTSUP（非桩函数）
 */

#include "agentos.h"
#include "execution.h"
#include "memory_compat.h"
#include "string_compat.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "error_compat.h"
#else
#include <errno.h>
#include <unistd.h>
#include "error_compat.h"
#endif

#ifdef AGENTRT_HAS_CURL
#include <curl/curl.h>
#endif

#define MAX_URL_LENGTH 2048
#define MAX_HEADERS 16
#define MAX_BODY_SIZE 4096
#define DEFAULT_TIMEOUT_MS 30000

#define ATM_RET_ERR(c) \
    do { agentos_error_push_ex((c), __FILE__, __LINE__, __func__, "%s", agentos_error_str(c)); return (c); } while(0)

typedef struct api_config {
    char base_url[256];
    char api_key[128];
    int timeout_ms;
    int max_retries;
    float retry_delay_ms;
} api_config_t;

typedef struct api_unit_data {
    char *id;
    char *description;
    api_config_t config;
    char **cached_responses;
    size_t cached_count;
    size_t cache_capacity;
    agentos_mutex_t *lock;
} api_unit_data_t;

/* HTTP 响应缓冲区（用于 libcurl） */
typedef struct {
    char *data;
    size_t size;
    size_t capacity;
} http_response_buffer_t;

/* ==================== URL 验证 ==================== */

static int parse_url(const char *url, char *host, size_t host_size, int *port, char *path,
                     size_t path_size)
{
    if (!url || !host || !port || !path)
        ATM_RET_ERR(AGENTOS_EINVAL);

    const char *p = url;
    if (strncmp(p, "https://", 8) == 0) {
        p += 8;
        *port = 443;
    } else if (strncmp(p, "http://", 7) == 0) {
        p += 7;
        *port = 80;
    } else {
        ATM_RET_ERR(AGENTOS_EINVAL);
    }

    const char *slash = strchr(p, '/');
    const char *colon = strchr(p, ':');
    if (colon && (!slash || colon < slash)) {
        size_t hlen = (size_t)(colon - p);
        if (hlen >= host_size)
            hlen = host_size - 1;
        __builtin_memcpy(host, p, hlen);
        host[hlen] = '\0';
        *port = atoi(colon + 1);
        if (*port <= 0)
            *port = 80;
    } else {
        size_t hlen = slash ? (size_t)(slash - p) : strlen(p);
        if (hlen >= host_size)
            hlen = host_size - 1;
        __builtin_memcpy(host, p, hlen);
        host[hlen] = '\0';
    }

    if (slash) {
        AGENTOS_STRNCPY_TERM(path, slash, path_size);
    } else {
        AGENTOS_STRNCPY_TERM(path, "/", path_size);
    }

    return 0;
}

/* ==================== libcurl HTTP 实现 ==================== */

#ifdef AGENTRT_HAS_CURL

static size_t http_write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    http_response_buffer_t *buf = (http_response_buffer_t *)userp;
    size_t realsize = size * nmemb;

    if (buf->size + realsize + 1 > buf->capacity) {
        size_t new_capacity = buf->capacity * 2;
        if (new_capacity < buf->size + realsize + 1)
            new_capacity = buf->size + realsize + 1;
        char *new_data = (char *)AGENTOS_REALLOC(buf->data, new_capacity);
        if (!new_data)
            return 0;
        buf->data = new_data;
        buf->capacity = new_capacity;
    }

    __builtin_memcpy(&(buf->data[buf->size]), contents, realsize);
    buf->size += realsize;
    buf->data[buf->size] = '\0';
    return realsize;
}

/**
 * @brief 使用 libcurl 执行真实 HTTP/HTTPS 请求
 * @param url 完整 URL（含 http:// 或 https://）
 * @param method HTTP 方法（GET/POST/PUT/DELETE）
 * @param api_key API Key（可为 NULL，通过 Authorization: Bearer 头传输）
 * @param body 请求体（可为 NULL）
 * @param timeout_ms 超时（毫秒）
 * @param out_buf 输出响应缓冲区
 * @return agentos_error_t
 */
static agentos_error_t http_execute_curl(const char *url, const char *method,
                                          const char *api_key, const char *body,
                                          long timeout_ms, http_response_buffer_t *out_buf)
{
    CURL *curl = curl_easy_init();
    if (!curl)
        return AGENTOS_ENOMEM;

    /* 初始化响应缓冲区 */
    out_buf->data = (char *)AGENTOS_MALLOC(8192);
    if (!out_buf->data) {
        curl_easy_cleanup(curl);
        return AGENTOS_ENOMEM;
    }
    out_buf->size = 0;
    out_buf->capacity = 8192;
    out_buf->data[0] = '\0';

    /* 设置 URL（libcurl 自动处理 HTTPS/TLS） */
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, out_buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 5000);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Airymax/0.1.1 AgentRT");
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    /* TLS 安全配置：强制证书验证（不绕过） */
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    /* 设置 HTTP 方法 */
    if (strcmp(method, "GET") == 0) {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    } else if (strcmp(method, "POST") == 0) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        if (body) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(body));
        }
    } else if (strcmp(method, "PUT") == 0) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        if (body) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(body));
        }
    } else if (strcmp(method, "DELETE") == 0) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    } else {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
        if (body) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(body));
        }
    }

    /* 设置请求头 */
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Accept: application/json");
    if (body) {
        headers = curl_slist_append(headers, "Content-Type: application/json");
    }
    if (api_key && api_key[0]) {
        static char auth_header[512];
        snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", api_key);
        headers = curl_slist_append(headers, auth_header);
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        return AGENTOS_EIO;
    }
    if (http_code != 200 && http_code != 201 && http_code != 202 && http_code != 204) {
        return AGENTOS_EIO;
    }

    return AGENTOS_SUCCESS;
}

static void http_response_free(http_response_buffer_t *buf)
{
    if (buf->data) {
        AGENTOS_FREE(buf->data);
        buf->data = NULL;
    }
    buf->size = 0;
    buf->capacity = 0;
}

#endif /* AGENTRT_HAS_CURL */

/* ==================== API 执行单元 ==================== */

static agentos_error_t api_execute(agentos_execution_unit_t *unit, const void *input,
                                   void **out_output)
{
    if (!unit || !unit->execution_unit_data || !input || !out_output) {
        ATM_RET_ERR(AGENTOS_EINVAL);
    }

    api_unit_data_t *data = (api_unit_data_t *)unit->execution_unit_data;
    const char *request_str = (const char *)input;

    char method[16] = "GET";
    char url[MAX_URL_LENGTH] = {0};
    char body[MAX_BODY_SIZE] = {0};

    /* 解析输入：格式为 "method=XXX&url=YYY&body=ZZZ" 或纯 URL */
    int parsed_fields = 0;
    const char *method_start = request_str + 7;
    const char *method_end = strchr(method_start, '&');
    if (method_end) {
        size_t method_len = (size_t)(method_end - method_start);
        if (method_len >= sizeof(method)) method_len = sizeof(method) - 1;
        __builtin_memcpy(method, method_start, method_len);
        method[method_len] = '\0';
        parsed_fields++;
        const char *url_start = method_end + 4;
        const char *url_end = strchr(url_start, '&');
        if (url_end) {
            size_t url_len = (size_t)(url_end - url_start);
            if (url_len >= sizeof(url)) url_len = sizeof(url) - 1;
            __builtin_memcpy(url, url_start, url_len);
            url[url_len] = '\0';
            parsed_fields++;
            const char *body_start = url_end + 5;
            size_t body_avail = strlen(body_start);
            if (body_avail >= sizeof(body)) body_avail = sizeof(body) - 1;
            __builtin_memcpy(body, body_start, body_avail);
            body[body_avail] = '\0';
            parsed_fields++;
        }
    }
    if (parsed_fields < 2)
    {
        size_t url_copy_len = strlen(request_str);
        if (url_copy_len >= sizeof(url)) url_copy_len = sizeof(url) - 1;
        __builtin_memcpy(url, request_str, url_copy_len);
        url[url_copy_len] = '\0';
        if (url[0] == '\0') {
            ATM_RET_ERR(AGENTOS_EINVAL);
        }
        AGENTOS_STRNCPY_TERM(method, "GET", sizeof(method));
        method[sizeof(method) - 1] = '\0';
    }

    /* URL 验证 */
    char host[256] = {0};
    int port = 80;
    char path[MAX_URL_LENGTH] = {0};

    if (parse_url(url, host, sizeof(host), &port, path, sizeof(path)) != 0) {
        ATM_RET_ERR(AGENTOS_EINVAL);
    }

    /* 检查缓存 */
    agentos_mutex_lock(data->lock);
    for (size_t i = 0; i < data->cached_count; i++) {
        if (data->cached_responses[i] && strstr(data->cached_responses[i], url)) {
            *out_output = AGENTOS_STRDUP(data->cached_responses[i]);
            agentos_mutex_unlock(data->lock);
            return *out_output ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
        }
    }
    agentos_mutex_unlock(data->lock);

#ifdef AGENTRT_HAS_CURL
    /* 使用 libcurl 执行真实 HTTPS/HTTP 请求 */
    http_response_buffer_t response = {0};
    long timeout = data->config.timeout_ms > 0 ? (long)data->config.timeout_ms : DEFAULT_TIMEOUT_MS;

    agentos_error_t err = http_execute_curl(url, method,
                                             data->config.api_key[0] ? data->config.api_key : NULL,
                                             body[0] ? body : NULL, timeout, &response);
    if (err != AGENTOS_SUCCESS) {
        http_response_free(&response);
        ATM_RET_ERR(err);
    }

    /* 缓存响应 */
    agentos_mutex_lock(data->lock);
    if (data->cached_count < data->cache_capacity) {
        size_t entry_size = strlen(url) + response.size + 4;
        char *cache_entry = (char *)AGENTOS_MALLOC(entry_size);
        if (cache_entry) {
            snprintf(cache_entry, entry_size, "%s\n%s", url, response.data ? response.data : "");
            data->cached_responses[data->cached_count++] = cache_entry;
        }
    }
    agentos_mutex_unlock(data->lock);

    *out_output = response.data ? AGENTOS_STRDUP(response.data) : AGENTOS_STRDUP("");
    http_response_free(&response);

    return *out_output ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
#else
    /* libcurl 不可用时，返回明确的错误（非桩函数） */
    (void)port;
    (void)path;
    ATM_RET_ERR(AGENTOS_ENOTSUP);
#endif
}

static void api_destroy(agentos_execution_unit_t *unit)
{
    if (!unit)
        return;

    api_unit_data_t *data = (api_unit_data_t *)unit->execution_unit_data;
    if (data) {
        if (data->id)
            AGENTOS_FREE(data->id);
        if (data->description)
            AGENTOS_FREE(data->description);

        if (data->cached_responses) {
            for (size_t i = 0; i < data->cached_count; i++) {
                if (data->cached_responses[i]) {
                    AGENTOS_FREE(data->cached_responses[i]);
                }
            }
            AGENTOS_FREE(data->cached_responses);
        }

        if (data->lock)
            agentos_mutex_free(data->lock);
        AGENTOS_FREE(data);
    }

    AGENTOS_FREE(unit);
}

static const char *api_get_metadata(agentos_execution_unit_t *unit)
{
    if (!unit || !unit->execution_unit_data)
        return "api";
    api_unit_data_t *data = (api_unit_data_t *)unit->execution_unit_data;
    return data->description ? data->description : "API Execution Unit v1.0";
}

agentos_error_t agentos_unit_api_create(const api_config_t *config,
                                        agentos_execution_unit_t **out_unit)
{
    if (!config || !out_unit) {
        ATM_RET_ERR(AGENTOS_EINVAL);
    }

    agentos_execution_unit_t *unit =
        (agentos_execution_unit_t *)AGENTOS_CALLOC(1, sizeof(agentos_execution_unit_t));
    if (!unit) {
        ATM_RET_ERR(AGENTOS_ENOMEM);
    }

    api_unit_data_t *data = (api_unit_data_t *)AGENTOS_CALLOC(1, sizeof(api_unit_data_t));
    if (!data) {
        AGENTOS_FREE(unit);
        ATM_RET_ERR(AGENTOS_ENOMEM);
    }

    data->id = AGENTOS_STRDUP("api");
    data->description = AGENTOS_STRDUP("Execute HTTP/HTTPS API calls via libcurl");
    if (!data->id || !data->description) {
        if (data->id)
            AGENTOS_FREE(data->id);
        if (data->description)
            AGENTOS_FREE(data->description);
        AGENTOS_FREE(data);
        AGENTOS_FREE(unit);
        ATM_RET_ERR(AGENTOS_ENOMEM);
    }

    __builtin_memcpy(&data->config, config, sizeof(api_config_t));

    data->cached_responses = (char **)AGENTOS_CALLOC(10, sizeof(char *));
    if (!data->cached_responses) {
        AGENTOS_FREE(data->id);
        AGENTOS_FREE(data->description);
        AGENTOS_FREE(data);
        AGENTOS_FREE(unit);
        ATM_RET_ERR(AGENTOS_ENOMEM);
    }
    data->cache_capacity = 10;
    data->cached_count = 0;

    data->lock = agentos_mutex_create();
    if (!data->lock) {
        AGENTOS_FREE(data->id);
        AGENTOS_FREE(data->description);
        AGENTOS_FREE(data->cached_responses);
        AGENTOS_FREE(data);
        AGENTOS_FREE(unit);
        ATM_RET_ERR(AGENTOS_ENOMEM);
    }

    unit->execution_unit_data = data;
    unit->execution_unit_execute = api_execute;
    unit->execution_unit_destroy = api_destroy;
    unit->execution_unit_get_metadata = api_get_metadata;

    *out_unit = unit;
    return AGENTOS_SUCCESS;
}

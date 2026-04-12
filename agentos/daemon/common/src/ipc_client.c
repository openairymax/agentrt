/**
 * @file ipc_client.c
 * @brief IPC 客户端实现（线程安全版本）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * 
 * 改进：
 * 1. 线程安全的连接池
 * 2. 修复内存安全问题
 * 3. 支持连接复用
 * 4. 添加超时和重试机制
 */

#include "svc_common.h"
#include "platform.h"
#include "error.h"
#include <curl/curl.h>
#include <cjson/cJSON.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* ==================== 配置常量 ==================== */

#define IPC_POOL_SIZE 4
#define IPC_DEFAULT_TIMEOUT_MS 30000
#define IPC_MAX_RESPONSE_SIZE (16 * 1024 * 1024)  /* 16MB */
#define IPC_MAX_RETRIES 3
#define IPC_RETRY_DELAY_MS 100

/* ==================== 响应缓冲区 ==================== */

typedef struct {
    char* data;
    size_t size;
    size_t capacity;
} ipc_response_buffer_t;

/* ==================== 连接池条目 ==================== */

typedef struct {
    CURL* curl;
    agentos_mutex_t lock;
    int in_use;
    uint64_t last_used;
} ipc_pool_entry_t;

/* ==================== IPC 客户端上下文 ==================== */

struct ipc_client {
    char* base_url;
    ipc_pool_entry_t pool[IPC_POOL_SIZE];
    agentos_mutex_t pool_lock;
    uint32_t default_timeout_ms;
    int initialized;
};

static struct ipc_client* g_ipc_client = NULL;
static agentos_mutex_t g_init_lock = {0};
static int g_curl_initialized = 0;

/* ==================== 内部函数 ==================== */

/**
 * @brief 初始化响应缓冲区
 */
static int buffer_init(ipc_response_buffer_t* buf) {
    buf->capacity = 4096;
    buf->data = (char*)malloc(buf->capacity);
    if (!buf->data) {
        return -1;
    }
    buf->data[0] = '\0';
    buf->size = 0;
    return 0;
}

/**
 * @brief 释放响应缓冲区
 */
static void buffer_free(ipc_response_buffer_t* buf) {
    if (buf->data) {
        free(buf->data);
        buf->data = NULL;
    }
    buf->size = 0;
    buf->capacity = 0;
}

/**
 * @brief libcurl 写回调（内存安全版本）
 */
static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    ipc_response_buffer_t* buf = (ipc_response_buffer_t*)userp;
    
    /* 检查是否超过最大响应大小 */
    if (buf->size + realsize > IPC_MAX_RESPONSE_SIZE) {
        return 0;  /* 返回0表示错误 */
    }
    
    /* 检查是否需要扩展缓冲区 */
    size_t new_size = buf->size + realsize + 1;
    if (new_size > buf->capacity) {
        size_t new_capacity = buf->capacity * 2;
        if (new_capacity < new_size) {
            new_capacity = new_size;
        }
        
        char* new_data = (char*)realloc(buf->data, new_capacity);
        if (!new_data) {
            return 0;
        }
        
        buf->data = new_data;
        buf->capacity = new_capacity;
    }
    
    /* 追加数据 */
    memcpy(buf->data + buf->size, contents, realsize);
    buf->size += realsize;
    buf->data[buf->size] = '\0';
    
    return realsize;
}

/**
 * @brief 从连接池获取可用连接
 */
static ipc_pool_entry_t* pool_acquire(struct ipc_client* client) {
    ipc_pool_entry_t* entry = NULL;
    
    agentos_mutex_lock(&client->pool_lock);
    
    /* 查找空闲连接 */
    for (int i = 0; i < IPC_POOL_SIZE; i++) {
        if (!client->pool[i].in_use) {
            entry = &client->pool[i];
            entry->in_use = 1;
            break;
        }
    }
    
    agentos_mutex_unlock(&client->pool_lock);
    
    /* 如果没有空闲连接，等待并重试 */
    if (!entry) {
        for (int retry = 0; retry < 10; retry++) {
            agentos_mutex_lock(&client->pool_lock);
            for (int i = 0; i < IPC_POOL_SIZE; i++) {
                if (!client->pool[i].in_use) {
                    entry = &client->pool[i];
                    entry->in_use = 1;
                    break;
                }
            }
            agentos_mutex_unlock(&client->pool_lock);
            
            if (entry) break;
            
            /* 短暂等待 */
            agentos_mutex_lock(&client->pool_lock);
            agentos_mutex_unlock(&client->pool_lock);
        }
    }
    
    return entry;
}

/**
 * @brief 释放连接回连接池
 */
static void pool_release(struct ipc_client* client, ipc_pool_entry_t* entry) {
    if (entry) {
        agentos_mutex_lock(&client->pool_lock);
        entry->in_use = 0;
        entry->last_used = agentos_time_ms();
        agentos_mutex_unlock(&client->pool_lock);
    }
}

/**
 * @brief 执行 RPC 调用（带重试）
 */
static int do_rpc_call(ipc_pool_entry_t* entry,
                       const char* base_url,
                       const char* request,
                       ipc_response_buffer_t* response,
                       uint32_t timeout_ms,
                       int max_retries) {
    CURLcode res;
    long http_code = 0;
    int retry = 0;
    
    while (retry <= max_retries) {
        /* 重置 curl 状态 */
        curl_easy_reset(entry->curl);
        
        /* 设置请求选项 */
        curl_easy_setopt(entry->curl, CURLOPT_URL, base_url);
        curl_easy_setopt(entry->curl, CURLOPT_POST, 1L);
        curl_easy_setopt(entry->curl, CURLOPT_POSTFIELDS, request);
        curl_easy_setopt(entry->curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(entry->curl, CURLOPT_WRITEDATA, response);
        curl_easy_setopt(entry->curl, CURLOPT_TIMEOUT_MS, (long)timeout_ms);
        curl_easy_setopt(entry->curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(entry->curl, CURLOPT_FOLLOWLOCATION, 1L);
        
        /* 设置请求头 */
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(entry->curl, CURLOPT_HTTPHEADER, headers);
        
        /* 执行请求 */
        res = curl_easy_perform(entry->curl);
        curl_slist_free_all(headers);
        
        if (res == CURLE_OK) {
            curl_easy_getinfo(entry->curl, CURLINFO_RESPONSE_CODE, &http_code);
            if (http_code == 200) {
                return SVC_OK;
            }
        }
        
        /* 重试前清空响应缓冲区 */
        response->size = 0;
        if (response->data) {
            response->data[0] = '\0';
        }
        
        retry++;
        
        if (retry <= max_retries) {
            uint32_t delay = IPC_RETRY_DELAY_MS * retry;
            agentos_sleep_ms(delay);
        }
    }
    
    return SVC_ERR_RPC;
}

/* ==================== 公共接口实现 ==================== */

/* 本地错误处理宏 */
#define SVC_ERROR(code, msg) \
    do { \
        agentos_error_push_ex((code), __FILE__, __LINE__, __func__, "%s", (msg)); \
        return (code); \
    } while (0)

int svc_ipc_init(const char* baseruntime_url) {
    int ret = SVC_OK;
    struct ipc_client* client = NULL;
    int curl_initialized_here = 0;
    
    if (!baseruntime_url) {
        SVC_ERROR(SVC_ERR_INVALID_PARAM, "baseruntime_url is NULL");
    }
    
    agentos_mutex_lock(&g_init_lock);
    
    /* 初始化 libcurl（仅一次） */
    if (!g_curl_initialized) {
        CURLcode curl_res = curl_global_init(CURL_GLOBAL_ALL);
        if (curl_res != CURLE_OK) {
            agentos_mutex_unlock(&g_init_lock);
            SVC_ERROR(SVC_ERR_IO, "Failed to initialize libcurl");
        }
        g_curl_initialized = 1;
        curl_initialized_here = 1;
    }
    
    /* 如果已经初始化，直接返回 */
    if (g_ipc_client) {
        agentos_mutex_unlock(&g_init_lock);
        return SVC_OK;
    }
    
    /* 创建客户端上下文 */
    client = (struct ipc_client*)calloc(1, sizeof(struct ipc_client));
    if (!client) {
        agentos_mutex_unlock(&g_init_lock);
        SVC_ERROR(SVC_ERR_OUT_OF_MEMORY, "Failed to allocate IPC client");
    }
    
    client->base_url = strdup(baseruntime_url);
    if (!client->base_url) {
        free(client);
        agentos_mutex_unlock(&g_init_lock);
        SVC_ERROR(SVC_ERR_OUT_OF_MEMORY, "Failed to duplicate base URL");
    }
    
    client->default_timeout_ms = IPC_DEFAULT_TIMEOUT_MS;
    if (agentos_mutex_init(&client->pool_lock) != 0) {
        free(client->base_url);
        free(client);
        agentos_mutex_unlock(&g_init_lock);
        SVC_ERROR(SVC_ERR_OUT_OF_MEMORY, "Failed to initialize pool mutex");
    }
    
    /* 初始化连接池 */
    int i;
    for (i = 0; i < IPC_POOL_SIZE; i++) {
        client->pool[i].curl = curl_easy_init();
        if (!client->pool[i].curl) {
            /* 记录错误并清理 */
            agentos_error_push_ex(SVC_ERR_OUT_OF_MEMORY, __FILE__, __LINE__, __func__,
                                "Failed to initialize CURL handle %d", i);
            break;
        }
        if (agentos_mutex_init(&client->pool[i].lock) != 0) {
            curl_easy_cleanup(client->pool[i].curl);
            client->pool[i].curl = NULL;
            agentos_error_push_ex(SVC_ERR_OUT_OF_MEMORY, __FILE__, __LINE__, __func__,
                                "Failed to initialize pool entry mutex %d", i);
            break;
        }
        client->pool[i].in_use = 0;
        client->pool[i].last_used = 0;
    }
    
    /* 检查连接池初始化是否成功 */
    if (i < IPC_POOL_SIZE) {
        /* 清理已创建的资源 */
        for (int j = 0; j < i; j++) {
            if (client->pool[j].curl) {
                curl_easy_cleanup(client->pool[j].curl);
            }
            agentos_mutex_destroy(&client->pool[j].lock);
        }
        agentos_mutex_destroy(&client->pool_lock);
        free(client->base_url);
        free(client);
        agentos_mutex_unlock(&g_init_lock);
        return SVC_ERR_OUT_OF_MEMORY;
    }
    
    client->initialized = 1;
    g_ipc_client = client;
    
    agentos_mutex_unlock(&g_init_lock);
    return SVC_OK;
}

#undef SVC_ERROR

void svc_ipc_cleanup(void) {
    agentos_mutex_lock(&g_init_lock);
    
    if (g_ipc_client) {
        /* 清理连接池 */
        for (int i = 0; i < IPC_POOL_SIZE; i++) {
            if (g_ipc_client->pool[i].curl) {
                curl_easy_cleanup(g_ipc_client->pool[i].curl);
            }
            agentos_mutex_destroy(&g_ipc_client->pool[i].lock);
        }
        
        agentos_mutex_destroy(&g_ipc_client->pool_lock);
        free(g_ipc_client->base_url);
        free(g_ipc_client);
        g_ipc_client = NULL;
    }
    
    if (g_curl_initialized) {
        curl_global_cleanup();
        g_curl_initialized = 0;
    }
    
    agentos_mutex_unlock(&g_init_lock);
}

int svc_rpc_call(const char* method, const char* params, char** out_result, uint32_t timeout_ms) {
    if (!method || !out_result) {
        return SVC_ERR_INVALID_PARAM;
    }
    
    if (!g_ipc_client || !g_ipc_client->initialized) {
        return SVC_ERR_RPC;
    }
    
    *out_result = NULL;
    
    /* 从连接池获取连接 */
    ipc_pool_entry_t* entry = pool_acquire(g_ipc_client);
    if (!entry) {
        return SVC_ERR_RPC;
    }
    
    /* 构建 JSON-RPC 请求 */
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "jsonrpc", "2.0");
    cJSON_AddStringToObject(root, "method", method);
    
    if (params) {
        cJSON* params_json = cJSON_Parse(params);
        if (params_json) {
            cJSON_AddItemToObject(root, "params", params_json);
        } else {
            /* 如果解析失败，作为字符串参数 */
            cJSON_AddStringToObject(root, "params", params);
        }
    }
    
    cJSON_AddNumberToObject(root, "id", (double)agentos_time_ns());
    
    char* request = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    if (!request) {
        pool_release(g_ipc_client, entry);
        return SVC_ERR_OUT_OF_MEMORY;
    }
    
    /* 初始化响应缓冲区 */
    ipc_response_buffer_t response;
    if (buffer_init(&response) != 0) {
        free(request);
        pool_release(g_ipc_client, entry);
        return SVC_ERR_OUT_OF_MEMORY;
    }
    
    /* 执行 RPC 调用 */
    if (timeout_ms == 0) {
        timeout_ms = g_ipc_client->default_timeout_ms;
    }
    
    int ret = do_rpc_call(entry, g_ipc_client->base_url, request, 
                          &response, timeout_ms, IPC_MAX_RETRIES);
    
    free(request);
    
    if (ret != SVC_OK) {
        buffer_free(&response);
        pool_release(g_ipc_client, entry);
        return ret;
    }
    
    /* 验证 JSON-RPC 响应格式 */
    cJSON* resp_json = cJSON_Parse(response.data);
    if (!resp_json) {
        buffer_free(&response);
        pool_release(g_ipc_client, entry);
        return SVC_ERR_RPC;
    }
    
    /* 检查是否有错误 */
    cJSON* error = cJSON_GetObjectItem(resp_json, "error");
    if (error) {
        cJSON_Delete(resp_json);
        buffer_free(&response);
        pool_release(g_ipc_client, entry);
        return SVC_ERR_RPC;
    }
    
    cJSON_Delete(resp_json);
    
    /* 返回结果 */
    *out_result = response.data;
    
    pool_release(g_ipc_client, entry);
    return SVC_OK;
}

/* ==================== 扩展接口 ==================== */

/**
 * @brief 设置默认超时时间
 */
int svc_ipc_set_timeout(uint32_t timeout_ms) {
    if (!g_ipc_client || !g_ipc_client->initialized) {
        return SVC_ERR_RPC;
    }
    
    g_ipc_client->default_timeout_ms = timeout_ms;
    return SVC_OK;
}

/**
 * @brief 获取连接池状态
 */
int svc_ipc_get_pool_status(int* total, int* available) {
    if (!g_ipc_client || !g_ipc_client->initialized) {
        return SVC_ERR_RPC;
    }
    
    if (total) *total = IPC_POOL_SIZE;
    
    if (available) {
        *available = 0;
        agentos_mutex_lock(&g_ipc_client->pool_lock);
        for (int i = 0; i < IPC_POOL_SIZE; i++) {
            if (!g_ipc_client->pool[i].in_use) {
                (*available)++;
            }
        }
        agentos_mutex_unlock(&g_ipc_client->pool_lock);
    }
    
    return SVC_OK;
}

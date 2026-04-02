/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file http_gateway.c
 * @brief HTTP网关实现 - libmicrohttpd集成
 *
 * 实现JSON-RPC 2.0协议处理，通过系统调用接口与内核通信。
 * 网关层只负责协议转换，不包含业务逻辑。
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "http_gateway.h"
#include "../utils/jsonrpc.h"
#include "syscalls.h"
#include "agentos.h"

#include <microhttpd.h>
#include <cJSON.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdatomic.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

/* ========== HTTP网关内部结构 ========== */

/**
 * @brief HTTP请求上下文
 */
typedef struct http_request_context {
    const char* method;              /**< HTTP方法 */
    const char* url;                 /**< 请求URL */
    const char* upload_data;         /**< 上传数据 */
    size_t upload_data_size;         /**< 上传数据大小 */
    
    cJSON* json_request;             /**< JSON请求对象 */
    uint64_t start_time_ns;          /**< 请求开始时间 */
} http_request_context_t;

/**
 * @brief HTTP网关内部结构
 */
typedef struct http_gateway {
    struct MHD_Daemon* daemon;       /**< MHD守护进程 */
    uint16_t port;                   /**< 监听端口 */
    char* host;                      /**< 监听地址 */
    
    gateway_request_handler_t handler; /**< 请求处理回调 */
    void* handler_data;              /**< 回调用户数据 */
    
    atomic_bool running;             /**< 运行标志 */
    
    atomic_uint_fast64_t requests_total;    /**< 总请求数 */
    atomic_uint_fast64_t requests_failed;   /**< 失败请求数 */
    atomic_uint_fast64_t bytes_received;    /**< 接收字节数 */
    atomic_uint_fast64_t bytes_sent;        /**< 发送字节数 */
    
    size_t max_request_size;         /**< 最大请求大小 */
} http_gateway_t;

/* ========== 辅助函数 ========== */

/**
 * @brief 获取当前时间（纳秒）
 * @return 当前时间戳
 */
static uint64_t time_ns(void) {
#ifdef _WIN32
    FILETIME ft;
    ULARGE_INTEGER uli;
    GetSystemTimeAsFileTime(&ft);
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return (uli.QuadPart - 116444736000000000ULL) * 100;
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
#endif
}

/* ========== 系统调用路由 ========== */

/**
 * @brief 处理系统调用请求
 * @param method 方法名
 * @param params 参数对象
 * @param request_id 请求ID
 * @return JSON响应字符串
 */
static char* handle_system_call(const char* method, cJSON* params, cJSON* request_id) {
    cJSON* result = NULL;
    agentos_error_t err = AGENTOS_SUCCESS;
    
    /* 任务管理 */
    if (strcmp(method, "agentos_sys_task_submit") == 0) {
        cJSON* input = cJSON_GetObjectItem(params, "input");
        cJSON* timeout = cJSON_GetObjectItem(params, "timeout_ms");
        
        if (input && cJSON_IsString(input)) {
            char* out_result = NULL;
            uint32_t timeout_ms = timeout ? (uint32_t)timeout->valueint : 0;
            err = agentos_sys_task_submit(
                input->valuestring,
                strlen(input->valuestring),
                timeout_ms,
                &out_result);
            
            if (err == AGENTOS_SUCCESS && out_result) {
                result = cJSON_CreateObject();
                cJSON_AddStringToObject(result, "result", out_result);
                free(out_result);
            }
        } else {
            return jsonrpc_create_error_response(request_id, -32602, "Invalid params: input required", NULL);
        }
    }
    else if (strcmp(method, "agentos_sys_task_query") == 0) {
        cJSON* task_id = cJSON_GetObjectItem(params, "task_id");
        
        if (task_id && cJSON_IsString(task_id)) {
            int status = 0;
            err = agentos_sys_task_query(task_id->valuestring, &status);
            
            if (err == AGENTOS_SUCCESS) {
                result = cJSON_CreateObject();
                cJSON_AddNumberToObject(result, "status", status);
            }
        } else {
            return jsonrpc_create_error_response(request_id, -32602, "Invalid params: task_id required", NULL);
        }
    }
    else if (strcmp(method, "agentos_sys_task_wait") == 0) {
        cJSON* task_id = cJSON_GetObjectItem(params, "task_id");
        cJSON* timeout = cJSON_GetObjectItem(params, "timeout_ms");
        
        if (task_id && cJSON_IsString(task_id)) {
            char* out_result = NULL;
            uint32_t timeout_ms = timeout ? (uint32_t)timeout->valueint : 0;
            err = agentos_sys_task_wait(task_id->valuestring, timeout_ms, &out_result);
            
            if (err == AGENTOS_SUCCESS && out_result) {
                result = cJSON_CreateObject();
                cJSON_AddStringToObject(result, "result", out_result);
                free(out_result);
            }
        } else {
            return jsonrpc_create_error_response(request_id, -32602, "Invalid params: task_id required", NULL);
        }
    }
    else if (strcmp(method, "agentos_sys_task_cancel") == 0) {
        cJSON* task_id = cJSON_GetObjectItem(params, "task_id");
        
        if (task_id && cJSON_IsString(task_id)) {
            err = agentos_sys_task_cancel(task_id->valuestring);
            if (err == AGENTOS_SUCCESS) {
                result = cJSON_CreateObject();
                cJSON_AddBoolToObject(result, "cancelled", true);
            }
        } else {
            return jsonrpc_create_error_response(request_id, -32602, "Invalid params: task_id required", NULL);
        }
    }
    /* 记忆管理 */
    else if (strcmp(method, "agentos_sys_memory_write") == 0) {
        cJSON* data = cJSON_GetObjectItem(params, "data");
        cJSON* metadata = cJSON_GetObjectItem(params, "metadata");
        
        if (data && cJSON_IsString(data)) {
            char* out_record_id = NULL;
            const char* meta_str = metadata ? cJSON_PrintUnformatted(metadata) : NULL;
            err = agentos_sys_memory_write(
                data->valuestring,
                strlen(data->valuestring),
                meta_str,
                &out_record_id);
            
            if (meta_str) free((void*)meta_str);
            
            if (err == AGENTOS_SUCCESS && out_record_id) {
                result = cJSON_CreateObject();
                cJSON_AddStringToObject(result, "record_id", out_record_id);
                free(out_record_id);
            }
        } else {
            return jsonrpc_create_error_response(request_id, -32602, "Invalid params: data required", NULL);
        }
    }
    else if (strcmp(method, "agentos_sys_memory_search") == 0) {
        cJSON* query = cJSON_GetObjectItem(params, "query");
        cJSON* limit = cJSON_GetObjectItem(params, "limit");
        
        if (query && cJSON_IsString(query)) {
            char** record_ids = NULL;
            float* scores = NULL;
            size_t count = 0;
            uint32_t lim = limit ? (uint32_t)limit->valueint : 10;
            
            err = agentos_sys_memory_search(query->valuestring, lim, &record_ids, &scores, &count);
            
            if (err == AGENTOS_SUCCESS) {
                result = cJSON_CreateObject();
                cJSON* results = cJSON_CreateArray();
                for (size_t i = 0; i < count; i++) {
                    cJSON* item = cJSON_CreateObject();
                    cJSON_AddStringToObject(item, "record_id", record_ids[i]);
                    cJSON_AddNumberToObject(item, "score", scores[i]);
                    cJSON_AddItemToArray(results, item);
                    free(record_ids[i]);
                }
                cJSON_AddItemToObject(result, "results", results);
                cJSON_AddNumberToObject(result, "total", count);
                free(record_ids);
                free(scores);
            }
        } else {
            return jsonrpc_create_error_response(request_id, -32602, "Invalid params: query required", NULL);
        }
    }
    else if (strcmp(method, "agentos_sys_memory_get") == 0) {
        cJSON* record_id = cJSON_GetObjectItem(params, "record_id");
        
        if (record_id && cJSON_IsString(record_id)) {
            void* out_data = NULL;
            size_t out_len = 0;
            err = agentos_sys_memory_get(record_id->valuestring, &out_data, &out_len);
            
            if (err == AGENTOS_SUCCESS && out_data) {
                result = cJSON_CreateObject();
                cJSON_AddStringToObject(result, "data", (char*)out_data);
                cJSON_AddNumberToObject(result, "length", out_len);
                free(out_data);
            }
        } else {
            return jsonrpc_create_error_response(request_id, -32602, "Invalid params: record_id required", NULL);
        }
    }
    else if (strcmp(method, "agentos_sys_memory_delete") == 0) {
        cJSON* record_id = cJSON_GetObjectItem(params, "record_id");
        
        if (record_id && cJSON_IsString(record_id)) {
            err = agentos_sys_memory_delete(record_id->valuestring);
            if (err == AGENTOS_SUCCESS) {
                result = cJSON_CreateObject();
                cJSON_AddBoolToObject(result, "deleted", true);
            }
        } else {
            return jsonrpc_create_error_response(request_id, -32602, "Invalid params: record_id required", NULL);
        }
    }
    /* 会话管理 */
    else if (strcmp(method, "agentos_sys_session_create") == 0) {
        cJSON* metadata = cJSON_GetObjectItem(params, "metadata");
        char* out_session_id = NULL;
        const char* meta_str = metadata ? cJSON_PrintUnformatted(metadata) : NULL;
        
        err = agentos_sys_session_create(meta_str, &out_session_id);
        
        if (meta_str) free((void*)meta_str);
        
        if (err == AGENTOS_SUCCESS && out_session_id) {
            result = cJSON_CreateObject();
            cJSON_AddStringToObject(result, "session_id", out_session_id);
            free(out_session_id);
        }
    }
    else if (strcmp(method, "agentos_sys_session_get") == 0) {
        cJSON* session_id = cJSON_GetObjectItem(params, "session_id");
        
        if (session_id && cJSON_IsString(session_id)) {
            char* out_info = NULL;
            err = agentos_sys_session_get(session_id->valuestring, &out_info);
            
            if (err == AGENTOS_SUCCESS && out_info) {
                result = cJSON_Parse(out_info);
                free(out_info);
            }
        } else {
            return jsonrpc_create_error_response(request_id, -32602, "Invalid params: session_id required", NULL);
        }
    }
    else if (strcmp(method, "agentos_sys_session_close") == 0) {
        cJSON* session_id = cJSON_GetObjectItem(params, "session_id");
        
        if (session_id && cJSON_IsString(session_id)) {
            err = agentos_sys_session_close(session_id->valuestring);
            if (err == AGENTOS_SUCCESS) {
                result = cJSON_CreateObject();
                cJSON_AddBoolToObject(result, "closed", true);
            }
        } else {
            return jsonrpc_create_error_response(request_id, -32602, "Invalid params: session_id required", NULL);
        }
    }
    else if (strcmp(method, "agentos_sys_session_list") == 0) {
        char* sessions_json = NULL;
        err = agentos_sys_session_list(&sessions_json);
        
        if (err == AGENTOS_SUCCESS && sessions_json) {
            result = cJSON_Parse(sessions_json);
            free(sessions_json);
        }
    }
    /* 可观测性 */
    else if (strcmp(method, "agentos_sys_telemetry_metrics") == 0) {
        char* out_metrics = NULL;
        err = agentos_sys_telemetry_metrics(&out_metrics);
        
        if (err == AGENTOS_SUCCESS && out_metrics) {
            result = cJSON_Parse(out_metrics);
            free(out_metrics);
        }
    }
    else if (strcmp(method, "agentos_sys_telemetry_traces") == 0) {
        cJSON* trace_id = cJSON_GetObjectItem(params, "trace_id");
        const char* tid = (trace_id && cJSON_IsString(trace_id)) ? trace_id->valuestring : NULL;
        char* out_traces = NULL;
        err = agentos_sys_telemetry_traces(tid, &out_traces);
        
        if (err == AGENTOS_SUCCESS && out_traces) {
            result = cJSON_Parse(out_traces);
            free(out_traces);
        }
    }
    else {
        return jsonrpc_create_error_response(request_id, -32601, "Method not found", NULL);
    }
    
    if (err != AGENTOS_SUCCESS) {
        cJSON_Delete(result);
        char err_msg[64];
        snprintf(err_msg, sizeof(err_msg), "System call failed: %d", err);
        return jsonrpc_create_error_response(request_id, -32000, err_msg, NULL);
    }
    
    return jsonrpc_create_success_response(request_id, result);
}

/* ========== HTTP响应生成 ========== */

/**
 * @brief 生成HTTP响应
 * @param status_code HTTP状态码
 * @param content 响应内容
 * @param content_len 内容长度
 * @return MHD响应对象
 */
static struct MHD_Response* create_http_response(int status_code, const char* content, size_t content_len) {
    (void)status_code;
    
    struct MHD_Response* response = MHD_create_response_from_buffer(
        content_len, (void*)content, MHD_RESPMEM_MUST_COPY);
    
    if (!response) {
        return NULL;
    }
    
    MHD_add_response_header(response, "Content-Type", "application/json");
    MHD_add_response_header(response, "Server", "AgentOS-gateway/1.0");
    MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
    MHD_add_response_header(response, "Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    MHD_add_response_header(response, "Access-Control-Allow-Headers", "Content-Type, Authorization");
    
    return response;
}

/* ========== 请求处理 ========== */

/**
 * @brief 解析HTTP请求头
 */
static int parse_headers(void* cls, enum MHD_ValueKind kind, const char* key, const char* value) {
    (void)cls;
    (void)kind;
    (void)key;
    (void)value;
    return MHD_YES;
}

/**
 * @brief 解析JSON请求体
 * @param context 请求上下文
 * @param data 请求体数据
 * @param size 数据大小
 * @return 0 成功，非0 失败
 */
static int parse_json_request(http_gateway_t* gateway, http_request_context_t* context, const char* data, size_t size) {
    if (!data || size == 0) {
        return -1;
    }
    
    if (size > gateway->max_request_size) {
        return -1;
    }
    
    context->json_request = cJSON_Parse(data);
    if (!context->json_request) {
        return -1;
    }
    
    if (jsonrpc_validate_request(context->json_request) != 0) {
        cJSON_Delete(context->json_request);
        context->json_request = NULL;
        return -1;
    }
    
    return 0;
}
/**
 * @brief 处理JSON-RPC请求
 * @param gateway 网关实例
 * @param context 请求上下文
 * @return JSON响应字符串
 */
static char* handle_jsonrpc_request(http_gateway_t* gateway, http_request_context_t* context) {
    (void)gateway;
    
    cJSON* method = cJSON_GetObjectItem(context->json_request, "method");
    cJSON* params = cJSON_GetObjectItem(context->json_request, "params");
    cJSON* request_id = cJSON_GetObjectItem(context->json_request, "id");
    
    if (!method) {
        return jsonrpc_create_error_response(NULL, -32600, "Invalid Request", NULL);
    }
    
    return handle_system_call(method->valuestring, params, request_id);
}
/**
 * @brief HTTP请求处理器
 */
static int handle_http_request(void* cls, struct MHD_Connection* connection,
                                   const char* url, const char* method,
                                   const char* version, const char* upload_data,
                                   size_t* upload_data_size, void** con_cls) {
    http_gateway_t* gateway = (http_gateway_t*)cls;
    http_request_context_t* context = (http_request_context_t*)*con_cls;
    
    (void)connection;
    (void)version;
    
    /* 初始化请求上下文 */
    if (!context) {
        context = calloc(1, sizeof(http_request_context_t));
        if (!context) {
            return MHD_NO;
        }
        context->method = method;
        context->url = url;
        context->start_time_ns = time_ns();
        *con_cls = context;
        
        MHD_get_connection_values(connection, MHD_HEADER_KIND, parse_headers, context);
        
        return MHD_YES;
    }
    
    /* 处理POST数据 */
    if (strcmp(method, "POST") == 0 && upload_data && *upload_data_size > 0) {
        if (*upload_data_size > gateway->max_request_size) {
            char* error_response = jsonrpc_create_error_response(NULL, -413, "Request too large", NULL);
            struct MHD_Response* response = create_http_response(413, error_response, strlen(error_response));
            free(error_response);
            
            atomic_fetch_add(&gateway->requests_failed, 1);
            atomic_fetch_add(&gateway->bytes_received, *upload_data_size);
            
            int ret = MHD_queue_response(connection, 413, response);
            MHD_destroy_response(response);
            free(context);
            *con_cls = NULL;
            
            return ret;
        }
        
        context->upload_data = upload_data;
        context->upload_data_size = *upload_data_size;
        
        if (parse_json_request(gateway, context, upload_data, *upload_data_size) != 0) {
            char* error_response = jsonrpc_create_error_response(NULL, -32700, "Parse error", NULL);
            struct MHD_Response* response = create_http_response(400, error_response, strlen(error_response));
            free(error_response);
            
            atomic_fetch_add(&gateway->requests_failed, 1);
            atomic_fetch_add(&gateway->bytes_received, *upload_data_size);
            
            int ret = MHD_queue_response(connection, 400, response);
            MHD_destroy_response(response);
            free(context);
            *con_cls = NULL;
            
            return ret;
        }
        
        *upload_data_size = 0;
        return MHD_YES;
    }
    
    /* 完整请求处理 */
    if (strcmp(method, "POST") == 0 && context->json_request) {
        char* json_response = handle_jsonrpc_request(gateway, context);
        struct MHD_Response* response = create_http_response(200, json_response, strlen(json_response));
        
        uint64_t response_time_ns = time_ns() - context->start_time_ns;
        atomic_fetch_add(&gateway->requests_total, 1);
        atomic_fetch_add(&gateway->bytes_received, context->upload_data_size);
        atomic_fetch_add(&gateway->bytes_sent, strlen(json_response));
        
        int ret = MHD_queue_response(connection, 200, response);
        MHD_destroy_response(response);
        free(json_response);
        cJSON_Delete(context->json_request);
        free(context);
        *con_cls = NULL;
        
        return ret;
    }
    
    /* OPTIONS请求（CORS预检） */
    if (strcmp(method, "OPTIONS") == 0) {
        struct MHD_Response* response = MHD_create_response_from_buffer(0, NULL, MHD_RESPMEM_PERSISTENT);
        MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
        MHD_add_response_header(response, "Access-Control-Allow-Methods", "POST, GET, OPTIONS");
        MHD_add_response_header(response, "Access-Control-Allow-Headers", "Content-Type, Authorization");
        
        int ret = MHD_queue_response(connection, 200, response);
        MHD_destroy_response(response);
        free(context);
        *con_cls = NULL;
        
        return ret;
    }
    
    /* GET请求 - 健康检查 */
    if (strcmp(method, "GET") == 0 && strcmp(url, "/health") == 0) {
        const char* health_json = "{\"status\":\"healthy\",\"service\":\"gateway\"}";
        struct MHD_Response* response = create_http_response(200, health_json, strlen(health_json));
        
        atomic_fetch_add(&gateway->requests_total, 1);
        
        int ret = MHD_queue_response(connection, 200, response);
        MHD_destroy_response(response);
        free(context);
        *con_cls = NULL;
        
        return ret;
    }
    
    /* GET请求 - 指标导出 */
    if (strcmp(method, "GET") == 0 && strcmp(url, "/metrics") == 0) {
        char* metrics_json = NULL;
        agentos_error_t err = agentos_sys_telemetry_metrics(&metrics_json);
        
        if (err != AGENTOS_SUCCESS || !metrics_json) {
            metrics_json = strdup("{\"error\":\"failed to get metrics\"}");
        }
        
        struct MHD_Response* response = create_http_response(200, metrics_json, strlen(metrics_json));
        free(metrics_json);
        
        atomic_fetch_add(&gateway->requests_total, 1);
        
        int ret = MHD_queue_response(connection, 200, response);
        MHD_destroy_response(response);
        free(context);
        *con_cls = NULL;
        
        return ret;
    }
    
    /* 404 Not Found */
    char* error_response = jsonrpc_create_error_response(NULL, -32601, "Not Found", NULL);
    struct MHD_Response* response = create_http_response(404, error_response, strlen(error_response));
    free(error_response);
    
    atomic_fetch_add(&gateway->requests_failed, 1);
    
    int ret = MHD_queue_response(connection, 404, response);
    MHD_destroy_response(response);
    free(context);
    *con_cls = NULL;
    
    return ret;
}
/* ========== 网关操作表 ========== */

static agentos_error_t http_gateway_start(void* gateway_impl) {
    http_gateway_t* gateway = (http_gateway_t*)gateway_impl;
    
    gateway->daemon = MHD_start_daemon(
        MHD_USE_THREAD_PER_CONNECTION,
        gateway->port,
        NULL, NULL,
        handle_http_request,
        gateway,
        MHD_OPTION_CONNECTION_LIMIT, 1000,
        MHD_OPTION_CONNECTION_TIMEOUT, 30,
        MHD_OPTION_END);
    
    if (!gateway->daemon) {
        return AGENTOS_EBUSY;
    }
    
    atomic_store(&gateway->running, true);
    
    return AGENTOS_SUCCESS;
}
static void http_gateway_stop(void* gateway_impl) {
    http_gateway_t* gateway = (http_gateway_t*)gateway_impl;
    
    atomic_store(&gateway->running, false);
    
    if (gateway->daemon) {
        MHD_stop_daemon(gateway->daemon);
        gateway->daemon = NULL;
    }
}
static void http_gateway_destroy(void* gateway_impl) {
    http_gateway_t* gateway = (http_gateway_t*)gateway_impl;
    
    http_gateway_stop(gateway);
    
    if (gateway->host) {
        free(gateway->host);
    }
    
    free(gateway);
}
static const char* http_gateway_get_name(void* gateway_impl) {
    (void)gateway_impl;
    return "HTTP Gateway";
}
static agentos_error_t http_gateway_get_stats(void* gateway_impl, char** out_json) {
    http_gateway_t* gateway = (http_gateway_t*)gateway_impl;
    
    cJSON* stats = cJSON_CreateObject();
    cJSON_AddNumberToObject(stats, "requests_total", (double)atomic_load(&gateway->requests_total));
    cJSON_AddNumberToObject(stats, "requests_failed", (double)atomic_load(&gateway->requests_failed));
    cJSON_AddNumberToObject(stats, "bytes_received", (double)atomic_load(&gateway->bytes_received));
    cJSON_AddNumberToObject(stats, "bytes_sent", (double)atomic_load(&gateway->bytes_sent));
    
    char* json_str = cJSON_Print(stats);
    cJSON_Delete(stats);
    
    *out_json = json_str;
    return AGENTOS_SUCCESS;
}
static const gateway_ops_t http_gateway_ops = {
    .start = http_gateway_start,
    .stop = http_gateway_stop,
    .destroy = http_gateway_destroy,
    .get_name = http_gateway_get_name,
    .get_stats = http_gateway_get_stats,
    .is_running = http_gateway_is_running
};
/* ========== 公共接口 ========== */
gateway_t* http_gateway_create(const char* host, uint16_t port) {
    if (!host) {
        return NULL;
    }
    
    http_gateway_t* gateway = calloc(1, sizeof(http_gateway_t));
    if (!gateway) {
        return NULL;
    }
    
    gateway->port = port;
    gateway->host = strdup(host);
    
    if (!gateway->host) {
        free(gateway);
        return NULL;
    }
    
    atomic_init(&gateway->running, false);
    atomic_init(&gateway->requests_total, 0);
    atomic_init(&gateway->requests_failed, 0);
    atomic_init(&gateway->bytes_received, 0);
    atomic_init(&gateway->bytes_sent, 0);
    
    gateway->max_request_size = 10 * 1024 * 1024; /* 10MB */
    
    gateway_t* gw = malloc(sizeof(gateway_t));
    if (!gw) {
        free(gateway->host);
        free(gateway);
        return NULL;
    }
    
    gw->ops = &http_gateway_ops;
    gw->impl = gateway;
    gw->type = GATEWAY_TYPE_HTTP;
    
    return gw;
}

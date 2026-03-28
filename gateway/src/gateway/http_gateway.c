/**
 * @file http_gateway.c
 * @brief HTTP网关实现 - libmicrohttpd集成
 * 
 * 实现JSON-RPC 2.0协议处理，支持与AgentOS内核的系统调用接口�?
 * 包含请求验证、响应生成、错误处理等完整功能�?
 * 
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "http_gateway.h"
#include "../server.h"
#include "../session.h"
#include "../health.h"
#include "../telemetry.h"
#include "../auth.h"
#include "../ratelimit.h"
#include "../platform.h"

#include <microhttpd.h>
#include <cJSON.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>
#include <arpa/inet.h>

/* ========== HTTP网关内部结构 ========== */

/**
 * @brief HTTP请求上下�?
 */
typedef struct http_request_context {
    gateway_server_t* server;        /**< 服务器引�?*/
    const char* method;              /**< HTTP方法 */
    const char* url;                 /**< 请求URL */
    const char* version;             /**< HTTP版本 */
    const char* upload_data;         /**< 上传数据 */
    size_t upload_data_size;         /**< 上传数据大小 */
    void* connection_cls;            /**< 连接�?*/
    
    /* 解析后的数据 */
    cJSON* json_request;             /**< JSON请求对象 */
    char* session_id;                /**< 会话ID */
    char* authorization;             /**< 认证�?*/
    
    /* 统计信息 */
    uint64_t start_time_ns;          /**< 请求开始时�?*/
} http_request_context_t;

/**
 * @brief HTTP网关内部结构
 */
typedef struct http_gateway {
    gateway_server_t* server;        /**< 服务器引�?*/
    struct MHD_Daemon* daemon;       /**< MHD守护进程 */
    uint16_t port;                   /**< 监听端口 */
    char* host;                      /**< 监听地址 */
    
    /* 统计信息 */
    atomic_uint_fast64_t requests_total;    /**< 总请求数 */
    atomic_uint_fast64_t requests_failed;   /**< 失败请求�?*/
    atomic_uint_fast64_t bytes_received;    /**< 接收字节�?*/
    atomic_uint_fast64_t bytes_sent;        /**< 发送字节数 */
    
    /* 限流�?*/
    ratelimiter_t* ratelimiter;      /**< 请求限流�?*/
    
    /* 安全配置 */
    size_t max_request_size;         /**< 最大请求大�?*/
} http_gateway_t;

/* ========== JSON-RPC 2.0处理 ========== */

/**
 * @brief 验证JSON-RPC 2.0请求格式
 * @param json JSON对象
 * @return 0 有效，非0 无效
 */
static int validate_jsonrpc_request(cJSON* json) {
    if (!json) return -1;
    
    /* 检查必需字段 */
    if (!cJSON_HasObjectItem(json, "jsonrpc") || 
        !cJSON_HasObjectItem(json, "method") || 
        !cJSON_HasObjectItem(json, "id")) {
        return -1;
    }
    
    cJSON* jsonrpc = cJSON_GetObjectItem(json, "jsonrpc");
    cJSON* method = cJSON_GetObjectItem(json, "method");
    cJSON* id = cJSON_GetObjectItem(json, "id");
    
    /* 验证字段类型和�?*/
    if (!cJSON_IsString(jsonrpc) || strcmp(jsonrpc->valuestring, "2.0") != 0) {
        return -1;
    }
    
    if (!cJSON_IsString(method) || strlen(method->valuestring) == 0) {
        return -1;
    }
    
    if (!cJSON_IsNumber(id) && !cJSON_IsString(id)) {
        return -1;
    }
    
    return 0;
}

/**
 * @brief 创建JSON-RPC 2.0成功响应
 * @param id 请求ID
 * @param result 结果对象
 * @return JSON字符串，需调用者free
 */
static char* create_jsonrpc_success_response(cJSON* id, cJSON* result) {
    cJSON* response = cJSON_CreateObject();
    
    cJSON_AddStringToObject(response, "jsonrpc", "2.0");
    cJSON_AddItemToObject(response, "result", result ? result : cJSON_CreateNull());
    cJSON_AddItemToObject(response, "id", id ? cJSON_Duplicate(id, 1) : cJSON_CreateNull());
    
    char* json_str = cJSON_PrintUnformatted(response);
    cJSON_Delete(response);
    
    return json_str;
}

/**
 * @brief 创建JSON-RPC 2.0错误响应
 * @param id 请求ID
 * @param code 错误�?
 * @param message 错误消息
 * @param data 错误数据
 * @return JSON字符串，需调用者free
 */
static char* create_jsonrpc_error_response(cJSON* id, int code, const char* message, cJSON* data) {
    cJSON* response = cJSON_CreateObject();
    cJSON* error = cJSON_CreateObject();
    
    cJSON_AddNumberToObject(error, "code", code);
    cJSON_AddStringToObject(error, "message", message ? message : "Internal error");
    
    if (data) {
        cJSON_AddItemToObject(error, "data", data);
    }
    
    cJSON_AddStringToObject(response, "jsonrpc", "2.0");
    cJSON_AddItemToObject(response, "error", error);
    cJSON_AddItemToObject(response, "id", id ? cJSON_Duplicate(id, 1) : cJSON_CreateNull());
    
    char* json_str = cJSON_PrintUnformatted(response);
    cJSON_Delete(response);
    
    return json_str;
}

/* ========== 系统调用接口 ========== */

/**
 * @brief 处理系统调用请求
 * @param context 请求上下�?
 * @param method 方法�?
 * @param params 参数对象
 * @return JSON响应字符�?
 */
static char* handle_system_call(http_request_context_t* context, const char* method, cJSON* params) {
    /* 这里实现与AgentOS内核的系统调用接�?*/
    /* 目前返回模拟响应，实际实现需要调用agentos_sys_*函数 */
    
    AGENTOS_LOG_DEBUG("Handling system call: %s", method);
    
    /* 模拟系统调用响应 */
    cJSON* response = cJSON_CreateObject();
    
    if (strcmp(method, "agentos_sys_task_submit") == 0) {
        /* 提交任务 */
        cJSON_AddStringToObject(response, "status", "accepted");
        cJSON_AddStringToObject(response, "task_id", "task_001");
        cJSON_AddNumberToObject(response, "estimated_time", 5000);
    } 
    else if (strcmp(method, "agentos_sys_memory_search") == 0) {
        /* 记忆搜索 */
        cJSON* results = cJSON_CreateArray();
        cJSON_AddItemToArray(results, cJSON_CreateString("sample_memory_1"));
        cJSON_AddItemToArray(results, cJSON_CreateString("sample_memory_2"));
        cJSON_AddItemToObject(response, "results", results);
        cJSON_AddNumberToObject(response, "total", 2);
    }
    else if (strcmp(method, "agentos_sys_session_create") == 0) {
        /* 创建会话 */
        char* session_id = NULL;
        agentos_error_t err = session_manager_create_session(
            context->server->session_mgr, 
            params ? cJSON_Print(params) : NULL, 
            &session_id);
        
        if (err == AGENTOS_SUCCESS) {
            cJSON_AddStringToObject(response, "session_id", session_id);
            free(session_id);
        } else {
            cJSON_AddStringToObject(response, "error", "Failed to create session");
        }
    }
    else {
        /* 未知方法 */
        cJSON_Delete(response);
        return create_jsonrpc_error_response(
            context->json_request ? cJSON_GetObjectItem(context->json_request, "id") : NULL,
            -32601, "Method not found", NULL);
    }
    
    return create_jsonrpc_success_response(
        context->json_request ? cJSON_GetObjectItem(context->json_request, "id") : NULL,
        response);
}

/* ========== 认证和授�?========== */

/**
 * @brief 验证请求认证
 * @param context 请求上下�?
 * @return 0 成功，非0 失败
 */
static int authenticate_request(http_request_context_t* context) {
    if (!context->authorization) {
        return -1; /* 缺少认证�?*/
    }
    
    /* 解析Bearer token */
    if (strncmp(context->authorization, "Bearer ", 7) == 0) {
        const char* token = context->authorization + 7;
        
        /* 验证token有效�?*/
        if (context->server->auth_context) {
            auth_result_t result = auth_validate(context->server->auth_context, token);
            if (result == AUTH_RESULT_ALLOWED) {
                return 0; /* 认证成功 */
            }
        }
    }
    
    return -1; /* 认证失败 */
}

/* ========== 请求处理 ========== */

/**
 * @brief 解析HTTP请求�?
 * @param cls 客户端数�?
 * @param kind 值类�?
 * @param key 头键
 * @param value 头�?
 * @return MHD_YES 继续处理
 */
static int parse_headers(void* cls, enum MHD_ValueKind kind, const char* key, const char* value) {
    http_request_context_t* context = (http_request_context_t*)cls;
    (void)kind; /* 未使用参�?*/
    
    if (strcmp(key, "Authorization") == 0) {
        context->authorization = strdup(value ? value : "");
    }
    else if (strcmp(key, "Content-Type") == 0) {
        if (value && strstr(value, "application/json") == NULL) {
            /* 不支持的Content-Type */
            context->json_request = NULL;
        }
    }
    
    return MHD_YES;
}

/**
 * @brief 解析JSON请求�?
 * @param context 请求上下�?
 * @param data 请求体数�?
 * @param size 数据大小
 * @return 0 成功，非0 失败
 */
static int parse_json_request(http_request_context_t* context, const char* data, size_t size) {
    if (!data || size == 0) {
        return -1;
    }
    
    /* 验证请求大小 */
    if (size > context->server->manager.max_request_size) {
        return -1;
    }
    
    context->json_request = cJSON_Parse(data);
    if (!context->json_request) {
        return -1;
    }
    
    /* 验证JSON-RPC 2.0格式 */
    if (validate_jsonrpc_request(context->json_request) != 0) {
        cJSON_Delete(context->json_request);
        context->json_request = NULL;
        return -1;
    }
    
    return 0;
}

/**
 * @brief 处理JSON-RPC请求
 * @param context 请求上下�?
 * @return JSON响应字符�?
 */
static char* handle_jsonrpc_request(http_request_context_t* context) {
    cJSON* method = cJSON_GetObjectItem(context->json_request, "method");
    cJSON* params = cJSON_GetObjectItem(context->json_request, "params");
    
    if (!method) {
        return create_jsonrpc_error_response(NULL, -32600, "Invalid Request", NULL);
    }
    
    /* 检查会话（如果需要） */
    if (strcmp(method->valuestring, "agentos_sys_session_create") != 0) {
        /* 验证会话ID */
        cJSON* id_item = cJSON_GetObjectItem(context->json_request, "id");
        if (id_item && cJSON_IsString(id_item)) {
            context->session_id = strdup(id_item->valuestring);
            
            /* 验证会话有效�?*/
            char* session_info = NULL;
            if (session_manager_get_session(context->server->session_mgr, context->session_id, &session_info) != AGENTOS_SUCCESS) {
                free(context->session_id);
                context->session_id = NULL;
                return create_jsonrpc_error_response(id_item, -32001, "Invalid session", NULL);
            }
            free(session_info);
        }
    }
    
    /* 处理系统调用 */
    return handle_system_call(context, method->valuestring, params);
}

/**
 * @brief 生成HTTP响应
 * @param status_code HTTP状态码
 * @param content 响应内容
 * @param content_len 内容长度
 * @return MHD响应对象
 */
static struct MHD_Response* create_http_response(int status_code, const char* content, size_t content_len) {
    (void)status_code; /* 未使用参�?*/
    
    struct MHD_Response* response = MHD_create_response_from_buffer(
        content_len, (void*)content, MHD_RESPMEM_MUST_COPY);
    
    if (!response) {
        return NULL;
    }
    
    /* 设置响应�?*/
    MHD_add_response_header(response, "Content-Type", "application/json");
    MHD_add_response_header(response, "Server", "AgentOS-gateway/1.1.0");
    MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
    MHD_add_response_header(response, "Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    MHD_add_response_header(response, "Access-Control-Allow-Headers", "Content-Type, Authorization");
    
    return response;
}

/**
 * @brief 检查限�?
 * @param gateway 网关实例
 * @param connection MHD连接
 * @return 0 允许，非0 拒绝
 */
static int check_rate_limit(http_gateway_t* gateway, struct MHD_Connection* connection) {
    if (!gateway->ratelimiter) {
        return 0; /* 无限流器，允许通过 */
    }
    
    /* 获取客户端IP地址 */
    char ip_buf[64] = "unknown";
    const union MHD_ConnectionInfo* info = MHD_get_connection_info(connection, MHD_CONNECTION_INFO_CLIENT_ADDRESS);
    
    if (info && info->client_addr) {
        struct sockaddr* addr = (struct sockaddr*)info->client_addr;
        if (addr->sa_family == AF_INET) {
            struct sockaddr_in* addr_in = (struct sockaddr_in*)addr;
            inet_ntop(AF_INET, &addr_in->sin_addr, ip_buf, sizeof(ip_buf));
        } else if (addr->sa_family == AF_INET6) {
            struct sockaddr_in6* addr_in6 = (struct sockaddr_in6*)addr;
            inet_ntop(AF_INET6, &addr_in6->sin6_addr, ip_buf, sizeof(ip_buf));
        }
    }
    
    /* 检查限�?*/
    ratelimit_result_t result = ratelimiter_check(gateway->ratelimiter, ip_buf);
    return (result == RATELIMIT_ALLOWED) ? 0 : -1;
}

/**
 * @brief HTTP请求处理�?
 * @param cls 网关实例
 * @param connection MHD连接
 * @param url 请求URL
 * @param method HTTP方法
 * @param version HTTP版本
 * @param upload_data 上传数据
 * @param upload_data_size 上传数据大小
 * @param connection_cls 连接�?
 * @return MHD_YES 继续处理
 */
static int handle_http_request(void* cls, struct MHD_Connection* connection, 
                             const char* url, const char* method, 
                             const char* version, const char* upload_data,
                             size_t* upload_data_size, void** connection_cls) {
    
    http_gateway_t* gateway = (http_gateway_t*)cls;
    http_request_context_t* context = (http_request_context_t*)*connection_cls;
    
    /* 初始化请求上下文 */
    if (!context) {
        context = calloc(1, sizeof(http_request_context_t));
        if (!context) {
            return MHD_NO;
        }
        context->server = gateway->server;
        context->method = method;
        context->url = url;
        context->version = version;
        context->start_time_ns = platform_get_current_time_ms() * 1000000ULL;
        *connection_cls = context;
        
        /* 解析请求�?*/
        MHD_get_connection_values(connection, MHD_HEADER_KIND, parse_headers, context);
        
        return MHD_YES;
    }
    
    /* 处理POST数据 */
    if (strcmp(method, "POST") == 0 && upload_data && *upload_data_size > 0) {
        /* 验证请求大小 */
        if (*upload_data_size > gateway->max_request_size) {
            char* error_response = create_jsonrpc_error_response(
                NULL, -413, "Request too large", NULL);
            struct MHD_Response* response = create_http_response(413, error_response, strlen(error_response));
            free(error_response);
            
            atomic_fetch_add(&gateway->requests_failed, 1);
            atomic_fetch_add(&gateway->bytes_received, *upload_data_size);
            
            int ret = MHD_queue_response(connection, 413, response);
            MHD_destroy_response(response);
            free(context);
            *connection_cls = NULL;
            
            return ret;
        }
        
        context->upload_data = upload_data;
        context->upload_data_size = *upload_data_size;
        
        /* 解析JSON请求 */
        if (parse_json_request(context, upload_data, *upload_data_size) != 0) {
            /* JSON解析失败 */
            char* error_response = create_jsonrpc_error_response(
                NULL, -32700, "Parse error", NULL);
            struct MHD_Response* response = create_http_response(400, error_response, strlen(error_response));
            free(error_response);
            
            atomic_fetch_add(&gateway->requests_failed, 1);
            atomic_fetch_add(&gateway->bytes_received, *upload_data_size);
            
            int ret = MHD_queue_response(connection, 400, response);
            MHD_destroy_response(response);
            free(context);
            *connection_cls = NULL;
            
            return ret;
        }
        
        *upload_data_size = 0; /* 标记数据接收完成 */
        return MHD_YES;
    }
    
    /* 完整请求处理 */
    if (strcmp(method, "POST") == 0 && context->json_request) {
        /* 检查限�?*/
        if (check_rate_limit(gateway, connection) != 0) {
            char* error_response = create_jsonrpc_error_response(
                NULL, -429, "Too many requests", NULL);
            struct MHD_Response* response = create_http_response(429, error_response, strlen(error_response));
            free(error_response);
            
            atomic_fetch_add(&gateway->requests_failed, 1);
            atomic_fetch_add(&gateway->bytes_received, context->upload_data_size);
            
            int ret = MHD_queue_response(connection, 429, response);
            MHD_destroy_response(response);
            free(context);
            *connection_cls = NULL;
            
            return ret;
        }
        
        /* 处理JSON-RPC请求 */
        char* json_response = handle_jsonrpc_request(context);
        
        /* 生成HTTP响应 */
        struct MHD_Response* response = create_http_response(200, json_response, strlen(json_response));
        
        /* 更新统计信息 */
        uint64_t response_time_ns = platform_get_current_time_ms() * 1000000ULL - context->start_time_ns;
        atomic_fetch_add(&gateway->requests_total, 1);
        atomic_fetch_add(&gateway->bytes_received, context->upload_data_size);
        atomic_fetch_add(&gateway->bytes_sent, strlen(json_response));
        
        /* 记录指标 */
        if (gateway->server->telemetry) {
            telemetry_increment_counter(gateway->server->telemetry, "http_requests_total", 1, "method,endpoint");
            telemetry_observe_histogram(gateway->server->telemetry, "http_request_duration_seconds", 
                                       response_time_ns / 1000000000.0, "method,endpoint");
        }
        
        AGENTOS_LOG_DEBUG("HTTP request processed: %s %s (%.3f ms)", 
                          method, url, response_time_ns / 1000000.0);
        
        int ret = MHD_queue_response(connection, 200, response);
        MHD_destroy_response(response);
        free(json_response);
        free(context->session_id);
        free(context->authorization);
        cJSON_Delete(context->json_request);
        free(context);
        *connection_cls = NULL;
        
        return ret;
    }
    
    /* OPTIONS请求（CORS预检�?*/
    if (strcmp(method, "OPTIONS") == 0) {
        struct MHD_Response* response = MHD_create_response_from_buffer(0, NULL, MHD_RESPMEM_PERSISTENT);
        MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
        MHD_add_response_header(response, "Access-Control-Allow-Methods", "POST, GET, OPTIONS");
        MHD_add_response_header(response, "Access-Control-Allow-Headers", "Content-Type, Authorization");
        
        int ret = MHD_queue_response(connection, 200, response);
        MHD_destroy_response(response);
        free(context);
        *connection_cls = NULL;
        
        return ret;
    }
    
    /* GET请求 - 健康检�?*/
    if (strcmp(method, "GET") == 0 && strcmp(url, "/health") == 0) {
        char* health_json = NULL;
        if (gateway->server->health) {
            health_checker_get_report(gateway->server->health, &health_json);
        } else {
            health_json = strdup("{\"status\":\"healthy\"}");
        }
        
        struct MHD_Response* response = create_http_response(200, health_json, strlen(health_json));
        free(health_json);
        
        atomic_fetch_add(&gateway->requests_total, 1);
        
        int ret = MHD_queue_response(connection, 200, response);
        MHD_destroy_response(response);
        free(context);
        *connection_cls = NULL;
        
        return ret;
    }
    
    /* GET请求 - 指标导出 */
    if (strcmp(method, "GET") == 0 && strcmp(url, "/metrics") == 0) {
        char* metrics_json = NULL;
        if (gateway->server->telemetry) {
            telemetry_export_metrics(gateway->server->telemetry, &metrics_json);
        } else {
            metrics_json = strdup("{}");
        }
        
        struct MHD_Response* response = create_http_response(200, metrics_json, strlen(metrics_json));
        free(metrics_json);
        
        atomic_fetch_add(&gateway->requests_total, 1);
        
        int ret = MHD_queue_response(connection, 200, response);
        MHD_destroy_response(response);
        free(context);
        *connection_cls = NULL;
        
        return ret;
    }
    
    /* 404 Not Found */
    char* error_response = create_jsonrpc_error_response(
        NULL, -32601, "Not Found", NULL);
    struct MHD_Response* response = create_http_response(404, error_response, strlen(error_response));
    free(error_response);
    
    atomic_fetch_add(&gateway->requests_failed, 1);
    
    int ret = MHD_queue_response(connection, 404, response);
    MHD_destroy_response(response);
    free(context);
    *connection_cls = NULL;
    
    return ret;
}

/* ========== 网关操作�?========== */

/**
 * @brief 启动HTTP网关
 * @param gateway_impl 网关实例
 * @return AGENTOS_SUCCESS 成功
 */
static agentos_error_t http_gateway_start(void* gateway_impl) {
    http_gateway_t* gateway = (http_gateway_t*)gateway_impl;
    
    /* 创建MHD守护进程 */
    gateway->daemon = MHD_start_daemon(
        MHD_USE_THREAD_PER_CONNECTION | MHD_USE_INTERNAL_POLLING_THREAD,
        gateway->port,
        NULL, NULL,
        handle_http_request,
        gateway,
        MHD_OPTION_CONNECTION_LIMIT, 1000,
        MHD_OPTION_CONNECTION_TIMEOUT, 30,
        MHD_OPTION_END);
    
    if (!gateway->daemon) {
        AGENTOS_LOG_ERROR("Failed to start HTTP gateway on port %d", gateway->port);
        return AGENTOS_EBUSY;
    }
    
    AGENTOS_LOG_INFO("HTTP gateway started on %s:%d", gateway->host, gateway->port);
    return AGENTOS_SUCCESS;
}

/**
 * @brief 停止HTTP网关
 * @param gateway_impl 网关实例
 */
static void http_gateway_stop(void* gateway_impl) {
    http_gateway_t* gateway = (http_gateway_t*)gateway_impl;
    
    if (gateway->daemon) {
        MHD_stop_daemon(gateway->daemon);
        gateway->daemon = NULL;
        AGENTOS_LOG_INFO("HTTP gateway stopped");
    }
}

/**
 * @brief 销毁HTTP网关
 * @param gateway_impl 网关实例
 */
static void http_gateway_destroy(void* gateway_impl) {
    http_gateway_t* gateway = (http_gateway_t*)gateway_impl;
    
    http_gateway_stop(gateway);
    
    if (gateway->host) {
        free(gateway->host);
    }
    
    if (gateway->ratelimiter) {
        ratelimiter_destroy(gateway->ratelimiter);
    }
    
    free(gateway);
}

/**
 * @brief 获取HTTP网关名称
 * @param gateway_impl 网关实例
 * @return 名称字符�?
 */
static const char* http_gateway_get_name(void* gateway_impl) {
    (void)gateway_impl;
    return "HTTP Gateway";
}

/**
 * @brief 获取HTTP网关统计信息
 * @param gateway_impl 网关实例
 * @param out_json 输出JSON字符串（需调用者free�?
 * @return AGENTOS_SUCCESS 成功
 */
static agentos_error_t http_gateway_get_stats(void* gateway_impl, char** out_json) {
    http_gateway_t* gateway = (http_gateway_t*)gateway_impl;
    
    cJSON* stats = cJSON_CreateObject();
    cJSON_AddNumberToObject(stats, "requests_total", atomic_load(&gateway->requests_total));
    cJSON_AddNumberToObject(stats, "requests_failed", atomic_load(&gateway->requests_failed));
    cJSON_AddNumberToObject(stats, "bytes_received", atomic_load(&gateway->bytes_received));
    cJSON_AddNumberToObject(stats, "bytes_sent", atomic_load(&gateway->bytes_sent));
    
    char* json_str = cJSON_Print(stats);
    cJSON_Delete(stats);
    
    *out_json = json_str;
    return AGENTOS_SUCCESS;
}

/* ========== 网关操作�?========== */

static const gateway_ops_t http_gateway_ops = {
    .start = http_gateway_start,
    .stop = http_gateway_stop,
    .destroy = http_gateway_destroy,
    .get_name = http_gateway_get_name,
    .get_stats = http_gateway_get_stats
};

/* ========== 公共接口 ========== */

/**
 * @brief 创建HTTP网关
 * @param host 监听地址
 * @param port 监听端口
 * @param server 服务器实�?
 * @return 网关实例，失败返回NULL
 */
gateway_t* http_gateway_create(const char* host, uint16_t port, gateway_server_t* server) {
    if (!host || !server) {
        return NULL;
    }
    
    http_gateway_t* gateway = calloc(1, sizeof(http_gateway_t));
    if (!gateway) {
        return NULL;
    }
    
    gateway->server = server;
    gateway->port = port;
    gateway->host = strdup(host);
    
    if (!gateway->host) {
        free(gateway);
        return NULL;
    }
    
    /* 初始化统计信�?*/
    atomic_init(&gateway->requests_total, 0);
    atomic_init(&gateway->requests_failed, 0);
    atomic_init(&gateway->bytes_received, 0);
    atomic_init(&gateway->bytes_sent, 0);
    
    /* 初始化安全配�?*/
    gateway->max_request_size = 10 * 1024 * 1024; /* 默认10MB */
    
    /* 创建限流�?*/
    gateway->ratelimiter = ratelimiter_create_simple(100, 60); /* 100请求/分钟 */
    if (!gateway->ratelimiter) {
        free(gateway->host);
        free(gateway);
        return NULL;
    }
    
    /* 创建网关实例 */
    gateway_t* gw = malloc(sizeof(gateway_t));
    if (!gw) {
        ratelimiter_destroy(gateway->ratelimiter);
        free(gateway->host);
        free(gateway);
        return NULL;
    }
    
    gw->ops = &http_gateway_ops;
    gw->server = server;
    gw->impl = gateway;
    
    return gw;
}


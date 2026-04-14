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
#include "../utils/syscall_router.h"
#include "../utils/gateway_utils.h"
#include "../utils/gateway_rate_limiter.h"
#include "../utils/gateway_rpc_handler.h"
#include "../utils/gateway_protocol_handler.h"

#include <microhttpd.h>
#include <cJSON.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* 跨平台原子操作支持 - 使用统一的 atomic_compat.h */
#include <agentos/atomic_compat.h>

/* 平台特定头文件 */
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
#else
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
#endif

/* ========== HTTP网关内部结构 ========== */

/* http_request_context_t 和 http_gateway_t 已移至 http_gateway.h */

/* ========== 辅助函数（使用 gateway_utils.h 中的公共实现） ========== */

/*
 * time_ns() 已迁移至 gateway_utils.h (gateway_time_ns)
 * 本文件统一使用 gateway_time_ns()
 */

/* ========== HTTP 响应生成 ========== */

/**
 * @brief 检查 Origin 是否在 CORS 白名单中
 * @param gateway HTTP网关实例
 * @param origin 请求头中的Origin值
 * @return true 允许访问，false 拒绝访问
 */
static bool is_cors_origin_allowed(const http_gateway_t* gateway, const char* origin) {
    if (!origin || !gateway) return false;
    
    /* 开发模式：允许所有来源 */
    if (gateway->cors.allow_all_origins) {
        return true;
    }
    
    /* 生产模式：白名单匹配 */
    for (size_t i = 0; i < gateway->cors.allowed_origins_count; i++) {
        if (gateway->cors.allowed_origins[i] &&
            strcmp(origin, gateway->cors.allowed_origins[i]) == 0) {
            return true;
        }
    }
    
    return false;
}

/**
 * @brief 生成 HTTP 响应（安全CORS版本）
 * @param gateway HTTP网关实例
 * @param connection MHD连接对象
 * @param status_code HTTP 状态码
 * @param content 响应内容
 * @param content_len 内容长度
 * @return MHD 响应对象
 */
static struct MHD_Response* create_http_response_ex(
    http_gateway_t* gateway,
    struct MHD_Connection* connection,
    int status_code, 
    const char* content, 
    size_t content_len
) {
    (void)status_code;
    
    struct MHD_Response* response = MHD_create_response_from_buffer(
        content_len, (void*)content, MHD_RESPMEM_MUST_COPY);
    
    if (!response) {
        return NULL;
    }
    
    MHD_add_response_header(response, "Content-Type", "application/json");
    MHD_add_response_header(response, "Server", "AgentOS-gateway/1.0");
    
    /* 安全的 CORS 头设置 */
    const char* origin = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Origin");
    if (is_cors_origin_allowed(gateway, origin)) {
        MHD_add_response_header(response, "Access-Control-Allow-Origin", origin);
        
        if (gateway->cors.allowed_methods) {
            MHD_add_response_header(response, "Access-Control-Allow-Methods", 
                                    gateway->cors.allowed_methods);
        }
        
        if (gateway->cors.allowed_headers) {
            MHD_add_response_header(response, "Access-Control-Allow-Headers", 
                                    gateway->cors.allowed_headers);
        }
        
        if (gateway->cors.max_age > 0) {
            char max_age_str[16];
            snprintf(max_age_str, sizeof(max_age_str), "%d", gateway->cors.max_age);
            MHD_add_response_header(response, "Access-Control-Max-Age", max_age_str);
        }
    }
    
    return response;
}

/**
 * @brief 生成 HTTP 响应（兼容旧版本）
 * @param status_code HTTP 状态码
 * @param content 响应内容
 * @param content_len 内容长度
 * @return MHD 响应对象
 * @deprecated 请使用 create_http_response_ex() 以获得安全的CORS处理
 */
static struct MHD_Response* create_http_response(int status_code, const char* content, size_t content_len) {
    struct MHD_Response* response = MHD_create_response_from_buffer(
        content_len, (void*)content, MHD_RESPMEM_MUST_COPY);
    
    if (!response) {
        return NULL;
    }
    
    MHD_add_response_header(response, "Content-Type", "application/json");
    MHD_add_response_header(response, "Server", "AgentOS-gateway/1.0");
    /* 注意：此函数不设置CORS头，请使用create_http_response_ex() */
    
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
 * @param gateway 网关实例
 * @param context 请求上下文
 * @param data 请求体数据
 * @param size 数据大小
 * @return 0 成功，非0 失败
 */
static int parse_json_request(http_gateway_t* gateway, http_request_context_t* context, const char* data, size_t size) {
    if (!data || size == 0) {
        return -1;
    }
    
    /* 强化大小限制检查 */
    if (size > gateway->max_request_size) {
        /* 记录安全事件（如果有日志系统） */
        atomic_fetch_add(&gateway->requests_failed, 1);
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
 * @brief 请求处理适配器 - 将公共回调签名转换为内部使用
 *
 * 公共签名: (const char* request_json, char** response_json, void* user_data) -> int
 * 内部签名: (void* request, void* user_data) -> char*
 *
 * 此函数在内部存储公共类型的回调，并在调用时进行适配。
 */
typedef struct {
    int (*public_handler)(const char*, char**, void*);
    void* user_data;
} http_handler_adapter_t;

/**
 * @brief 内部回调包装函数（符合内部 gateway_request_handler_t 签名）
 * @param request cJSON 请求对象
 * @param user_data 指向 http_handler_adapter_t 的指针
 * @return JSON 响应字符串（需调用者 free），或 NULL
 */
static char* http_handler_adapter(void* request, void* user_data) {
    http_handler_adapter_t* adapter = (http_handler_adapter_t*)user_data;
    if (!adapter || !adapter->public_handler) return NULL;

    char* request_json = cJSON_Print((cJSON*)request);
    if (!request_json) return NULL;

    char* response_json = NULL;
    int ret = adapter->public_handler(request_json, &response_json, adapter->user_data);
    free(request_json);

    if (ret != 0 || !response_json) {
        return NULL;
    }

    /* response_json 的所有权转移给调用者 */
    return response_json;
}
/**
 * @brief 处理JSON-RPC请求（使用统一RPC处理器）
 * @param gateway 网关实例
 * @param context 请求上下文
 * @return JSON响应字符串
 */
static char* handle_jsonrpc_request(http_gateway_t* gateway, http_request_context_t* context) {
    rpc_result_t result;
    
    /* 检查是否有多协议处理器和原始数据 */
    if (gateway->protocol_handler && context->upload_data && context->upload_data_size > 0) {
        /* 使用多协议处理器处理原始请求数据 */
        result = gateway_protocol_handle_request(
            gateway->protocol_handler,
            context->upload_data,
            context->upload_data_size,
            UNIFIED_PROTOCOL_AUTO,  /* 自动检测协议类型 */
            gateway->handler,
            gateway->handler_data
        );
    } else if (context->json_request) {
        /* 使用原有的JSON-RPC处理器 */
        result = gateway_rpc_handle_request(
            context->json_request,
            gateway->handler,
            gateway->handler_data
        );
    } else {
        /* 无效请求 */
        return jsonrpc_create_error_response(NULL, -32600, "Invalid request", NULL);
    }

    if (result.error_code != 0 || !result.response_json) {
        /* 错误情况：返回错误响应 */
        char* error_resp = result.response_json ? result.response_json : 
                          jsonrpc_create_error_response(NULL, -32603, "Internal error", NULL);
        if (result.response_json) {
            result.response_json = NULL;  /* 防止 gateway_rpc_free 释放 */
        }
        gateway_rpc_free(&result);
        return error_resp;
    }

    /* 成功情况：提取响应并清理 */
    char* success_resp = result.response_json;
    result.response_json = NULL;  /* 防止 gateway_rpc_free 释放 */
    gateway_rpc_free(&result);
    
    return success_resp;
}
/* handle_http_request() 函数已迁移至 http_gateway_routes.c */
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

    if (gateway->handler_adapter) {
        free(gateway->handler_adapter);
        gateway->handler_adapter = NULL;
    }
    gateway->handler = NULL;
    gateway->handler_data = NULL;

    if (gateway->host) {
        free(gateway->host);
    }
    
    /* 清理 CORS 配置资源 */
    if (gateway->cors.allowed_methods) {
        free(gateway->cors.allowed_methods);
    }
    if (gateway->cors.allowed_headers) {
        free(gateway->cors.allowed_headers);
    }
    if (gateway->cors.allowed_origins) {
        for (size_t i = 0; i < gateway->cors.allowed_origins_count; i++) {
            if (gateway->cors.allowed_origins[i]) {
                free(gateway->cors.allowed_origins[i]);
            }
        }
        free(gateway->cors.allowed_origins);
    }
    
    /* 清理速率限制器 */
    if (gateway->rate_limiter) {
        gateway_rate_limiter_destroy(gateway->rate_limiter);
    }
    
    /* 清理协议处理器 */
    if (gateway->protocol_handler) {
        gateway_protocol_handler_destroy(gateway->protocol_handler);
        gateway->protocol_handler = NULL;
    }

    free(gateway);
}
static const char* http_gateway_get_name(void* gateway_impl) {
    (void)gateway_impl;
    return "HTTP Gateway";
}
static agentos_error_t http_gateway_get_stats(void* gateway_impl, char** out_json) {
    http_gateway_t* gateway = (http_gateway_t*)gateway_impl;
    if (!gateway || !out_json) return AGENTOS_EINVAL;

    cJSON* stats = cJSON_CreateObject();
    if (!stats) return AGENTOS_ENOMEM;
    cJSON_AddNumberToObject(stats, "requests_total", (double)atomic_load(&gateway->requests_total));
    cJSON_AddNumberToObject(stats, "requests_failed", (double)atomic_load(&gateway->requests_failed));
    cJSON_AddNumberToObject(stats, "bytes_received", (double)atomic_load(&gateway->bytes_received));
    cJSON_AddNumberToObject(stats, "bytes_sent", (double)atomic_load(&gateway->bytes_sent));
    
    char* json_str = cJSON_Print(stats);
    cJSON_Delete(stats);
    
    *out_json = json_str;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 检查 HTTP 网关是否运行中
 * @param gateway_impl 网关实现指针
 * @return true 运行中，false 已停止或无效
 */
static bool http_gateway_is_running(void* gateway_impl) {
    http_gateway_t* gateway = (http_gateway_t*)gateway_impl;
    if (!gateway) return false;
    return atomic_load(&gateway->running);
}

/**
 * @brief 设置请求处理回调
 *
 * 支持两种回调模式：
 * 1. 内部模式：直接传入 (void*, void*) -> char* 类型的回调
 * 2. 公共模式（推荐）：通过 gateway_set_handler() API 传入，
 *    自动创建适配器将公共签名 (const char*, char**, void*) -> int 转换为内部签名
 */
static agentos_error_t http_gateway_set_handler(void* gateway_impl, gateway_request_handler_t handler, void* user_data) {
    http_gateway_t* gateway = (http_gateway_t*)gateway_impl;
    if (!gateway) return AGENTOS_EINVAL;

    /* 清理旧适配器 */
    if (gateway->handler_adapter) {
        free(gateway->handler_adapter);
        gateway->handler_adapter = NULL;
    }

    gateway->handler = handler;
    gateway->handler_data = user_data;

    return AGENTOS_SUCCESS;
}

static const gateway_ops_t http_gateway_ops = {
    .start = http_gateway_start,
    .stop = http_gateway_stop,
    .destroy = http_gateway_destroy,
    .get_name = http_gateway_get_name,
    .get_stats = http_gateway_get_stats,
    .is_running = http_gateway_is_running,
    .set_handler = http_gateway_set_handler
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
    gateway->handler_adapter = NULL;
    gateway->handler = NULL;
    gateway->handler_data = NULL;
    
    if (!gateway->host) {
        free(gateway);
        return NULL;
    }
    
    atomic_init(&gateway->running, false);
    atomic_init(&gateway->requests_total, 0);
    atomic_init(&gateway->requests_failed, 0);
    atomic_init(&gateway->bytes_received, 0);
    atomic_init(&gateway->bytes_sent, 0);
    
    /* 设置最大请求体大小（默认1MB，更安全） */
    gateway->max_request_size = 1 * 1024 * 1024; /* 1MB */
    
    /* 从环境变量读取最大请求体大小 */
    const char* env_max_size = getenv("GATEWAY_MAX_REQUEST_SIZE");
    if (env_max_size) {
        long size = strtol(env_max_size, NULL, 10);
        if (size > 0 && size <= 100 * 1024 * 1024) {  /* 最大100MB */
            gateway->max_request_size = (size_t)size;
        }
    }
    
    /* 初始化 CORS 配置（默认生产模式） */
    gateway->cors.allow_all_origins = false;
    gateway->cors.allowed_origins = NULL;
    gateway->cors.allowed_origins_count = 0;
    gateway->cors.allowed_methods = strdup("POST, GET, OPTIONS");
    gateway->cors.allowed_headers = strdup("Content-Type, Authorization");
    gateway->cors.max_age = 3600;  /* 1小时缓存 */
    
    /* 从环境变量读取 CORS 模式 */
    const char* cors_mode = getenv("GATEWAY_CORS_MODE");
    if (cors_mode && strcmp(cors_mode, "dev") == 0) {
        gateway->cors.allow_all_origins = true;
        /* 开发模式日志（如果有日志系统） */
    }
    
    /* 从环境变量读取允许的来源列表 */
    const char* cors_origins = getenv("GATEWAY_CORS_ORIGINS");
    if (cors_origins && !gateway->cors.allow_all_origins) {
        /* 简单解析逗号分隔的来源列表 */
        char* origins_copy = strdup(cors_origins);
        if (origins_copy) {
            size_t count = 1;
            for (char* p = origins_copy; *p; p++) {
                if (*p == ',') count++;
            }
            
            gateway->cors.allowed_origins = malloc(count * sizeof(char*));
            if (gateway->cors.allowed_origins) {
                char* token = strtok(origins_copy, ",");
                size_t i = 0;
                while (token && i < count) {
                    gateway->cors.allowed_origins[i++] = strdup(token);
                    token = strtok(NULL, ",");
                }
                gateway->cors.allowed_origins_count = i;
            }
            free(origins_copy);
        }
    }
    
    /* 初始化速率限制器（默认禁用） */
    gateway->rate_limiter = NULL;
    const char* rate_limit_enabled = getenv("GATEWAY_RATE_LIMIT_ENABLED");
    if (rate_limit_enabled && strcmp(rate_limit_enabled, "true") == 0) {
        gateway_rate_limit_config_t rl_config;
        gateway_rate_limiter_get_default_config(&rl_config);
        rl_config.enabled = true;
        
        /* 从环境变量读取配置 */
        const char* rps = getenv("GATEWAY_RATE_LIMIT_RPS");
        if (rps) {
            rl_config.requests_per_second = (uint32_t)strtol(rps, NULL, 10);
        }
        
        const char* rpm = getenv("GATEWAY_RATE_LIMIT_RPM");
        if (rpm) {
            rl_config.requests_per_minute = (uint32_t)strtol(rpm, NULL, 10);
        }
        
        gateway->rate_limiter = gateway_rate_limiter_create(&rl_config);
    }
    
    /* 初始化多协议处理器 */
    gateway->protocol_handler = gateway_protocol_handler_create(NULL);
    if (!gateway->protocol_handler) {
        /* 协议处理器创建失败，但不影响基本功能 */
        /* 可以降级为纯JSON-RPC模式 */
    }
    
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

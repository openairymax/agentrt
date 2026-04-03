/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file ws_gateway.c
 * @brief WebSocket网关实现 - libwebsockets集成
 *
 * 实现WebSocket双向通信协议，通过系统调用接口与内核通信。
 * 网关层只负责协议转换，不包含业务逻辑。
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "ws_gateway.h"
#include "../utils/jsonrpc.h"
#include "../utils/syscall_router.h"
#include "../utils/gateway_utils.h"

#include <libwebsockets.h>
#include <cJSON.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdatomic.h>
#include <time.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/time.h>
#endif

/* ========== 前向声明 ========== */

struct ws_gateway;
typedef struct ws_gateway ws_gateway_t;

static int ws_callback(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len);

/* ========== WebSocket协议定义 ========== */

static const struct lws_protocols ws_protocols[] = {
    {
        "agentos-rpc",
        ws_callback,
        sizeof(void*),
        4096,
    },
    { NULL, NULL, 0, 0 }
};

/* ========== 辅助函数（使用 gateway_utils.h 中的公共实现） ========== */

/*
 * time_ns() 已迁移至 gateway_utils.h (gateway_time_ns)
 */

/* ========== WebSocket网关内部结构 ========== */

/**
 * @brief WebSocket连接上下文
 */
typedef struct ws_connection_context {
    struct lws* wsi;                 /**< WebSocket实例 */
    char* session_id;                /**< 会话ID */
    char* remote_addr;               /**< 远程地址 */
    uint64_t connect_time_ns;        /**< 连接时间 */
    uint64_t last_activity_ns;       /**< 最后活动时间 */
    
    size_t messages_sent;            /**< 发送消息数 */
    size_t messages_received;        /**< 接收消息数 */
    size_t bytes_sent;               /**< 发送字节数 */
    size_t bytes_received;           /**< 接收字节数 */
} ws_connection_context_t;

/**
 * @brief WebSocket网关内部结构
 */
struct ws_gateway {
    struct lws_context* context;     /**< LWS上下文 */
    uint16_t port;                   /**< 监听端口 */
    char* host;                      /**< 监听地址 */

    void* handler_adapter;           /**< 公共回调适配器（动态分配） */
    gateway_request_handler_t handler; /**< 内部请求处理回调 */
    void* handler_data;              /**< 回调用户数据 */
    
    atomic_bool running;             /**< 运行标志 */
    
    atomic_uint_fast64_t connections_total;    /**< 总连接数 */
    atomic_uint_fast64_t connections_active;   /**< 活跃连接数 */
    atomic_uint_fast64_t messages_total;       /**< 总消息数 */
    atomic_uint_fast64_t bytes_sent;           /**< 发送字节数 */
    atomic_uint_fast64_t bytes_received;       /**< 接收字节数 */
    
    size_t max_request_size;         /**< 最大请求大小 */
};

/* ========== 系统调用路由 ========== */

/**
 * @brief 处理系统调用请求（使用公共路由器）
 * @param method 方法名
 * @param params 参数对象
 * @param request_id 请求ID
 * @return JSON响应字符串
 */
static char* handle_system_call(const char* method, cJSON* params, cJSON* request_id) {
    return gateway_syscall_route(method, params, request_id);
}

/* ========== 消息协议定义 ========== */

/**
 * @brief WebSocket消息类型
 */
typedef enum {
    WS_MSG_TYPE_PING = 1,           /**< Ping消息 */
    WS_MSG_TYPE_PONG,               /**< Pong消息 */
    WS_MSG_TYPE_RPC_REQUEST,        /**< RPC请求 */
    WS_MSG_TYPE_RPC_RESPONSE,       /**< RPC响应 */
    WS_MSG_TYPE_NOTIFICATION,       /**< 通知消息 */
    WS_MSG_TYPE_ERROR               /**< 错误消息 */
} ws_message_type_t;

/**
 * @brief WebSocket消息结构
 */
typedef struct ws_message {
    ws_message_type_t type;         /**< 消息类型 */
    char* session_id;               /**< 会话ID */
    cJSON* payload;                 /**< 消息载荷 */
    uint64_t timestamp_ns;          /**< 时间戳 */
} ws_message_t;

/**
 * @brief 创建WebSocket消息
 */
static ws_message_t* ws_message_create(ws_message_type_t type, const char* session_id, cJSON* payload) {
    ws_message_t* msg = calloc(1, sizeof(ws_message_t));
    if (!msg) return NULL;
    
    msg->type = type;
    msg->session_id = session_id ? strdup(session_id) : NULL;
    msg->payload = payload ? cJSON_Duplicate(payload, 1) : NULL;
    msg->timestamp_ns = gateway_time_ns();
    
    return msg;
}

/**
 * @brief 销毁WebSocket消息
 */
static void ws_message_destroy(ws_message_t* msg) {
    if (!msg) return;
    
    if (msg->session_id) free(msg->session_id);
    if (msg->payload) cJSON_Delete(msg->payload);
    free(msg);
}

/**
 * @brief 序列化WebSocket消息为JSON字符串
 */
static char* ws_message_to_json(ws_message_t* msg) {
    cJSON* json = cJSON_CreateObject();
    
    const char* type_str = NULL;
    switch (msg->type) {
        case WS_MSG_TYPE_PING: type_str = "ping"; break;
        case WS_MSG_TYPE_PONG: type_str = "pong"; break;
        case WS_MSG_TYPE_RPC_REQUEST: type_str = "rpc_request"; break;
        case WS_MSG_TYPE_RPC_RESPONSE: type_str = "rpc_response"; break;
        case WS_MSG_TYPE_NOTIFICATION: type_str = "notification"; break;
        case WS_MSG_TYPE_ERROR: type_str = "error"; break;
    }
    cJSON_AddStringToObject(json, "type", type_str);
    
    if (msg->session_id) {
        cJSON_AddStringToObject(json, "session_id", msg->session_id);
    }
    
    cJSON_AddNumberToObject(json, "timestamp", msg->timestamp_ns / 1000000000.0);
    
    if (msg->payload) {
        cJSON_AddItemToObject(json, "payload", cJSON_Duplicate(msg->payload, 1));
    }
    
    char* json_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    
    return json_str;
}

/* ========== RPC处理 ========== */

/**
 * @brief 处理RPC请求
 */
static char* handle_rpc_request(ws_gateway_t* gateway, cJSON* request) {
    cJSON* id = cJSON_GetObjectItem(request, "id");
    cJSON* method = cJSON_GetObjectItem(request, "method");
    cJSON* params = cJSON_GetObjectItem(request, "params");

    if (!method || !cJSON_IsString(method)) {
        return jsonrpc_create_error_response(id, -32600, "Invalid Request", NULL);
    }

    /* 如果设置了自定义处理回调，优先使用 */
    if (gateway->handler) {
        char* result = gateway->handler(request, gateway->handler_data);
        if (result) {
            return result;
        }
        /* 回调返回 NULL，回退到默认 syscall 路由 */
    }

    return handle_system_call(method->valuestring, params, id);
}

/* ========== WebSocket回调函数 ========== */

/**
 * @brief WebSocket回调函数
 */
static int ws_callback(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len) {
    ws_gateway_t* gateway = (ws_gateway_t*)lws_context_user(lws_get_context(wsi));
    ws_connection_context_t* context = (ws_connection_context_t*)*(void**)user;
    
    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED:
            context = calloc(1, sizeof(ws_connection_context_t));
            if (!context) return -1;
            
            context->wsi = wsi;
            context->connect_time_ns = gateway_time_ns();
            context->last_activity_ns = gateway_time_ns();
            
            *(void**)user = context;
            
            atomic_fetch_add(&gateway->connections_total, 1);
            atomic_fetch_add(&gateway->connections_active, 1);
            break;
            
        case LWS_CALLBACK_RECEIVE:
            if (!context) return -1;
            
            context->last_activity_ns = gateway_time_ns();
            context->messages_received++;
            context->bytes_received += len;
            atomic_fetch_add(&gateway->messages_total, 1);
            atomic_fetch_add(&gateway->bytes_received, len);
            
            cJSON* json = cJSON_Parse((char*)in);
            if (!json) {
                char* error_resp = jsonrpc_create_error_response(NULL, -32700, "Parse error", NULL);
                ws_message_t* error_msg = ws_message_create(WS_MSG_TYPE_ERROR, NULL, cJSON_Parse(error_resp));
                char* error_json = ws_message_to_json(error_msg);
                ws_message_destroy(error_msg);
                free(error_resp);
                
                size_t out_len = strlen(error_json);
                lws_write(wsi, (unsigned char*)error_json, out_len, LWS_WRITE_TEXT);
                free(error_json);
                return 0;
            }
            
            cJSON* type = cJSON_GetObjectItem(json, "type");
            if (type && cJSON_IsString(type)) {
                if (strcmp(type->valuestring, "ping") == 0) {
                    ws_message_t* pong_msg = ws_message_create(WS_MSG_TYPE_PONG, context->session_id, NULL);
                    char* pong_json = ws_message_to_json(pong_msg);
                    ws_message_destroy(pong_msg);

                    size_t out_len = strlen(pong_json);
                    lws_write(wsi, (unsigned char*)pong_json, out_len, LWS_WRITE_TEXT);
                    free(pong_json);
                }
                else if (strcmp(type->valuestring, "rpc_request") == 0) {
                    cJSON* rpc_request = cJSON_GetObjectItem(json, "payload");
                    if (rpc_request) {
                        char* response = handle_rpc_request(gateway, rpc_request);
                        if (response) {
                            ws_message_t* response_msg = ws_message_create(
                                WS_MSG_TYPE_RPC_RESPONSE, context->session_id, cJSON_Parse(response));
                            char* response_json = ws_message_to_json(response_msg);
                            ws_message_destroy(response_msg);
                            free(response);

                            size_t out_len = strlen(response_json);
                            lws_write(wsi, (unsigned char*)response_json, out_len, LWS_WRITE_TEXT);
                            free(response_json);

                            atomic_fetch_add(&gateway->bytes_sent, out_len);
                        }
                    }
                }
                else {
                    /* 未知消息类型，返回错误 */
                    char err_buf[128];
                    snprintf(err_buf, sizeof(err_buf), "Unknown message type: %s", type->valuestring);
                    char* error_resp = jsonrpc_create_error_response(NULL, -32600, err_buf, NULL);
                    ws_message_t* error_msg = ws_message_create(WS_MSG_TYPE_ERROR, NULL, cJSON_Parse(error_resp));
                    char* error_json = ws_message_to_json(error_msg);
                    ws_message_destroy(error_msg);
                    free(error_resp);

                    size_t out_len = strlen(error_json);
                    lws_write(wsi, (unsigned char*)error_json, out_len, LWS_WRITE_TEXT);
                    free(error_json);
                }
            }
            
            cJSON_Delete(json);
            break;
            
        case LWS_CALLBACK_CLOSED:
            if (context) {
                atomic_fetch_sub(&gateway->connections_active, 1);
                
                if (context->session_id) free(context->session_id);
                if (context->remote_addr) free(context->remote_addr);
                free(context);
                *(void**)user = NULL;
            }
            break;
            
        default:
            break;
    }
    
    return 0;
}

/* ========== 网关操作表 ========== */

static agentos_error_t ws_gateway_start(void* gateway_impl) {
    ws_gateway_t* gateway = (ws_gateway_t*)gateway_impl;
    
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = gateway->port;
    info.iface = gateway->host;
    info.protocols = ws_protocols;
    info.user = gateway;
    
    gateway->context = lws_create_context(&info);
    if (!gateway->context) {
        return AGENTOS_EBUSY;
    }
    
    atomic_store(&gateway->running, true);
    
    return AGENTOS_SUCCESS;
}

static void ws_gateway_stop(void* gateway_impl) {
    ws_gateway_t* gateway = (ws_gateway_t*)gateway_impl;
    
    atomic_store(&gateway->running, false);
    
    if (gateway->context) {
        lws_context_destroy(gateway->context);
        gateway->context = NULL;
    }
}

static void ws_gateway_destroy(void* gateway_impl) {
    ws_gateway_t* gateway = (ws_gateway_t*)gateway_impl;

    ws_gateway_stop(gateway);

    if (gateway->handler_adapter) {
        free(gateway->handler_adapter);
        gateway->handler_adapter = NULL;
    }
    gateway->handler = NULL;
    gateway->handler_data = NULL;

    if (gateway->host) {
        free(gateway->host);
    }

    free(gateway);
}

static const char* ws_gateway_get_name(void* gateway_impl) {
    (void)gateway_impl;
    return "WebSocket Gateway";
}

static bool ws_gateway_is_running(void* gateway_impl) {
    ws_gateway_t* gateway = (ws_gateway_t*)gateway_impl;
    if (!gateway) return false;
    return atomic_load(&gateway->running);
}

static agentos_error_t ws_gateway_get_stats(void* gateway_impl, char** out_json) {
    ws_gateway_t* gateway = (ws_gateway_t*)gateway_impl;
    if (!gateway || !out_json) return AGENTOS_EINVAL;

    cJSON* stats = cJSON_CreateObject();
    if (!stats) return AGENTOS_ENOMEM;
    cJSON_AddNumberToObject(stats, "connections_total", (double)atomic_load(&gateway->connections_total));
    cJSON_AddNumberToObject(stats, "connections_active", (double)atomic_load(&gateway->connections_active));
    cJSON_AddNumberToObject(stats, "messages_total", (double)atomic_load(&gateway->messages_total));
    cJSON_AddNumberToObject(stats, "bytes_sent", (double)atomic_load(&gateway->bytes_sent));
    cJSON_AddNumberToObject(stats, "bytes_received", (double)atomic_load(&gateway->bytes_received));
    
    char* json_str = cJSON_Print(stats);
    cJSON_Delete(stats);
    
    *out_json = json_str;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 设置请求处理回调
 */
static agentos_error_t ws_gateway_set_handler(void* gateway_impl, gateway_request_handler_t handler, void* user_data) {
    ws_gateway_t* gateway = (ws_gateway_t*)gateway_impl;
    if (!gateway) return AGENTOS_EINVAL;

    if (gateway->handler_adapter) {
        free(gateway->handler_adapter);
        gateway->handler_adapter = NULL;
    }

    gateway->handler = handler;
    gateway->handler_data = user_data;

    return AGENTOS_SUCCESS;
}

static const gateway_ops_t ws_gateway_ops = {
    .start = ws_gateway_start,
    .stop = ws_gateway_stop,
    .destroy = ws_gateway_destroy,
    .get_name = ws_gateway_get_name,
    .get_stats = ws_gateway_get_stats,
    .is_running = ws_gateway_is_running,
    .set_handler = ws_gateway_set_handler
};

/* ========== 公共接口 ========== */

gateway_t* ws_gateway_create(const char* host, uint16_t port) {
    if (!host) {
        return NULL;
    }
    
    ws_gateway_t* gateway = calloc(1, sizeof(ws_gateway_t));
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
    atomic_init(&gateway->connections_total, 0);
    atomic_init(&gateway->connections_active, 0);
    atomic_init(&gateway->messages_total, 0);
    atomic_init(&gateway->bytes_sent, 0);
    atomic_init(&gateway->bytes_received, 0);
    
    gateway->max_request_size = 10 * 1024 * 1024;
    
    gateway_t* gw = malloc(sizeof(gateway_t));
    if (!gw) {
        free(gateway->host);
        free(gateway);
        return NULL;
    }
    
    gw->ops = &ws_gateway_ops;
    gw->impl = gateway;
    gw->type = GATEWAY_TYPE_WS;
    
    return gw;
}

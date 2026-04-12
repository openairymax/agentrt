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
 * 设计原则：
 *   K-1 内核极简：只做协议转换，零业务逻辑
 *   S-2 层次分解：每层职责单一，易于测试和维护
 *   E-8 可测试性：路由处理函数独立可测试
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "ws_gateway.h"
#include "../utils/jsonrpc.h"
#include "../utils/syscall_router.h"
#include "../utils/gateway_utils.h"
#include "../utils/gateway_rpc_handler.h"

#include <libwebsockets.h>
#include <cJSON.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

/* 跨平台原子操作支持 */
#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <intrin.h>
#include "../../../commons/utils/include/atomic_compat.h"
#else
#include <stdatomic.h>
#endif

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/time.h>
#endif

/* ========== 前向声明 ========== */

struct ws_gateway;
typedef struct ws_gateway ws_gateway_t;

/* ========== WebSocket协议定义 ========== */

static int ws_callback(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len);

static const struct lws_protocols ws_protocols[] = {
    {
        "agentos-rpc",
        ws_callback,
        sizeof(void*),
        4096,
    },
    { NULL, NULL, 0, 0 }
};

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
    cJSON* payload;                /**< 消息载荷 */
    uint64_t timestamp_ns;          /**< 时间戳 */
} ws_message_t;

/* ========== 辅助函数 ========== */

/**
 * @brief 创建WebSocket消息
 * @param type 消息类型
 * @param session_id 会话ID（可为NULL）
 * @param payload 消息载荷（可为NULL）
 * @return 消息结构指针，失败返回NULL
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
 * @param msg 消息结构指针
 */
static void ws_message_destroy(ws_message_t* msg) {
    if (!msg) return;

    if (msg->session_id) free(msg->session_id);
    if (msg->payload) cJSON_Delete(msg->payload);
    free(msg);
}

/**
 * @brief 序列化WebSocket消息为JSON字符串
 * @param msg 消息结构指针
 * @return JSON字符串，需调用者free()
 */
static char* ws_message_to_json(ws_message_t* msg) {
    cJSON* json = cJSON_CreateObject();
    if (!json) return NULL;

    const char* type_str = NULL;
    switch (msg->type) {
        case WS_MSG_TYPE_PING: type_str = "ping"; break;
        case WS_MSG_TYPE_PONG: type_str = "pong"; break;
        case WS_MSG_TYPE_RPC_REQUEST: type_str = "rpc_request"; break;
        case WS_MSG_TYPE_RPC_RESPONSE: type_str = "rpc_response"; break;
        case WS_MSG_TYPE_NOTIFICATION: type_str = "notification"; break;
        case WS_MSG_TYPE_ERROR: type_str = "error"; break;
    }
    cJSON_AddStringToObject(json, "type", type_str ? type_str : "unknown");

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

/**
 * @brief 发送WebSocket消息
 * @param wsi WebSocket实例
 * @param msg 消息结构指针
 * @return 成功返回发送字节数，失败返回-1
 */
static int ws_send_message(struct lws* wsi, ws_message_t* msg) {
    if (!wsi || !msg) return -1;

    char* json_str = ws_message_to_json(msg);
    if (!json_str) return -1;

    size_t out_len = strlen(json_str);
    int result = lws_write(wsi, (unsigned char*)json_str, out_len, LWS_WRITE_TEXT);

    free(json_str);
    return result;
}

/* ========== RPC处理（使用统一处理器） ========== */

/**
 * @brief 处理RPC请求（使用统一RPC处理器）
 *
 * 通过 gateway_rpc_handle_request() 实现统一的请求处理流程，
 * 消除与 HTTP/Stdio 网关的代码重复。
 *
 * @param gateway 网关实例
 * @param request JSON-RPC请求对象
 * @return JSON响应字符串，需调用者free()
 */
static char* handle_rpc_request(ws_gateway_t* gateway, cJSON* request) {
    if (!gateway || !request) {
        return jsonrpc_create_error_response(NULL, -32600, "Invalid request", NULL);
    }

    rpc_result_t result = gateway_rpc_handle_request(
        request,
        (int (*)(const char*, char**, void*))gateway->handler,
        gateway->handler_data
    );

    if (result.error_code != 0 || !result.response_json) {
        char* error_resp = result.response_json ? result.response_json :
                          jsonrpc_create_error_response(NULL, -32603, "Internal error", NULL);
        if (result.response_json) result.response_json = NULL;
        gateway_rpc_free(&result);
        return error_resp;
    }

    char* success_resp = result.response_json;
    result.response_json = NULL;
    gateway_rpc_free(&result);
    return success_resp;
}

/* ========== 路由处理函数（降低圈复杂度） ========== */

/**
 * @brief 处理连接建立
 * @param gateway 网关实例
 * @param context 连接上下文
 * @param user 用户指针
 * @return 成功返回0，失败返回-1
 */
static int handle_ws_established(ws_gateway_t* gateway, ws_connection_context_t** context_ptr, void** user) {
    ws_connection_context_t* context = calloc(1, sizeof(ws_connection_context_t));
    if (!context) return -1;

    context->wsi = (struct lws*)*user;
    context->connect_time_ns = gateway_time_ns();
    context->last_activity_ns = gateway_time_ns();

    *context_ptr = context;
    *user = context;

    atomic_fetch_add(&gateway->connections_total, 1);
    atomic_fetch_add(&gateway->connections_active, 1);

    return 0;
}

/**
 * @brief 处理Ping消息
 * @param context 连接上下文
 * @param wsi WebSocket实例
 * @return 成功返回0
 */
static int handle_ws_ping(ws_connection_context_t* context, struct lws* wsi) {
    ws_message_t* pong_msg = ws_message_create(WS_MSG_TYPE_PONG, context->session_id, NULL);
    if (pong_msg) {
        ws_send_message(wsi, pong_msg);
        ws_message_destroy(pong_msg);
    }
    return 0;
}

/**
 * @brief 处理RPC请求消息
 * @param gateway 网关实例
 * @param context 连接上下文
 * @param rpc_request RPC请求对象
 * @param wsi WebSocket实例
 * @return 成功返回0
 */
static int handle_ws_rpc_request(ws_gateway_t* gateway, ws_connection_context_t* context,
                                  cJSON* rpc_request, struct lws* wsi) {
    char* response = handle_rpc_request(gateway, rpc_request);
    if (!response) return -1;

    cJSON* response_json = cJSON_Parse(response);
    if (!response_json) {
        free(response);
        return -1;
    }

    ws_message_t* response_msg = ws_message_create(
        WS_MSG_TYPE_RPC_RESPONSE, context->session_id, response_json);
    cJSON_Delete(response_json);

    if (response_msg) {
        ws_send_message(wsi, response_msg);
        ws_message_destroy(response_msg);
    }

    free(response);
    return 0;
}

/**
 * @brief 处理未知消息类型
 * @param wsi WebSocket实例
 * @param unknown_type 未知类型字符串
 * @return 成功返回0
 */
static int handle_ws_unknown_message(struct lws* wsi, const char* unknown_type) {
    char err_buf[128];
    snprintf(err_buf, sizeof(err_buf), "Unknown message type: %s",
             unknown_type ? unknown_type : "null");

    char* error_json = jsonrpc_create_error_response(NULL, -32600, err_buf, NULL);
    if (!error_json) return -1;

    ws_message_t* error_msg = ws_message_create(WS_MSG_TYPE_ERROR, NULL, NULL);
    if (error_msg) {
        cJSON* payload = cJSON_CreateObject();
        if (payload) {
            cJSON_AddStringToObject(payload, "error", err_buf);
            error_msg->payload = payload;
        }
        ws_send_message(wsi, error_msg);
        ws_message_destroy(error_msg);
    }

    free(error_json);
    return 0;
}

/**
 * @brief 处理连接关闭
 * @param gateway 网关实例
 * @param context_ptr 连接上下文指针
 * @return 成功返回0
 */
static int handle_ws_closed(ws_gateway_t* gateway, ws_connection_context_t** context_ptr) {
    ws_connection_context_t* context = *context_ptr;
    if (!context) return 0;

    atomic_fetch_sub(&gateway->connections_active, 1);

    if (context->session_id) free(context->session_id);
    if (context->remote_addr) free(context->remote_addr);
    free(context);

    *context_ptr = NULL;

    return 0;
}

/* ========== WebSocket回调函数（重构后版本） ========== */

/**
 * @brief WebSocket回调函数
 *
 * 采用路由模式，将不同reason的处理分离到独立函数，
 * 大幅降低圈复杂度，提高可测试性。
 *
 * @param wsi WebSocket实例
 * @param reason 回调原因
 * @param user 用户指针
 * @param in 输入数据
 * @param len 数据长度
 * @return 成功返回0，失败返回-1
 */
static int ws_callback(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len) {
    ws_gateway_t* gateway = (ws_gateway_t*)lws_context_user(lws_get_context(wsi));
    ws_connection_context_t* context = (ws_connection_context_t*)*(void**)user;

    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED:
            return handle_ws_established(gateway, &context, &user);

        case LWS_CALLBACK_RECEIVE:
            if (!context) return -1;

            /* 更新统计信息 */
            context->last_activity_ns = gateway_time_ns();
            context->messages_received++;
            context->bytes_received += len;
            atomic_fetch_add(&gateway->messages_total, 1);
            atomic_fetch_add(&gateway->bytes_received, len);

            /* 解析JSON消息 */
            cJSON* json = cJSON_Parse((const char*)in);
            if (!json) {
                /* 解析失败，发送错误响应 */
                char* error_json = jsonrpc_create_error_response(NULL, -32700, "Parse error", NULL);
                if (error_json) {
                    ws_message_t* error_msg = ws_message_create(
                        WS_MSG_TYPE_ERROR, NULL, cJSON_Parse(error_json));
                    if (error_msg) {
                        ws_send_message(wsi, error_msg);
                        ws_message_destroy(error_msg);
                    }
                    free(error_json);
                }
                return 0;
            }

            /* 提取消息类型 */
            cJSON* type = cJSON_GetObjectItem(json, "type");
            if (!type || !cJSON_IsString(type)) {
                cJSON_Delete(json);
                return handle_ws_unknown_message(wsi, "missing type field");
            }

            /* 根据消息类型路由处理 */
            const char* type_str = type->valuestring;
            int result = 0;

            if (strcmp(type_str, "ping") == 0) {
                result = handle_ws_ping(context, wsi);
            }
            else if (strcmp(type_str, "rpc_request") == 0) {
                cJSON* rpc_request = cJSON_GetObjectItem(json, "payload");
                if (rpc_request) {
                    result = handle_ws_rpc_request(gateway, context, rpc_request, wsi);
                }
            }
            else {
                result = handle_ws_unknown_message(wsi, type_str);
            }

            cJSON_Delete(json);
            return result;

        case LWS_CALLBACK_CLOSED:
            return handle_ws_closed(gateway, &context);

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

    cJSON_AddNumberToObject(stats, "connections_total",
                           (double)atomic_load(&gateway->connections_total));
    cJSON_AddNumberToObject(stats, "connections_active",
                           (double)atomic_load(&gateway->connections_active));
    cJSON_AddNumberToObject(stats, "messages_total",
                           (double)atomic_load(&gateway->messages_total));
    cJSON_AddNumberToObject(stats, "bytes_sent",
                           (double)atomic_load(&gateway->bytes_sent));
    cJSON_AddNumberToObject(stats, "bytes_received",
                           (double)atomic_load(&gateway->bytes_received));

    char* json_str = cJSON_Print(stats);
    cJSON_Delete(stats);

    if (!json_str) return AGENTOS_ENOMEM;

    *out_json = json_str;
    return AGENTOS_SUCCESS;
}

static agentos_error_t ws_gateway_set_handler(void* gateway_impl,
                                              gateway_request_handler_t handler,
                                              void* user_data) {
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

/**
 * @brief 创建WebSocket网关实例
 * @param host 监听地址（如 "127.0.0.1", "0.0.0.0"），不能为NULL
 * @param port 监听端口（如 8081）
 * @return 网关句柄，失败返回NULL（内存不足或参数无效）
 *
 * @ownership 调用者必须通过 gateway_destroy() 释放
 * @threadsafe 安全
 * @since 1.0.0
 */
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

    gateway->max_request_size = 10 * 1024 * 1024;  /* 10MB */

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

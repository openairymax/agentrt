/**
 * @file ws_gateway.c
 * @brief WebSocket网关实现 - libwebsockets集成
 * 
 * 实现WebSocket双向通信协议，支持实时消息推送和事件通知。
 * 集成会话管理和认证机制，提供与AgentOS内核的系统调用接口。
 * 
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "ws_gateway.h"
#include "../server.h"
#include "../session.h"
#include "../health.h"
#include "../telemetry.h"
#include "../auth.h"
#include "../ratelimit.h"
#include "../platform.h"
#include "../utils/jsonrpc.h"

#include <libwebsockets.h>
#include <cJSON.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>

/* ========== JSON-RPC 2.0处理（使用公共模块） ========== */
/* JSON-RPC 函数已移至 utils/jsonrpc.c */

/* ========== 系统调用处理 ========== */

/**
 * @brief 处理系统调用请求
 * @param context 连接上下文
 * @param method 方法名
 * @param params 参数对象
 * @return JSON响应字符串
 */
static char* handle_system_call(ws_connection_context_t* context, const char* method, cJSON* params) {
    AGENTOS_LOG_DEBUG("Handling system call: %s", method);
    
    cJSON* response = cJSON_CreateObject();
    
    if (strcmp(method, "agentos_sys_task_submit") == 0) {
        cJSON_AddStringToObject(response, "status", "accepted");
        cJSON_AddStringToObject(response, "task_id", "task_001");
        cJSON_AddNumberToObject(response, "estimated_time", 5000);
    } 
    else if (strcmp(method, "agentos_sys_memory_search") == 0) {
        cJSON* results = cJSON_CreateArray();
        cJSON_AddItemToArray(results, cJSON_CreateString("sample_memory_1"));
        cJSON_AddItemToArray(results, cJSON_CreateString("sample_memory_2"));
        cJSON_AddItemToObject(response, "results", results);
        cJSON_AddNumberToObject(response, "total", 2);
    }
    else if (strcmp(method, "agentos_sys_session_create") == 0) {
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
        cJSON_Delete(response);
        return jsonrpc_create_error_response(NULL, -32601, "Method not found", NULL);
    }
    
    return jsonrpc_create_success_response(NULL, response);
}

/* ========== WebSocket网关内部结构 ========== */

/**
 * @brief WebSocket连接状态
 */
typedef enum {
    WS_STATE_CONNECTING = 0,    /**< 连接中 */
    WS_STATE_CONNECTED,         /**< 已连接 */
    WS_STATE_AUTHENTICATED,    /**< 已认证 */
    WS_STATE_DISCONNECTING,     /**< 断开连接中 */
    WS_STATE_DISCONNECTED       /**< 已断开 */
} ws_connection_state_t;

/**
 * @brief WebSocket连接上下文
 */
typedef struct ws_connection_context {
    gateway_server_t* server;        /**< 服务器引用 */
    struct lws* wsi;                 /**< WebSocket实例 */
    ws_connection_state_t state;     /**< 连接状态 */
    char* session_id;                /**< 会话ID */
    char* remote_addr;               /**< 远程地址 */
    uint64_t connect_time_ns;       /**< 连接时间 */
    uint64_t last_activity_ns;       /**< 最后活动时间 */
    
    /* 统计信息 */
    size_t messages_sent;            /**< 发送消息数 */
    size_t messages_received;        /**< 接收消息数 */
    size_t bytes_sent;               /**< 发送字节数 */
    size_t bytes_received;           /**< 接收字节数 */
    
    /* 消息队列 */
    cJSON* message_queue;           /**< 消息队列 */
    pthread_mutex_t queue_lock;      /**< 队列锁 */
    pthread_cond_t queue_cond;      /**< 队列条件变量 */
} ws_connection_context_t;

/**
 * @brief WebSocket网关内部结构
 */
typedef struct ws_gateway {
    gateway_server_t* server;        /**< 服务器引用 */
    struct lws_context* context;     /**< LWS上下文 */
    struct lws_context_creation_info info; /**< LWS创建信息 */
    uint16_t port;                   /**< 监听端口 */
    char* host;                      /**< 监听地址 */
    
    /* 统计信息 */
    atomic_uint_fast64_t connections_total;    /**< 总连接数 */
    atomic_uint_fast64_t connections_active;   /**< 活跃连接数 */
    atomic_uint_fast64_t messages_total;       /**< 总消息数 */
    atomic_uint_fast64_t bytes_sent;          /**< 发送字节数 */
    atomic_uint_fast64_t bytes_received;       /**< 接收字节数 */
    
    /* 连接管理 */
    pthread_mutex_t connections_lock; /**< 连接列表锁 */
    ws_connection_context_t** connections;    /**< 连接列表 */
    size_t connections_capacity;     /**< 连接列表容量 */
    size_t connections_count;        /**< 连接数量 */
    
    /* 限流器 */
    ratelimiter_t* ratelimiter;        /**< 消息限流器 */
} ws_gateway_t;

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

/* ========== 消息处理 ========== */

/**
 * @brief 创建WebSocket消息
 * @param type 消息类型
 * @param session_id 会话ID
 * @param payload 载荷JSON
 * @return 消息对象，需调用者free
 */
static ws_message_t* ws_message_create(ws_message_type_t type, const char* session_id, cJSON* payload) {
    ws_message_t* msg = calloc(1, sizeof(ws_message_t));
    if (!msg) return NULL;
    
    msg->type = type;
    msg->session_id = session_id ? strdup(session_id) : NULL;
    msg->payload = payload ? cJSON_Duplicate(payload, 1) : NULL;
    msg->timestamp_ns = time_ns();
    
    return msg;
}

/**
 * @brief 销毁WebSocket消息
 * @param msg 消息对象
 */
static void ws_message_destroy(ws_message_t* msg) {
    if (!msg) return;
    
    if (msg->session_id) free(msg->session_id);
    if (msg->payload) cJSON_Delete(msg->payload);
    free(msg);
}

/**
 * @brief 序列化WebSocket消息为JSON字符串
 * @param msg 消息对象
 * @return JSON字符串，需调用者free
 */
static char* ws_message_to_json(ws_message_t* msg) {
    cJSON* json = cJSON_CreateObject();
    
    /* 消息类型 */
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
    
    /* 会话ID */
    if (msg->session_id) {
        cJSON_AddStringToObject(json, "session_id", msg->session_id);
    }
    
    /* 时间戳 */
    cJSON_AddNumberToObject(json, "timestamp", msg->timestamp_ns / 1000000000.0);
    
    /* 载荷 */
    if (msg->payload) {
        cJSON_AddItemToObject(json, "payload", msg->payload);
    }
    
    char* json_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    
    return json_str;
}

/* ========== 连接管理 ========== */

/**
 * @brief 添加连接到连接列表
 * @param gateway 网关实例
 * @param context 连接上下文
 */
static void ws_gateway_add_connection(ws_gateway_t* gateway, ws_connection_context_t* context) {
    pthread_mutex_lock(&gateway->connections_lock);
    
    /* 扩容连接列表 */
    if (gateway->connections_count >= gateway->connections_capacity) {
        size_t new_capacity = gateway->connections_capacity * 2 + 1;
        ws_connection_context_t** new_connections = realloc(
            gateway->connections, 
            new_capacity * sizeof(ws_connection_context_t*));
        
        if (!new_connections) {
            pthread_mutex_unlock(&gateway->connections_lock);
            return;
        }
        
        gateway->connections = new_connections;
        gateway->connections_capacity = new_capacity;
    }
    
    gateway->connections[gateway->connections_count++] = context;
    atomic_fetch_add(&gateway->connections_active, 1);
    
    pthread_mutex_unlock(&gateway->connections_lock);
}

/**
 * @brief 从连接列表移除连接
 * @param gateway 网关实例
 * @param context 连接上下文
 */
static void ws_gateway_remove_connection(ws_gateway_t* gateway, ws_connection_context_t* context) {
    pthread_mutex_lock(&gateway->connections_lock);
    
    /* 查找并移除连接 */
    for (size_t i = 0; i < gateway->connections_count; i++) {
        if (gateway->connections[i] == context) {
            /* 移动后续元素 */
            for (size_t j = i; j < gateway->connections_count - 1; j++) {
                gateway->connections[j] = gateway->connections[j + 1];
            }
            gateway->connections_count--;
            
            atomic_fetch_sub(&gateway->connections_active, 1);
            break;
        }
    }
    
    pthread_mutex_unlock(&gateway->connections_lock);
}

/**
 * @brief 查找连接
 * @param gateway 网关实例
 * @param wsi WebSocket实例
 * @return 连接上下文，未找到返回NULL
 */
static ws_connection_context_t* ws_gateway_find_connection(ws_gateway_t* gateway, struct lws* wsi) {
    pthread_mutex_lock(&gateway->connections_lock);
    
    ws_connection_context_t* context = NULL;
    for (size_t i = 0; i < gateway->connections_count; i++) {
        if (gateway->connections[i]->wsi == wsi) {
            context = gateway->connections[i];
            break;
        }
    }
    
    pthread_mutex_unlock(&gateway->connections_lock);
    return context;
}

/* ========== RPC处理 ========== */

/**
 * @brief 处理RPC请求
 * @param context 连接上下文
 * @param request 请求JSON
 * @return 响应JSON字符串
 */
static char* handle_rpc_request(ws_connection_context_t* context, cJSON* request) {
    cJSON* id = cJSON_GetObjectItem(request, "id");
    cJSON* method = cJSON_GetObjectItem(request, "method");
    cJSON* params = cJSON_GetObjectItem(request, "params");
    
    if (!id || !method || !cJSON_IsString(method)) {
        return jsonrpc_create_error_response(id, -32600, "Invalid Request", NULL);
    }
    
    /* 验证会话（除了创建会话的请求） */
    if (strcmp(method->valuestring, "agentos_sys_session_create") != 0 && !context->session_id) {
        return jsonrpc_create_error_response(id, -32001, "Authentication required", NULL);
    }
    
    /* 处理系统调用（与HTTP网关相同的逻辑） */
    return handle_system_call(context, method->valuestring, params);
}

/* ========== WebSocket回调函数 ========== */

/**
 * @brief WebSocket回调函数
 * @param wsi WebSocket实例
 * @param reason 回调原因
 * @param user 用户数据
 * @param in 输入数据
 * @param len 输入长度
 * @param len_cb 回调长度
 * @return 状态码
 */
static int ws_callback(struct lws* wsi, enum lws_callback_reasons reason, void* user, 
                      void* in, size_t len, size_t* len_cb, size_t* len_acked) {
    ws_gateway_t* gateway = (ws_gateway_t*)user;
    ws_connection_context_t* context = NULL;
    
    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED:
            /* 新连接建立 */
            context = calloc(1, sizeof(ws_connection_context_t));
            if (!context) return -1;
            
            context->server = gateway->server;
            context->wsi = wsi;
            context->state = WS_STATE_CONNECTED;
            context->connect_time_ns = time_ns();
            context->last_activity_ns = time_ns();
            
            /* 获取远程地址 */
            char addr_buf[46];
            int len = lws_peer_simple_address(wsi, addr_buf, sizeof(addr_buf) - 1);
            if (len > 0) {
                context->remote_addr = strdup(addr_buf);
            }
            
            /* 初始化消息队列 */
            pthread_mutex_init(&context->queue_lock, NULL);
            pthread_cond_init(&context->queue_cond, NULL);
            
            ws_gateway_add_connection(gateway, context);
            atomic_fetch_add(&gateway->connections_total, 1);
            
            AGENTOS_LOG_INFO("WebSocket connection established from %s", context->remote_addr ? context->remote_addr : "unknown");
            break;
            
        case LWS_CALLBACK_RECEIVE:
            /* 接收消息 */
            context = ws_gateway_find_connection(gateway, wsi);
            if (!context) return -1;
            
            context->last_activity_ns = time_ns();
            context->messages_received++;
            context->bytes_received += len;
            
            /* 解析JSON消息 */
            cJSON* json = cJSON_Parse((char*)in);
            if (!json) {
                /* 发送错误响应 */
                ws_message_t* error_msg = ws_message_create(WS_MSG_TYPE_ERROR, NULL, NULL);
                char* error_json = ws_message_to_json(error_msg);
                ws_message_destroy(error_msg);
                
                int len = strlen(error_json);
                lws_write(wsi, error_json, &len, LWS_WRITE_TEXT);
                free(error_json);
                return 0;
            }
            
            /* 处理消息 */
            cJSON* type = cJSON_GetObjectItem(json, "type");
            if (type && cJSON_IsString(type)) {
                if (strcmp(type->valuestring, "ping") == 0) {
                    /* Ping-Pong */
                    ws_message_t* pong_msg = ws_message_create(WS_MSG_TYPE_PONG, context->session_id, NULL);
                    char* pong_json = ws_message_to_json(pong_msg);
                    ws_message_destroy(pong_msg);
                    
                    int len = strlen(pong_json);
                    lws_write(wsi, pong_json, &len, LWS_WRITE_TEXT);
                    free(pong_json);
                }
                else if (strcmp(type->valuestring, "rpc_request") == 0) {
                    /* RPC请求 */
                    char* response = handle_rpc_request(context, json);
                    if (response) {
                        ws_message_t* response_msg = ws_message_create(WS_MSG_TYPE_RPC_RESPONSE, context->session_id, cJSON_Parse(response));
                        char* response_json = ws_message_to_json(response_msg);
                        ws_message_destroy(response_msg);
                        free(response);
                        
                        int len = strlen(response_json);
                        lws_write(wsi, response_json, &len, LWS_WRITE_TEXT);
                        free(response_json);
                    }
                }
                else if (strcmp(type->valuestring, "authenticate") == 0) {
                    /* 认证请求 */
                    cJSON* token = cJSON_GetObjectItem(json, "token");
                    if (token && cJSON_IsString(token)) {
                        if (auth_manager_validate_token(gateway->server->auth_manager, token->valuestring) == 0) {
                            context->session_id = strdup(token->valuestring);
                            context->state = WS_STATE_AUTHENTICATED;
                            
                            /* 发送认证成功响应 */
                            ws_message_t* auth_msg = ws_message_create(WS_MSG_TYPE_NOTIFICATION, context->session_id, NULL);
                            cJSON* payload = cJSON_CreateObject();
                            cJSON_AddStringToObject(payload, "status", "authenticated");
                            auth_msg->payload = payload;
                            char* auth_json = ws_message_to_json(auth_msg);
                            ws_message_destroy(auth_msg);
                            
                            int len = strlen(auth_json);
                            lws_write(wsi, auth_json, &len, LWS_WRITE_TEXT);
                            free(auth_json);
                        } else {
                            /* 认证失败 */
                            ws_message_t* error_msg = ws_message_create(WS_MSG_TYPE_ERROR, NULL, NULL);
                            cJSON* payload = cJSON_CreateObject();
                            cJSON_AddStringToObject(payload, "message", "Authentication failed");
                            error_msg->payload = payload;
                            char* error_json = ws_message_to_json(error_msg);
                            ws_message_destroy(error_msg);
                            
                            int len = strlen(error_json);
                            lws_write(wsi, error_json, &len, LWS_WRITE_TEXT);
                            free(error_json);
                        }
                    }
                }
            }
            
            cJSON_Delete(json);
            break;
            
        case LWS_CALLBACK_CLOSED:
            /* 连接关闭 */
            context = ws_gateway_find_connection(gateway, wsi);
            if (context) {
                ws_gateway_remove_connection(gateway, context);
                
                AGENTOS_LOG_INFO("WebSocket connection closed from %s", context->remote_addr ? context->remote_addr : "unknown");
                
                /* 清理资源 */
                if (context->session_id) free(context->session_id);
                if (context->remote_addr) free(context->remote_addr);
                pthread_mutex_destroy(&context->queue_lock);
                pthread_cond_destroy(&context->queue_cond);
                free(context);
            }
            break;
            
        case LWS_CALLBACK_SERVER_WRITEABLE:
            /* 可写事件 */
            context = ws_gateway_find_connection(gateway, wsi);
            if (context) {
                /* 检查消息队列并发送 */
                pthread_mutex_lock(&context->queue_lock);
                
                /* 这里可以实现消息队列的发送逻辑 */
                /* 目前简单处理，不实现队列 */
                
                pthread_mutex_unlock(&context->queue_lock);
            }
            break;
            
        default:
            break;
    }
    
    return 0;
}

/* ========== 网关操作表 ========== */

/**
 * @brief 启动WebSocket网关
 * @param gateway 网关实例
 * @return AGENTOS_SUCCESS 成功
 */
static agentos_error_t ws_gateway_start(void* gateway_impl) {
    ws_gateway_t* gateway = (ws_gateway_t*)gateway_impl;
    
    /* 初始化LWS上下文 */
    memset(&gateway->info, 0, sizeof(gateway->info));
    gateway->info.port = gateway->port;
    gateway->info.iface = gateway->host;
    gateway->info.protocols = ws_protocols;
    gateway->info.ssl_cert_filepath = NULL;
    gateway->info.ssl_private_key_filepath = NULL;
    gateway->info.gid = -1;
    gateway->info.uid = -1;
    gateway->info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    
    gateway->context = lws_create_context(&gateway->info);
    if (!gateway->context) {
        AGENTOS_LOG_ERROR("Failed to create WebSocket context on port %d", gateway->port);
        return AGENTOS_EBUSY;
    }
    
    AGENTOS_LOG_INFO("WebSocket gateway started on %s:%d", gateway->host, gateway->port);
    return AGENTOS_SUCCESS;
}

/**
 * @brief 停止WebSocket网关
 * @param gateway 网关实例
 */
static void ws_gateway_stop(void* gateway_impl) {
    ws_gateway_t* gateway = (ws_gateway_t*)gateway_impl;
    
    if (gateway->context) {
        lws_context_destroy(gateway->context);
        gateway->context = NULL;
        AGENTOS_LOG_INFO("WebSocket gateway stopped");
    }
}

/**
 * @brief 销毁WebSocket网关
 * @param gateway 网关实例
 */
static void ws_gateway_destroy(void* gateway_impl) {
    ws_gateway_t* gateway = (ws_gateway_t*)gateway_impl;
    
    ws_gateway_stop(gateway);
    
    /* 清理连接列表 */
    pthread_mutex_lock(&gateway->connections_lock);
    for (size_t i = 0; i < gateway->connections_count; i++) {
        if (gateway->connections[i]->session_id) free(gateway->connections[i]->session_id);
        if (gateway->connections[i]->remote_addr) free(gateway->connections[i]->remote_addr);
        pthread_mutex_destroy(&gateway->connections[i]->queue_lock);
        pthread_cond_destroy(&gateway->connections[i]->queue_cond);
        free(gateway->connections[i]);
    }
    free(gateway->connections);
    pthread_mutex_unlock(&gateway->connections_lock);
    
    pthread_mutex_destroy(&gateway->connections_lock);
    
    if (gateway->host) {
        free(gateway->host);
    }
    
    if (gateway->ratelimiter) {
        ratelimiter_destroy(gateway->ratelimiter);
    }
    
    free(gateway);
}

/**
 * @brief 获取WebSocket网关名称
 * @param gateway 网关实例
 * @return 名称字符串
 */
static const char* ws_gateway_get_name(void* gateway_impl) {
    return "WebSocket Gateway";
}

/**
 * @brief 获取WebSocket网关统计信息
 * @param gateway 网关实例
 * @param out_json 输出JSON字符串（需调用者free）
 * @return AGENTOS_SUCCESS 成功
 */
static agentos_error_t ws_gateway_get_stats(void* gateway_impl, char** out_json) {
    ws_gateway_t* gateway = (ws_gateway_t*)gateway_impl;
    
    cJSON* stats = cJSON_CreateObject();
    cJSON_AddNumberToObject(stats, "connections_total", atomic_load(&gateway->connections_total));
    cJSON_AddNumberToObject(stats, "connections_active", atomic_load(&gateway->connections_active));
    cJSON_AddNumberToObject(stats, "messages_total", atomic_load(&gateway->messages_total));
    cJSON_AddNumberToObject(stats, "bytes_sent", atomic_load(&gateway->bytes_sent));
    cJSON_AddNumberToObject(stats, "bytes_received", atomic_load(&gateway->bytes_received));
    
    char* json_str = cJSON_Print(stats);
    cJSON_Delete(stats);
    
    *out_json = json_str;
    return AGENTOS_SUCCESS;
}

/* ========== 网关操作表 ========== */

static const gateway_ops_t ws_gateway_ops = {
    .start = ws_gateway_start,
    .stop = ws_gateway_stop,
    .destroy = ws_gateway_destroy,
    .get_name = ws_gateway_get_name,
    .get_stats = ws_gateway_get_stats
};

/* ========== WebSocket协议定义 ========== */

static const struct lws_protocols ws_protocols[] = {
    {
        "agentos-rpc",
        ws_callback,
        0,
        4096,
    },
    {
        "http",
        lws_callback_http_dummy,
        0,
        0,
    },
    { NULL, NULL, 0, 0 } /* terminator */
};

/* ========== 公共接口 ========== */

gateway_t* ws_gateway_create(const char* host, uint16_t port, gateway_server_t* server) {
    if (!host || !server) {
        return NULL;
    }
    
    ws_gateway_t* gateway = calloc(1, sizeof(ws_gateway_t));
    if (!gateway) {
        return NULL;
    }
    
    gateway->server = server;
    gateway->port = port;
    gateway->host = strdup(host);
    
    /* 初始化统计信息 */
    atomic_init(&gateway->connections_total, 0);
    atomic_init(&gateway->connections_active, 0);
    atomic_init(&gateway->messages_total, 0);
    atomic_init(&gateway->bytes_sent, 0);
    atomic_init(&gateway->bytes_received, 0);
    
    /* 初始化连接管理 */
    pthread_mutex_init(&gateway->connections_lock, NULL);
    gateway->connections_capacity = 16;
    gateway->connections = calloc(gateway->connections_capacity, sizeof(ws_connection_context_t*));
    if (!gateway->connections) {
        pthread_mutex_destroy(&gateway->connections_lock);
        free(gateway->host);
        free(gateway);
        return NULL;
    }
    gateway->connections_count = 0;
    
    /* 创建限流器 */
    gateway->ratelimiter = ratelimiter_create_simple(1000, 60); /* 1000消息/分钟 */
    if (!gateway->ratelimiter) {
        pthread_mutex_destroy(&gateway->connections_lock);
        free(gateway->connections);
        free(gateway->host);
        free(gateway);
        return NULL;
    }
    
    /* 创建网关实例 */
    gateway_t* gw = malloc(sizeof(gateway_t));
    if (!gw) {
        pthread_mutex_destroy(&gateway->connections_lock);
        free(gateway->connections);
        ratelimiter_destroy(gateway->ratelimiter);
        free(gateway->host);
        free(gateway);
        return NULL;
    }
    
    gw->ops = &ws_gateway_ops;
    gw->server = server;
    gw->impl = gateway;
    
    return gw;
}
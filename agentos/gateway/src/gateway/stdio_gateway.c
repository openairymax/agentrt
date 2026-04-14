/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file stdio_gateway.c
 * @brief Stdio网关实现 - 本地进程通信协议
 *
 * 实现标准输入输出通信协议，通过系统调用接口与内核通信。
 * 网关层只负责协议转换，不包含业务逻辑。
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "stdio_gateway.h"
#include "../utils/jsonrpc.h"
#include "../utils/syscall_router.h"
#include "../utils/gateway_utils.h"
#include "../utils/gateway_rpc_handler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 跨平台原子操作支持 - 使用统一的 atomic_compat.h */
#include <agentos/atomic_compat.h>

/* 平台特定头文件 */
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #define STDIN_FILENO 0
#else
    #include <unistd.h>
    #include <sys/select.h>
#endif

/* ========== 辅助函数（使用 gateway_utils.h 中的公共实现） ========== */

/*
 * time_ns() 已迁移至 gateway_utils.h (gateway_time_ns)
 * portable_sleep() 已迁移至 gateway_utils.h (gateway_sleep)
 */

/* ========== Stdio网关内部结构 ========== */

/**
 * @brief Stdio网关内部结构
 */
typedef struct stdio_gateway {
    void* handler_adapter;           /**< 公共回调适配器（动态分配） */
    gateway_request_handler_t handler; /**< 请求处理回调 */
    void* handler_data;              /**< 回调用户数据 */
    
    atomic_bool running;             /**< 运行标志 */
    
    atomic_uint_fast64_t commands_total;     /**< 总命令数 */
    atomic_uint_fast64_t commands_failed;    /**< 失败命令数 */
    atomic_uint_fast64_t bytes_received;     /**< 接收字节数 */
    atomic_uint_fast64_t bytes_sent;         /**< 发送字节数 */
    
    char input_buffer[8192];         /**< 输入缓冲区 */
    size_t input_buffer_pos;         /**< 输入缓冲区位置 */
} stdio_gateway_t;

/* ========== 命令处理（使用统一RPC处理器） ========== */

/**
 * @brief 显示帮助信息
 * @return 帮助字符串（需调用者free）
 */
static char* show_help(void) {
    return strdup(
        "AgentOS Stdio Gateway - Available Commands:\n"
        "  help                     - Show this help\n"
        "  rpc <json-rpc>           - Execute JSON-RPC call\n"
        "  stats                    - Show gateway statistics\n"
        "  exit                     - Exit gateway\n"
        "\n"
        "JSON-RPC Methods:\n"
        "  agentos_sys_task_submit    - Submit a task\n"
        "  agentos_sys_task_query     - Query task status\n"
        "  agentos_sys_memory_search  - Search memory\n"
        "  agentos_sys_session_create - Create session\n"
        "  agentos_sys_session_list   - List sessions\n"
        "  agentos_sys_telemetry_metrics - Get metrics\n"
    );
}

/**
 * @brief 处理JSON-RPC请求（使用统一RPC处理器）
 *
 * 通过 gateway_rpc_handle_request() 实现统一的请求处理流程，
 * 消除与 HTTP/WS 网关的代码重复。
 *
 * @param gateway 网关实例
 * @param json_str JSON字符串
 * @return 响应字符串
 */
static char* handle_jsonrpc(stdio_gateway_t* gateway, const char* json_str) {
    cJSON* request = cJSON_Parse(json_str);
    if (!request) {
        return jsonrpc_create_error_response(NULL, -32700, "Parse error", NULL);
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
        cJSON_Delete(request);
        return error_resp;
    }

    char* success_resp = result.response_json;
    result.response_json = NULL;
    gateway_rpc_free(&result);
    cJSON_Delete(request);
    return success_resp;
}

/**
 * @brief 处理命令
 * @param gateway 网关实例
 * @param input 输入字符串
 * @return 响应字符串
 */
static char* process_command(stdio_gateway_t* gateway, const char* input) {
    if (!input || strlen(input) == 0) {
        return strdup("");
    }
    
    char* trimmed = strdup(input);
    char* start = trimmed;
    while (*start == ' ' || *start == '\t') start++;
    char* end = start + strlen(start) - 1;
    while (end > start && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end--;
    *(end + 1) = '\0';
    
    if (strlen(start) == 0) {
        free(trimmed);
        return strdup("");
    }
    
    char* response = NULL;
    
    if (strcmp(start, "help") == 0 || strcmp(start, "?") == 0) {
        response = show_help();
    }
    else if (strcmp(start, "stats") == 0) {
        cJSON* stats = cJSON_CreateObject();
        cJSON_AddNumberToObject(stats, "commands_total", (double)atomic_load(&gateway->commands_total));
        cJSON_AddNumberToObject(stats, "commands_failed", (double)atomic_load(&gateway->commands_failed));
        cJSON_AddNumberToObject(stats, "bytes_received", (double)atomic_load(&gateway->bytes_received));
        cJSON_AddNumberToObject(stats, "bytes_sent", (double)atomic_load(&gateway->bytes_sent));
        response = cJSON_Print(stats);
        cJSON_Delete(stats);
    }
    else if (strcmp(start, "exit") == 0 || strcmp(start, "quit") == 0) {
        atomic_store(&gateway->running, false);
        response = strdup("Gateway shutting down...\n");
    }
    else if (strncmp(start, "rpc ", 4) == 0) {
        const char* json_str = start + 4;
        response = handle_jsonrpc(gateway, json_str);
    }
    else {
        response = strdup("Unknown command. Type 'help' for available commands.\n");
    }
    
    free(trimmed);
    return response;
}

/* ========== 网关操作表 ========== */

static agentos_error_t stdio_gateway_start(void* gateway_impl) {
    stdio_gateway_t* gateway = (stdio_gateway_t*)gateway_impl;
    
    gateway->input_buffer_pos = 0;
    atomic_store(&gateway->running, true);
    
    printf("AgentOS Stdio Gateway started. Type 'help' for available commands.\n");
    printf("> ");
    fflush(stdout);
    
    while (atomic_load(&gateway->running)) {
#ifdef _WIN32
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int ret = select(1, &read_fds, NULL, NULL, &timeout);
#else
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int ret = select(STDIN_FILENO + 1, &read_fds, NULL, NULL, &timeout);
#endif
        
        if (ret > 0) {
            char buffer[1024];
            if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
                size_t input_len = strlen(buffer);
                atomic_fetch_add(&gateway->bytes_received, input_len);
                
                if (gateway->input_buffer_pos + input_len < sizeof(gateway->input_buffer)) {
                    memcpy(gateway->input_buffer + gateway->input_buffer_pos, buffer, input_len);
                    gateway->input_buffer_pos += input_len;
                    
                    char* newline = memchr(gateway->input_buffer, '\n', gateway->input_buffer_pos);
                    if (newline) {
                        *newline = '\0';
                        char* command_line = strdup(gateway->input_buffer);
                        gateway->input_buffer_pos -= (newline + 1 - gateway->input_buffer);
                        memmove(gateway->input_buffer, newline + 1, gateway->input_buffer_pos);
                        
                        char* response = process_command(gateway, command_line);
                        free(command_line);
                        
                        if (response) {
                            printf("%s\n", response);
                            atomic_fetch_add(&gateway->bytes_sent, strlen(response));
                            atomic_fetch_add(&gateway->commands_total, 1);
                            free(response);
                        }
                        
                        if (atomic_load(&gateway->running)) {
                            printf("> ");
                            fflush(stdout);
                        }
                    }
                }
            }
        }
    }
    
    printf("Gateway stopped.\n");
    return AGENTOS_SUCCESS;
}

static void stdio_gateway_stop(void* gateway_impl) {
    stdio_gateway_t* gateway = (stdio_gateway_t*)gateway_impl;
    atomic_store(&gateway->running, false);
}

static void stdio_gateway_destroy(void* gateway_impl) {
    stdio_gateway_t* gateway = (stdio_gateway_t*)gateway_impl;
    if (!gateway) return;

    stdio_gateway_stop(gateway);

    if (gateway->handler_adapter) {
        free(gateway->handler_adapter);
        gateway->handler_adapter = NULL;
    }
    gateway->handler = NULL;
    gateway->handler_data = NULL;

    free(gateway);
}

static const char* stdio_gateway_get_name(void* gateway_impl) {
    (void)gateway_impl;
    return "Stdio Gateway";
}

static bool stdio_gateway_is_running(void* gateway_impl) {
    stdio_gateway_t* gateway = (stdio_gateway_t*)gateway_impl;
    if (!gateway) return false;
    return atomic_load(&gateway->running);
}

static agentos_error_t stdio_gateway_get_stats(void* gateway_impl, char** out_json) {
    stdio_gateway_t* gateway = (stdio_gateway_t*)gateway_impl;
    if (!gateway || !out_json) return AGENTOS_EINVAL;

    cJSON* stats = cJSON_CreateObject();
    if (!stats) return AGENTOS_ENOMEM;
    cJSON_AddNumberToObject(stats, "commands_total", (double)atomic_load(&gateway->commands_total));
    cJSON_AddNumberToObject(stats, "commands_failed", (double)atomic_load(&gateway->commands_failed));
    cJSON_AddNumberToObject(stats, "bytes_received", (double)atomic_load(&gateway->bytes_received));
    cJSON_AddNumberToObject(stats, "bytes_sent", (double)atomic_load(&gateway->bytes_sent));
    
    char* json_str = cJSON_Print(stats);
    cJSON_Delete(stats);
    
    *out_json = json_str;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 设置请求处理回调
 */
static agentos_error_t stdio_gateway_set_handler(void* gateway_impl, gateway_request_handler_t handler, void* user_data) {
    stdio_gateway_t* gateway = (stdio_gateway_t*)gateway_impl;
    if (!gateway) return AGENTOS_EINVAL;

    if (gateway->handler_adapter) {
        free(gateway->handler_adapter);
        gateway->handler_adapter = NULL;
    }

    gateway->handler = handler;
    gateway->handler_data = user_data;

    return AGENTOS_SUCCESS;
}

static const gateway_ops_t stdio_gateway_ops = {
    .start = stdio_gateway_start,
    .stop = stdio_gateway_stop,
    .destroy = stdio_gateway_destroy,
    .get_name = stdio_gateway_get_name,
    .get_stats = stdio_gateway_get_stats,
    .is_running = stdio_gateway_is_running,
    .set_handler = stdio_gateway_set_handler
};

/* ========== 公共接口 ========== */

gateway_t* stdio_gateway_create(void) {
    stdio_gateway_t* gateway = calloc(1, sizeof(stdio_gateway_t));
    if (!gateway) {
        return NULL;
    }

    gateway->handler_adapter = NULL;
    gateway->handler = NULL;
    gateway->handler_data = NULL;
    
    atomic_init(&gateway->running, false);
    atomic_init(&gateway->commands_total, 0);
    atomic_init(&gateway->commands_failed, 0);
    atomic_init(&gateway->bytes_received, 0);
    atomic_init(&gateway->bytes_sent, 0);
    
    gateway_t* gw = malloc(sizeof(gateway_t));
    if (!gw) {
        free(gateway);
        return NULL;
    }
    
    gw->ops = &stdio_gateway_ops;
    gw->impl = gateway;
    gw->type = GATEWAY_TYPE_STDIO;
    
    return gw;
}

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
#include "syscalls.h"
#include "agentos.h"
#include "gateway.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#define STDIN_FILENO 0
#else
#include <unistd.h>
#include <sys/select.h>
#endif

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

/**
 * @brief 跨平台sleep函数
 * @param seconds 秒数
 */
static void portable_sleep(unsigned int seconds) {
#ifdef _WIN32
    Sleep(seconds * 1000);
#else
    sleep(seconds);
#endif
}

/* ========== Stdio网关内部结构 ========== */

/**
 * @brief Stdio网关内部结构
 */
typedef struct stdio_gateway {
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

/* ========== 系统调用路由 ========== */

/**
 * @brief 处理系统调用请求
 * @param method 方法名
 * @param params 参数对象
 * @return JSON响应字符串
 */
static char* handle_system_call(const char* method, cJSON* params) {
    cJSON* result = NULL;
    agentos_error_t err = AGENTOS_SUCCESS;
    
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
            return jsonrpc_create_error_response(NULL, -32602, "Invalid params: input required", NULL);
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
            return jsonrpc_create_error_response(NULL, -32602, "Invalid params: task_id required", NULL);
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
            return jsonrpc_create_error_response(NULL, -32602, "Invalid params: query required", NULL);
        }
    }
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
    else if (strcmp(method, "agentos_sys_session_list") == 0) {
        char** sessions = NULL;
        size_t count = 0;
        err = agentos_sys_session_list(&sessions, &count);
        
        if (err == AGENTOS_SUCCESS) {
            result = cJSON_CreateObject();
            cJSON* arr = cJSON_CreateArray();
            for (size_t i = 0; i < count; i++) {
                cJSON_AddItemToArray(arr, cJSON_CreateString(sessions[i]));
                free(sessions[i]);
            }
            cJSON_AddItemToObject(result, "sessions", arr);
            cJSON_AddNumberToObject(result, "count", count);
            free(sessions);
        }
    }
    else if (strcmp(method, "agentos_sys_telemetry_metrics") == 0) {
        char* out_metrics = NULL;
        err = agentos_sys_telemetry_metrics(&out_metrics);
        
        if (err == AGENTOS_SUCCESS && out_metrics) {
            result = cJSON_Parse(out_metrics);
            free(out_metrics);
        }
    }
    else {
        return jsonrpc_create_error_response(NULL, -32601, "Method not found", NULL);
    }
    
    if (err != AGENTOS_SUCCESS) {
        cJSON_Delete(result);
        char err_msg[64];
        snprintf(err_msg, sizeof(err_msg), "System call failed: %d", err);
        return jsonrpc_create_error_response(NULL, -32000, err_msg, NULL);
    }
    
    return jsonrpc_create_success_response(NULL, result);
}

/* ========== 命令处理 ========== */

/**
 * @brief 显示帮助信息
 * @return 帮助字符串
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
 * @brief 处理JSON-RPC请求
 * @param json_str JSON字符串
 * @return 响应字符串
 */
static char* handle_jsonrpc(const char* json_str) {
    cJSON* request = cJSON_Parse(json_str);
    if (!request) {
        return jsonrpc_create_error_response(NULL, -32700, "Parse error", NULL);
    }
    
    if (jsonrpc_validate_request(request) != 0) {
        cJSON_Delete(request);
        return jsonrpc_create_error_response(NULL, -32600, "Invalid Request", NULL);
    }
    
    const char* method = jsonrpc_get_method(request);
    const cJSON* params = jsonrpc_get_params(request);
    const cJSON* id = jsonrpc_get_id(request);
    
    char* response = handle_system_call(method, (cJSON*)params);
    
    if (id && response) {
        cJSON* parsed = cJSON_Parse(response);
        if (parsed) {
            cJSON_AddItemToObject(parsed, "id", cJSON_Duplicate(id, 1));
            free(response);
            response = cJSON_PrintUnformatted(parsed);
            cJSON_Delete(parsed);
        }
    }
    
    cJSON_Delete(request);
    return response;
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
        response = handle_jsonrpc(json_str);
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
    free(gateway);
}

static const char* stdio_gateway_get_name(void* gateway_impl) {
    (void)gateway_impl;
    return "Stdio Gateway";
}

static agentos_error_t stdio_gateway_get_stats(void* gateway_impl, char** out_json) {
    stdio_gateway_t* gateway = (stdio_gateway_t*)gateway_impl;
    
    cJSON* stats = cJSON_CreateObject();
    cJSON_AddNumberToObject(stats, "commands_total", (double)atomic_load(&gateway->commands_total));
    cJSON_AddNumberToObject(stats, "commands_failed", (double)atomic_load(&gateway->commands_failed));
    cJSON_AddNumberToObject(stats, "bytes_received", (double)atomic_load(&gateway->bytes_received));
    cJSON_AddNumberToObject(stats, "bytes_sent", (double)atomic_load(&gateway->bytes_sent));
    
    char* json_str = cJSON_Print(stats);
    cJSON_Delete(stats);
    
    *out_json = json_str;
    return AGENTOS_SUCCESS;
}

static const gateway_ops_t stdio_gateway_ops = {
    .start = stdio_gateway_start,
    .stop = stdio_gateway_stop,
    .destroy = stdio_gateway_destroy,
    .get_name = stdio_gateway_get_name,
    .get_stats = stdio_gateway_get_stats
};

/* ========== 公共接口 ========== */

gateway_t* stdio_gateway_create(void) {
    stdio_gateway_t* gateway = calloc(1, sizeof(stdio_gateway_t));
    if (!gateway) {
        return NULL;
    }
    
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
    
    return gw;
}

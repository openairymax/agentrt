/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file syscall_router.c
 * @brief 系统调用路由器实现
 *
 * 统一处理 JSON-RPC 请求到系统调用的路由。
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "syscall_router.h"
#include "jsonrpc.h"
#include "syscalls.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/**
 * @brief 路由任务管理相关系统调用
 */
static char* route_task_methods(const char* method, cJSON* params, cJSON* request_id) {
    cJSON* result = NULL;
    agentos_error_t err = AGENTOS_SUCCESS;
    
    if (strcmp(method, "agentos_sys_task_submit") == 0) {
        cJSON* input = cJSON_GetObjectItem(params, "input");
        cJSON* timeout = cJSON_GetObjectItem(params, "timeout_ms");
        
        if (!input || !cJSON_IsString(input)) {
            return jsonrpc_create_error_response(request_id, -32602, 
                "Invalid params: input required", NULL);
        }
        
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
    }
    else if (strcmp(method, "agentos_sys_task_query") == 0) {
        cJSON* task_id = cJSON_GetObjectItem(params, "task_id");
        
        if (!task_id || !cJSON_IsString(task_id)) {
            return jsonrpc_create_error_response(request_id, -32602, 
                "Invalid params: task_id required", NULL);
        }
        
        int status = 0;
        err = agentos_sys_task_query(task_id->valuestring, &status);
        
        if (err == AGENTOS_SUCCESS) {
            result = cJSON_CreateObject();
            cJSON_AddNumberToObject(result, "status", status);
        }
    }
    else if (strcmp(method, "agentos_sys_task_wait") == 0) {
        cJSON* task_id = cJSON_GetObjectItem(params, "task_id");
        cJSON* timeout = cJSON_GetObjectItem(params, "timeout_ms");
        
        if (!task_id || !cJSON_IsString(task_id)) {
            return jsonrpc_create_error_response(request_id, -32602, 
                "Invalid params: task_id required", NULL);
        }
        
        char* out_result = NULL;
        uint32_t timeout_ms = timeout ? (uint32_t)timeout->valueint : 0;
        err = agentos_sys_task_wait(task_id->valuestring, timeout_ms, &out_result);
        
        if (err == AGENTOS_SUCCESS && out_result) {
            result = cJSON_CreateObject();
            cJSON_AddStringToObject(result, "result", out_result);
            free(out_result);
        }
    }
    else if (strcmp(method, "agentos_sys_task_cancel") == 0) {
        cJSON* task_id = cJSON_GetObjectItem(params, "task_id");
        
        if (!task_id || !cJSON_IsString(task_id)) {
            return jsonrpc_create_error_response(request_id, -32602, 
                "Invalid params: task_id required", NULL);
        }
        
        err = agentos_sys_task_cancel(task_id->valuestring);
        if (err == AGENTOS_SUCCESS) {
            result = cJSON_CreateObject();
            cJSON_AddBoolToObject(result, "cancelled", true);
        }
    }
    
    /* 处理错误 */
    if (err != AGENTOS_SUCCESS) {
        cJSON_Delete(result);
        char err_msg[64];
        snprintf(err_msg, sizeof(err_msg), "System call failed: %d", err);
        return jsonrpc_create_error_response(request_id, -32000, err_msg, NULL);
    }
    
    return jsonrpc_create_success_response(request_id, result);
}

/**
 * @brief 路由记忆管理相关系统调用
 */
static char* route_memory_methods(const char* method, cJSON* params, cJSON* request_id) {
    cJSON* result = NULL;
    agentos_error_t err = AGENTOS_SUCCESS;
    
    if (strcmp(method, "agentos_sys_memory_write") == 0) {
        cJSON* data = cJSON_GetObjectItem(params, "data");
        cJSON* metadata = cJSON_GetObjectItem(params, "metadata");
        
        if (!data || !cJSON_IsString(data)) {
            return jsonrpc_create_error_response(request_id, -32602, 
                "Invalid params: data required", NULL);
        }
        
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
    }
    else if (strcmp(method, "agentos_sys_memory_search") == 0) {
        cJSON* query = cJSON_GetObjectItem(params, "query");
        cJSON* limit = cJSON_GetObjectItem(params, "limit");
        
        if (!query || !cJSON_IsString(query)) {
            return jsonrpc_create_error_response(request_id, -32602, 
                "Invalid params: query required", NULL);
        }
        
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
    }
    else if (strcmp(method, "agentos_sys_memory_get") == 0) {
        cJSON* record_id = cJSON_GetObjectItem(params, "record_id");
        
        if (!record_id || !cJSON_IsString(record_id)) {
            return jsonrpc_create_error_response(request_id, -32602, 
                "Invalid params: record_id required", NULL);
        }
        
        void* out_data = NULL;
        size_t out_len = 0;
        err = agentos_sys_memory_get(record_id->valuestring, &out_data, &out_len);
        
        if (err == AGENTOS_SUCCESS && out_data) {
            result = cJSON_CreateObject();
            cJSON_AddStringToObject(result, "data", (char*)out_data);
            cJSON_AddNumberToObject(result, "length", out_len);
            free(out_data);
        }
    }
    else if (strcmp(method, "agentos_sys_memory_delete") == 0) {
        cJSON* record_id = cJSON_GetObjectItem(params, "record_id");
        
        if (!record_id || !cJSON_IsString(record_id)) {
            return jsonrpc_create_error_response(request_id, -32602, 
                "Invalid params: record_id required", NULL);
        }
        
        err = agentos_sys_memory_delete(record_id->valuestring);
        if (err == AGENTOS_SUCCESS) {
            result = cJSON_CreateObject();
            cJSON_AddBoolToObject(result, "deleted", true);
        }
    }
    
    /* 处理错误 */
    if (err != AGENTOS_SUCCESS) {
        cJSON_Delete(result);
        char err_msg[64];
        snprintf(err_msg, sizeof(err_msg), "System call failed: %d", err);
        return jsonrpc_create_error_response(request_id, -32000, err_msg, NULL);
    }
    
    return jsonrpc_create_success_response(request_id, result);
}

/**
 * @brief 路由会话管理相关系统调用
 */
static char* route_session_methods(const char* method, cJSON* params, cJSON* request_id) {
    cJSON* result = NULL;
    agentos_error_t err = AGENTOS_SUCCESS;
    
    if (strcmp(method, "agentos_sys_session_create") == 0) {
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
        
        if (!session_id || !cJSON_IsString(session_id)) {
            return jsonrpc_create_error_response(request_id, -32602, 
                "Invalid params: session_id required", NULL);
        }
        
        char* out_info = NULL;
        err = agentos_sys_session_get(session_id->valuestring, &out_info);
        
        if (err == AGENTOS_SUCCESS && out_info) {
            result = cJSON_Parse(out_info);
            free(out_info);
        }
    }
    else if (strcmp(method, "agentos_sys_session_close") == 0) {
        cJSON* session_id = cJSON_GetObjectItem(params, "session_id");
        
        if (!session_id || !cJSON_IsString(session_id)) {
            return jsonrpc_create_error_response(request_id, -32602, 
                "Invalid params: session_id required", NULL);
        }
        
        err = agentos_sys_session_close(session_id->valuestring);
        if (err == AGENTOS_SUCCESS) {
            result = cJSON_CreateObject();
            cJSON_AddBoolToObject(result, "closed", true);
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
    
    /* 处理错误 */
    if (err != AGENTOS_SUCCESS) {
        cJSON_Delete(result);
        char err_msg[64];
        snprintf(err_msg, sizeof(err_msg), "System call failed: %d", err);
        return jsonrpc_create_error_response(request_id, -32000, err_msg, NULL);
    }
    
    return jsonrpc_create_success_response(request_id, result);
}

/**
 * @brief 路由可观测性相关系统调用
 */
static char* route_telemetry_methods(const char* method, cJSON* params, cJSON* request_id) {
    cJSON* result = NULL;
    agentos_error_t err = AGENTOS_SUCCESS;
    
    if (strcmp(method, "agentos_sys_telemetry_metrics") == 0) {
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
    
    /* 处理错误 */
    if (err != AGENTOS_SUCCESS) {
        cJSON_Delete(result);
        char err_msg[64];
        snprintf(err_msg, sizeof(err_msg), "System call failed: %d", err);
        return jsonrpc_create_error_response(request_id, -32000, err_msg, NULL);
    }
    
    return jsonrpc_create_success_response(request_id, result);
}

/* ========== 公共接口 ========== */

/**
 * @brief 路由系统调用请求
 */
char* gateway_syscall_route(const char* method, cJSON* params, cJSON* request_id) {
    if (!method || strlen(method) == 0) {
        return jsonrpc_create_error_response(request_id, -32600, "Invalid Request", NULL);
    }
    
    /* 根据方法前缀分发到对应的处理函数 */
    if (strncmp(method, "agentos_sys_task_", 18) == 0) {
        return route_task_methods(method, params, request_id);
    }
    else if (strncmp(method, "agentos_sys_memory_", 20) == 0) {
        return route_memory_methods(method, params, request_id);
    }
    else if (strncmp(method, "agentos_sys_session_", 20) == 0) {
        return route_session_methods(method, params, request_id);
    }
    else if (strncmp(method, "agentos_sys_telemetry_", 22) == 0) {
        return route_telemetry_methods(method, params, request_id);
    }
    
    /* 方法未找到 */
    return jsonrpc_create_error_response(request_id, -32601, "Method not found", NULL);
}

/**
 * @file error_utils.c
 * @brief 错误处理工具函数实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "error_utils.h"
#include "agentos.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <cjson/cJSON.h>

/**
 * @brief 错误码与错误消息的映射表
 */
static const struct {
    agentos_error_t code;
    const char* name;
    const char* message;
} error_map[] = {
    {AGENTOS_SUCCESS, "SUCCESS", "操作成功"},
    {AGENTOS_EINVAL, "EINVAL", "无效参数"},
    {AGENTOS_ENOMEM, "ENOMEM", "内存分配失败"},
    {AGENTOS_ENOTSUP, "ENOTSUP", "不支持的操作"},
    // From data intelligence emerges. by spharx
    {AGENTOS_EBUSY, "EBUSY", "系统忙"},
    {AGENTOS_ETIMEDOUT, "ETIMEDOUT", "操作超时"},
    {AGENTOS_ENOENT, "ENOENT", "实体不存在"},
    {AGENTOS_EIO, "EIO", "输入输出错误"},
    {AGENTOS_EOVERFLOW, "EOVERFLOW", "溢出错误"},
    {AGENTOS_EEXIST, "EEXIST", "实体已存在"},
    {AGENTOS_EACCES, "EACCES", "权限不足"},
    {AGENTOS_ECONNREFUSED, "ECONNREFUSED", "连接被拒绝"},
    {AGENTOS_ECONNRESET, "ECONNRESET", "连接被重置"},
    {AGENTOS_ENOTCONN, "ENOTCONN", "未连接"},
    {AGENTOS_EPROTO, "EPROTO", "协议错误"},
    {AGENTOS_EMSGSIZE, "EMSGSIZE", "消息过大"},
    {AGENTOS_ENOSPC, "ENOSPC", "空间不足"},
    {AGENTOS_ERANGE, "ERANGE", "超出范围"},
    {AGENTOS_EDEADLK, "EDEADLK", "死锁"},
    {AGENTOS_EAGAIN, "EAGAIN", "资源暂时不可用"},
    {AGENTOS_EINTR, "EINTR", "操作被中断"},
    {AGENTOS_UNKNOWN, "UNKNOWN", "未知错误"}
};

#define ERROR_MAP_SIZE (sizeof(error_map) / sizeof(error_map[0]))

const char* agentos_error_string(agentos_error_t err) {
    for (size_t i = 0; i < ERROR_MAP_SIZE; i++) {
        if (error_map[i].code == err) {
            return error_map[i].message;
        }
    }
    return "未知错误";
}

agentos_error_t agentos_error_to_json(
    agentos_error_t err,
    const char* message,
    char** out_json) {
    
    if (!out_json) return AGENTOS_EINVAL;
    
    cJSON* root = cJSON_CreateObject();
    if (!root) return AGENTOS_ENOMEM;
    
    // 查找错误信息
    const char* err_name = "UNKNOWN";
    const char* err_msg = "未知错误";
    for (size_t i = 0; i < ERROR_MAP_SIZE; i++) {
        if (error_map[i].code == err) {
            err_name = error_map[i].name;
            err_msg = error_map[i].message;
            break;
        }
    }
    
    cJSON_AddNumberToObject(root, "code", err);
    cJSON_AddStringToObject(root, "name", err_name);
    cJSON_AddStringToObject(root, "message", err_msg);
    
    if (message) {
        cJSON_AddStringToObject(root, "detail", message);
    }
    
    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    if (!json) return AGENTOS_ENOMEM;
    
    *out_json = json;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_error_context_create(
    agentos_error_t code,
    const char* message,
    const char* file,
    int line,
    const char* function,
    agentos_error_context_t** out_context) {
    
    if (!out_context) return AGENTOS_EINVAL;
    
    agentos_error_context_t* ctx = (agentos_error_context_t*)calloc(1, sizeof(agentos_error_context_t));
    if (!ctx) return AGENTOS_ENOMEM;
    
    ctx->code = code;
    ctx->timestamp_ns = agentos_time_monotonic_ns();
    
    if (message) {
        ctx->message = strdup(message);
        if (!ctx->message) {
            free(ctx);
            return AGENTOS_ENOMEM;
        }
    }
    
    if (file) {
        ctx->file = strdup(file);
        if (!ctx->file) {
            if (ctx->message) free(ctx->message);
            free(ctx);
            return AGENTOS_ENOMEM;
        }
    }
    
    ctx->line = line;
    
    if (function) {
        ctx->function = strdup(function);
        if (!ctx->function) {
            if (ctx->message) free(ctx->message);
            if (ctx->file) free(ctx->file);
            free(ctx);
            return AGENTOS_ENOMEM;
        }
    }
    
    *out_context = ctx;
    return AGENTOS_SUCCESS;
}

void agentos_error_context_free(agentos_error_context_t* context) {
    if (!context) return;
    
    if (context->message) free(context->message);
    if (context->file) free(context->file);
    if (context->function) free(context->function);
    free(context);
}

agentos_error_t agentos_error_context_to_json(
    const agentos_error_context_t* context,
    char** out_json) {
    
    if (!context || !out_json) return AGENTOS_EINVAL;
    
    cJSON* root = cJSON_CreateObject();
    if (!root) return AGENTOS_ENOMEM;
    
    // 查找错误信息
    const char* err_name = "UNKNOWN";
    const char* err_msg = "未知错误";
    for (size_t i = 0; i < ERROR_MAP_SIZE; i++) {
        if (error_map[i].code == context->code) {
            err_name = error_map[i].name;
            err_msg = error_map[i].message;
            break;
        }
    }
    
    cJSON_AddNumberToObject(root, "code", context->code);
    cJSON_AddStringToObject(root, "name", err_name);
    cJSON_AddStringToObject(root, "message", err_msg);
    
    if (context->message) {
        cJSON_AddStringToObject(root, "detail", context->message);
    }
    
    if (context->file) {
        cJSON_AddStringToObject(root, "file", context->file);
    }
    
    cJSON_AddNumberToObject(root, "line", context->line);
    
    if (context->function) {
        cJSON_AddStringToObject(root, "function", context->function);
    }
    
    cJSON_AddNumberToObject(root, "timestamp_ns", context->timestamp_ns);
    
    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    if (!json) return AGENTOS_ENOMEM;
    
    *out_json = json;
    return AGENTOS_SUCCESS;
}

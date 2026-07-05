/**
 * @file error_utils.c
 * @brief 错误处理工具函数实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "error_utils.h"

#include "agentrt.h"

#include <stdlib.h>

/* Unified base library compatibility layer */
#include "memory_compat.h"
#include "string_compat.h"

#include <stdio.h>
#include <string.h>
#ifdef AGENTRT_HAS_CJSON
#include <cjson/cJSON.h>
#endif

/**
 * @brief 错误码与信息映射表
 */
static const struct {
    agentrt_error_t code;
    const char *name;
    const char *message;
} error_map[] = {{AGENTRT_SUCCESS, "SUCCESS", "操作成功"},
                 {AGENTRT_EINVAL, "EINVAL", "无效参数"},
                 {AGENTRT_ENOMEM, "ENOMEM", "内存分配失败"},
                 {AGENTRT_ENOTSUP, "ENOTSUP", "不支持的操作"},
                 {AGENTRT_EBUSY, "EBUSY", "系统忙"},
                 {AGENTRT_ETIMEDOUT, "ETIMEDOUT", "操作超时"},
                 {AGENTRT_ENOENT, "ENOENT", "实体不存在"},
                 {AGENTRT_EIO, "EIO", "输入输出错误"},
                 {AGENTRT_EOVERFLOW, "EOVERFLOW", "缓冲区溢出"},
                 {AGENTRT_EEXIST, "EEXIST", "实体已存在"},
                 {AGENTRT_EACCES, "EACCES", "权限不足"},
                 {AGENTRT_ECONNREFUSED, "ECONNREFUSED", "连接被拒绝"},
                 {AGENTRT_ECONNRESET, "ECONNRESET", "连接被重置"},
                 {AGENTRT_ENOTCONN, "ENOTCONN", "未连接"},
                 {AGENTRT_EPROTO, "EPROTO", "协议错误"},
                 {AGENTRT_EMSGSIZE, "EMSGSIZE", "消息过长"},
                 {AGENTRT_ENOSPC, "ENOSPC", "空间不足"},
                 {AGENTRT_ERANGE, "ERANGE", "数值范围"},
                 {AGENTRT_EDEADLK, "EDEADLK", "死锁"},
                 {AGENTRT_EAGAIN, "EAGAIN", "资源暂时不可用"},
                 {AGENTRT_EINTR, "EINTR", "操作被中断"},
                 {AGENTRT_EPLATFORM, "EPLATFORM", "平台未初始化"},
                 {AGENTRT_EPROTONOSUPPORT, "EPROTONOSUPPORT", "协议/命令不支持"},
                 {AGENTRT_ESERVICE, "ESERVICE", "服务不可用"},
                 {AGENTRT_EUNKNOWN, "UNKNOWN", "未知错误"}};

#define ERROR_MAP_SIZE (sizeof(error_map) / sizeof(error_map[0]))

const char *agentrt_error_string(agentrt_error_t err)
{
    for (size_t i = 0; i < ERROR_MAP_SIZE; i++) {
        if (error_map[i].code == err) {
            return error_map[i].message;
        }
    }
    return "未知错误";
}

#ifdef AGENTRT_HAS_CJSON
agentrt_error_t agentrt_error_to_json(agentrt_error_t err, const char *message, char **out_json)
{

    if (!out_json)
        return AGENTRT_EINVAL;

    cJSON *root = cJSON_CreateObject();
    if (!root)
        return AGENTRT_ENOMEM;

    // 查找错误信息
    const char *err_name = "UNKNOWN";
    const char *err_msg = "未知错误";
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

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!json)
        return AGENTRT_ENOMEM;

    *out_json = json;
    return AGENTRT_SUCCESS;
}
#endif /* AGENTRT_HAS_CJSON */

agentrt_error_t agentrt_error_context_create(agentrt_error_t code, const char *message,
                                             const char *file, int line, const char *function,
                                             agentrt_error_context_t **out_context)
{

    if (!out_context)
        return AGENTRT_EINVAL;

    agentrt_error_context_t *ctx =
        (agentrt_error_context_t *)AGENTRT_CALLOC(1, sizeof(agentrt_error_context_t));
    if (!ctx)
        return AGENTRT_ENOMEM;

    ctx->code = code;
    ctx->timestamp_ns = agentrt_time_monotonic_ns();

    if (message) {
        ctx->message = AGENTRT_STRDUP(message);
        if (!ctx->message) {
            AGENTRT_FREE(ctx);
            return AGENTRT_ENOMEM;
        }
    }

    if (file) {
        ctx->file = AGENTRT_STRDUP(file);
        if (!ctx->file) {
            if (ctx->message)
                AGENTRT_FREE(ctx->message);
            AGENTRT_FREE(ctx);
            return AGENTRT_ENOMEM;
        }
    }

    ctx->line = line;

    if (function) {
        ctx->function = AGENTRT_STRDUP(function);
        if (!ctx->function) {
            if (ctx->message)
                AGENTRT_FREE(ctx->message);
            if (ctx->file)
                AGENTRT_FREE(ctx->file);
            AGENTRT_FREE(ctx);
            return AGENTRT_ENOMEM;
        }
    }

    *out_context = ctx;
    return AGENTRT_SUCCESS;
}

void agentrt_error_context_free(agentrt_error_context_t *context)
{
    if (!context)
        return;

    if (context->message)
        AGENTRT_FREE(context->message);
    if (context->file)
        AGENTRT_FREE(context->file);
    if (context->function)
        AGENTRT_FREE(context->function);
    AGENTRT_FREE(context);
}

#ifdef AGENTRT_HAS_CJSON
agentrt_error_t agentrt_error_context_to_json(const agentrt_error_context_t *context,
                                              char **out_json)
{

    if (!context || !out_json)
        return AGENTRT_EINVAL;

    cJSON *root = cJSON_CreateObject();
    if (!root)
        return AGENTRT_ENOMEM;

    // 查找错误信息
    const char *err_name = "UNKNOWN";
    const char *err_msg = "未知错误";
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

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!json)
        return AGENTRT_ENOMEM;

    *out_json = json;
    return AGENTRT_SUCCESS;
}
#endif /* AGENTRT_HAS_CJSON */

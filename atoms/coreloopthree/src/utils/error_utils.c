/**
 * @file error_utils.c
 * @brief ��������ߺ���ʵ��
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "error_utils.h"
#include "agentos.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../commons/utils/memory/include/memory_compat.h"
#include "../../../commons/utils/string/include/string_compat.h"
#include <string.h>
#include <stdio.h>
#include <cjson/cJSON.h>

/**
 * @brief �������������Ϣ��ӳ���
 */
static const struct {
    agentos_error_t code;
    const char* name;
    const char* message;
} error_map[] = {
    {AGENTOS_SUCCESS, "SUCCESS", "�����ɹ�"},
    {AGENTOS_EINVAL, "EINVAL", "��Ч����"},
    {AGENTOS_ENOMEM, "ENOMEM", "�ڴ����ʧ��"},
    {AGENTOS_ENOTSUP, "ENOTSUP", "��֧�ֵĲ���"},
    {AGENTOS_EBUSY, "EBUSY", "ϵͳæ"},
    {AGENTOS_ETIMEDOUT, "ETIMEDOUT", "������ʱ"},
    {AGENTOS_ENOENT, "ENOENT", "ʵ�岻����"},
    {AGENTOS_EIO, "EIO", "�����������"},
    {AGENTOS_EOVERFLOW, "EOVERFLOW", "�������"},
    {AGENTOS_EEXIST, "EEXIST", "ʵ���Ѵ���"},
    {AGENTOS_EACCES, "EACCES", "Ȩ�޲���"},
    {AGENTOS_ECONNREFUSED, "ECONNREFUSED", "���ӱ��ܾ�"},
    {AGENTOS_ECONNRESET, "ECONNRESET", "���ӱ�����"},
    {AGENTOS_ENOTCONN, "ENOTCONN", "δ����"},
    {AGENTOS_EPROTO, "EPROTO", "Э�����"},
    {AGENTOS_EMSGSIZE, "EMSGSIZE", "��Ϣ����"},
    {AGENTOS_ENOSPC, "ENOSPC", "�ռ䲻��"},
    {AGENTOS_ERANGE, "ERANGE", "������Χ"},
    {AGENTOS_EDEADLK, "EDEADLK", "����"},
    {AGENTOS_EAGAIN, "EAGAIN", "��Դ��ʱ������"},
    {AGENTOS_EINTR, "EINTR", "�������ж�"},
    {AGENTOS_UNKNOWN, "UNKNOWN", "δ֪����"}
};

#define ERROR_MAP_SIZE (sizeof(error_map) / sizeof(error_map[0]))

const char* agentos_error_string(agentos_error_t err) {
    for (size_t i = 0; i < ERROR_MAP_SIZE; i++) {
        if (error_map[i].code == err) {
            return error_map[i].message;
        }
    }
    return "δ֪����";
}

agentos_error_t agentos_error_to_json(
    agentos_error_t err,
    const char* message,
    char** out_json) {
    
    if (!out_json) return AGENTOS_EINVAL;
    
    cJSON* root = cJSON_CreateObject();
    if (!root) return AGENTOS_ENOMEM;
    
    // ���Ҵ�����Ϣ
    const char* err_name = "UNKNOWN";
    const char* err_msg = "δ֪����";
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
    
    agentos_error_context_t* ctx = (agentos_error_context_t*)AGENTOS_CALLOC(1, sizeof(agentos_error_context_t));
    if (!ctx) return AGENTOS_ENOMEM;
    
    ctx->code = code;
    ctx->timestamp_ns = agentos_time_monotonic_ns();
    
    if (message) {
        ctx->message = AGENTOS_STRDUP(message);
        if (!ctx->message) {
            AGENTOS_FREE(ctx);
            return AGENTOS_ENOMEM;
        }
    }
    
    if (file) {
        ctx->file = AGENTOS_STRDUP(file);
        if (!ctx->file) {
            if (ctx->message) AGENTOS_FREE(ctx->message);
            AGENTOS_FREE(ctx);
            return AGENTOS_ENOMEM;
        }
    }
    
    ctx->line = line;
    
    if (function) {
        ctx->function = AGENTOS_STRDUP(function);
        if (!ctx->function) {
            if (ctx->message) AGENTOS_FREE(ctx->message);
            if (ctx->file) AGENTOS_FREE(ctx->file);
            AGENTOS_FREE(ctx);
            return AGENTOS_ENOMEM;
        }
    }
    
    *out_context = ctx;
    return AGENTOS_SUCCESS;
}

void agentos_error_context_free(agentos_error_context_t* context) {
    if (!context) return;
    
    if (context->message) AGENTOS_FREE(context->message);
    if (context->file) AGENTOS_FREE(context->file);
    if (context->function) AGENTOS_FREE(context->function);
    AGENTOS_FREE(context);
}

agentos_error_t agentos_error_context_to_json(
    const agentos_error_context_t* context,
    char** out_json) {
    
    if (!context || !out_json) return AGENTOS_EINVAL;
    
    cJSON* root = cJSON_CreateObject();
    if (!root) return AGENTOS_ENOMEM;
    
    // ���Ҵ�����Ϣ
    const char* err_name = "UNKNOWN";
    const char* err_msg = "δ֪����";
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

/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file jsonrpc.c
 * @brief JSON-RPC 2.0 协议工具函数实现
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "jsonrpc.h"
#include <string.h>
#include <stdlib.h>

#ifdef GATEWAY_HAS_CJSON
#include <cJSON.h>
#endif

/* ==================== 标准错误消息 ==================== */

static const char* const g_error_messages[] = {
    [JSONRPC_PARSE_ERROR + 32700]     = "Parse error",
    [JSONRPC_INVALID_REQUEST + 32700] = "Invalid Request",
    [JSONRPC_METHOD_NOT_FOUND + 32700] = "Method not found",
    [JSONRPC_INVALID_PARAMS + 32700]  = "Invalid params",
    [JSONRPC_INTERNAL_ERROR + 32700]  = "Internal error",
};

static const char* const g_custom_error_messages[] = {
    [JSONRPC_RATE_LIMITED + 32001]      = "Rate limit exceeded",
    [JSONRPC_AUTH_FAILED + 32002]       = "Authentication failed",
    [JSONRPC_SESSION_EXPIRED + 32003]   = "Session expired",
    [JSONRPC_SERVICE_UNAVAILABLE + 32004] = "Service unavailable",
};

/* ==================== 请求验证 ==================== */

int jsonrpc_validate_request(const cJSON* json) {
#ifdef GATEWAY_HAS_CJSON
    if (!json) {
        return -1;
    }

    /* 检查必需字段 */
    if (!cJSON_HasObjectItem(json, "jsonrpc") ||
        !cJSON_HasObjectItem(json, "method") ||
        !cJSON_HasObjectItem(json, "id")) {
        return -1;
    }

    const cJSON* jsonrpc = cJSON_GetObjectItemCaseSensitive(json, "jsonrpc");
    const cJSON* method = cJSON_GetObjectItemCaseSensitive(json, "method");
    const cJSON* id = cJSON_GetObjectItemCaseSensitive(json, "id");

    /* 验证 jsonrpc 字段 */
    if (!cJSON_IsString(jsonrpc)) {
        return -2;
    }
    if (strcmp(jsonrpc->valuestring, "2.0") != 0) {
        return -3;
    }

    /* 验证 method 字段 */
    if (!cJSON_IsString(method)) {
        return -2;
    }
    if (strlen(method->valuestring) == 0) {
        return -1;
    }

    /* 验证 id 字段（可以是数字、字符串或 null） */
    if (!cJSON_IsNumber(id) && !cJSON_IsString(id) && !cJSON_IsNull(id)) {
        return -2;
    }

    return 0;
#else
    (void)json;
    return -1; /* 无cJSON时返回无效 */
#endif
}

const char* jsonrpc_get_method(const cJSON* json) {
#ifdef GATEWAY_HAS_CJSON
    if (!json) {
        return NULL;
    }
    const cJSON* method = cJSON_GetObjectItemCaseSensitive(json, "method");
    if (!cJSON_IsString(method)) {
        return NULL;
    }
    return method->valuestring;
#else
    (void)json;
    return NULL;
#endif
}

const cJSON* jsonrpc_get_params(const cJSON* json) {
#ifdef GATEWAY_HAS_CJSON
    if (!json) {
        return NULL;
    }
    return cJSON_GetObjectItemCaseSensitive(json, "params");
#else
    (void)json;
    return NULL;
#endif
}

const cJSON* jsonrpc_get_id(const cJSON* json) {
#ifdef GATEWAY_HAS_CJSON
    if (!json) {
        return NULL;
    }
    return cJSON_GetObjectItemCaseSensitive(json, "id");
#else
    (void)json;
    return NULL;
#endif
}

/* ==================== 响应生成 ==================== */

char* jsonrpc_create_success_response(const cJSON* id, cJSON* result) {
#ifdef GATEWAY_HAS_CJSON
    cJSON* response = cJSON_CreateObject();
    if (!response) {
        return NULL;
    }

    /* 添加 jsonrpc 版本 */
    cJSON_AddStringToObject(response, "jsonrpc", "2.0");

    /* 添加结果 */
    if (result) {
        cJSON_AddItemToObject(response, "result", result);
    } else {
        cJSON_AddNullToObject(response, "result");
    }

    /* 添加 ID */
    if (id) {
        cJSON_AddItemToObject(response, "id", cJSON_Duplicate(id, 1));
    } else {
        cJSON_AddNullToObject(response, "id");
    }

    char* json_str = cJSON_PrintUnformatted(response);
    cJSON_Delete(response);

    return json_str;
#else
    (void)id;
    (void)result;
    return NULL;
#endif
}

char* jsonrpc_create_error_response(
    const cJSON* id,
    int code,
    const char* message,
    cJSON* data
) {
#ifdef GATEWAY_HAS_CJSON
    cJSON* response = cJSON_CreateObject();
    if (!response) {
        if (data) cJSON_Delete(data);
        return NULL;
    }

    cJSON* error = cJSON_CreateObject();
    if (!error) {
        cJSON_Delete(response);
        if (data) cJSON_Delete(data);
        return NULL;
    }

    /* 添加错误码 */
    cJSON_AddNumberToObject(error, "code", code);

    /* 添加错误消息 */
    const char* msg = message;
    if (!msg) {
        msg = jsonrpc_get_error_message(code);
    }
    cJSON_AddStringToObject(error, "message", msg ? msg : "Internal error");

    /* 添加错误数据（可选） */
    if (data) {
        cJSON_AddItemToObject(error, "data", data);
    }

    /* 构建响应 */
    cJSON_AddStringToObject(response, "jsonrpc", "2.0");
    cJSON_AddItemToObject(response, "error", error);

    /* 添加 ID */
    if (id) {
        cJSON_AddItemToObject(response, "id", cJSON_Duplicate(id, 1));
    } else {
        cJSON_AddNullToObject(response, "id");
    }

    char* json_str = cJSON_PrintUnformatted(response);
    cJSON_Delete(response);

    return json_str;
#else
    (void)id;
    (void)code;
    (void)message;
    (void)data;
    return NULL;
#endif
}

/* ==================== 便捷响应函数 ==================== */

char* jsonrpc_create_parse_error_response(void) {
    return jsonrpc_create_error_response(NULL, JSONRPC_PARSE_ERROR, NULL, NULL);
}

char* jsonrpc_create_invalid_request_response(void) {
    return jsonrpc_create_error_response(NULL, JSONRPC_INVALID_REQUEST, NULL, NULL);
}

char* jsonrpc_create_method_not_found_response(const cJSON* id) {
    return jsonrpc_create_error_response(id, JSONRPC_METHOD_NOT_FOUND, NULL, NULL);
}

char* jsonrpc_create_invalid_params_response(const cJSON* id, const char* detail) {
#ifdef GATEWAY_HAS_CJSON
    cJSON* data = NULL;
    if (detail) {
        data = cJSON_CreateString(detail);
    }
    return jsonrpc_create_error_response(id, JSONRPC_INVALID_PARAMS, NULL, data);
#else
    (void)id;
    (void)detail;
    return NULL;
#endif
}

char* jsonrpc_create_internal_error_response(const cJSON* id, const char* detail) {
#ifdef GATEWAY_HAS_CJSON
    cJSON* data = NULL;
    if (detail) {
        data = cJSON_CreateString(detail);
    }
    return jsonrpc_create_error_response(id, JSONRPC_INTERNAL_ERROR, NULL, data);
#else
    (void)id;
    (void)detail;
    return NULL;
#endif
}

char* jsonrpc_create_rate_limited_response(const cJSON* id) {
    return jsonrpc_create_error_response(id, JSONRPC_RATE_LIMITED, NULL, NULL);
}

char* jsonrpc_create_auth_failed_response(const cJSON* id) {
    return jsonrpc_create_error_response(id, JSONRPC_AUTH_FAILED, NULL, NULL);
}

/* ==================== 错误消息获取 ==================== */

const char* jsonrpc_get_error_message(int code) {
    /* 标准错误码 */
    if (code >= -32700 && code <= -32600) {
        int idx = code + 32700;
        if (idx >= 0 && idx < (int)(sizeof(g_error_messages) / sizeof(g_error_messages[0]))) {
            return g_error_messages[idx];
        }
    }

    /* 自定义错误码 */
    if (code >= -32099 && code <= -32000) {
        int idx = code + 32001;
        if (idx >= 0 && idx < (int)(sizeof(g_custom_error_messages) / sizeof(g_custom_error_messages[0]))) {
            const char* msg = g_custom_error_messages[idx];
            if (msg) {
                return msg;
            }
        }
    }

    return "Unknown error";
}

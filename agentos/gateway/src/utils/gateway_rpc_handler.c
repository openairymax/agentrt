/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file gateway_rpc_handler.c
 * @brief 统一的RPC请求处理模块实现
 *
 * 实现HTTP/WS/Stdio三种网关共享的RPC处理逻辑。
 * 圈复杂度控制在7以下，确保高可维护性。
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "gateway_rpc_handler.h"
#include "jsonrpc.h"
#include "syscall_router.h"

#include <stdlib.h>
#include <string.h>

/* ========== 内部辅助函数 ========== */

/**
 * @brief 验证JSON-RPC请求格式 (CC=3)
 */
static int validate_rpc_request(const cJSON* request) {
    if (!request || !cJSON_IsObject(request)) return -1;

    const cJSON* jsonrpc = cJSON_GetObjectItem(request, "jsonrpc");
    if (!jsonrpc || !cJSON_IsString(jsonrpc)) return -1;
    if (strcmp(jsonrpc->valuestring, "2.0") != 0) return -1;

    const cJSON* method = cJSON_GetObjectItem(request, "method");
    if (!method || !cJSON_IsString(method)) return -1;

    const cJSON* id = cJSON_GetObjectItem(request, "id");
    if (!id && id->type != cJSON_NULL) return -1;

    return 0;  /* 有效请求 */
}

/**
 * @brief 提取请求字段 (CC=4)
 */
static int extract_request_fields(const cJSON* request,
                                  const char** method_out,
                                  const cJSON** params_out,
                                  const cJSON** id_out) {
    if (!request || !method_out || !params_out || !id_out) return -1;

    *method_out = NULL;
    *params_out = NULL;
    *id_out = NULL;

    const cJSON* method = cJSON_GetObjectItem(request, "method");
    if (!method || !cJSON_IsString(method)) return -1;
    *method_out = method->valuestring;

    *params_out = cJSON_GetObjectItem(request, "params");

    *id_out = cJSON_GetObjectItem(request, "id");

    return 0;
}

/* ========== 公共接口实现 ========== */

rpc_result_t gateway_rpc_handle_request(const cJSON* request,
                                       int (*handler)(const char*, char**, void*),
                                       void* handler_data) {
    rpc_result_t result = {NULL, 0, NULL};

    /* 1. 参数验证 (CC=1) */
    if (!request) {
        result = gateway_rpc_create_error(-32600, "Invalid request: NULL");
        return result;
    }

    /* 2. 请求格式验证 (CC=1) */
    if (validate_rpc_request(request) != 0) {
        result = gateway_rpc_create_error(-32600, "Invalid Request");
        return result;
    }

    /* 3. 提取字段 (CC=1) */
    const char* method = NULL;
    const cJSON* params = NULL;
    const cJSON* id = NULL;

    if (extract_request_fields(request, &method, &params, &id) != 0) {
        result = gateway_rpc_create_error(-32600, "Missing required fields");
        return result;
    }

    /* 4. 调用handler或默认路由 (CC=2) */
    char* response_str = NULL;

    if (handler) {
        /* 自定义handler优先 */
        char* request_str = cJSON_PrintUnformatted((cJSON*)request);
        if (!request_str) {
            result = gateway_rpc_create_error(-32000, "Memory allocation failed");
            return result;
        }

        int ret = handler(request_str, &response_str, handler_data);
        free(request_str);

        if (ret != 0 || !response_str) {
            result = gateway_rpc_create_error(-32000, "Handler error");
            return result;
        }
    } else {
        /* 默认：路由到syscall */
        response_str = gateway_syscall_route(method, (cJSON*)params, (cJSON*)id);

        if (!response_str) {
            result = gateway_rpc_create_error(-32000, "Internal error");
            return result;
        }
    }

    /* 5. 构建结果 (CC=1) */
    result.response_json = response_str;
    result.error_code = 0;
    result.error_message = NULL;

    return result;
}

rpc_result_t gateway_rpc_create_error(int code, const char* message) {
    rpc_result_t result = {NULL, 0, NULL};

    result.response_json = jsonrpc_create_error_response(
        NULL, code, message ? message : "Unknown error", NULL);

    if (result.response_json) {
        result.error_code = code;
        result.error_message = message;
    } else {
        result.error_code = -32700;  /* 内存分配失败 */
        result.error_message = "Failed to create error response";
    }

    return result;
}

void gateway_rpc_free(rpc_result_t* result) {
    if (!result) return;

    if (result->response_json) {
        free(result->response_json);
        result->response_json = NULL;
    }

    result->error_code = 0;
    result->error_message = NULL;
}

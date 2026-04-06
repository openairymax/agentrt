/**
 * @file param_validator.c
 * @brief JSON-RPC 参数验证工具实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "param_validator.h"
#include <stdarg.h>
#include <string.h>

/**
 * @brief 验证必需字段是否存在
 */
int validate_required_fields(cJSON* obj, ...) {
    if (!obj || !cJSON_IsObject(obj)) {
        return -1;
    }
    
    va_list args;
    va_start(args, obj);
    
    const char* field_name;
    while ((field_name = va_arg(args, const char*)) != NULL) {
        cJSON* field = cJSON_GetObjectItem(obj, field_name);
        if (!field) {
            va_end(args);
            return -1;
        }
    }
    
    va_end(args);
    return 0;
}

/**
 * @brief 验证 JSON-RPC 请求的基本结构
 */
int validate_jsonrpc_request(cJSON* req, 
                             cJSON** jsonrpc, 
                             cJSON** method, 
                             cJSON** params, 
                             cJSON** id) {
    if (!req || !cJSON_IsObject(req)) {
        return -1;
    }
    
    *jsonrpc = cJSON_GetObjectItem(req, "jsonrpc");
    *method = cJSON_GetObjectItem(req, "method");
    *params = cJSON_GetObjectItem(req, "params");
    *id = cJSON_GetObjectItem(req, "id");
    
    /* 验证必需字段 */
    if (!cJSON_IsString(*jsonrpc) || 
        strcmp((*jsonrpc)->valuestring, "2.0") != 0 ||
        !cJSON_IsString(*method) || 
        !(*id)) {
        return -1;
    }
    
    return 0;
}

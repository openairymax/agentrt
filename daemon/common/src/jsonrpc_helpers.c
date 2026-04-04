/**
 * @file jsonrpc_helpers.c
 * @brief JSON-RPC 2.0 公共辅助函数库实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * @version 1.0.0
 * @date 2026-04-04
 */

#include "jsonrpc_helpers.h"
#include "svc_logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ==================== 响应构建函数 ==================== */

char* jsonrpc_build_error(int code, const char* message, int id) {
    if (!message) {
        message = jsonrpc_get_error_message(code);
    }
    
    cJSON* root = cJSON_CreateObject();
    if (!root) {
        SVC_LOG_ERROR("Failed to create JSON object for error response");
        return NULL;
    }
    
    cJSON_AddStringToObject(root, "jsonrpc", "2.0");
    cJSON_AddNumberToObject(root, "id", id);
    
    cJSON* error = cJSON_CreateObject();
    if (!error) {
        cJSON_Delete(root);
        SVC_LOG_ERROR("Failed to create error object");
        return NULL;
    }
    
    cJSON_AddNumberToObject(error, "code", code);
    cJSON_AddStringToObject(error, "message", message);
    cJSON_AddItemToObject(root, "error", error);
    
    char* result = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    if (!result) {
        SVC_LOG_ERROR("Failed to print JSON error response");
        return NULL;
    }
    
    SVC_LOG_DEBUG("Built JSON-RPC error response: code=%d, message=%s", code, message);
    return result;
}

char* jsonrpc_build_success(cJSON* result, int id) {
    cJSON* root = cJSON_CreateObject();
    if (!root) {
        SVC_LOG_ERROR("Failed to create JSON object for success response");
        if (result) cJSON_Delete(result);
        return NULL;
    }
    
    cJSON_AddStringToObject(root, "jsonrpc", "2.0");
    cJSON_AddNumberToObject(root, "id", id);
    
    if (result) {
        cJSON_AddItemToObject(root, "result", result);
    } else {
        cJSON_AddNullToObject(root, "result");
    }
    
    char* resp = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    if (!resp) {
        SVC_LOG_ERROR("Failed to print JSON success response");
        return NULL;
    }
    
    SVC_LOG_DEBUG("Built JSON-RPC success response: id=%d", id);
    return resp;
}

char* jsonrpc_build_success_string(const char* result_str, int id) {
    cJSON* result = cJSON_CreateString(result_str ? result_str : "");
    if (!result) {
        SVC_LOG_ERROR("Failed to create result string");
        return NULL;
    }
    
    return jsonrpc_build_success(result, id);
}

/* ==================== 请求解析函数 ==================== */

int jsonrpc_parse_request(const char* raw, 
                           char** out_method, 
                           cJSON** out_params, 
                           int* out_id) {
    if (!raw || !out_method || !out_params || !out_id) {
        SVC_LOG_ERROR("Invalid parameters for jsonrpc_parse_request");
        return -1;
    }
    
    *out_method = NULL;
    *out_params = NULL;
    *out_id = 0;
    
    cJSON* req = cJSON_Parse(raw);
    if (!req) {
        SVC_LOG_ERROR("Failed to parse JSON request: %s", raw);
        return JSONRPC_PARSE_ERROR;
    }
    
    if (jsonrpc_validate_request(req) != 0) {
        cJSON_Delete(req);
        return JSONRPC_INVALID_REQUEST;
    }
    
    cJSON* method_obj = cJSON_GetObjectItem(req, "method");
    if (method_obj && cJSON_IsString(method_obj)) {
        *out_method = strdup(method_obj->valuestring);
        if (!*out_method) {
            SVC_LOG_ERROR("Failed to duplicate method name");
            cJSON_Delete(req);
            return JSONRPC_INTERNAL_ERROR;
        }
    }
    
    cJSON* params_obj = cJSON_GetObjectItem(req, "params");
    if (params_obj) {
        *out_params = cJSON_Duplicate(params_obj, 1);
        if (!*out_params) {
            SVC_LOG_ERROR("Failed to duplicate params");
            free(*out_method);
            *out_method = NULL;
            cJSON_Delete(req);
            return JSONRPC_INTERNAL_ERROR;
        }
    }
    
    cJSON* id_obj = cJSON_GetObjectItem(req, "id");
    if (id_obj) {
        if (cJSON_IsNumber(id_obj)) {
            *out_id = id_obj->valueint;
        } else if (cJSON_IsString(id_obj)) {
            *out_id = atoi(id_obj->valuestring);
        }
    }
    
    cJSON_Delete(req);
    
    SVC_LOG_DEBUG("Parsed JSON-RPC request: method=%s, id=%d", 
                  *out_method ? *out_method : "NULL", *out_id);
    
    return 0;
}

int jsonrpc_validate_request(cJSON* req) {
    if (!req) {
        return -1;
    }
    
    cJSON* jsonrpc = cJSON_GetObjectItem(req, "jsonrpc");
    if (!jsonrpc || !cJSON_IsString(jsonrpc) || 
        strcmp(jsonrpc->valuestring, "2.0") != 0) {
        SVC_LOG_WARN("Invalid or missing jsonrpc version");
        return -1;
    }
    
    cJSON* method = cJSON_GetObjectItem(req, "method");
    if (!method || !cJSON_IsString(method)) {
        SVC_LOG_WARN("Invalid or missing method");
        return -1;
    }
    
    return 0;
}

/* ==================== 参数提取函数 ==================== */

const char* jsonrpc_get_string_param(cJSON* params, 
                                      const char* key, 
                                      const char* default_value) {
    if (!params || !key) {
        return default_value;
    }
    
    cJSON* item = cJSON_GetObjectItem(params, key);
    if (!item || !cJSON_IsString(item)) {
        return default_value;
    }
    
    return item->valuestring;
}

int jsonrpc_get_int_param(cJSON* params, const char* key, int default_value) {
    if (!params || !key) {
        return default_value;
    }
    
    cJSON* item = cJSON_GetObjectItem(params, key);
    if (!item || !cJSON_IsNumber(item)) {
        return default_value;
    }
    
    return item->valueint;
}

int jsonrpc_get_bool_param(cJSON* params, const char* key, int default_value) {
    if (!params || !key) {
        return default_value;
    }
    
    cJSON* item = cJSON_GetObjectItem(params, key);
    if (!item || !cJSON_IsBool(item)) {
        return default_value;
    }
    
    return cJSON_IsTrue(item) ? 1 : 0;
}

cJSON* jsonrpc_get_array_param(cJSON* params, const char* key) {
    if (!params || !key) {
        return NULL;
    }
    
    cJSON* item = cJSON_GetObjectItem(params, key);
    if (!item || !cJSON_IsArray(item)) {
        return NULL;
    }
    
    return item;
}

cJSON* jsonrpc_get_object_param(cJSON* params, const char* key) {
    if (!params || !key) {
        return NULL;
    }
    
    cJSON* item = cJSON_GetObjectItem(params, key);
    if (!item || !cJSON_IsObject(item)) {
        return NULL;
    }
    
    return item;
}

/* ==================== 通知消息处理 ==================== */

int jsonrpc_is_notification(cJSON* req) {
    if (!req) {
        return 0;
    }
    
    cJSON* id = cJSON_GetObjectItem(req, "id");
    return (id == NULL) ? 1 : 0;
}

char* jsonrpc_build_notification(const char* method, cJSON* params) {
    if (!method) {
        SVC_LOG_ERROR("Method is required for notification");
        if (params) cJSON_Delete(params);
        return NULL;
    }
    
    cJSON* root = cJSON_CreateObject();
    if (!root) {
        SVC_LOG_ERROR("Failed to create JSON object for notification");
        if (params) cJSON_Delete(params);
        return NULL;
    }
    
    cJSON_AddStringToObject(root, "jsonrpc", "2.0");
    cJSON_AddStringToObject(root, "method", method);
    
    if (params) {
        cJSON_AddItemToObject(root, "params", params);
    }
    
    char* resp = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    if (!resp) {
        SVC_LOG_ERROR("Failed to print JSON notification");
        return NULL;
    }
    
    SVC_LOG_DEBUG("Built JSON-RPC notification: method=%s", method);
    return resp;
}

/* ==================== 批量请求支持 ==================== */

int jsonrpc_is_batch_request(const char* raw) {
    if (!raw) {
        return 0;
    }
    
    while (*raw && (*raw == ' ' || *raw == '\t' || *raw == '\n' || *raw == '\r')) {
        raw++;
    }
    
    return (*raw == '[') ? 1 : 0;
}

int jsonrpc_parse_batch(const char* raw, cJSON** out_requests) {
    if (!raw || !out_requests) {
        return -1;
    }
    
    *out_requests = NULL;
    
    cJSON* batch = cJSON_Parse(raw);
    if (!batch) {
        SVC_LOG_ERROR("Failed to parse batch request");
        return -1;
    }
    
    if (!cJSON_IsArray(batch)) {
        cJSON_Delete(batch);
        SVC_LOG_ERROR("Batch request is not an array");
        return -1;
    }
    
    int count = cJSON_GetArraySize(batch);
    *out_requests = batch;
    
    SVC_LOG_DEBUG("Parsed batch request with %d items", count);
    return count;
}

char* jsonrpc_build_batch_response(char** responses, size_t count) {
    if (!responses || count == 0) {
        return NULL;
    }
    
    cJSON* batch = cJSON_CreateArray();
    if (!batch) {
        SVC_LOG_ERROR("Failed to create batch response array");
        return NULL;
    }
    
    for (size_t i = 0; i < count; i++) {
        if (responses[i]) {
            cJSON* item = cJSON_Parse(responses[i]);
            if (item) {
                cJSON_AddItemToArray(batch, item);
            }
            free(responses[i]);
        }
    }
    
    char* result = cJSON_PrintUnformatted(batch);
    cJSON_Delete(batch);
    
    if (!result) {
        SVC_LOG_ERROR("Failed to print batch response");
        return NULL;
    }
    
    SVC_LOG_DEBUG("Built batch response with %zu items", count);
    return result;
}

/* ==================== 错误消息辅助 ==================== */

const char* jsonrpc_get_error_message(int code) {
    switch (code) {
        case JSONRPC_PARSE_ERROR:
            return "Parse error";
        case JSONRPC_INVALID_REQUEST:
            return "Invalid request";
        case JSONRPC_METHOD_NOT_FOUND:
            return "Method not found";
        case JSONRPC_INVALID_PARAMS:
            return "Invalid params";
        case JSONRPC_INTERNAL_ERROR:
            return "Internal error";
        default:
            if (code >= -32099 && code <= -32000) {
                return "Server error";
            }
            return "Unknown error";
    }
}

char* jsonrpc_build_error_with_data(int code, 
                                     const char* message, 
                                     cJSON* data, 
                                     int id) {
    if (!message) {
        message = jsonrpc_get_error_message(code);
    }
    
    cJSON* root = cJSON_CreateObject();
    if (!root) {
        SVC_LOG_ERROR("Failed to create JSON object for error with data");
        if (data) cJSON_Delete(data);
        return NULL;
    }
    
    cJSON_AddStringToObject(root, "jsonrpc", "2.0");
    cJSON_AddNumberToObject(root, "id", id);
    
    cJSON* error = cJSON_CreateObject();
    if (!error) {
        cJSON_Delete(root);
        if (data) cJSON_Delete(data);
        SVC_LOG_ERROR("Failed to create error object");
        return NULL;
    }
    
    cJSON_AddNumberToObject(error, "code", code);
    cJSON_AddStringToObject(error, "message", message);
    
    if (data) {
        cJSON_AddItemToObject(error, "data", data);
    }
    
    cJSON_AddItemToObject(root, "error", error);
    
    char* result = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    if (!result) {
        SVC_LOG_ERROR("Failed to print JSON error response with data");
        return NULL;
    }
    
    SVC_LOG_DEBUG("Built JSON-RPC error response with data: code=%d", code);
    return result;
}

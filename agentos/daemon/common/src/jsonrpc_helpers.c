/**
 * @file jsonrpc_helpers.c
 * @brief JSON-RPC 2.0 公共辅助函数实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "jsonrpc_helpers.h"
#include "svc_logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

char* jsonrpc_build_error(int code, const char* message, int id) {
    if (!message) message = jsonrpc_get_error_message(code);
    
    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;
    
    cJSON_AddStringToObject(root, "jsonrpc", "2.0");
    cJSON_AddNumberToObject(root, "id", id);
    
    cJSON* error = cJSON_CreateObject();
    cJSON_AddNumberToObject(error, "code", code);
    cJSON_AddStringToObject(error, "message", message);
    cJSON_AddItemToObject(root, "error", error);
    
    char* result = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return result;
}

char* jsonrpc_build_success(cJSON* result, int id) {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "jsonrpc", "2.0");
    cJSON_AddNumberToObject(root, "id", id);
    
    if (result) {
        cJSON_AddItemToObject(root, "result", result);
    } else {
        cJSON_AddNullToObject(root, "result");
    }
    
    char* resp = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return resp;
}

char* jsonrpc_build_success_string(const char* result_str, int id) {
    cJSON* result = cJSON_CreateString(result_str ? result_str : "");
    return jsonrpc_build_success(result, id);
}

int jsonrpc_parse_request(const char* raw, char** out_method, cJSON** out_params, int* out_id) {
    if (!raw || !out_method || !out_params || !out_id) return -1;
    
    *out_method = NULL;
    *out_params = NULL;
    *out_id = 0;
    
    cJSON* req = cJSON_Parse(raw);
    if (!req) return JSONRPC_PARSE_ERROR;
    
    if (jsonrpc_validate_request(req) != 0) {
        cJSON_Delete(req);
        return JSONRPC_INVALID_REQUEST;
    }
    
    cJSON* method_obj = cJSON_GetObjectItem(req, "method");
    if (method_obj && cJSON_IsString(method_obj)) {
        *out_method = strdup(method_obj->valuestring);
    }
    
    cJSON* params_obj = cJSON_GetObjectItem(req, "params");
    if (params_obj) {
        *out_params = cJSON_Duplicate(params_obj, 1);
    }
    
    cJSON* id_obj = cJSON_GetObjectItem(req, "id");
    if (id_obj && cJSON_IsNumber(id_obj)) {
        *out_id = id_obj->valueint;
    }
    
    cJSON_Delete(req);
    return 0;
}

int jsonrpc_parse_request_ptr(cJSON* req, char** out_method, cJSON** out_params, int* out_id) {
    if (!req || !out_method || !out_params || !out_id) return -1;
    
    *out_method = NULL;
    *out_params = NULL;
    *out_id = 0;
    
    if (jsonrpc_validate_request(req) != 0) return JSONRPC_INVALID_REQUEST;
    
    cJSON* method_obj = cJSON_GetObjectItem(req, "method");
    if (method_obj && cJSON_IsString(method_obj)) {
        *out_method = strdup(method_obj->valuestring);
    }
    
    cJSON* params_obj = cJSON_GetObjectItem(req, "params");
    if (params_obj) {
        *out_params = cJSON_Duplicate(params_obj, 1);
    }
    
    cJSON* id_obj = cJSON_GetObjectItem(req, "id");
    if (id_obj && cJSON_IsNumber(id_obj)) {
        *out_id = id_obj->valueint;
    }
    
    return 0;
}

int jsonrpc_validate_request(cJSON* req) {
    if (!req) return -1;
    
    cJSON* jsonrpc = cJSON_GetObjectItem(req, "jsonrpc");
    if (!jsonrpc || !cJSON_IsString(jsonrpc) || strcmp(jsonrpc->valuestring, "2.0") != 0) return -1;
    
    cJSON* method = cJSON_GetObjectItem(req, "method");
    if (!method || !cJSON_IsString(method)) return -1;
    
    return 0;
}

const char* jsonrpc_get_string_param(cJSON* params, const char* key, const char* default_value) {
    if (!params || !key) return default_value;
    cJSON* item = cJSON_GetObjectItem(params, key);
    if (!item || !cJSON_IsString(item)) return default_value;
    return item->valuestring;
}

int jsonrpc_get_int_param(cJSON* params, const char* key, int default_value) {
    if (!params || !key) return default_value;
    cJSON* item = cJSON_GetObjectItem(params, key);
    if (!item || !cJSON_IsNumber(item)) return default_value;
    return item->valueint;
}

int jsonrpc_get_bool_param(cJSON* params, const char* key, int default_value) {
    if (!params || !key) return default_value;
    cJSON* item = cJSON_GetObjectItem(params, key);
    if (!item || !cJSON_IsBool(item)) return default_value;
    return cJSON_IsTrue(item) ? 1 : 0;
}

cJSON* jsonrpc_get_array_param(cJSON* params, const char* key) {
    if (!params || !key) return NULL;
    cJSON* item = cJSON_GetObjectItem(params, key);
    if (!item || !cJSON_IsArray(item)) return NULL;
    return item;
}

cJSON* jsonrpc_get_object_param(cJSON* params, const char* key) {
    if (!params || !key) return NULL;
    cJSON* item = cJSON_GetObjectItem(params, key);
    if (!item || !cJSON_IsObject(item)) return NULL;
    return item;
}

int jsonrpc_is_notification(cJSON* req) {
    if (!req) return 0;
    cJSON* id = cJSON_GetObjectItem(req, "id");
    return (id == NULL) ? 1 : 0;
}

char* jsonrpc_build_notification(const char* method, cJSON* params) {
    if (!method) {
        if (params) cJSON_Delete(params);
        return NULL;
    }
    
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "jsonrpc", "2.0");
    cJSON_AddStringToObject(root, "method", method);
    if (params) cJSON_AddItemToObject(root, "params", params);
    
    char* resp = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return resp;
}

const char* jsonrpc_get_error_message(int code) {
    switch (code) {
        case JSONRPC_PARSE_ERROR: return "Parse error";
        case JSONRPC_INVALID_REQUEST: return "Invalid request";
        case JSONRPC_METHOD_NOT_FOUND: return "Method not found";
        case JSONRPC_INVALID_PARAMS: return "Invalid params";
        case JSONRPC_INTERNAL_ERROR: return "Internal error";
        default: return "Unknown error";
    }
}

char* jsonrpc_build_error_with_data(int code, const char* message, cJSON* data, int id) {
    if (!message) message = jsonrpc_get_error_message(code);
    
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "jsonrpc", "2.0");
    cJSON_AddNumberToObject(root, "id", id);
    
    cJSON* error = cJSON_CreateObject();
    cJSON_AddNumberToObject(error, "code", code);
    cJSON_AddStringToObject(error, "message", message);
    if (data) cJSON_AddItemToObject(error, "data", data);
    cJSON_AddItemToObject(root, "error", error);
    
    char* result = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return result;
}

int jsonrpc_is_batch_request(const char* raw) {
    if (!raw) return 0;
    
    const char* p = raw;
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
        p++;
    }
    
    return (*p == '[') ? 1 : 0;
}

/**
 * @file jsonrpc_helpers.h
 * @brief JSON-RPC 2.0 公共辅助函数库
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * @version 2.0.0
 * @date 2026-04-04
 */

#ifndef AGENTOS_JSONRPC_HELPERS_H
#define AGENTOS_JSONRPC_HELPERS_H

#include <cjson/cJSON.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define JSONRPC_PARSE_ERROR      -32700
#define JSONRPC_INVALID_REQUEST  -32600
#define JSONRPC_METHOD_NOT_FOUND -32601
#define JSONRPC_INVALID_PARAMS   -32602
#define JSONRPC_INTERNAL_ERROR   -32000

AGENTOS_API char* jsonrpc_build_error(int code, const char* message, int id);
AGENTOS_API char* jsonrpc_build_success(cJSON* result, int id);
AGENTOS_API char* jsonrpc_build_success_string(const char* result_str, int id);
AGENTOS_API int jsonrpc_parse_request(const char* raw, char** out_method, cJSON** out_params, int* out_id);
AGENTOS_API int jsonrpc_parse_request_ptr(cJSON* req, char** out_method, cJSON** out_params, int* out_id);
AGENTOS_API int jsonrpc_validate_request(cJSON* req);
AGENTOS_API const char* jsonrpc_get_string_param(cJSON* params, const char* key, const char* default_value);
AGENTOS_API int jsonrpc_get_int_param(cJSON* params, const char* key, int default_value);
AGENTOS_API int jsonrpc_get_bool_param(cJSON* params, const char* key, int default_value);
AGENTOS_API cJSON* jsonrpc_get_array_param(cJSON* params, const char* key);
AGENTOS_API cJSON* jsonrpc_get_object_param(cJSON* params, const char* key);
AGENTOS_API int jsonrpc_is_notification(cJSON* req);
AGENTOS_API char* jsonrpc_build_notification(const char* method, cJSON* params);
AGENTOS_API const char* jsonrpc_get_error_message(int code);
AGENTOS_API char* jsonrpc_build_error_with_data(int code, const char* message, cJSON* data, int id);
AGENTOS_API int jsonrpc_is_batch_request(const char* raw);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_JSONRPC_HELPERS_H */

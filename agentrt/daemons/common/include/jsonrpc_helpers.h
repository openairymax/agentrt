/**
 * @file jsonrpc_helpers.h
 * @brief JSON-RPC 2.0 公共辅助函数库
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * @version 0.1.0
 * @date 2026-04-04
 */

#ifndef AGENTRT_JSONRPC_HELPERS_H
#define AGENTRT_JSONRPC_HELPERS_H

#include <cjson/cJSON.h>
#include <stddef.h>
#include <stdint.h>

#include "error.h"

#ifndef AGENTRT_API
#if defined(_WIN32) && defined(AGENTRT_BUILD_DLL)
#define AGENTRT_API __declspec(dllexport)
#elif defined(_WIN32)
#define AGENTRT_API __declspec(dllimport)
#else
#define AGENTRT_API __attribute__((visibility("default")))
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define JSONRPC_PARSE_ERROR -32700
#define JSONRPC_INVALID_REQUEST -32600
#define JSONRPC_METHOD_NOT_FOUND -32601
#define JSONRPC_INVALID_PARAMS -32602
#define JSONRPC_INTERNAL_ERROR -32000

AGENTRT_API char *jsonrpc_build_error(int code, const char *message, int id);
AGENTRT_API char *jsonrpc_build_success(cJSON *result, int id);
AGENTRT_API char *jsonrpc_build_success_string(const char *result_str, int id);
AGENTRT_API int jsonrpc_parse_request(const char *raw, char **out_method, cJSON **out_params,
                                      int *out_id);
AGENTRT_API int jsonrpc_parse_request_ptr(cJSON *req, char **out_method, cJSON **out_params,
                                          int *out_id);
AGENTRT_API int jsonrpc_validate_request(cJSON *req);
AGENTRT_API const char *jsonrpc_get_string_param(cJSON *params, const char *key,
                                                 const char *default_value);
AGENTRT_API int jsonrpc_get_int_param(cJSON *params, const char *key, int default_value);
AGENTRT_API int jsonrpc_get_bool_param(cJSON *params, const char *key, int default_value);
AGENTRT_API cJSON *jsonrpc_get_array_param(cJSON *params, const char *key);
AGENTRT_API cJSON *jsonrpc_get_object_param(cJSON *params, const char *key);
AGENTRT_API int jsonrpc_is_notification(cJSON *req);
AGENTRT_API char *jsonrpc_build_notification(const char *method, cJSON *params);
AGENTRT_API const char *jsonrpc_get_error_message(int code);
AGENTRT_API char *jsonrpc_build_error_with_data(int code, const char *message, cJSON *data, int id);
AGENTRT_API int jsonrpc_is_batch_request(const char *raw);

/* ==================== 响应发送辅助宏（消除重复代码） ==================== */

/**
 * @brief 发送 JSON-RPC 错误响应到客户端（自动构建+发送+释放）
 * @param socket 客户端 socket 描述符
 * @param error_code 错误码
 * @param message 错误消息
 * @param id 请求 ID
 * @note 替代手动: build_error → send → free 三行组合
 */
#define JSONRPC_SEND_ERROR(socket, error_code, message, id)              \
    do {                                                                 \
        char *_err = jsonrpc_build_error((error_code), (message), (id)); \
        if (_err) {                                                      \
            agentrt_socket_send((socket), _err, strlen(_err));           \
            AGENTRT_FREE(_err);                                          \
        }                                                                \
    } while (0)

/**
 * @brief 发送 JSON-RPC 成功响应到客户端（自动构建+发送+释放）
 * @param socket 客户端 socket 描述符
 * @param result cJSON 结果对象（函数会自动 Delete）
 * @param id 请求 ID
 * @note 替代手动: build_success → send → delete → free 四行组合
 */
#define JSONRPC_SEND_SUCCESS(socket, result, id)                       \
    do {                                                               \
        char *_success = jsonrpc_build_success((result), (id));        \
        cJSON_Delete((result));                                        \
        if (_success) {                                                \
            agentrt_socket_send((socket), _success, strlen(_success)); \
            AGENTRT_FREE(_success);                                    \
        }                                                              \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_JSONRPC_HELPERS_H */

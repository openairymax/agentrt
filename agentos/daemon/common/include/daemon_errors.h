/**
 * @file daemon_errors.h
 * @brief 守护进程统一错误码定义
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * 统一所有守护进程的 JSON-RPC 错误码和服务特有错误码，
 * 消除各 daemon main.c 中的重复定义。
 *
 * 使用方法:
 *   #include "daemon_errors.h"
 *
 * @note 所有守护进程应包含此文件，移除重复的 #define 定义
 */

#ifndef AGENTOS_DAEMON_ERRORS_H
#define AGENTOS_DAEMON_ERRORS_H

#include <stdint.h>
#include <stdbool.h>
#include "../../../commons/utils/error/include/error.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef AGENTOS_EINIT
#define AGENTOS_EINIT    (-100)
#endif

#ifndef AGENTOS_ESTATE
#define AGENTOS_ESTATE   (-101)
#endif

#ifndef AGENTOS_EHEALTH
#define AGENTOS_EHEALTH  (-102)
#endif

#ifndef AGENTOS_EDEPEND
#define AGENTOS_EDEPEND  (-103)
#endif

#ifndef AGENTOS_EFAIL
#define AGENTOS_EFAIL   (-104)
#endif

#ifndef CUPOLAS_ERR_INVALID_PARAM
#define CUPOLAS_ERR_INVALID_PARAM (-2001)
#endif

#ifndef AGENTOS_ERR_NOT_SUPPORTED
#define AGENTOS_ERR_NOT_SUPPORTED  (-105)
#endif

#ifndef AGENTOS_ERR_PERMISSION_DENIED
#define AGENTOS_ERR_PERMISSION_DENIED  (-10)
#endif

#ifndef AGENTOS_ERR_ALREADY_EXISTS
#define AGENTOS_ERR_ALREADY_EXISTS     (-11)
#endif

#ifndef AGENTOS_OK
#define AGENTOS_OK                      0
#endif

#ifndef AGENTOS_ERR_NOT_FOUND
#define AGENTOS_ERR_NOT_FOUND           (-6)
#endif

#ifndef AGENTOS_ERR_STATE_ERROR
#define AGENTOS_ERR_STATE_ERROR         (-13)
#endif

#ifndef AGENTOS_ERR_OVERFLOW
#define AGENTOS_ERR_OVERFLOW            (-14)
#endif

#ifndef AGENTOS_ERR_PARSE_ERROR
#define AGENTOS_ERR_PARSE_ERROR          (-16)
#endif

#ifndef AGENTOS_ERR_UNKNOWN
#define AGENTOS_ERR_UNKNOWN             (-17)
#endif

/**
 * @brief JSON-RPC 2.0 标准错误码
 * @{
 */

/**
 * @brief Parse error - 无效的 JSON
 * @details 当 JSON 解析失败时返回
 */
#define DAEMON_JSONRPC_PARSE_ERROR     -32700

/**
 * @brief Invalid Request - 无效的请求
 * @details 当 JSON 是有效的但不是有效的请求对象时返回
 */
#define DAEMON_JSONRPC_INVALID_REQUEST -32600

/**
 * @brief Method not found - 方法未找到
 * @details 当请求的方法不存在时返回
 */
#define DAEMON_JSONRPC_METHOD_NOT_FOUND -32601

/**
 * @brief Invalid params - 无效的参数
 * @details 当参数无效时返回
 */
#define DAEMON_JSONRPC_INVALID_PARAMS  -32602

/**
 * @brief Internal error - 内部错误
 * @details 当服务器内部错误时返回
 */
#define DAEMON_JSONRPC_INTERNAL_ERROR   -32603

/** @} */

/**
 * @brief 守护进程特有错误码基准值
 */
#define DAEMON_ERR_BASE                -32000

/**
 * @brief 错误码定义宏
 * @param code 相对基准值的偏移
 */
#define DAEMON_ERR(code) ((int32_t)(DAEMON_ERR_BASE - (code)))

/**
 * @brief 守护进程特有错误码
 * @{
 */

/**
 * @brief 配置无效
 */
#define DAEMON_ERR_INVALID_CONFIG     DAEMON_ERR(1)

/**
 * @brief 服务不可用
 */
#define DAEMON_ERR_SERVICE_UNAVAILABLE DAEMON_ERR(2)

/**
 * @brief 操作超时
 */
#define DAEMON_ERR_TIMEOUT            DAEMON_ERR(3)

/**
 * @brief 资源耗尽
 */
#define DAEMON_ERR_RESOURCE_EXHAUSTED  DAEMON_ERR(4)

/**
 * @brief 认证失败
 */
#define DAEMON_ERR_AUTH_FAILED        DAEMON_ERR(5)

/**
 * @brief 权限不足
 */
#define DAEMON_ERR_PERMISSION_DENIED   DAEMON_ERR(6)

/**
 * @brief 资源不存在
 */
#define DAEMON_ERR_NOT_FOUND           DAEMON_ERR(7)

/**
 * @brief 资源已存在
 */
#define DAEMON_ERR_ALREADY_EXISTS      DAEMON_ERR(8)

/**
 * @brief 请求过大
 */
#define DAEMON_ERR_REQUEST_TOO_LARGE   DAEMON_ERR(9)

/**
 * @brief 服务器忙
 */
#define DAEMON_ERR_SERVER_BUSY         DAEMON_ERR(10)

/**
 * @brief 方法不支持
 */
#define DAEMON_ERR_METHOD_NOT_SUPPORTED DAEMON_ERR(11)

/**
 * @brief 服务未初始化
 */
#define DAEMON_ERR_NOT_INITIALIZED     DAEMON_ERR(12)

/**
 * @brief 缓存未命中
 */
#define DAEMON_ERR_CACHE_MISS         DAEMON_ERR(13)

/**
 * @brief 速率限制
 */
#define DAEMON_ERR_RATE_LIMITED        DAEMON_ERR(14)

/** @} */

/**
 * @brief 错误码转字符串
 * @param error_code 错误码
 * @return 错误描述字符串
 */
static inline const char* daemon_strerror(int error_code) {
    switch (error_code) {
        case DAEMON_JSONRPC_PARSE_ERROR:
            return "Parse error";
        case DAEMON_JSONRPC_INVALID_REQUEST:
            return "Invalid Request";
        case DAEMON_JSONRPC_METHOD_NOT_FOUND:
            return "Method not found";
        case DAEMON_JSONRPC_INVALID_PARAMS:
            return "Invalid params";
        case DAEMON_JSONRPC_INTERNAL_ERROR:
            return "Internal error";
        case DAEMON_ERR_INVALID_CONFIG:
            return "Invalid configuration";
        case DAEMON_ERR_SERVICE_UNAVAILABLE:
            return "Service unavailable";
        case DAEMON_ERR_TIMEOUT:
            return "Operation timeout";
        case DAEMON_ERR_RESOURCE_EXHAUSTED:
            return "Resource exhausted";
        case DAEMON_ERR_AUTH_FAILED:
            return "Authentication failed";
        case DAEMON_ERR_PERMISSION_DENIED:
            return "Permission denied";
        case DAEMON_ERR_NOT_FOUND:
            return "Resource not found";
        case DAEMON_ERR_ALREADY_EXISTS:
            return "Resource already exists";
        case DAEMON_ERR_REQUEST_TOO_LARGE:
            return "Request too large";
        case DAEMON_ERR_SERVER_BUSY:
            return "Server busy";
        case DAEMON_ERR_METHOD_NOT_SUPPORTED:
            return "Method not supported";
        case DAEMON_ERR_NOT_INITIALIZED:
            return "Service not initialized";
        case DAEMON_ERR_CACHE_MISS:
            return "Cache miss";
        case DAEMON_ERR_RATE_LIMITED:
            return "Rate limited";
        default:
            return "Unknown error";
    }
}

/**
 * @brief 检查错误码是否为 JSON-RPC 标准错误
 * @param error_code 错误码
 * @return true 如果是 JSON-RPC 标准错误
 */
static inline bool daemon_is_jsonrpc_error(int error_code) {
    return (error_code >= DAEMON_JSONRPC_PARSE_ERROR &&
            error_code >= DAEMON_JSONRPC_INVALID_REQUEST &&
            error_code <= DAEMON_JSONRPC_INTERNAL_ERROR);
}

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_DAEMON_ERRORS_H */

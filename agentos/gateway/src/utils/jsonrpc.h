/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file jsonrpc.h
 * @brief JSON-RPC 2.0 协议工具函数
 *
 * 提供标准的 JSON-RPC 2.0 请求验证和响应生成功能
 * 供 HTTP 网关和 WebSocket 网关共同使用
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef GATEWAY_JSONRPC_H
#define GATEWAY_JSONRPC_H

#include <stddef.h>
#include <stdbool.h>

/* 前向声明 cJSON 类型 */
struct cJSON;
typedef struct cJSON cJSON;

/* ==================== JSON-RPC 2.0 标准错误码 ==================== */

#define JSONRPC_PARSE_ERROR       (-32700)  /**< 解析错误 */
#define JSONRPC_INVALID_REQUEST   (-32600)  /**< 无效请求 */
#define JSONRPC_METHOD_NOT_FOUND  (-32601)  /**< 方法未找到 */
#define JSONRPC_INVALID_PARAMS    (-32602)  /**< 无效参数 */
#define JSONRPC_INTERNAL_ERROR    (-32603)  /**< 内部错误 */

/* 自定义错误码范围：-32000 到 -32099 */
#define JSONRPC_SERVER_ERROR_BASE (-32000)  /**< 服务器错误基址 */
#define JSONRPC_RATE_LIMITED      (-32001)  /**< 请求被限流 */
#define JSONRPC_AUTH_FAILED       (-32002)  /**< 认证失败 */
#define JSONRPC_SESSION_EXPIRED   (-32003)  /**< 会话过期 */
#define JSONRPC_SERVICE_UNAVAILABLE (-32004) /**< 服务不可用 */

/* ==================== 请求验证 ==================== */

/**
 * @brief 验证 JSON-RPC 2.0 请求格式
 *
 * 检查请求是否包含必需字段（jsonrpc, method, id），
 * 并验证字段类型和值是否符合规范。
 *
 * @param[in] json JSON 请求对象
 * @return 0 有效
 * @return -1 无效（缺少必需字段）
 * @return -2 无效（字段类型错误）
 * @return -3 无效（jsonrpc 版本不是 "2.0"）
 */
int jsonrpc_validate_request(const cJSON* json);

/**
 * @brief 从 JSON-RPC 请求中提取方法名
 *
 * @param[in] json JSON 请求对象
 * @return 方法名字符串指针（不转让所有权）
 * @return NULL 如果请求无效或方法字段不存在
 */
const char* jsonrpc_get_method(const cJSON* json);

/**
 * @brief 从 JSON-RPC 请求中提取参数
 *
 * @param[in] json JSON 请求对象
 * @return 参数对象指针（不转让所有权）
 * @return NULL 如果没有参数
 */
const cJSON* jsonrpc_get_params(const cJSON* json);

/**
 * @brief 从 JSON-RPC 请求中提取 ID
 *
 * @param[in] json JSON 请求对象
 * @return ID 对象指针（不转让所有权）
 * @return NULL 如果 ID 字段不存在
 */
const cJSON* jsonrpc_get_id(const cJSON* json);

/* ==================== 响应生成 ==================== */

/**
 * @brief 创建 JSON-RPC 2.0 成功响应
 *
 * 响应格式：
 * {
 *   "jsonrpc": "2.0",
 *   "result": <result>,
 *   "id": <id>
 * }
 *
 * @param[in] id 请求 ID（可为 NULL）
 * @param[in] result 结果对象（可为 NULL，函数会创建 null）
 * @return JSON 字符串，需调用者 free()
 * @return NULL 内存分配失败
 *
 * @note result 对象的所有权会转移给响应
 * @note 如果 result 为 NULL，响应中会包含 null
 */
char* jsonrpc_create_success_response(const cJSON* id, cJSON* result);

/**
 * @brief 创建 JSON-RPC 2.0 错误响应
 *
 * 响应格式：
 * {
 *   "jsonrpc": "2.0",
 *   "error": {
 *     "code": <code>,
 *     "message": "<message>",
 *     "data": <data>  // 可选
 *   },
 *   "id": <id>
 * }
 *
 * @param[in] id 请求 ID（可为 NULL）
 * @param[in] code 错误码
 * @param[in] message 错误消息（可为 NULL，使用默认消息）
 * @param[in] data 错误数据（可为 NULL）
 * @return JSON 字符串，需调用者 free()
 * @return NULL 内存分配失败
 *
 * @note data 对象的所有权会转移给响应
 */
char* jsonrpc_create_error_response(
    const cJSON* id,
    int code,
    const char* message,
    cJSON* data
);

/* ==================== 便捷响应函数 ==================== */

/**
 * @brief 创建解析错误响应
 */
char* jsonrpc_create_parse_error_response(void);

/**
 * @brief 创建无效请求响应
 */
char* jsonrpc_create_invalid_request_response(void);

/**
 * @brief 创建方法未找到响应
 */
char* jsonrpc_create_method_not_found_response(const cJSON* id);

/**
 * @brief 创建无效参数响应
 */
char* jsonrpc_create_invalid_params_response(const cJSON* id, const char* detail);

/**
 * @brief 创建内部错误响应
 */
char* jsonrpc_create_internal_error_response(const cJSON* id, const char* detail);

/**
 * @brief 创建限流响应
 */
char* jsonrpc_create_rate_limited_response(const cJSON* id);

/**
 * @brief 创建认证失败响应
 */
char* jsonrpc_create_auth_failed_response(const cJSON* id);

/* ==================== 错误消息获取 ==================== */

/**
 * @brief 获取标准错误消息
 *
 * @param[in] code 错误码
 * @return 错误消息字符串
 */
const char* jsonrpc_get_error_message(int code);

/* ==================== Batch Requests (PROTO-004) ==================== */

#define JSONRPC_MAX_BATCH_SIZE 64

/**
 * @brief 验证并解析批量请求（JSON数组）
 *
 * 批量请求格式: [{req1}, {req2}, ...]
 * 每个子请求必须符合JSON-RPC 2.0规范。
 *
 * @param[in] batch_json JSON数组对象
 * @param[out] out_count 解析出的请求数量
 * @return 0 成功
 * @return -1 输入为NULL或非数组
 * @return -2 数组元素不是对象
 * @return -3 超过最大批量大小
 * @return -4 包含无效子请求（部分成功）
 */
int jsonrpc_validate_batch_request(const cJSON* batch_json, size_t* out_count);

/**
 * @brief 处理批量请求，返回响应数组
 *
 * 对每个子请求调用处理函数，收集所有响应。
 * 即使部分请求失败，也会返回其他成功的响应。
 *
 * @param[in] batch_json 批量请求数组
 * @param[in] handler 单请求处理回调
 * @param[in] user_data 用户数据传给handler
 * @return JSON响应数组字符串(需free)，失败返回NULL
 */
char* jsonrpc_process_batch(
    const cJSON* batch_json,
    char* (*handler)(const cJSON* request, void* user_data),
    void* user_data
);

/* ==================== Notifications (PROTO-004) ==================== */

/**
 * @brief 创建通知（无id字段的特殊请求）
 *
 * 通知格式:
 * {
 *   "jsonrpc": "2.0",
 *   "method": "<method>",
 *   "params": <params>
 * }
 *
 * 与普通请求的区别：没有"id"字段，服务器不应返回响应。
 *
 * @param[in] method 方法名
 * @param[in] params 参数对象（可为NULL）
 * @return JSON通知字符串(需free)，失败返回NULL
 */
char* jsonrpc_create_notification(const char* method, cJSON* params);

/**
 * @brief 验证是否为通知（无id字段）
 *
 * @param[in] json JSON对象
 * @return true 是通知
 * @return false 不是通知或有id字段
 */
bool jsonrpc_is_notification(const cJSON* json);

/**
 * @brief 创建参数化通知（便捷函数）
 *
 * @param[in] method 方法名
 * @param[in] params_json 参数JSON字符串
 * @return JSON通知字符串(需free)
 */
char* jsonrpc_create_notification_params(
    const char* method,
    const char* params_json
);

#endif /* GATEWAY_JSONRPC_H */

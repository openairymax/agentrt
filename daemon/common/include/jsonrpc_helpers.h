/**
 * @file jsonrpc_helpers.h
 * @brief JSON-RPC 2.0 公共辅助函数库
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * @version 1.0.0
 * @date 2026-04-04
 * 
 * 本模块提供统一的 JSON-RPC 2.0 请求/响应处理函数，
 * 消除各服务中的重复代码，遵循 DRY 原则。
 * 
 * @see ARCHITECTURAL_PRINCIPLES.md E-3 资源确定性原则
 * @see ARCHITECTURAL_PRINCIPLES.md A-1 简约至上原则
 */

#ifndef AGENTOS_JSONRPC_HELPERS_H
#define AGENTOS_JSONRPC_HELPERS_H

#include <cjson/cJSON.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== JSON-RPC 2.0 标准错误码 ==================== */

#define JSONRPC_PARSE_ERROR      -32700  /**< 解析错误 */
#define JSONRPC_INVALID_REQUEST  -32600  /**< 无效请求 */
#define JSONRPC_METHOD_NOT_FOUND -32601  /**< 方法未找到 */
#define JSONRPC_INVALID_PARAMS   -32602  /**< 无效参数 */
#define JSONRPC_INTERNAL_ERROR   -32000  /**< 内部错误 */

/* ==================== 响应构建函数 ==================== */

/**
 * @brief 构建 JSON-RPC 错误响应
 * 
 * @param code 错误码（标准JSON-RPC错误码或自定义）
 * @param message 错误消息（不能为NULL）
 * @param id 请求ID
 * @return char* JSON字符串指针，调用者负责free()
 * 
 * @note 返回的字符串需要调用者释放
 * @note 符合 JSON-RPC 2.0 规范
 * 
 * @example
 * char* resp = jsonrpc_build_error(JSONRPC_INVALID_PARAMS, "Missing required field", 1);
 * send_response(client_fd, resp);
 * free(resp);
 */
AGENTOS_API char* jsonrpc_build_error(int code, const char* message, int id);

/**
 * @brief 构建 JSON-RPC 成功响应
 * 
 * @param result 结果对象（将被消费，可为NULL）
 * @param id 请求ID
 * @return char* JSON字符串指针，调用者负责free()
 * 
 * @note result对象会被此函数消费，调用者无需释放
 * @note 如果result为NULL，将返回null结果
 * 
 * @example
 * cJSON* result = cJSON_CreateObject();
 * cJSON_AddStringToObject(result, "status", "ok");
 * char* resp = jsonrpc_build_success(result, 1);
 * send_response(client_fd, resp);
 * free(resp);
 */
AGENTOS_API char* jsonrpc_build_success(cJSON* result, int id);

/**
 * @brief 构建 JSON-RPC 成功响应（字符串结果）
 * 
 * @param result_str 结果字符串（会被复制）
 * @param id 请求ID
 * @return char* JSON字符串指针，调用者负责free()
 * 
 * @note 便捷函数，自动包装字符串为JSON对象
 * 
 * @example
 * char* resp = jsonrpc_build_success_string("Operation completed", 1);
 * send_response(client_fd, resp);
 * free(resp);
 */
AGENTOS_API char* jsonrpc_build_success_string(const char* result_str, int id);

/* ==================== 请求解析函数 ==================== */

/**
 * @brief 解析 JSON-RPC 请求
 * 
 * @param raw 原始JSON字符串（不能为NULL）
 * @param out_method [out] 方法名指针（需调用者free()）
 * @param out_params [out] 参数对象（需调用者cJSON_Delete()）
 * @param out_id [out] 请求ID
 * @return int 0成功，非0失败
 * 
 * @note out_method和out_params在失败时不会被设置
 * @note out_params可能为NULL（请求无参数时）
 * 
 * @example
 * char* method = NULL;
 * cJSON* params = NULL;
 * int id = 0;
 * 
 * if (jsonrpc_parse_request(buffer, &method, &params, &id) == 0) {
 *     // 处理请求
 *     free(method);
 *     if (params) cJSON_Delete(params);
 * }
 */
AGENTOS_API int jsonrpc_parse_request(const char* raw, 
                                       char** out_method, 
                                       cJSON** out_params, 
                                       int* out_id);

/**
 * @brief 验证 JSON-RPC 请求格式
 * 
 * @param req 已解析的JSON对象
 * @return int 0有效，非0无效
 * 
 * @note 检查jsonrpc、method、id字段是否存在
 */
AGENTOS_API int jsonrpc_validate_request(cJSON* req);

/* ==================== 参数提取函数 ==================== */

/**
 * @brief 从参数对象中提取字符串参数
 * 
 * @param params 参数对象
 * @param key 参数键名
 * @param default_value 默认值（参数缺失时返回）
 * @return const char* 参数值字符串，缺失时返回default_value
 * 
 * @note 返回的指针指向JSON对象内部，无需释放
 * @note 符合 ARCHITECTURAL_PRINCIPLES.md E-3 资源确定性原则
 */
AGENTOS_API const char* jsonrpc_get_string_param(cJSON* params, 
                                                  const char* key, 
                                                  const char* default_value);

/**
 * @brief 从参数对象中提取整数参数
 * 
 * @param params 参数对象
 * @param key 参数键名
 * @param default_value 默认值
 * @return int 参数值，缺失或类型错误时返回default_value
 */
AGENTOS_API int jsonrpc_get_int_param(cJSON* params, 
                                       const char* key, 
                                       int default_value);

/**
 * @brief 从参数对象中提取布尔参数
 * 
 * @param params 参数对象
 * @param key 参数键名
 * @param default_value 默认值
 * @return int 参数值（1=true, 0=false），缺失时返回default_value
 */
AGENTOS_API int jsonrpc_get_bool_param(cJSON* params, 
                                        const char* key, 
                                        int default_value);

/**
 * @brief 从参数对象中提取数组参数
 * 
 * @param params 参数对象
 * @param key 参数键名
 * @return cJSON* 数组对象指针，缺失时返回NULL
 * 
 * @note 返回的指针指向JSON对象内部，无需释放
 */
AGENTOS_API cJSON* jsonrpc_get_array_param(cJSON* params, const char* key);

/**
 * @brief 从参数对象中提取对象参数
 * 
 * @param params 参数对象
 * @param key 参数键名
 * @return cJSON* 对象指针，缺失时返回NULL
 * 
 * @note 返回的指针指向JSON对象内部，无需释放
 */
AGENTOS_API cJSON* jsonrpc_get_object_param(cJSON* params, const char* key);

/* ==================== 通知消息处理 ==================== */

/**
 * @brief 检查是否为通知消息（无id字段）
 * 
 * @param req 请求对象
 * @return int 1是通知，0不是通知
 * 
 * @note 通知消息不需要响应
 */
AGENTOS_API int jsonrpc_is_notification(cJSON* req);

/**
 * @brief 构建 JSON-RPC 通知消息
 * 
 * @param method 方法名
 * @param params 参数对象（可为NULL，会被消费）
 * @return char* JSON字符串指针，调用者负责free()
 */
AGENTOS_API char* jsonrpc_build_notification(const char* method, cJSON* params);

/* ==================== 批量请求支持 ==================== */

/**
 * @brief 检查是否为批量请求
 * 
 * @param raw 原始JSON字符串
 * @return int 1是批量请求，0不是
 */
AGENTOS_API int jsonrpc_is_batch_request(const char* raw);

/**
 * @brief 解析批量请求
 * 
 * @param raw 原始JSON字符串
 * @param out_requests [out] 请求数组（需调用者cJSON_Delete()）
 * @return int 请求数量，失败返回-1
 */
AGENTOS_API int jsonrpc_parse_batch(const char* raw, cJSON** out_requests);

/**
 * @brief 构建批量响应
 * 
 * @param responses 响应字符串数组
 * @param count 响应数量
 * @return char* JSON数组字符串，调用者负责free()
 */
AGENTOS_API char* jsonrpc_build_batch_response(char** responses, size_t count);

/* ==================== 错误消息辅助 ==================== */

/**
 * @brief 获取标准错误消息
 * 
 * @param code 错误码
 * @return const char* 错误消息
 */
AGENTOS_API const char* jsonrpc_get_error_message(int code);

/**
 * @brief 构建详细错误响应（包含data字段）
 * 
 * @param code 错误码
 * @param message 错误消息
 * @param data 附加数据（可为NULL，会被消费）
 * @param id 请求ID
 * @return char* JSON字符串指针，调用者负责free()
 */
AGENTOS_API char* jsonrpc_build_error_with_data(int code, 
                                                  const char* message, 
                                                  cJSON* data, 
                                                  int id);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_JSONRPC_HELPERS_H */

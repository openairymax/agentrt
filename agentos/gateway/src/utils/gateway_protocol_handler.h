// SPDX-FileCopyrightText: 2026 SPHARX.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file gateway_protocol_handler.h
 * @brief 多协议网关请求处理器
 * 
 * 扩展gateway_rpc_handler.h，支持MCP/A2A/OpenAI API等协议的自适应处理。
 * 提供协议检测、转换和统一处理接口。
 * 
 * 设计原则：
 * 1. 向后兼容 - 保持现有JSON-RPC客户端的完全兼容性
 * 2. 协议自适应 - 自动检测请求协议类型
 * 3. 统一接口 - 提供与gateway_rpc_handler相同的API签名
 * 4. 可扩展性 - 易于添加新的协议支持
 */

#ifndef GATEWAY_PROTOCOL_HANDLER_H
#define GATEWAY_PROTOCOL_HANDLER_H

#include "gateway_rpc_handler.h"
#include <agentos/unified_protocol.h>
#include <agentos/protocol_router.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 类型定义
// ============================================================================

/**
 * @brief 协议处理器配置
 */
typedef struct {
    bool enable_mcp_protocol;          /**< 启用MCP协议支持 */
    bool enable_a2a_protocol;          /**< 启用A2A协议支持 */
    bool enable_openai_protocol;       /**< 启用OpenAI API协议支持 */
    const char* default_protocol;      /**< 默认协议（当检测失败时使用） */
    uint32_t max_request_size;         /**< 最大请求大小（字节） */
    bool enable_protocol_detection;    /**< 启用自动协议检测 */
} gateway_protocol_config_t;

/**
 * @brief 协议处理器句柄
 */
typedef struct gateway_protocol_handler_s* gateway_protocol_handler_t;

// ============================================================================
// 核心API
// ============================================================================

/**
 * @brief 创建协议处理器实例
 * @param config 处理器配置（可为NULL，使用默认配置）
 * @return 协议处理器句柄，失败返回NULL
 */
gateway_protocol_handler_t gateway_protocol_handler_create(
    const gateway_protocol_config_t* config);

/**
 * @brief 销毁协议处理器实例
 * @param handler 协议处理器句柄
 */
void gateway_protocol_handler_destroy(gateway_protocol_handler_t handler);

/**
 * @brief 处理网关请求（支持多协议）
 * @param handler 协议处理器句柄
 * @param request_data 原始请求数据
 * @param request_size 请求数据大小
 * @param protocol_type 建议的协议类型（可为UNIFIED_PROTOCOL_AUTO，表示自动检测）
 * @param custom_handler 自定义处理回调（可为NULL，使用默认syscall路由）
 * @param handler_data 回调用户数据
 * @return RPC处理结果，需调用者使用rpc_result_free()释放
 * 
 * @note 此函数替代gateway_rpc_handle_request，支持多协议输入
 * @note 如果protocol_type为UNIFIED_PROTOCOL_AUTO，会自动检测协议类型
 */
rpc_result_t gateway_protocol_handle_request(
    gateway_protocol_handler_t handler,
    const char* request_data,
    size_t request_size,
    unified_protocol_type_t protocol_type,
    int (*custom_handler)(const char*, char**, void*),
    void* handler_data);

/**
 * @brief 获取默认协议处理器配置
 * @param config 配置输出
 */
void gateway_protocol_handler_get_default_config(
    gateway_protocol_config_t* config);

/**
 * @brief 从JSON配置加载协议处理器配置
 * @param config 配置输出
 * @param json_config JSON配置字符串
 * @return 0成功，负数错误码
 */
int gateway_protocol_handler_load_config_from_json(
    gateway_protocol_config_t* config,
    const char* json_config);

/**
 * @brief 获取协议处理器统计信息
 * @param handler 协议处理器句柄
 * @param stats_json 统计信息JSON字符串（输出，需要调用者释放）
 * @return 0成功，负数错误码
 */
int gateway_protocol_handler_get_stats(
    gateway_protocol_handler_t handler,
    char** stats_json);

// ============================================================================
// 协议检测函数
// ============================================================================

/**
 * @brief 检测请求数据的协议类型
 * @param request_data 请求数据
 * @param request_size 请求数据大小
 * @return 检测到的协议类型，或UNIFIED_PROTOCOL_UNKNOWN如果无法检测
 */
unified_protocol_type_t gateway_protocol_detect(
    const char* request_data,
    size_t request_size);

/**
 * @brief 检测是否为JSON-RPC请求
 * @param request_data 请求数据
 * @param request_size 请求数据大小
 * @return 1是JSON-RPC，0不是
 */
int gateway_protocol_is_jsonrpc(
    const char* request_data,
    size_t request_size);

/**
 * @brief 检测是否为MCP请求
 * @param request_data 请求数据
 * @param request_size 请求数据大小
 * @return 1是MCP，0不是
 */
int gateway_protocol_is_mcp(
    const char* request_data,
    size_t request_size);

/**
 * @brief 检测是否为A2A请求
 * @param request_data 请求数据
 * @param request_size 请求数据大小
 * @return 1是A2A，0不是
 */
int gateway_protocol_is_a2a(
    const char* request_data,
    size_t request_size);

/**
 * @brief 检测是否为OpenAI API请求
 * @param request_data 请求数据
 * @param request_size 请求数据大小
 * @return 1是OpenAI API，0不是
 */
int gateway_protocol_is_openai(
    const char* request_data,
    size_t request_size);

// ============================================================================
// 协议转换函数
// ============================================================================

/**
 * @brief 将任意协议请求转换为JSON-RPC
 * @param handler 协议处理器句柄
 * @param request_data 原始请求数据
 * @param request_size 请求数据大小
 * @param protocol_type 源协议类型
 * @param jsonrpc_out 转换后的JSON-RPC字符串（输出，需要调用者释放）
 * @return 0成功，负数错误码
 */
int gateway_protocol_convert_to_jsonrpc(
    gateway_protocol_handler_t handler,
    const char* request_data,
    size_t request_size,
    unified_protocol_type_t protocol_type,
    char** jsonrpc_out);

/**
 * @brief 将JSON-RPC响应转换为目标协议格式
 * @param handler 协议处理器句柄
 * @param jsonrpc_response JSON-RPC响应字符串
 * @param target_protocol 目标协议类型
 * @param target_response 转换后的响应字符串（输出，需要调用者释放）
 * @return 0成功，负数错误码
 */
int gateway_protocol_convert_from_jsonrpc(
    gateway_protocol_handler_t handler,
    const char* jsonrpc_response,
    unified_protocol_type_t target_protocol,
    char** target_response);

// ============================================================================
// 向后兼容接口
// ============================================================================

/**
 * @brief 向后兼容接口：处理JSON-RPC请求（与gateway_rpc_handle_request相同）
 * @param request JSON-RPC请求对象
 * @param handler 自定义处理回调
 * @param handler_data 回调用户数据
 * @return RPC处理结果
 * 
 * @note 此函数提供了与现有代码的完全兼容性
 */
rpc_result_t gateway_protocol_handle_jsonrpc(
    const cJSON* request,
    int (*handler)(const char*, char**, void*),
    void* handler_data);

#ifdef __cplusplus
}
#endif

#endif // GATEWAY_PROTOCOL_HANDLER_H
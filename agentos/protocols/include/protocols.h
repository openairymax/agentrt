// SPDX-FileCopyrightText: 2026 SPHARX.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file protocols.h
 * @brief Main header for AgentOS Unified Protocols Framework
 * 
 * 统一协议栈框架主头文件，包含所有公共API和类型定义。
 * 使用此框架可实现协议无关的通信层。
 */

#ifndef AGENTOS_PROTOCOLS_H
#define AGENTOS_PROTOCOLS_H

#include "unified_protocol.h"

/**
 * @defgroup protocols Unified Protocols Framework
 * @brief 统一协议栈框架
 * 
 * 提供跨协议的统一抽象层，支持HTTP、WebSocket、gRPC、MQTT等协议的统一处理。
 * 
 * 主要特性：
 * 1. 协议无关的API设计
 * 2. 统一的消息模型
 * 3. 高性能的消息路由
 * 4. 可扩展的适配器架构
 * 5. 内置的连接池和负载均衡
 * 
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 初始化与清理
// ============================================================================

/**
 * @brief 初始化协议栈框架
 * @note 必须在调用任何其他协议栈函数之前调用
 * @return 0成功，负数错误码
 */
int protocols_framework_init(void);

/**
 * @brief 清理协议栈框架
 * @note 程序退出前调用
 */
void protocols_framework_cleanup(void);

/**
 * @brief 获取框架版本信息
 * @return 版本字符串
 */
const char* protocols_framework_version(void);

// ============================================================================
// 协议栈管理器
// ============================================================================

/**
 * @brief 协议栈管理器句柄
 */
typedef struct protocol_manager_s* protocol_manager_handle_t;

/**
 * @brief 创建协议栈管理器
 * @return 管理器句柄，失败返回NULL
 */
protocol_manager_handle_t protocol_manager_create(void);

/**
 * @brief 销毁协议栈管理器
 * @param manager 管理器句柄
 */
void protocol_manager_destroy(protocol_manager_handle_t manager);

/**
 * @brief 通过管理器创建协议栈
 * @param manager 管理器句柄
 * @param config 协议栈配置
 * @return 协议栈句柄，失败返回NULL
 */
protocol_stack_handle_t protocol_manager_create_stack(protocol_manager_handle_t manager,
                                                     const protocol_stack_config_t* config);

/**
 * @brief 通过管理器销毁协议栈
 * @param manager 管理器句柄
 * @param stack 协议栈句柄
 */
void protocol_manager_destroy_stack(protocol_manager_handle_t manager,
                                   protocol_stack_handle_t stack);

/**
 * @brief 获取所有协议栈列表
 * @param manager 管理器句柄
 * @param stacks 协议栈句柄数组（输出）
 * @param max_count 数组最大容量
 * @return 实际协议栈数量
 */
size_t protocol_manager_get_stacks(protocol_manager_handle_t manager,
                                  protocol_stack_handle_t* stacks,
                                  size_t max_count);

// ============================================================================
// 默认适配器
// ============================================================================

/**
 * @brief 获取HTTP协议适配器
 * @return HTTP协议适配器
 */
const protocol_adapter_t* protocol_adapter_http(void);

/**
 * @brief 获取WebSocket协议适配器
 * @return WebSocket协议适配器
 */
const protocol_adapter_t* protocol_adapter_websocket(void);

/**
 * @brief 获取gRPC协议适配器
 * @return gRPC协议适配器
 */
const protocol_adapter_t* protocol_adapter_grpc(void);

/**
 * @brief 获取MQTT协议适配器
 * @return MQTT协议适配器
 */
const protocol_adapter_t* protocol_adapter_mqtt(void);

// ============================================================================
// 错误处理
// ============================================================================

/**
 * @brief 协议栈错误码
 */
typedef enum {
    PROTOCOL_SUCCESS = 0,           /**< 成功 */
    PROTOCOL_ERROR_INVALID_ARG,     /**< 无效参数 */
    PROTOCOL_ERROR_MEMORY,          /**< 内存不足 */
    PROTOCOL_ERROR_NOT_INITIALIZED, /**< 未初始化 */
    PROTOCOL_ERROR_NOT_CONNECTED,   /**< 未连接 */
    PROTOCOL_ERROR_TIMEOUT,         /**< 超时 */
    PROTOCOL_ERROR_ENCODE,          /**< 编码失败 */
    PROTOCOL_ERROR_DECODE,          /**< 解码失败 */
    PROTOCOL_ERROR_NETWORK,         /**< 网络错误 */
    PROTOCOL_ERROR_PROTOCOL,        /**< 协议错误 */
    PROTOCOL_ERROR_INTERNAL         /**< 内部错误 */
} protocol_error_t;

/**
 * @brief 获取错误码描述
 * @param error 错误码
 * @return 错误描述字符串
 */
const char* protocol_error_to_string(protocol_error_t error);

/**
 * @brief 获取最近一次错误信息
 * @return 错误信息字符串
 */
const char* protocol_get_last_error(void);

// ============================================================================
// 配置辅助函数
// ============================================================================

/**
 * @brief 创建默认协议栈配置
 * @param name 协议栈名称
 * @return 默认配置
 */
protocol_stack_config_t protocol_stack_config_default(const char* name);

/**
 * @brief 释放协议栈配置资源
 * @param config 配置结构
 */
void protocol_stack_config_destroy(protocol_stack_config_t* config);

#ifdef __cplusplus
}
#endif

/** @} */ // end of protocols group

#endif // AGENTOS_PROTOCOLS_H
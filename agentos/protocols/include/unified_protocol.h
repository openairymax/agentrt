// SPDX-FileCopyrightText: 2026 SPHARX.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file unified_protocol.h
 * @brief Unified Protocol Interface for AgentOS
 * 
 * 统一协议接口，为AgentOS提供跨协议的统一抽象层。
 * 支持HTTP、WebSocket、gRPC、MQTT等协议的统一处理。
 * 
 * 设计原则：
 * 1. 协议无关性：上层应用不关心底层协议细节
 * 2. 统一消息模型：所有协议使用相同的消息格式
 * 3. 可扩展性：支持新协议的无缝接入
 * 4. 高性能：零拷贝设计，高效的消息路由
 */

#ifndef AGENTOS_UNIFIED_PROTOCOL_H
#define AGENTOS_UNIFIED_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 类型定义
// ============================================================================

/**
 * @brief 协议类型枚举
 */
typedef enum {
    PROTOCOL_HTTP = 0,      /**< HTTP/1.1, HTTP/2 */
    PROTOCOL_WEBSOCKET,     /**< WebSocket */
    PROTOCOL_GRPC,          /**< gRPC */
    PROTOCOL_MQTT,          /**< MQTT 3.1.1, 5.0 */
    PROTOCOL_AMQP,          /**< AMQP 1.0 */
    PROTOCOL_RAW_TCP,       /**< 原始TCP */
    PROTOCOL_RAW_UDP,       /**< 原始UDP */
    PROTOCOL_CUSTOM         /**< 自定义协议 */
} protocol_type_t;

/**
 * @brief 消息方向枚举
 */
typedef enum {
    DIRECTION_REQUEST = 0,  /**< 请求消息 */
    DIRECTION_RESPONSE,     /**< 响应消息 */
    DIRECTION_NOTIFICATION  /**< 通知消息 */
} message_direction_t;

/**
 * @brief 统一消息结构
 */
typedef struct {
    uint64_t message_id;            /**< 消息ID */
    protocol_type_t protocol;       /**< 协议类型 */
    message_direction_t direction;  /**< 消息方向 */
    const char* endpoint;           /**< 端点/路径 */
    const void* payload;            /**< 负载数据 */
    size_t payload_size;            /**< 负载大小 */
    uint32_t flags;                 /**< 消息标志 */
    uint64_t timestamp;             /**< 时间戳（纳秒） */
    void* user_data;                /**< 用户数据 */
} unified_message_t;

/**
 * @brief 协议适配器接口
 */
typedef struct {
    protocol_type_t type;           /**< 协议类型 */
    
    // 生命周期管理
    int (*init)(void* context);     /**< 初始化适配器 */
    void (*destroy)(void* context); /**< 销毁适配器 */
    
    // 消息处理
    int (*encode)(void* context, const unified_message_t* msg, void** encoded, size_t* size);  /**< 编码消息 */
    int (*decode)(void* context, const void* data, size_t size, unified_message_t* msg);       /**< 解码消息 */
    
    // 连接管理
    int (*connect)(void* context, const char* address);    /**< 建立连接 */
    int (*disconnect)(void* context);                      /**< 断开连接 */
    bool (*is_connected)(void* context);                   /**< 检查连接状态 */
    
    // 统计信息
    void (*get_stats)(void* context, void* stats);         /**< 获取统计信息 */
} protocol_adapter_t;

/**
 * @brief 协议栈配置
 */
typedef struct {
    const char* name;               /**< 协议栈名称 */
    protocol_type_t default_protocol; /**< 默认协议 */
    size_t max_message_size;        /**< 最大消息大小 */
    uint32_t timeout_ms;            /**< 默认超时时间（毫秒） */
    bool enable_compression;        /**< 是否启用压缩 */
    bool enable_encryption;         /**< 是否启用加密 */
    void* custom_config;            /**< 自定义配置 */
} protocol_stack_config_t;

/**
 * @brief 协议栈句柄
 */
typedef struct protocol_stack_s* protocol_stack_handle_t;

// ============================================================================
// 核心API
// ============================================================================

/**
 * @brief 创建协议栈实例
 * @param config 配置参数
 * @return 协议栈句柄，失败返回NULL
 */
protocol_stack_handle_t protocol_stack_create(const protocol_stack_config_t* config);

/**
 * @brief 销毁协议栈实例
 * @param handle 协议栈句柄
 */
void protocol_stack_destroy(protocol_stack_handle_t handle);

/**
 * @brief 注册协议适配器
 * @param handle 协议栈句柄
 * @param adapter 适配器接口
 * @return 0成功，负数错误码
 */
int protocol_stack_register_adapter(protocol_stack_handle_t handle, const protocol_adapter_t* adapter);

/**
 * @brief 发送消息
 * @param handle 协议栈句柄
 * @param message 消息结构
 * @return 0成功，负数错误码
 */
int protocol_stack_send(protocol_stack_handle_t handle, const unified_message_t* message);

/**
 * @brief 接收消息
 * @param handle 协议栈句柄
 * @param message 消息结构（输出）
 * @param timeout_ms 超时时间（毫秒）
 * @return 0成功，负数错误码
 */
int protocol_stack_receive(protocol_stack_handle_t handle, unified_message_t* message, uint32_t timeout_ms);

/**
 * @brief 设置消息回调
 * @param handle 协议栈句柄
 * @param callback 回调函数
 * @param user_data 用户数据
 * @return 0成功，负数错误码
 */
int protocol_stack_set_callback(protocol_stack_handle_t handle, 
                               void (*callback)(const unified_message_t* message, void* user_data),
                               void* user_data);

/**
 * @brief 获取协议栈统计信息
 * @param handle 协议栈句柄
 * @param stats 统计信息结构（输出）
 * @return 0成功，负数错误码
 */
int protocol_stack_get_stats(protocol_stack_handle_t handle, void* stats);

// ============================================================================
// 工具函数
// ============================================================================

/**
 * @brief 创建统一消息
 * @param protocol 协议类型
 * @param direction 消息方向
 * @param endpoint 端点/路径
 * @param payload 负载数据
 * @param payload_size 负载大小
 * @return 消息结构
 */
unified_message_t unified_message_create(protocol_type_t protocol,
                                        message_direction_t direction,
                                        const char* endpoint,
                                        const void* payload,
                                        size_t payload_size);

/**
 * @brief 释放统一消息资源
 * @param message 消息结构
 */
void unified_message_destroy(unified_message_t* message);

/**
 * @brief 获取协议类型字符串
 * @param type 协议类型
 * @return 协议类型字符串
 */
const char* protocol_type_to_string(protocol_type_t type);

/**
 * @brief 从字符串解析协议类型
 * @param str 协议类型字符串
 * @return 协议类型
 */
protocol_type_t protocol_type_from_string(const char* str);

#ifdef __cplusplus
}
#endif

#endif // AGENTOS_UNIFIED_PROTOCOL_H
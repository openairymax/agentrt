/**
 * @file connection_pool.h
 * @brief 连接池管理器接口
 * 
 * 提供高效的连接复用和管理，减少连接创建和销毁的开销。
 * 支持多种连接类型和动态扩缩容。
 * 
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef gateway_CONNECTION_POOL_H
#define gateway_CONNECTION_POOL_H

#include <stddef.h>
#include <stdint.h>
#include "agentos.h"

/**
 * @brief 连接类型枚举
 */
typedef enum {
    CONN_TYPE_HTTP = 0,    /**< HTTP连接 */
    CONN_TYPE_WS,          /**< WebSocket连接 */
    CONN_TYPE_TCP,         /**< TCP连接 */
    CONN_TYPE_UDP          /**< UDP连接 */
} connection_type_t;

/**
 * @brief 连接状态枚举
 */
typedef enum {
    CONN_STATE_IDLE = 0,    /**< 空闲状态 */
    CONN_STATE_ACTIVE,      /**< 活跃状态 */
    CONN_STATE_BUSY,        /**< 忙碌状态 */
    CONN_STATE_CLOSING,     /**< 关闭中状态 */
    CONN_STATE_CLOSED       /**< 已关闭状态 */
} connection_state_t;

/**
 * @brief 连接结构
 */
typedef struct connection {
    connection_type_t type;         /**< 连接类型 */
    connection_state_t state;       /**< 连接状态 */
    void* handle;                   /**< 连接句柄 */
    uint64_t created_at_ns;          /**< 创建时间 */
    uint64_t last_used_ns;          /**< 最后使用时间 */
    size_t usage_count;              /**< 使用次数 */
    size_t bytes_sent;              /**< 发送字节数 */
    size_t bytes_received;          /**< 接收字节数 */
    
    /* 连接特定数据 */
    union {
        struct {
            char* remote_addr;       /**< 远程地址 */
            uint16_t remote_port;   /**< 远程端口 */
        } tcp;
        struct {
            char* url;               /**< URL */
            char* method;            /**< HTTP方法 */
        } http;
        struct {
            char* endpoint;          /**< WebSocket端点 */
        } ws;
    } data;
    
    struct connection* next;        /**< 下一个连接 */
} connection_t;

/**
 * @brief 连接池配置
 */
typedef struct {
    connection_type_t type;         /**< 连接类型 */
    size_t initial_size;            /**< 初始大小 */
    size_t max_size;               /**< 最大大小 */
    size_t min_size;               /**< 最小大小 */
    uint32_t idle_timeout_ms;       /**< 空闲超时（毫秒） */
    uint32_t max_lifetime_ms;      /**< 最大生命周期（毫秒） */
    uint32_t max_usage_count;      /**< 最大使用次数 */
    size_t max_memory_usage;       /**< 最大内存使用量 */
} pool_config_t;

/**
 * @brief 连接池管理器不透明句柄
 */
typedef struct connection_pool connection_pool_t;

/**
 * @brief 创建连接池
 * 
 * @param manager 连接池配置
 * @return 连接池句柄，失败返回NULL
 * 
 * @ownership 调用者需通过 connection_pool_destroy() 释放
 */
connection_pool_t* connection_pool_create(const pool_config_t* manager);

/**
 * @brief 销毁连接池
 * @param pool 连接池句柄
 */
void connection_pool_destroy(connection_pool_t* pool);

/**
 * @brief 获取连接
 * 
 * @param pool 连接池句柄
 * @param out_connection 输出连接
 * @return AGENTOS_SUCCESS 成功
 * @return AGENTOS_ENOENT 无可用连接
 */
agentos_error_t connection_pool_get(connection_pool_t* pool, connection_t** out_connection);

/**
 * @brief 释放连接
 * 
 * @param pool 连接池句柄
 * @param connection 连接
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t connection_pool_release(connection_pool_t* pool, connection_t* connection);

/**
 * @brief 关闭连接
 * 
 * @param pool 连接池句柄
 * @param connection 连接
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t connection_pool_close(connection_pool_t* pool, connection_t* connection);

/**
 * @brief 获取连接池统计信息
 * 
 * @param pool 连接池句柄
 * @param out_json 输出JSON字符串（需调用者free）
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t connection_pool_get_stats(connection_pool_t* pool, char** out_json);

/**
 * @brief 清理空闲连接
 * 
 * @param pool 连接池句柄
 * @param max_idle_count 最大空闲连接数
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t connection_pool_cleanup(connection_pool_t* pool, size_t max_idle_count);

/**
 * @brief 重置连接池
 * 
 * @param pool 连接池句柄
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t connection_pool_reset(connection_pool_t* pool);

/**
 * @brief 设置连接池回调
 * 
 * @param pool 连接池句柄
 * @param create_callback 创建连接回调
 * @param destroy_callback 销毁连接回调
 * @param user_data 用户数据
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t connection_pool_set_callbacks(
    connection_pool_t* pool,
    connection_t* (*create_callback)(connection_type_t type),
    void (*destroy_callback)(connection_t* connection),
    void* user_data
);

/**
 * @brief 获取连接池配置
 * 
 * @param pool 连接池句柄
 * @param out_config 输出配置
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t connection_pool_get_config(connection_pool_t* pool, pool_config_t* out_config);

/**
 * @brief 更新连接池配置
 * 
 * @param pool 连接池句柄
 * @param manager 新配置
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t connection_pool_update_config(connection_pool_t* pool, const pool_config_t* manager);

#endif /* gateway_CONNECTION_POOL_H */
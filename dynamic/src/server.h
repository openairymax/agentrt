/**
 * @file server.h
 * @brief Dynamic 服务器内部数据结构声明
 * 
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * @internal 此头文件仅供 Dynamic 模块内部使用
 */

#ifndef DYNAMIC_SERVER_INTERNAL_H
#define DYNAMIC_SERVER_INTERNAL_H

#include "dynamic.h"
#include "session.h"
#include "health.h"
#include "telemetry.h"
#include "gateway.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

/* 服务器状态枚举 */
typedef enum {
    SERVER_STATE_IDLE = 0,      /**< 已创建，未启动 */
    SERVER_STATE_STARTING,      /**< 正在启动 */
    SERVER_STATE_RUNNING,       /**< 正在运行 */
    SERVER_STATE_STOPPING,      /**< 正在停止 */
    SERVER_STATE_STOPPED        /**< 已停止 */
} server_state_t;

/* 网关类型枚举 */
typedef enum {
    GATEWAY_TYPE_HTTP = 0,
    GATEWAY_TYPE_WEBSOCKET,
    GATEWAY_TYPE_STDIO,
    GATEWAY_TYPE_COUNT
} gateway_type_t;

/**
 * @brief Dynamic 服务器实例结构
 */
struct dynamic_server {
    /* 配置 */
    dynamic_config_t      config;              /**< 运行时配置副本 */
    
    /* 子系统 */
    session_manager_t*    session_mgr;         /**< 会话管理器 */
    health_checker_t*     health;              /**< 健康检查器 */
    telemetry_t*          telemetry;           /**< 可观测性 */
    
    /* 网关 */
    gateway_t*            gateways[GATEWAY_TYPE_COUNT]; /**< 网关实例数组 */
    size_t                gateway_count;       /**< 活跃网关数量 */
    
    /* 线程控制 */
    pthread_t             main_thread;         /**< 主线程 ID */
    pthread_mutex_t       state_lock;          /**< 状态变更锁 */
    pthread_cond_t        state_cond;          /**< 状态变更条件变量 */
    
    /* 原子状态 */
    atomic_int            state;               /**< 服务器状态 */
    atomic_int            shutdown_requested;  /**< 关闭请求标志 */
    
    /* 统计信息 */
    atomic_uint_fast64_t  requests_total;      /**< 总请求数 */
    atomic_uint_fast64_t  requests_failed;     /**< 失败请求数 */
    atomic_uint_fast64_t  bytes_received;      /**< 接收字节数 */
    atomic_uint_fast64_t  bytes_sent;          /**< 发送字节数 */
    
    /* 启动时间 */
    uint64_t              start_time_ns;       /**< 启动时间（纳秒） */
};

/* ========== 内部辅助函数 ========== */

/**
 * @brief 设置默认配置值
 * @param[out] cfg 配置结构
 */
void server_set_default_config(dynamic_config_t* cfg);

/**
 * @brief 验证配置参数
 * @param[in] cfg 配置结构
 * @return AGENTOS_SUCCESS 有效
 * @return AGENTOS_EINVAL 无效
 */
agentos_error_t server_validate_config(const dynamic_config_t* cfg);

/**
 * @brief 初始化服务器子系统
 * @param[in,out] server 服务器实例
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t server_init_subsystems(dynamic_server_t* server);

/**
 * @brief 销毁服务器子系统
 * @param[in,out] server 服务器实例
 */
void server_destroy_subsystems(dynamic_server_t* server);

/**
 * @brief 启动所有网关
 * @param[in,out] server 服务器实例
 * @return AGENTOS_SUCCESS 全部成功
 * @return AGENTOS_ERROR 部分失败
 */
agentos_error_t server_start_gateways(dynamic_server_t* server);

/**
 * @brief 停止所有网关
 * @param[in,out] server 服务器实例
 */
void server_stop_gateways(dynamic_server_t* server);

/**
 * @brief 生成服务器统计信息 JSON
 * @param[in] server 服务器实例
 * @param[out] out_json 输出 JSON 字符串（需调用者 free）
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t server_generate_stats_json(
    const dynamic_server_t* server,
    char** out_json);

#endif /* DYNAMIC_SERVER_INTERNAL_H */

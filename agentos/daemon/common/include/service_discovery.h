// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file service_discovery.h
 * @brief 跨进程服务发现机制
 *
 * 基于共享内存的跨进程服务注册中心，支持：
 * - 跨进程服务注册与发现
 * - 服务健康状态传播
 * - 负载均衡服务选择（轮询/加权/最少连接）
 * - 服务依赖追踪
 * - 心跳与自动过期
 *
 * 设计原则：
 * 1. 零依赖：不依赖外部注册中心（如etcd/consul）
 * 2. 高性能：基于共享内存，发现时间<100ms
 * 3. 自愈性：自动清理过期服务，健康检查联动
 * 4. 跨平台：Windows/Linux/macOS共享内存抽象
 *
 * @see svc_common.h 服务管理框架
 * @see ipc_service_bus.h IPC服务总线
 */

#ifndef AGENTOS_SERVICE_DISCOVERY_H
#define AGENTOS_SERVICE_DISCOVERY_H

#include "svc_common.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 常量定义 ==================== */

#define SD_MAX_SERVICES             128
#define SD_MAX_NAME_LEN             64
#define SD_MAX_ENDPOINT_LEN         256
#define SD_MAX_TYPE_LEN             32
#define SD_MAX_TAGS_LEN             256
#define SD_MAX_DEPS_LEN             512
#define SD_MAX_INSTANCES            8
#define SD_DEFAULT_HEARTBEAT_MS     10000
#define SD_DEFAULT_EXPIRE_MS        30000
#define SD_SHM_NAME                 "/agentos_service_registry"

/* ==================== 服务实例信息 ==================== */

typedef struct {
    char instance_id[SD_MAX_NAME_LEN];
    char endpoint[SD_MAX_ENDPOINT_LEN];
    agentos_svc_state_t state;
    bool healthy;
    uint32_t weight;
    uint32_t active_connections;
    uint32_t max_connections;
    uint64_t last_heartbeat;
    uint64_t register_time;
    uint32_t pid;
} sd_instance_t;

/* ==================== 服务注册条目 ==================== */

typedef struct {
    char name[SD_MAX_NAME_LEN];
    char version[32];
    char service_type[SD_MAX_TYPE_LEN];
    char tags[SD_MAX_TAGS_LEN];
    char dependencies[SD_MAX_DEPS_LEN];
    uint32_t capabilities;
    sd_instance_t instances[SD_MAX_INSTANCES];
    uint32_t instance_count;
    bool active;
    uint64_t last_updated;
} sd_service_entry_t;

/* ==================== 负载均衡策略 ==================== */

typedef enum {
    SD_LB_ROUND_ROBIN      = 0,
    SD_LB_WEIGHTED         = 1,
    SD_LB_LEAST_CONNECTION = 2,
    SD_LB_RANDOM           = 3,
    SD_LB_LEAST_LOAD       = 4
} sd_lb_strategy_t;

/* ==================== 服务发现配置 ==================== */

typedef struct {
    uint32_t heartbeat_interval_ms;
    uint32_t expire_timeout_ms;
    sd_lb_strategy_t default_lb_strategy;
    bool enable_auto_expire;
    bool enable_health_propagation;
    char shm_name[256];
    uint32_t shm_size;
} sd_config_t;

/* ==================== 服务发现统计 ==================== */

typedef struct {
    uint64_t registrations;
    uint64_t deregistrations;
    uint64_t discoveries;
    uint64_t heartbeats;
    uint64_t expirations;
    uint64_t lb_selections;
    uint32_t active_services;
    uint32_t active_instances;
} sd_stats_t;

/* ==================== 服务发现句柄 ==================== */

typedef struct service_discovery_s* service_discovery_t;

/* ==================== 服务变更回调 ==================== */

typedef enum {
    SD_EVENT_REGISTERED     = 1,
    SD_EVENT_DEREGISTERED   = 2,
    SD_EVENT_HEALTH_CHANGE  = 3,
    SD_EVENT_EXPIRED        = 4,
    SD_EVENT_INSTANCE_UP    = 5,
    SD_EVENT_INSTANCE_DOWN  = 6
} sd_event_type_t;

typedef void (*sd_event_callback_t)(
    sd_event_type_t event,
    const char* service_name,
    const sd_instance_t* instance,
    void* user_data
);

/* ==================== 生命周期管理 ==================== */

/**
 * @brief 创建服务发现实例
 * @param config 配置参数（NULL使用默认）
 * @return 服务发现句柄，失败返回NULL
 */
AGENTOS_API service_discovery_t sd_create(const sd_config_t* config);

/**
 * @brief 销毁服务发现实例
 * @param sd 服务发现句柄
 */
AGENTOS_API void sd_destroy(service_discovery_t sd);

/**
 * @brief 启动服务发现
 * @param sd 服务发现句柄
 * @return 0成功，非0失败
 */
AGENTOS_API agentos_error_t sd_start(service_discovery_t sd);

/**
 * @brief 停止服务发现
 * @param sd 服务发现句柄
 * @return 0成功，非0失败
 */
AGENTOS_API agentos_error_t sd_stop(service_discovery_t sd);

/* ==================== 服务注册 ==================== */

/**
 * @brief 注册服务实例
 * @param sd 服务发现句柄
 * @param service_name 服务名称
 * @param service_type 服务类型
 * @param instance 实例信息
 * @param tags 标签（逗号分隔）
 * @param dependencies 依赖服务（逗号分隔）
 * @return 0成功，非0失败
 */
AGENTOS_API agentos_error_t sd_register(
    service_discovery_t sd,
    const char* service_name,
    const char* service_type,
    const sd_instance_t* instance,
    const char* tags,
    const char* dependencies
);

/**
 * @brief 注销服务实例
 * @param sd 服务发现句柄
 * @param service_name 服务名称
 * @param instance_id 实例ID
 * @return 0成功，非0失败
 */
AGENTOS_API agentos_error_t sd_deregister(
    service_discovery_t sd,
    const char* service_name,
    const char* instance_id
);

/**
 * @brief 注销服务的所有实例
 * @param sd 服务发现句柄
 * @param service_name 服务名称
 * @return 0成功，非0失败
 */
AGENTOS_API agentos_error_t sd_deregister_all(
    service_discovery_t sd,
    const char* service_name
);

/* ==================== 服务发现 ==================== */

/**
 * @brief 发现服务（获取所有健康实例）
 * @param sd 服务发现句柄
 * @param service_name 服务名称
 * @param instances [out] 实例数组
 * @param max_count 数组最大容量
 * @param found_count [out] 实际找到数量
 * @return 0成功，非0失败
 */
AGENTOS_API agentos_error_t sd_discover(
    service_discovery_t sd,
    const char* service_name,
    sd_instance_t* instances,
    uint32_t max_count,
    uint32_t* found_count
);

/**
 * @brief 按类型发现服务
 * @param sd 服务发现句柄
 * @param service_type 服务类型
 * @param entries [out] 服务条目数组
 * @param max_count 数组最大容量
 * @param found_count [out] 实际找到数量
 * @return 0成功，非0失败
 */
AGENTOS_API agentos_error_t sd_discover_by_type(
    service_discovery_t sd,
    const char* service_type,
    sd_service_entry_t* entries,
    uint32_t max_count,
    uint32_t* found_count
);

/**
 * @brief 按标签发现服务
 * @param sd 服务发现句柄
 * @param tags 标签过滤（逗号分隔）
 * @param entries [out] 服务条目数组
 * @param max_count 数组最大容量
 * @param found_count [out] 实际找到数量
 * @return 0成功，非0失败
 */
AGENTOS_API agentos_error_t sd_discover_by_tags(
    service_discovery_t sd,
    const char* tags,
    sd_service_entry_t* entries,
    uint32_t max_count,
    uint32_t* found_count
);

/**
 * @brief 选择最优实例（负载均衡）
 * @param sd 服务发现句柄
 * @param service_name 服务名称
 * @param strategy 负载均衡策略
 * @param instance [out] 选中的实例
 * @return 0成功，非0失败
 */
AGENTOS_API agentos_error_t sd_select_instance(
    service_discovery_t sd,
    const char* service_name,
    sd_lb_strategy_t strategy,
    sd_instance_t* instance
);

/* ==================== 心跳与健康 ==================== */

/**
 * @brief 发送心跳
 * @param sd 服务发现句柄
 * @param service_name 服务名称
 * @param instance_id 实例ID
 * @return 0成功，非0失败
 */
AGENTOS_API agentos_error_t sd_heartbeat(
    service_discovery_t sd,
    const char* service_name,
    const char* instance_id
);

/**
 * @brief 更新实例健康状态
 * @param sd 服务发现句柄
 * @param service_name 服务名称
 * @param instance_id 实例ID
 * @param healthy 是否健康
 * @return 0成功，非0失败
 */
AGENTOS_API agentos_error_t sd_update_health(
    service_discovery_t sd,
    const char* service_name,
    const char* instance_id,
    bool healthy
);

/**
 * @brief 更新实例连接数
 * @param sd 服务发现句柄
 * @param service_name 服务名称
 * @param instance_id 实例ID
 * @param active_connections 当前活跃连接数
 * @return 0成功，非0失败
 */
AGENTOS_API agentos_error_t sd_update_connections(
    service_discovery_t sd,
    const char* service_name,
    const char* instance_id,
    uint32_t active_connections
);

/* ==================== 依赖管理 ==================== */

/**
 * @brief 获取服务依赖列表
 * @param sd 服务发现句柄
 * @param service_name 服务名称
 * @param dependencies [out] 依赖列表（逗号分隔）
 * @param max_len 缓冲区最大长度
 * @return 0成功，非0失败
 */
AGENTOS_API agentos_error_t sd_get_dependencies(
    service_discovery_t sd,
    const char* service_name,
    char* dependencies,
    size_t max_len
);

/**
 * @brief 检查服务依赖是否满足
 * @param sd 服务发现句柄
 * @param service_name 服务名称
 * @param missing_deps [out] 缺失依赖列表（逗号分隔），NULL不输出
 * @param max_len 缓冲区最大长度
 * @return 0所有依赖满足，非0有缺失依赖
 */
AGENTOS_API agentos_error_t sd_check_dependencies(
    service_discovery_t sd,
    const char* service_name,
    char* missing_deps,
    size_t max_len
);

/* ==================== 事件与统计 ==================== */

/**
 * @brief 注册服务变更回调
 * @param sd 服务发现句柄
 * @param callback 回调函数
 * @param user_data 用户数据
 * @return 0成功，非0失败
 */
AGENTOS_API agentos_error_t sd_register_event_callback(
    service_discovery_t sd,
    sd_event_callback_t callback,
    void* user_data
);

/**
 * @brief 获取服务发现统计
 * @param sd 服务发现句柄
 * @param stats [out] 统计信息
 * @return 0成功，非0失败
 */
AGENTOS_API agentos_error_t sd_get_stats(
    service_discovery_t sd,
    sd_stats_t* stats
);

/**
 * @brief 获取所有已注册服务数量
 * @param sd 服务发现句柄
 * @return 服务数量
 */
AGENTOS_API uint32_t sd_service_count(service_discovery_t sd);

/**
 * @brief 获取服务发现运行状态
 * @param sd 服务发现句柄
 * @return true运行中，false未运行
 */
AGENTOS_API bool sd_is_running(service_discovery_t sd);

/* ==================== 工具函数 ==================== */

/**
 * @brief 负载均衡策略转字符串
 * @param strategy 策略类型
 * @return 策略名称
 */
AGENTOS_API const char* sd_lb_strategy_to_string(sd_lb_strategy_t strategy);

/**
 * @brief 创建默认配置
 * @return 默认配置
 */
AGENTOS_API sd_config_t sd_create_default_config(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_SERVICE_DISCOVERY_H */

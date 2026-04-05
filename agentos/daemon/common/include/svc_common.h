// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file svc_common.h
 * @brief 服务公共定义
 * 
 * 提供所有服务共享的定义和接口。
 * 
 * 设计原则（映射架构设计原则 K-2 接口契约化原则）：
 * 1. 统一的服务接口定义
 * 2. 明确的生命周期管理
 * 3. 标准化的错误处理
 * 
 * @see agentos/manuals/specifications/agentos_contract/protocol_contract.md
 */

#ifndef AGENTOS_DAEMON_COMMON_SVC_COMMON_H
#define AGENTOS_DAEMON_COMMON_SVC_COMMON_H

#include "error.h"
#include "platform.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 服务状态 ==================== */

/**
 * @brief 服务状态枚举
 */
typedef enum {
    AGENTOS_SVC_STATE_NONE = 0,       /**< 未初始化 */
    AGENTOS_SVC_STATE_CREATED,        /**< 已创建 */
    AGENTOS_SVC_STATE_INITIALIZING,   /**< 初始化中 */
    AGENTOS_SVC_STATE_READY,          /**< 就绪 */
    AGENTOS_SVC_STATE_RUNNING,        /**< 运行中 */
    AGENTOS_SVC_STATE_PAUSED,         /**< 已暂停 */
    AGENTOS_SVC_STATE_STOPPING,       /**< 停止中 */
    AGENTOS_SVC_STATE_STOPPED,        /**< 已停止 */
    AGENTOS_SVC_STATE_ERROR           /**< 错误状态 */
} agentos_svc_state_t;

/* ==================== 服务能力标志 ==================== */

/**
 * @brief 服务能力标志
 */
typedef enum {
    AGENTOS_SVC_CAP_NONE         = 0,        /**< 无特殊能力 */
    AGENTOS_SVC_CAP_ASYNC        = 1 << 0,   /**< 支持异步操作 */
    AGENTOS_SVC_CAP_STREAMING    = 1 << 1,   /**< 支持流式处理 */
    AGENTOS_SVC_CAP_CANCELABLE   = 1 << 2,   /**< 支持取消操作 */
    AGENTOS_SVC_CAP_PAUSEABLE    = 1 << 3,   /**< 支持暂停/恢复 */
    AGENTOS_SVC_CAP_THROTTLE     = 1 << 4,   /**< 支持限流 */
    AGENTOS_SVC_CAP_BATCH        = 1 << 5,   /**< 支持批量处理 */
    AGENTOS_SVC_CAP_PRIORITY     = 1 << 6,   /**< 支持优先级 */
    AGENTOS_SVC_CAP_TIMEOUT      = 1 << 7,   /**< 支持超时控制 */
} agentos_svc_capability_t;

/* ==================== 服务配置 ==================== */

/**
 * @brief 服务配置结构
 */
typedef struct {
    const char* name;                  /**< 服务名称 */
    const char* version;               /**< 服务版本 */
    uint32_t capabilities;             /**< 能力标志 */
    uint32_t max_concurrent;           /**< 最大并发数 */
    uint32_t timeout_ms;               /**< 默认超时（毫秒） */
    int priority;                      /**< 默认优先级 */
    bool auto_start;                   /**< 是否自动启动 */
    bool enable_metrics;               /**< 是否启用指标收集 */
    bool enable_tracing;               /**< 是否启用追踪 */
} agentos_svc_config_t;

/* ==================== 服务统计 ==================== */

/**
 * @brief 服务统计信息
 */
typedef struct {
    uint64_t request_count;            /**< 请求总数 */
    uint64_t success_count;            /**< 成功数 */
    uint64_t error_count;              /**< 错误数 */
    uint64_t total_time_ms;            /**< 总处理时间（毫秒） */
    uint64_t max_time_ms;              /**< 最大处理时间 */
    uint64_t min_time_ms;              /**< 最小处理时间 */
    uint32_t current_concurrent;       /**< 当前并发数 */
    uint32_t peak_concurrent;          /**< 峰值并发数 */
    double avg_time_ms;                /**< 平均处理时间 */
} agentos_svc_stats_t;

/* ==================== 服务句柄类型 ==================== */

/**
 * @brief 服务句柄类型
 */
typedef struct agentos_service_s* agentos_service_t;

/* ==================== 服务接口定义 ==================== */

/**
 * @brief 服务初始化函数类型
 * @param service 服务句柄
 * @param config 配置参数
 * @return 0成功，非0失败
 */
typedef agentos_error_t (*agentos_svc_init_fn)(
    agentos_service_t service,
    const agentos_svc_config_t* config
);

/**
 * @brief 服务启动函数类型
 * @param service 服务句柄
 * @return 0成功，非0失败
 */
typedef agentos_error_t (*agentos_svc_start_fn)(
    agentos_service_t service
);

/**
 * @brief 服务停止函数类型
 * @param service 服务句柄
 * @param force 是否强制停止
 * @return 0成功，非0失败
 */
typedef agentos_error_t (*agentos_svc_stop_fn)(
    agentos_service_t service,
    bool force
);

/**
 * @brief 服务销毁函数类型
 * @param service 服务句柄
 */
typedef void (*agentos_svc_destroy_fn)(
    agentos_service_t service
);

/**
 * @brief 服务健康检查函数类型
 * @param service 服务句柄
 * @return 0健康，非0不健康
 */
typedef agentos_error_t (*agentos_svc_healthcheck_fn)(
    agentos_service_t service
);

/**
 * @brief 服务接口结构
 */
typedef struct {
    agentos_svc_init_fn init;          /**< 初始化函数 */
    agentos_svc_start_fn start;        /**< 启动函数 */
    agentos_svc_stop_fn stop;          /**< 停止函数 */
    agentos_svc_destroy_fn destroy;    /**< 销毁函数 */
    agentos_svc_healthcheck_fn healthcheck; /**< 健康检查函数 */
} agentos_svc_interface_t;

/* ==================== 服务生命周期管理 ==================== */

/**
 * @brief 创建服务实例
 * @param service [out] 服务句柄输出
 * @param name [in] 服务名称
 * @param iface [in] 服务接口
 * @param config [in] 服务配置
 * @return 0成功，非0失败
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API agentos_error_t agentos_service_create(
    agentos_service_t* service,
    const char* name,
    const agentos_svc_interface_t* iface,
    const agentos_svc_config_t* config
);

/**
 * @brief 销毁服务实例
 * @param service [in] 服务句柄
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API void agentos_service_destroy(agentos_service_t service);

/**
 * @brief 初始化服务
 * @param service [in] 服务句柄
 * @return 0成功，非0失败
 * @threadsafe 否
 * @reentrant 否
 */
AGENTOS_API agentos_error_t agentos_service_init(agentos_service_t service);

/**
 * @brief 启动服务
 * @param service [in] 服务句柄
 * @return 0成功，非0失败
 * @threadsafe 否
 * @reentrant 否
 */
AGENTOS_API agentos_error_t agentos_service_start(agentos_service_t service);

/**
 * @brief 停止服务
 * @param service [in] 服务句柄
 * @param force [in] 是否强制停止
 * @return 0成功，非0失败
 * @threadsafe 否
 * @reentrant 否
 */
AGENTOS_API agentos_error_t agentos_service_stop(agentos_service_t service, bool force);

/**
 * @brief 暂停服务
 * @param service [in] 服务句柄
 * @return 0成功，非0失败
 * @threadsafe 是
 * @reentrant 否
 */
AGENTOS_API agentos_error_t agentos_service_pause(agentos_service_t service);

/**
 * @brief 恢复服务
 * @param service [in] 服务句柄
 * @return 0成功，非0失败
 * @threadsafe 是
 * @reentrant 否
 */
AGENTOS_API agentos_error_t agentos_service_resume(agentos_service_t service);

/* ==================== 服务状态查询 ==================== */

/**
 * @brief 获取服务状态
 * @param service [in] 服务句柄
 * @return 服务状态
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API agentos_svc_state_t agentos_service_get_state(agentos_service_t service);

/**
 * @brief 检查服务是否就绪
 * @param service [in] 服务句柄
 * @return true就绪，false未就绪
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API bool agentos_service_is_ready(agentos_service_t service);

/**
 * @brief 检查服务是否运行中
 * @param service [in] 服务句柄
 * @return true运行中，false未运行
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API bool agentos_service_is_running(agentos_service_t service);

/**
 * @brief 获取服务名称
 * @param service [in] 服务句柄
 * @return 服务名称
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API const char* agentos_service_get_name(agentos_service_t service);

/**
 * @brief 获取服务版本
 * @param service [in] 服务句柄
 * @return 服务版本
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API const char* agentos_service_get_version(agentos_service_t service);

/* ==================== 服务统计 ==================== */

/**
 * @brief 获取服务统计信息
 * @param service [in] 服务句柄
 * @param stats [out] 统计信息输出
 * @return 0成功，非0失败
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API agentos_error_t agentos_service_get_stats(
    agentos_service_t service,
    agentos_svc_stats_t* stats
);

/**
 * @brief 重置服务统计信息
 * @param service [in] 服务句柄
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API void agentos_service_reset_stats(agentos_service_t service);

/* ==================== 服务健康检查 ==================== */

/**
 * @brief 执行服务健康检查
 * @param service [in] 服务句柄
 * @return 0健康，非0不健康
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API agentos_error_t agentos_service_healthcheck(agentos_service_t service);

/* ==================== 服务能力查询 ==================== */

/**
 * @brief 检查服务是否支持指定能力
 * @param service [in] 服务句柄
 * @param capability [in] 能力标志
 * @return true支持，false不支持
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API bool agentos_service_has_capability(
    agentos_service_t service,
    agentos_svc_capability_t capability
);

/* ==================== 服务状态字符串转换 ==================== */

/**
 * @brief 服务状态转字符串
 * @param state [in] 服务状态
 * @return 状态字符串
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API const char* agentos_svc_state_to_string(agentos_svc_state_t state);

/**
 * @brief 字符串转服务状态
 * @param str [in] 状态字符串
 * @return 服务状态
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API agentos_svc_state_t agentos_svc_state_from_string(const char* str);

/* ==================== 服务注册表 ==================== */

/**
 * @brief 注册服务
 * @param service [in] 服务句柄
 * @return 0成功，非0失败
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API agentos_error_t agentos_service_register(agentos_service_t service);

/**
 * @brief 注销服务
 * @param service [in] 服务句柄
 * @return 0成功，非0失败
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API agentos_error_t agentos_service_unregister(agentos_service_t service);

/**
 * @brief 根据名称查找服务
 * @param name [in] 服务名称
 * @return 服务句柄（未找到返回NULL）
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API agentos_service_t agentos_service_find(const char* name);

/**
 * @brief 获取所有服务数量
 * @return 服务数量
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API uint32_t agentos_service_count(void);

/**
 * @brief 遍历所有服务
 * @param callback [in] 回调函数
 * @param user_data [in] 用户数据
 * @threadsafe 是
 * @reentrant 是
 */
typedef void (*agentos_service_enum_fn)(agentos_service_t service, void* user_data);
AGENTOS_API void agentos_service_foreach(agentos_service_enum_fn callback, void* user_data);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_DAEMON_COMMON_SVC_COMMON_H */

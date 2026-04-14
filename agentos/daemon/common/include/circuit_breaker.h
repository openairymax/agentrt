// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file circuit_breaker.h
 * @brief 熔断器与自愈框架
 *
 * 实现熔断器模式（Circuit Breaker Pattern），提供：
 * - 三态熔断器：关闭（正常）→ 开启（熔断）→ 半开（探测）
 * - 自动故障检测与熔断
 * - 渐进式恢复探测
 * - 级联故障防护
 * - 自动故障转移
 * - 与服务发现联动
 *
 * 设计原则：
 * 1. 快速失败：熔断状态下立即返回错误，避免级联阻塞
 * 2. 渐进恢复：半开状态下逐步放行请求，验证服务恢复
 * 3. 可观测性：熔断状态变更触发事件通知
 * 4. 可配置性：阈值、超时、探测策略均可配置
 *
 * @see svc_common.h 服务管理框架
 * @see service_discovery.h 服务发现
 */

#ifndef AGENTOS_CIRCUIT_BREAKER_H
#define AGENTOS_CIRCUIT_BREAKER_H

#include "svc_common.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 常量定义 ==================== */

#define CB_MAX_BREAKERS         64
#define CB_MAX_NAME_LEN         64
#define CB_MAX_FALLBACKS        4
#define CB_DEFAULT_FAILURE_THRESHOLD   5
#define CB_DEFAULT_SUCCESS_THRESHOLD   3
#define CB_DEFAULT_TIMEOUT_MS          30000
#define CB_DEFAULT_HALF_OPEN_MAX       1

/* ==================== 熔断器状态 ==================== */

typedef enum {
    CB_STATE_CLOSED       = 0,
    CB_STATE_OPEN         = 1,
    CB_STATE_HALF_OPEN    = 2
} cb_state_t;

/* ==================== 熔断器配置 ==================== */

typedef struct {
    uint32_t failure_threshold;
    uint32_t success_threshold;
    uint32_t timeout_ms;
    uint32_t half_open_max_calls;
    uint32_t window_size_ms;
    uint32_t slow_call_duration_ms;
    uint32_t slow_call_rate_threshold;
    uint32_t failure_rate_threshold;
    bool enable_slow_call_detection;
    bool enable_auto_failover;
} cb_config_t;

/* ==================== 熔断器统计 ==================== */

typedef struct {
    uint64_t total_calls;
    uint64_t successful_calls;
    uint64_t failed_calls;
    uint64_t rejected_calls;
    uint64_t timeout_calls;
    uint64_t slow_calls;
    uint64_t state_transitions;
    uint64_t last_failure_time;
    uint64_t last_success_time;
    uint64_t last_state_change_time;
    double failure_rate;
    double slow_call_rate;
    uint32_t consecutive_failures;
    uint32_t consecutive_successes;
} cb_stats_t;

/* ==================== 熔断器事件 ==================== */

typedef enum {
    CB_EVENT_STATE_CHANGE    = 1,
    CB_EVENT_FAILURE         = 2,
    CB_EVENT_SUCCESS         = 3,
    CB_EVENT_REJECTED        = 4,
    CB_EVENT_SLOW_CALL       = 5,
    CB_EVENT_TIMEOUT         = 6,
    CB_EVENT_FAILOVER        = 7
} cb_event_type_t;

typedef struct {
    cb_event_type_t type;
    char breaker_name[CB_MAX_NAME_LEN];
    cb_state_t old_state;
    cb_state_t new_state;
    const char* message;
    uint64_t timestamp;
} cb_event_t;

typedef void (*cb_event_callback_t)(
    const cb_event_t* event,
    void* user_data
);

/* ==================== 故障转移策略 ==================== */

typedef enum {
    CB_FAILOVER_RETRY        = 0,
    CB_FAILOVER_FALLBACK     = 1,
    CB_FAILOVER_REDIRECT     = 2,
    CB_FAILOVER_CACHE        = 3
} cb_failover_strategy_t;

typedef struct {
    cb_failover_strategy_t strategy;
    char fallback_service[CB_MAX_NAME_LEN];
    uint32_t max_retries;
    uint32_t retry_delay_ms;
    uint32_t retry_backoff_factor;
} cb_failover_config_t;

/* ==================== 熔断器句柄 ==================== */

typedef struct circuit_breaker_s* circuit_breaker_t;

/* ==================== 熔断器管理器 ==================== */

typedef struct cb_manager_s* cb_manager_t;

/* ==================== 熔断器生命周期 ==================== */

/**
 * @brief 创建熔断器管理器
 * @return 管理器句柄，失败返回NULL
 */
AGENTOS_API cb_manager_t cb_manager_create(void);

/**
 * @brief 销毁熔断器管理器
 * @param manager 管理器句柄
 */
AGENTOS_API void cb_manager_destroy(cb_manager_t manager);

/* ==================== 熔断器操作 ==================== */

/**
 * @brief 创建熔断器
 * @param manager 管理器句柄
 * @param name 熔断器名称（通常为服务名称）
 * @param config 配置参数（NULL使用默认）
 * @return 熔断器句柄，失败返回NULL
 */
AGENTOS_API circuit_breaker_t cb_create(
    cb_manager_t manager,
    const char* name,
    const cb_config_t* config
);

/**
 * @brief 销毁熔断器
 * @param breaker 熔断器句柄
 */
AGENTOS_API void cb_destroy(circuit_breaker_t breaker);

/**
 * @brief 检查是否允许请求通过
 * @param breaker 熔断器句柄
 * @return true允许，false拒绝
 */
AGENTOS_API bool cb_allow_request(circuit_breaker_t breaker);

/**
 * @brief 记录成功调用
 * @param breaker 熔断器句柄
 * @param duration_ms 调用耗时
 */
AGENTOS_API void cb_record_success(circuit_breaker_t breaker, uint32_t duration_ms);

/**
 * @brief 记录失败调用
 * @param breaker 熔断器句柄
 * @param error_code 错误码
 */
AGENTOS_API void cb_record_failure(circuit_breaker_t breaker, int32_t error_code);

/**
 * @brief 记录超时调用
 * @param breaker 熔断器句柄
 */
AGENTOS_API void cb_record_timeout(circuit_breaker_t breaker);

/* ==================== 熔断器状态查询 ==================== */

/**
 * @brief 获取熔断器当前状态
 * @param breaker 熔断器句柄
 * @return 熔断器状态
 */
AGENTOS_API cb_state_t cb_get_state(circuit_breaker_t breaker);

/**
 * @brief 获取熔断器名称
 * @param breaker 熔断器句柄
 * @return 名称
 */
AGENTOS_API const char* cb_get_name(circuit_breaker_t breaker);

/**
 * @brief 获取熔断器统计
 * @param breaker 熔断器句柄
 * @param stats [out] 统计信息
 * @return 0成功，非0失败
 */
AGENTOS_API agentos_error_t cb_get_stats(circuit_breaker_t breaker, cb_stats_t* stats);

/**
 * @brief 重置熔断器到关闭状态
 * @param breaker 熔断器句柄
 */
AGENTOS_API void cb_reset(circuit_breaker_t breaker);

/**
 * @brief 强制打开熔断器
 * @param breaker 熔断器句柄
 */
AGENTOS_API void cb_force_open(circuit_breaker_t breaker);

/**
 * @brief 强制关闭熔断器
 * @param breaker 熔断器句柄
 */
AGENTOS_API void cb_force_close(circuit_breaker_t breaker);

/* ==================== 故障转移 ==================== */

/**
 * @brief 配置故障转移策略
 * @param breaker 熔断器句柄
 * @param config 故障转移配置
 * @return 0成功，非0失败
 */
AGENTOS_API agentos_error_t cb_set_failover_config(
    circuit_breaker_t breaker,
    const cb_failover_config_t* config
);

/**
 * @brief 执行故障转移
 * @param breaker 熔断器句柄
 * @param original_error 原始错误码
 * @param fallback_result [out] 故障转移结果
 * @param result_size 结果缓冲区大小
 * @return 0成功，非0失败
 */
AGENTOS_API agentos_error_t cb_execute_failover(
    circuit_breaker_t breaker,
    int32_t original_error,
    char* fallback_result,
    size_t result_size
);

/* ==================== 事件与回调 ==================== */

/**
 * @brief 注册熔断器事件回调
 * @param manager 管理器句柄
 * @param callback 回调函数
 * @param user_data 用户数据
 * @return 0成功，非0失败
 */
AGENTOS_API agentos_error_t cb_register_event_callback(
    cb_manager_t manager,
    cb_event_callback_t callback,
    void* user_data
);

/**
 * @brief 查找熔断器
 * @param manager 管理器句柄
 * @param name 熔断器名称
 * @return 熔断器句柄，未找到返回NULL
 */
AGENTOS_API circuit_breaker_t cb_find(cb_manager_t manager, const char* name);

/**
 * @brief 获取所有熔断器数量
 * @param manager 管理器句柄
 * @return 熔断器数量
 */
AGENTOS_API uint32_t cb_count(cb_manager_t manager);

/* ==================== 工具函数 ==================== */

/**
 * @brief 熔断器状态转字符串
 * @param state 状态
 * @return 状态名称
 */
AGENTOS_API const char* cb_state_to_string(cb_state_t state);

/**
 * @brief 创建默认配置
 * @return 默认配置
 */
AGENTOS_API cb_config_t cb_create_default_config(void);

/**
 * @brief 创建默认故障转移配置
 * @return 默认故障转移配置
 */
AGENTOS_API cb_failover_config_t cb_create_default_failover_config(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_CIRCUIT_BREAKER_H */

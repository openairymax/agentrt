// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file alert_manager.h
 * @brief 智能告警管理系统
 *
 * 提供统一的告警管理，支持：
 * - 多级告警（信息/警告/严重/紧急）
 * - 告警规则引擎（阈值/趋势/组合条件）
 * - 告警抑制与去重
 * - 多通道通知（日志/回调/Webhook）
 * - 告警升级策略
 * - 与熔断器和服务发现联动
 *
 * @see circuit_breaker.h 熔断器
 * @see service_discovery.h 服务发现
 */

#ifndef AGENTOS_ALERT_MANAGER_H
#define AGENTOS_ALERT_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 常量定义 ==================== */

#define AM_MAX_RULES            64
#define AM_MAX_NAME_LEN         64
#define AM_MAX_MESSAGE_LEN      512
#define AM_MAX_CHANNELS         8
#define AM_MAX_ACTIVE_ALERTS    256

/* ==================== 告警级别 ==================== */

typedef enum {
    AM_LEVEL_INFO     = 0,
    AM_LEVEL_WARNING  = 1,
    AM_LEVEL_CRITICAL = 2,
    AM_LEVEL_EMERGENCY = 3
} am_level_t;

/* ==================== 告警状态 ==================== */

typedef enum {
    AM_STATE_PENDING    = 0,
    AM_STATE_FIRING     = 1,
    AM_STATE_RESOLVED   = 2,
    AM_STATE_SUPPRESSED = 3,
    AM_STATE_ACKNOWLEDGED = 4
} am_state_t;

/* ==================== 告警规则类型 ==================== */

typedef enum {
    AM_RULE_THRESHOLD   = 0,
    AM_RULE_TREND       = 1,
    AM_RULE_COMPOSITE   = 2,
    AM_RULE_ANOMALY     = 3
} am_rule_type_t;

/* ==================== 比较运算符 ==================== */

typedef enum {
    AM_OP_GT   = 0,
    AM_OP_GTE  = 1,
    AM_OP_LT   = 2,
    AM_OP_LTE  = 3,
    AM_OP_EQ   = 4,
    AM_OP_NEQ  = 5
} am_comparison_t;

/* ==================== 通知通道类型 ==================== */

typedef enum {
    AM_CHANNEL_LOG      = 0,
    AM_CHANNEL_CALLBACK = 1,
    AM_CHANNEL_WEBHOOK  = 2,
    AM_CHANNEL_FILE     = 3
} am_channel_type_t;

/* ==================== 告警条目 ==================== */

typedef struct {
    char name[AM_MAX_NAME_LEN];
    am_level_t level;
    am_state_t state;
    char message[AM_MAX_MESSAGE_LEN];
    char source[64];
    char labels[256];
    uint64_t fired_at;
    uint64_t resolved_at;
    uint64_t last_notified;
    uint32_t notification_count;
    uint32_t trigger_count;
    bool acknowledged;
} am_alert_t;

/* ==================== 告警规则 ==================== */

typedef struct {
    char name[AM_MAX_NAME_LEN];
    am_rule_type_t type;
    am_level_t level;
    char metric_name[128];
    am_comparison_t comparison;
    double threshold;
    uint32_t duration_seconds;
    uint32_t cooldown_seconds;
    char composite_expr[256];
    bool enabled;
    uint64_t last_triggered;
} am_rule_t;

/* ==================== 通知通道 ==================== */

typedef struct {
    am_channel_type_t type;
    char name[AM_MAX_NAME_LEN];
    char config[512];
    am_level_t min_level;
    bool enabled;
} am_channel_t;

/* ==================== 告警管理器配置 ==================== */

typedef struct {
    uint32_t evaluation_interval_ms;
    uint32_t default_cooldown_ms;
    uint32_t max_notifications_per_alert;
    uint32_t escalation_timeout_ms;
    bool enable_deduplication;
    bool enable_suppression;
} am_config_t;

/* ==================== 告警回调 ==================== */

typedef void (*am_alert_callback_t)(
    const am_alert_t* alert,
    void* user_data
);

/* ==================== 生命周期管理 ==================== */

/**
 * @brief 创建告警管理器
 * @param config 配置参数（NULL使用默认）
 * @return 0成功，非0失败
 */
int am_init(const am_config_t* config);

/**
 * @brief 关闭告警管理器
 */
void am_shutdown(void);

/* ==================== 规则管理 ==================== */

/**
 * @brief 添加告警规则
 * @param rule 规则定义
 * @return 0成功，非0失败
 */
int am_add_rule(const am_rule_t* rule);

/**
 * @brief 移除告警规则
 * @param name 规则名称
 * @return 0成功，非0失败
 */
int am_remove_rule(const char* name);

/**
 * @brief 启用/禁用规则
 * @param name 规则名称
 * @param enabled 是否启用
 * @return 0成功，非0失败
 */
int am_set_rule_enabled(const char* name, bool enabled);

/* ==================== 告警触发 ==================== */

/**
 * @brief 触发告警
 * @param name 告警名称
 * @param level 告警级别
 * @param message 告警消息
 * @param source 来源
 * @param labels 标签
 * @return 0成功，非0失败
 */
int am_fire(const char* name, am_level_t level,
            const char* message, const char* source,
            const char* labels);

/**
 * @brief 解决告警
 * @param name 告警名称
 * @return 0成功，非0失败
 */
int am_resolve(const char* name);

/**
 * @brief 确认告警
 * @param name 告警名称
 * @return 0成功，非0失败
 */
int am_acknowledge(const char* name);

/* ==================== 指标评估 ==================== */

/**
 * @brief 评估指标值（检查是否触发规则）
 * @param metric_name 指标名称
 * @param value 指标值
 * @return 触发的告警数量（0表示未触发）
 */
int am_evaluate(const char* metric_name, double value);

/**
 * @brief 评估所有规则
 * @return 触发的告警数量
 */
int am_evaluate_all(void);

/* ==================== 通知通道 ==================== */

/**
 * @brief 注册通知通道
 * @param channel 通道配置
 * @return 0成功，非0失败
 */
int am_register_channel(const am_channel_t* channel);

/**
 * @brief 注册告警回调
 * @param callback 回调函数
 * @param user_data 用户数据
 * @param min_level 最低告警级别
 * @return 0成功，非0失败
 */
int am_register_callback(am_alert_callback_t callback, void* user_data,
                         am_level_t min_level);

/* ==================== 查询 ==================== */

/**
 * @brief 获取活跃告警列表
 * @param alerts [out] 告警数组
 * @param max_count 数组最大容量
 * @param found_count [out] 实际数量
 * @return 0成功，非0失败
 */
int am_get_active_alerts(am_alert_t* alerts, uint32_t max_count,
                         uint32_t* found_count);

/**
 * @brief 获取指定级别的活跃告警
 * @param level 告警级别
 * @param alerts [out] 告警数组
 * @param max_count 数组最大容量
 * @param found_count [out] 实际数量
 * @return 0成功，非0失败
 */
int am_get_alerts_by_level(am_level_t level, am_alert_t* alerts,
                           uint32_t max_count, uint32_t* found_count);

/**
 * @brief 获取活跃告警数量
 * @return 活跃告警数量
 */
uint32_t am_active_alert_count(void);

/* ==================== 工具函数 ==================== */

/**
 * @brief 告警级别转字符串
 * @param level 级别
 * @return 级别名称
 */
const char* am_level_to_string(am_level_t level);

/**
 * @brief 告警状态转字符串
 * @param state 状态
 * @return 状态名称
 */
const char* am_state_to_string(am_state_t state);

/**
 * @brief 创建默认配置
 * @return 默认配置
 */
am_config_t am_create_default_config(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_ALERT_MANAGER_H */

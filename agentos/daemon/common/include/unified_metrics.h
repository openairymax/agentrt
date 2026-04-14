// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file unified_metrics.h
 * @brief 统一指标收集器 - 聚合所有守护进程指标
 *
 * 提供全局统一的指标收集入口，将所有守护进程的指标
 * 聚合到一个Prometheus端点，支持：
 * - 全局指标注册中心
 * - 按模块/守护进程分组导出
 * - Prometheus格式全覆盖导出
 * - 指标自动标注（模块名、实例ID等）
 *
 * @see metrics.h 基础指标收集
 */

#ifndef AGENTOS_UNIFIED_METRICS_H
#define AGENTOS_UNIFIED_METRICS_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 常量定义 ==================== */

#define UM_MAX_MODULES          32
#define UM_MAX_METRICS_PER_MOD  256
#define UM_MODULE_NAME_LEN      32
#define UM_METRIC_NAME_LEN      128

/* ==================== 指标类型 ==================== */

typedef enum {
    UM_TYPE_COUNTER   = 0,
    UM_TYPE_GAUGE     = 1,
    UM_TYPE_HISTOGRAM = 2,
    UM_TYPE_SUMMARY   = 3
} um_metric_type_t;

/* ==================== 指标条目 ==================== */

typedef struct {
    char name[UM_METRIC_NAME_LEN];
    char help[256];
    um_metric_type_t type;
    char labels[256];
    double value;
    double sum;
    uint64_t count;
    uint64_t timestamp_ms;
} um_metric_entry_t;

/* ==================== 模块指标集合 ==================== */

typedef struct {
    char module_name[UM_MODULE_NAME_LEN];
    char instance_id[32];
    um_metric_entry_t metrics[UM_MAX_METRICS_PER_MOD];
    uint32_t metric_count;
    bool active;
} um_module_metrics_t;

/* ==================== 统一指标配置 ==================== */

typedef struct {
    char service_name[64];
    uint32_t scrape_interval_ms;
    uint32_t retention_ms;
    bool enable_default_metrics;
} um_config_t;

/* ==================== 统一指标统计 ==================== */

typedef struct {
    uint64_t total_registrations;
    uint64_t total_exports;
    uint64_t total_increments;
    uint64_t total_updates;
    uint32_t active_modules;
    uint32_t total_metrics;
} um_stats_t;

/* ==================== 生命周期管理 ==================== */

/**
 * @brief 初始化统一指标收集器
 * @param config 配置参数（NULL使用默认）
 * @return 0成功，非0失败
 */
int um_init(const um_config_t* config);

/**
 * @brief 关闭统一指标收集器
 */
void um_shutdown(void);

/**
 * @brief 检查是否已初始化
 * @return true已初始化，false未初始化
 */
bool um_is_initialized(void);

/* ==================== 模块注册 ==================== */

/**
 * @brief 注册指标模块
 * @param module_name 模块名称（如"gateway_d"、"sched_d"）
 * @param instance_id 实例ID（NULL使用默认）
 * @return 0成功，非0失败
 */
int um_register_module(const char* module_name, const char* instance_id);

/**
 * @brief 注销指标模块
 * @param module_name 模块名称
 * @return 0成功，非0失败
 */
int um_unregister_module(const char* module_name);

/* ==================== 指标操作 ==================== */

/**
 * @brief 注册指标定义
 * @param module_name 模块名称
 * @param name 指标名称
 * @param type 指标类型
 * @param help 帮助文本
 * @param labels 标签（如"method=\"GET\",path=\"/api\""）
 * @return 0成功，非0失败
 */
int um_register_metric(
    const char* module_name,
    const char* name,
    um_metric_type_t type,
    const char* help,
    const char* labels
);

/**
 * @brief 增加计数器
 * @param module_name 模块名称
 * @param name 指标名称
 * @param value 增加值
 * @return 0成功，非0失败
 */
int um_increment(const char* module_name, const char* name, uint64_t value);

/**
 * @brief 设置仪表值
 * @param module_name 模块名称
 * @param name 指标名称
 * @param value 值
 * @return 0成功，非0失败
 */
int um_gauge_set(const char* module_name, const char* name, double value);

/**
 * @brief 观察直方图/摘要值
 * @param module_name 模块名称
 * @param name 指标名称
 * @param value 观察值
 * @return 0成功，非0失败
 */
int um_observe(const char* module_name, const char* name, double value);

/* ==================== 导出 ==================== */

/**
 * @brief 导出所有指标为Prometheus格式
 * @return Prometheus格式字符串（需调用者释放），失败返回NULL
 */
char* um_export_prometheus(void);

/**
 * @brief 导出指定模块指标为Prometheus格式
 * @param module_name 模块名称（NULL导出全部）
 * @return Prometheus格式字符串（需调用者释放），失败返回NULL
 */
char* um_export_prometheus_module(const char* module_name);

/**
 * @brief 导出所有指标为JSON格式
 * @return JSON字符串（需调用者释放），失败返回NULL
 */
char* um_export_json(void);

/* ==================== 默认指标 ==================== */

/**
 * @brief 注册默认系统指标（CPU/内存/线程等）
 * @return 0成功，非0失败
 */
int um_register_default_metrics(void);

/**
 * @brief 更新默认系统指标
 */
void um_update_default_metrics(void);

/* ==================== 统计 ==================== */

/**
 * @brief 获取统一指标统计
 * @param stats [out] 统计信息
 * @return 0成功，非0失败
 */
int um_get_stats(um_stats_t* stats);

/**
 * @brief 创建默认配置
 * @return 默认配置
 */
um_config_t um_create_default_config(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_UNIFIED_METRICS_H */

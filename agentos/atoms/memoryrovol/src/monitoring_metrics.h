/**
 * @file monitoring_metrics.h
 * @brief 监控指标管理接口 - 指标收集、更新、查询
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_MONITORING_METRICS_H
#define AGENTOS_MONITORING_METRICS_H

#include "../../atoms/corekern/include/agentos.h"
#include <stdint.h>

/* 前向声明 */
typedef struct metrics_collector metrics_collector_t;

/**
 * @brief 指标类型枚举
 */
typedef enum {
    METRIC_COUNTER = 0,      /**< 计数器 */
    METRIC_GAUGE,            /**< 仪表盘 */
    METRIC_HISTOGRAM,        /**< 直方图 */
    METRIC_SUMMARY           /**< 摘要 */
} metric_type_t;

/**
 * @brief 创建指标收集器
 * @return 指标收集器，失败返回 NULL
 */
metrics_collector_t* monitoring_metrics_create(void);

/**
 * @brief 销毁指标收集器
 * @param collector 指标收集器
 */
void monitoring_metrics_destroy(metrics_collector_t* collector);

/**
 * @brief 添加计数器指标
 * @param collector 指标收集器
 * @param name 指标名称
 * @param description 描述
 * @param unit 单位
 * @return AGENTOS_SUCCESS 成功，其他为错误码
 */
agentos_error_t monitoring_add_counter(metrics_collector_t* collector, const char* name,
                                       const char* description, const char* unit);

/**
 * @brief 添加仪表盘指标
 * @param collector 指标收集器
 * @param name 指标名称
 * @param description 描述
 * @param unit 单位
 * @return AGENTOS_SUCCESS 成功，其他为错误码
 */
agentos_error_t monitoring_add_gauge(metrics_collector_t* collector, const char* name,
                                     const char* description, const char* unit);

/**
 * @brief 更新指标值
 * @param collector 指标收集器
 * @param name 指标名称
 * @param value 值
 * @return AGENTOS_SUCCESS 成功，其他为错误码
 */
agentos_error_t monitoring_update_metric(metrics_collector_t* collector, const char* name, double value);

/**
 * @brief 获取指标值
 * @param collector 指标收集器
 * @param name 指标名称
 * @param out_value 输出值
 * @return AGENTOS_SUCCESS 成功，其他为错误码
 */
agentos_error_t monitoring_get_metric(metrics_collector_t* collector, const char* name, double* out_value);

/**
 * @brief 导出指标为 JSON
 * @param collector 指标收集器
 * @param out_json 输出 JSON 字符串（需要调用者释放）
 * @return AGENTOS_SUCCESS 成功，其他为错误码
 */
agentos_error_t monitoring_export_metrics_json(metrics_collector_t* collector, char** out_json);

#endif /* AGENTOS_MONITORING_METRICS_H */

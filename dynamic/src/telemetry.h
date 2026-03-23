/**
 * @file telemetry.h
 * @brief 可观测性接口（指标、追踪、日志）
 * 
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef DYNAMIC_TELEMETRY_H
#define DYNAMIC_TELEMETRY_H

#include <stdint.h>
#include "agentos.h"

/**
 * @brief 可观测性句柄
 */
typedef struct telemetry telemetry_t;

/**
 * @brief 指标类型
 */
typedef enum {
    METRIC_TYPE_COUNTER,      /**< 计数器（只增不减） */
    METRIC_TYPE_GAUGE,        /**< 仪表（可增可减） */
    METRIC_TYPE_HISTOGRAM     /**< 直方图 */
} metric_type_t;

/**
 * @brief 创建可观测性实例
 * @return 句柄，失败返回 NULL
 * 
 * @ownership 调用者需通过 telemetry_destroy() 释放
 */
telemetry_t* telemetry_create(void);

/**
 * @brief 销毁可观测性实例
 * @param t 句柄
 */
void telemetry_destroy(telemetry_t* t);

/**
 * @brief 注册计数器指标
 * 
 * @param t 句柄
 * @param name 指标名称
 * @param help 帮助文本
 * @param labels 标签（如 "method,endpoint"）
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t telemetry_register_counter(
    telemetry_t* t,
    const char* name,
    const char* help,
    const char* labels);

/**
 * @brief 注册仪表指标
 * 
 * @param t 句柄
 * @param name 指标名称
 * @param help 帮助文本
 * @param labels 标签
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t telemetry_register_gauge(
    telemetry_t* t,
    const char* name,
    const char* help,
    const char* labels);

/**
 * @brief 递增计数器
 * 
 * @param t 句柄
 * @param name 指标名称
 * @param value 递增值
 * @param label_values 标签值（逗号分隔）
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t telemetry_increment_counter(
    telemetry_t* t,
    const char* name,
    double value,
    const char* label_values);

/**
 * @brief 设置仪表值
 * 
 * @param t 句柄
 * @param name 指标名称
 * @param value 值
 * @param label_values 标签值
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t telemetry_set_gauge(
    telemetry_t* t,
    const char* name,
    double value,
    const char* label_values);

/**
 * @brief 观察直方图值
 * 
 * @param t 句柄
 * @param name 指标名称
 * @param value 观察值
 * @param label_values 标签值
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t telemetry_observe_histogram(
    telemetry_t* t,
    const char* name,
    double value,
    const char* label_values);

/**
 * @brief 导出 Prometheus 格式指标
 * 
 * @param t 句柄
 * @param out_metrics 输出字符串（需调用者 free）
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t telemetry_export_metrics(
    telemetry_t* t,
    char** out_metrics);

#endif /* DYNAMIC_TELEMETRY_H */

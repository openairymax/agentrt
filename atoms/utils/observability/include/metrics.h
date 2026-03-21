/**
 * @file metrics.h
 * @brief 指标收集接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#ifndef AGENTOS_UTILS_METRICS_H
#define AGENTOS_UTILS_METRICS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct agentos_metrics agentos_metrics_t;

/**
 * @brief 创建指标收集器
 * @return 收集器句柄，失败返回 NULL
 */
agentos_metrics_t* agentos_metrics_create(void);

/**
 * @brief 销毁收集器
 */
void agentos_metrics_destroy(agentos_metrics_t* metrics);

/**
 * @brief 增加计数器
 * @param metrics 收集器
 * @param name 指标名
 * @param value 增加值
 */
void agentos_metrics_increment(agentos_metrics_t* metrics, const char* name, uint64_t value);

/**
 * @brief 设置仪表值
 * @param metrics 收集器
 * @param name 指标名
 * @param value 值
 */
void agentos_metrics_gauge(agentos_metrics_t* metrics, const char* name, double value);

/**
 * @brief 记录耗时
 * @param metrics 收集器
 * @param name 指标名
 * @param duration_ms 耗时（毫秒）
 */
void agentos_metrics_timing(agentos_metrics_t* metrics, const char* name, double duration_ms);

/**
 * @brief 导出指标为JSON字符串
 * @param metrics 收集器
 * @return JSON字符串（需调用者释放），失败返回 NULL
 */
char* agentos_metrics_export(agentos_metrics_t* metrics);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_UTILS_METRICS_H */
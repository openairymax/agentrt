/**
 * @file observability.h
 * @brief 可观测性工具（日志、指标、追踪）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTRT_UTILS_OBSERVABILITY_H
#define AGENTRT_UTILS_OBSERVABILITY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 日志 ==================== */
/* Note: AGENTRT_LOG_LEVEL_* constants defined as enum in types.h (agentrt_log_level_t) */

#ifndef AGENTRT_LOG_LEVEL
#define AGENTRT_LOG_LEVEL AGENTRT_LOG_LEVEL_INFO
#endif

/**
 * @brief 设置当前追踪ID
 * @param trace_id 追踪ID（若为NULL则自动生成）
 * @return 设置的追踪ID
 */
const char *agentrt_log_set_trace_id(const char *trace_id);

/**
 * @brief 获取当前追踪ID
 * @return 追踪ID，可能为NULL
 */
const char *agentrt_log_get_trace_id(void);

/**
 * @brief 记录日志
 * @param level 日志级别
 * @param file 文件名（通常用 __FILE__）
 * @param line 行号（通常用 __LINE__）
 * @param fmt 格式字符串
 * @param ... 参数
 */
void agentrt_log_write(int level, const char *file, int line, const char *fmt, ...);

#ifndef AGENTRT_LOG_ERROR
#define AGENTRT_LOG_ERROR(fmt, ...) \
    agentrt_log_write(AGENTRT_LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#endif
#ifndef AGENTRT_LOG_WARN
#define AGENTRT_LOG_WARN(fmt, ...) \
    agentrt_log_write(AGENTRT_LOG_LEVEL_WARN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#endif
#ifndef AGENTRT_LOG_INFO
#define AGENTRT_LOG_INFO(fmt, ...) \
    agentrt_log_write(AGENTRT_LOG_LEVEL_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#endif
#ifndef AGENTRT_LOG_DEBUG
#define AGENTRT_LOG_DEBUG(fmt, ...) \
    agentrt_log_write(AGENTRT_LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#endif

/* ==================== 指标 ==================== */

/**
 * @brief 指标收集器句柄
 */
typedef struct agentrt_metrics agentrt_metrics_t;

/**
 * @brief 创建指标收集器
 * @return 收集器句柄，失败返回NULL
 */
agentrt_metrics_t *agentrt_metrics_create(void);

/**
 * @brief 销毁指标收集器
 * @param metrics 收集器句柄
 */
void agentrt_metrics_destroy(agentrt_metrics_t *metrics);

/**
 * @brief 增加计数器
 * @param metrics 收集器
 * @param name 指标名
 * @param value 增加值
 */
void agentrt_metrics_increment(agentrt_metrics_t *metrics, const char *name, uint64_t value);

/**
 * @brief 设置仪表值
 * @param metrics 收集器
 * @param name 指标名
 * @param value 值
 */
void agentrt_metrics_gauge(agentrt_metrics_t *metrics, const char *name, double value);

/**
 * @brief 记录耗时
 * @param metrics 收集器
 * @param name 指标名
 * @param duration_ms 耗时（毫秒）
 */
void agentrt_metrics_timing(agentrt_metrics_t *metrics, const char *name, double duration_ms);

/**
 * @brief 导出指标为JSON
 * @param metrics 收集器
 * @return JSON字符串（需调用者释放），失败返回NULL
 */
char *agentrt_metrics_export(agentrt_metrics_t *metrics);

/* ==================== 追踪 ==================== */

/**
 * @brief 追踪跨度句柄
 */
typedef struct agentrt_trace_span agentrt_trace_span_t;

/**
 * @brief 开始一个跨度
 * @param name 跨度名
 * @param parent_id 父跨度ID（可为NULL）
 * @return 跨度句柄，失败返回NULL
 */
agentrt_trace_span_t *agentrt_trace_begin(const char *name, const char *parent_id);

/**
 * @brief 结束一个跨度
 * @param span 跨度句柄
 */
void agentrt_trace_end(agentrt_trace_span_t *span);

/**
 * @brief 向跨度添加事件
 * @param span 跨度句柄
 * @param name 事件名
 * @param attributes JSON格式的属性（可为NULL）
 */
void agentrt_trace_add_event(agentrt_trace_span_t *span, const char *name, const char *attributes);

/**
 * @brief 导出追踪数据为JSON（通常用于调试）
 * @return JSON字符串（需调用者释放），失败返回NULL
 */
char *agentrt_trace_export(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_UTILS_OBSERVABILITY_H */
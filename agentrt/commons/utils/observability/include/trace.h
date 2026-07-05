/**
 * @file trace.h
 * @brief 链路追踪接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTRT_UTILS_TRACE_H
#define AGENTRT_UTILS_TRACE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct agentrt_trace_span agentrt_trace_span_t;

/**
 * @brief 开始一个追踪跨度
 * @param name 跨度名称
 * @param parent_id 父跨度ID（可为 NULL）
 * @return 跨度句柄，失败返回 NULL
 */
agentrt_trace_span_t *agentrt_trace_begin(const char *name, const char *parent_id);

/**
 * @brief 结束一个跨度
 // From data intelligence emerges. by spharx
 * @param span 跨度句柄
 */
void agentrt_trace_end(agentrt_trace_span_t *span);

/**
 * @brief 向跨度添加事件
 * @param span 跨度句柄
 * @param name 事件名
 * @param attributes JSON格式的属性（可为 NULL）
 */
void agentrt_trace_add_event(agentrt_trace_span_t *span, const char *name, const char *attributes);

/**
 * @brief 导出所有追踪数据为JSON（用于调试）
 * @return JSON字符串，需调用者释放，失败返回 NULL
 */
char *agentrt_trace_export(void);

/**
 * @brief 清理所有追踪数据
 */
void agentrt_trace_cleanup(void);

/**
 * @brief 获取当前追踪span数量
 * @return span数量
 */
int agentrt_trace_get_span_count(void);

/* ==================== Span 字段访问器（用于持久化） ==================== */

/**
 * @brief 获取 span 的追踪 ID
 * @param span 跨度句柄（必须非 NULL）
 * @return 追踪 ID 字符串（只读，span 生命周期内有效）
 */
const char *agentrt_trace_span_get_trace_id(const agentrt_trace_span_t *span);

/**
 * @brief 获取 span 的 ID
 * @param span 跨度句柄（必须非 NULL）
 * @return Span ID 字符串（只读，span 生命周期内有效）
 */
const char *agentrt_trace_span_get_span_id(const agentrt_trace_span_t *span);

/**
 * @brief 获取 span 的父 ID
 * @param span 跨度句柄（必须非 NULL）
 * @return 父 Span ID 字符串（只读，可能为空字符串）
 */
const char *agentrt_trace_span_get_parent_id(const agentrt_trace_span_t *span);

/**
 * @brief 获取 span 的名称
 * @param span 跨度句柄（必须非 NULL）
 * @return 名称字符串（只读，span 生命周期内有效）
 */
const char *agentrt_trace_span_get_name(const agentrt_trace_span_t *span);

/**
 * @brief 获取 span 的开始时间
 * @param span 跨度句柄（必须非 NULL）
 * @return 开始时间（微秒）
 */
int64_t agentrt_trace_span_get_start_time_us(const agentrt_trace_span_t *span);

/**
 * @brief 获取 span 的结束时间
 * @param span 跨度句柄（必须非 NULL）
 * @return 结束时间（微秒），0 表示仍在运行
 */
int64_t agentrt_trace_span_get_end_time_us(const agentrt_trace_span_t *span);

/**
 * @brief 获取 span 的状态
 * @param span 跨度句柄（必须非 NULL）
 * @return 状态：0=运行中, 1=完成, 2=错误
 */
int agentrt_trace_span_get_status(const agentrt_trace_span_t *span);

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_UTILS_TRACE_H */
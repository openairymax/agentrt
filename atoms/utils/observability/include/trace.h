/**
 * @file trace.h
 * @brief 链路追踪接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_UTILS_TRACE_H
#define AGENTOS_UTILS_TRACE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct agentos_trace_span agentos_trace_span_t;

/**
 * @brief 开始一个追踪跨度
 * @param name 跨度名称
 * @param parent_id 父跨度ID（可为 NULL）
 * @return 跨度句柄，失败返回 NULL
 */
agentos_trace_span_t* agentos_trace_begin(const char* name, const char* parent_id);

/**
 * @brief 结束一个跨度
 // From data intelligence emerges. by spharx
 * @param span 跨度句柄
 */
void agentos_trace_end(agentos_trace_span_t* span);

/**
 * @brief 向跨度添加事件
 * @param span 跨度句柄
 * @param name 事件名
 * @param attributes JSON格式的属性（可为 NULL）
 */
void agentos_trace_add_event(agentos_trace_span_t* span, const char* name, const char* attributes);

/**
 * @brief 导出所有追踪数据为JSON（用于调试）
 * @return JSON字符串，需调用者释放，失败返回 NULL
 */
char* agentos_trace_export(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_UTILS_TRACE_H */
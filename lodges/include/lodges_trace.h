/**
 * @file lodges_trace.h
 * @brief AgentOS 数据分区追踪数据存储接口
 *
 * Copyright (c) 2026 SPHARX. All Rights Reserved.
 * "From data intelligence emerges."
 */

#ifndef AGENTOS_lodges_TRACE_H
#define AGENTOS_lodges_TRACE_H

#include "lodges.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Span 记录结构
 */
typedef struct lodges_span {
    char trace_id[64];
    char span_id[32];
    char parent_span_id[32];
    char name[128];
    uint64_t start_time_ns;
    uint64_t end_time_ns;
    char service_name[64];
    char status[32];
    void* attributes;
    size_t attribute_count;
} lodges_span_t;

/**
 * @brief 追踪导出器配置
 */
typedef struct lodges_trace_exporter_config {
    bool enabled;
    char export_path[256];
    size_t batch_size;
    uint32_t export_interval_sec;
    char export_format[16];
} lodges_trace_exporter_config_t;

/**
 * @brief 初始化追踪存储系统
 *
 * @return lodges_error_t 错误码
 */
lodges_error_t lodges_trace_init(void);

/**
 * @brief 关闭追踪存储系统
 */
void lodges_trace_shutdown(void);

/**
 * @brief 写入 Span 记录
 *
 * @param span Span 记录
 * @return lodges_error_t 错误码
 */
lodges_error_t lodges_trace_write_span(const lodges_span_t* span);

/**
 * @brief 批量写入 Span 记录
 *
 * @param spans Span 数组
 * @param count Span 数量
 * @return lodges_error_t 错误码
 */
lodges_error_t lodges_trace_write_spans_batch(const lodges_span_t* spans, size_t count);

/**
 * @brief 根据 trace_id 查询所有 span
 *
 * @param trace_id 追踪 ID
 * @param spans 输出 span 数组（需调用者释放）
 * @param count 输出 span 数量
 * @return lodges_error_t 错误码
 */
lodges_error_t lodges_trace_query_by_trace(const char* trace_id, lodges_span_t** spans, size_t* count);

/**
 * @brief 根据时间范围查询 span
 *
 * @param start_time 开始时间（纳秒）
 * @param end_time 结束时间（纳秒）
 * @param spans 输出 span 数组（需调用者释放）
 * @param count 输出 span 数量
 * @return lodges_error_t 错误码
 */
lodges_error_t lodges_trace_query_by_time_range(uint64_t start_time, uint64_t end_time, lodges_span_t** spans, size_t* count);

/**
 * @brief 释放 span 数组内存
 *
 * @param spans span 数组
 */
void lodges_trace_free_spans(lodges_span_t* spans);

/**
 * @brief 配置追踪导出器
 *
 * @param manager 导出器配置
 * @return lodges_error_t 错误码
 */
lodges_error_t lodges_trace_config_exporter(const lodges_trace_exporter_config_t* manager);

/**
 * @brief 强制导出待发送的追踪数据
 *
 * @return lodges_error_t 错误码
 */
lodges_error_t lodges_trace_flush(void);

/**
 * @brief 获取追踪存储统计信息
 *
 * @param total_spans 输出总 span 数
 * @param pending_spans 输出待导出 span 数
 * @param total_size_bytes 输出总存储大小
 * @return lodges_error_t 错误码
 */
lodges_error_t lodges_trace_get_stats(uint64_t* total_spans, uint64_t* pending_spans, uint64_t* total_size_bytes);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_lodges_TRACE_H */


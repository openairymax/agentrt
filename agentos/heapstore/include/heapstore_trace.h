/**
 * @file heapstore_trace.h
 * @brief AgentOS 数据分区追踪数据存储接口
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * "From data intelligence emerges."
 */

#ifndef AGENTOS_heapstore_TRACE_H
#define AGENTOS_heapstore_TRACE_H

#include "heapstore.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Span 记录结构
 */
typedef struct heapstore_span {
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
} heapstore_span_t;

/**
 * @brief 追踪导出器配置
 */
typedef struct heapstore_trace_exporter_config {
    bool enabled;
    char export_path[256];
    size_t batch_size;
    uint32_t export_interval_sec;
    char export_format[16];
} heapstore_trace_exporter_config_t;

/**
 * @brief 初始化追踪存储系统
 *
 * @return heapstore_error_t 错误码
 *
 * @ownership 内部管理所有资源
 * @threadsafe 否，不可多线程同时调用
 * @reentrant 否
 *
 * @see heapstore_trace_shutdown()
 * @since v1.0.0
 */
heapstore_error_t heapstore_trace_init(void);

/**
 * @brief 关闭追踪存储系统
 *
 * @ownership 内部释放所有资源
 * @threadsafe 否
 * @reentrant 否
 *
 * @see heapstore_trace_init()
 * @since v1.0.0
 */
void heapstore_trace_shutdown(void);

/**
 * @brief 写入 Span 记录
 *
 * @param span [in] Span 记录
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责 span 的生命周期
 * @threadsafe 是
 * @reentrant 否
 */
heapstore_error_t heapstore_trace_write_span(const heapstore_span_t* span);

/**
 * @brief 批量写入 Span 记录
 *
 * @param spans [in] Span 数组
 * @param count [in] Span 数量
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责 spans 的生命周期
 * @threadsafe 是
 * @reentrant 否

 * @since v1.0.0*/
heapstore_error_t heapstore_trace_write_spans_batch(const heapstore_span_t* spans, size_t count);

/**
 * @brief 根据 trace_id 查询所有 span
 *
 * @param trace_id [in] 追踪 ID
 * @param spans [out] 输出 span 数组（需调用者释放）
 * @param count [out] 输出 span 数量
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责调用 heapstore_trace_free_spans 释放 spans
 * @threadsafe 是
 * @reentrant 否

 * @since v1.0.0*/
heapstore_error_t heapstore_trace_query_by_trace(const char* trace_id, heapstore_span_t** spans, size_t* count);

/**
 * @brief 根据时间范围查询 span
 *
 * @param start_time [in] 开始时间（纳秒）
 * @param end_time [in] 结束时间（纳秒）
 * @param spans [out] 输出 span 数组（需调用者释放）
 * @param count [out] 输出 span 数量
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责调用 heapstore_trace_free_spans 释放 spans
 * @threadsafe 是
 * @reentrant 否

 * @since v1.0.0*/
heapstore_error_t heapstore_trace_query_by_time_range(uint64_t start_time, uint64_t end_time, heapstore_span_t** spans, size_t* count);

/**
 * @brief 释放 span 数组内存
 *
 * @param spans [in] span 数组
 *
 * @ownership 调用者负责传入有效的 spans 指针
 * @threadsafe 是
 * @reentrant 是

 * @since v1.0.0*/
void heapstore_trace_free_spans(heapstore_span_t* spans);

/**
 * @brief 配置追踪导出器
 *
 * @param manager [in] 导出器配置
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责 manager 的生命周期
 * @threadsafe 是
 * @reentrant 否

 * @since v1.0.0*/
heapstore_error_t heapstore_trace_config_exporter(const heapstore_trace_exporter_config_t* manager);

/**
 * @brief 强制导出待发送的追踪数据
 *
 * @return heapstore_error_t 错误码
 *
 * @threadsafe 是
 * @reentrant 否

 * @since v1.0.0*/
heapstore_error_t heapstore_trace_flush(void);

/**
 * @brief 获取追踪存储统计信息
 *
 * @param total_spans [out] 输出总 span 数
 * @param pending_spans [out] 输出待导出 span 数
 * @param total_size_bytes [out] 输出总存储大小
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责所有输出参数的分配和释放
 * @threadsafe 是
 * @reentrant 是

 * @since v1.0.0*/
heapstore_error_t heapstore_trace_get_stats(uint64_t* total_spans, uint64_t* pending_spans, uint64_t* total_size_bytes);

/**
 * @brief 清理过期追踪数据
 *
 * @param days_to_keep [in] 保留天数
 * @param freed_bytes [out] 释放的字节数
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责 freed_bytes 的分配和释放
 * @threadsafe 是
 * @reentrant 否

 * @since v1.0.0*/
heapstore_error_t heapstore_trace_cleanup(int days_to_keep, uint64_t* freed_bytes);

/**
 * @brief 检查追踪系统是否健康
 *
 * @return bool 健康返回 true
 *
 * @threadsafe 是
 * @reentrant 是

 * @since v1.0.0*/
bool heapstore_trace_is_healthy(void);

/**
 * @brief 将所有追踪数据导出为 JSON 字符串
 *
 * @param out_json [out] 输出的 JSON 字符串（需调用者释放）
 * @param include_events [in] 是否包含事件信息（预留参数）
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责调用 free() 释放 out_json
 * @threadsafe 是
 * @reentrant 否

 * @since v1.0.0*/
heapstore_error_t heapstore_trace_export_to_json(char** out_json, bool include_events);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_heapstore_TRACE_H */

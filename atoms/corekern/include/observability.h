﻿/**
 * @file observability.h
 * @brief AgentOS 微内核可观测性子系统
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * 
 * 提供生产级可观测性功能，包括指标收集、健康检查、性能监控、
 * 分布式追踪和日志聚合。支持99.999%可靠性标准的监控需求。
 */

#ifndef AGENTOS_OBSERVABILITY_H
#define AGENTOS_OBSERVABILITY_H

#include "export.h"
#include "error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 指标类型枚举
 * @note 支持多种指标类型，满足生产级监控需求
 */
typedef enum {
    AGENTOS_METRIC_COUNTER,      /**< 计数器，单调递增 */
    AGENTOS_METRIC_GAUGE,        /**< 仪表，可增可减 */
    AGENTOS_METRIC_HISTOGRAM,    /**< 直方图，统计分布 */
    AGENTOS_METRIC_SUMMARY,      /**< 摘要，分位数统计 */
} agentos_metric_type_t;

/**
 * @brief 健康检查状态
 */
typedef enum {
    AGENTOS_HEALTH_PASS,         /**< 健康检查通过 */
    AGENTOS_HEALTH_WARN,         /**< 健康检查警告，可继续运行 */
    AGENTOS_HEALTH_FAIL,         /**< 健康检查失败，需要干预 */
} agentos_health_status_t;

/**
 * @brief 追踪上下文结构
 * @note 用于分布式追踪，支持OpenTelemetry标准
 */
typedef struct agentos_trace_context {
    char trace_id[32];           /**< 追踪ID，16字节十六进制 */
    char span_id[16];            /**< 跨度ID，8字节十六进制 */
    char parent_span_id[16];     /**< 父跨度ID，8字节十六进制 */
    uint64_t start_ns;           /**< 开始时间（纳秒） */
    uint64_t end_ns;             /**< 结束时间（纳秒） */
    char service_name[64];       /**< 服务名称 */
    char operation_name[128];    /**< 操作名称 */
    int error_code;              /**< 错误码 */
} agentos_trace_context_t;

/**
 * @brief 健康检查回调函数类型
 * @param user_data 用户数据
 * @return 健康检查状态
 */
typedef agentos_health_status_t (*agentos_health_check_cb)(void* user_data);

/**
 * @brief 指标样本结构
 */
typedef struct {
    char name[128];              /**< 指标名称 */
    double value;                /**< 指标值 */
    agentos_metric_type_t type;  /**< 指标类型 */
    uint64_t timestamp_ns;       /**< 时间戳（纳秒） */
    char labels[256];            /**< 标签，JSON格式 */
} agentos_metric_sample_t;

/**
 * @brief 可观测性配置结构
 */
typedef struct {
    int enable_metrics;          /**< 是否启用指标收集 */
    int enable_tracing;          /**< 是否启用分布式追踪 */
    int enable_health_check;     /**< 是否启用健康检查 */
    int metrics_interval_ms;     /**< 指标收集间隔（毫秒） */
    int health_check_interval_ms;/**< 健康检查间隔（毫秒） */
    char metrics_endpoint[256];  /**< 指标上报端点 */
    char tracing_endpoint[256];  /**< 追踪上报端点 */
} agentos_observability_config_t;

/**
 * @brief 初始化可观测性子系统
 * @param manager 配置参数
 * @return 成功返回AGENTOS_SUCCESS，失败返回错误码
 * @note 线程安全，可重复调用
 */
AGENTOS_API int agentos_observability_init(const agentos_observability_config_t* manager);

/**
 * @brief 关闭可观测性子系统
 * @note 释放所有资源，停止所有监控线程
 */
AGENTOS_API void agentos_observability_shutdown(void);

/**
 * @brief 注册健康检查回调
 * @param name 健康检查名称
 * @param callback 回调函数
 * @param user_data 用户数据
 * @return 成功返回AGENTOS_SUCCESS，失败返回错误码
 */
AGENTOS_API int agentos_health_check_register(const char* name, 
                                             agentos_health_check_cb callback,
                                             void* user_data);

/**
 * @brief 执行健康检查
 * @param timeout_ms 超时时间（毫秒）
 * @return 整体健康状态
 */
AGENTOS_API agentos_health_status_t agentos_health_check_run(int timeout_ms);

/**
 * @brief 记录指标样本
 * @param sample 指标样本
 * @return 成功返回AGENTOS_SUCCESS，失败返回错误码
 */
AGENTOS_API int agentos_metric_record(const agentos_metric_sample_t* sample);

/**
 * @brief 创建计数器指标
 * @param name 指标名称
 * @param labels 标签（JSON格式）
 * @return 成功返回AGENTOS_SUCCESS，失败返回错误码
 */
AGENTOS_API int agentos_metric_counter_create(const char* name, const char* labels);

/**
 * @brief 计数器指标递增
 * @param name 指标名称
 * @param labels 标签（JSON格式）
 * @param value 递增值
 * @return 成功返回AGENTOS_SUCCESS，失败返回错误码
 */
AGENTOS_API int agentos_metric_counter_inc(const char* name, const char* labels, double value);

/**
 * @brief 创建仪表指标
 * @param name 指标名称
 * @param labels 标签（JSON格式）
 * @param initial_value 初始值
 * @return 成功返回AGENTOS_SUCCESS，失败返回错误码
 */
AGENTOS_API int agentos_metric_gauge_create(const char* name, const char* labels, double initial_value);

/**
 * @brief 仪表指标设置值
 * @param name 指标名称
 * @param labels 标签（JSON格式）
 * @param value 新值
 * @return 成功返回AGENTOS_SUCCESS，失败返回错误码
 */
AGENTOS_API int agentos_metric_gauge_set(const char* name, const char* labels, double value);

/**
 * @brief 开始追踪跨度
 * @param context 追踪上下文
 * @param service_name 服务名称
 * @param operation_name 操作名称
 * @return 成功返回AGENTOS_SUCCESS，失败返回错误码
 */
AGENTOS_API int agentos_trace_span_start(agentos_trace_context_t* context,
                                        const char* service_name,
                                        const char* operation_name);

/**
 * @brief 结束追踪跨度
 * @param context 追踪上下文
 * @param error_code 错误码（0表示成功）
 * @return 成功返回AGENTOS_SUCCESS，失败返回错误码
 */
AGENTOS_API int agentos_trace_span_end(agentos_trace_context_t* context, int error_code);

/**
 * @brief 设置追踪标签
 * @param context 追踪上下文
 * @param key 标签键
 * @param value 标签值
 * @return 成功返回AGENTOS_SUCCESS，失败返回错误码
 */
AGENTOS_API int agentos_trace_set_tag(agentos_trace_context_t* context,
                                     const char* key, const char* value);

/**
 * @brief 记录追踪日志
 * @param context 追踪上下文
 * @param message 日志消息
 * @return 成功返回AGENTOS_SUCCESS，失败返回错误码
 */
AGENTOS_API int agentos_trace_log(agentos_trace_context_t* context,
                                 const char* message);

/**
 * @brief 获取系统性能指标
 * @param out_cpu_usage 输出CPU使用率（0-100）
 * @param out_memory_usage 输出内存使用率（0-100）
 * @param out_thread_count 输出线程数量
 * @return 成功返回AGENTOS_SUCCESS，失败返回错误码
 */
AGENTOS_API int agentos_performance_get_metrics(double* out_cpu_usage,
                                               double* out_memory_usage,
                                               int* out_thread_count);

/**
 * @brief 导出所有指标为Prometheus格式
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return 成功返回写入的字节数，失败返回负数
 */
AGENTOS_API int agentos_metrics_export_prometheus(char* buffer, size_t buffer_size);

/**
 * @brief 导出健康检查状态
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return 成功返回写入的字节数，失败返回负数
 */
AGENTOS_API int agentos_health_export_status(char* buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_OBSERVABILITY_H */

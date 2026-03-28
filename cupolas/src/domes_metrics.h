/**
 * @file domes_metrics.h
 * @brief 性能指标导出 - Prometheus 格式
 * @author Spharx
 * @date 2024
 *
 * 设计原则：
 * - 标准格式：Prometheus exposition format
 * - 低开销：原子操作，无锁采样
 * - 可配置：采样间隔、保留窗口
 * - 多维度：支持标签/labels
 *
 * 导出指标：
 * - domes_permissions_total{agent, action, resource, result}
 * - domes_permissions_duration_seconds{agent, action, resource}
 * - domes_sanitizer_input_total{type, result}
 * - domes_sanitizer_duration_seconds{type}
 * - domes_workbench_execution_total{command, result}
 * - domes_workbench_duration_seconds{command}
 * - domes_workbench_memory_bytes{command}
 * - domes_workbench_cpu_seconds{command}
 * - domes_errors_total{module, error_type}
 */

#ifndef DOMES_METRICS_H
#define DOMES_METRICS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 指标类型 */
typedef enum metric_type {
    METRIC_TYPE_COUNTER = 0,
    METRIC_TYPE_GAUGE,
    METRIC_TYPE_HISTOGRAM,
    METRIC_TYPE_SUMMARY
} metric_type_t;

/* 指标描述符 */
typedef struct metric_desc {
    const char* name;
    const char* help;
    metric_type_t type;
    const char* const* label_names;
    size_t label_count;
    double* buckets;
    size_t bucket_count;
} metric_desc_t;

/* 指标值 */
typedef struct metric_value {
    double value;
    uint64_t timestamp_ns;
} metric_value_t;

/* histogram 桶 */
typedef struct histogram_bucket {
    double cumulative_count;
    double sum;
} histogram_bucket_t;

/* 指标样本 */
typedef struct metric_sample {
    const char* name;
    const char** label_values;
    size_t label_count;
    double value;
    uint64_t timestamp_ns;
} metric_sample_t;

/* 指标迭代器 */
typedef struct metric_iterator metric_iterator_t;

/**
 * @brief 初始化指标系统
 * @param sampling_interval_ms 采样间隔（毫秒）
 * @return 0 成功，其他失败
 */
int metrics_init(uint32_t sampling_interval_ms);

/**
 * @brief 关闭指标系统
 */
void metrics_shutdown(void);

/**
 * @brief 注册指标描述符
 * @param desc 指标描述符
 * @return 0 成功，其他失败
 */
int metrics_register(const metric_desc_t* desc);

/**
 * @brief 增加计数器
 * @param name 指标名称
 * @param label_values 标签值数组
 * @param count 增加量
 */
void metrics_counter_inc(const char* name, const char** label_values, double count);

/**
 * @brief 设置 gauge 值
 * @param name 指标名称
 * @param label_values 标签值数组
 * @param value gauge 值
 */
void metrics_gauge_set(const char* name, const char** label_values, double value);

/**
 * @brief 增加 gauge 值
 * @param name 指标名称
 * @param label_values 标签值数组
 * @param value 增加量
 */
void metrics_gauge_add(const char* name, const char** label_values, double value);

/**
 * @brief 减少 gauge 值
 * @param name 指标名称
 * @param label_values 标签值数组
 * @param value 减少量
 */
void metrics_gauge_sub(const char* name, const char** label_values, double value);

/**
 * @brief 观察 histogram 值
 * @param name 指标名称
 * @param label_values 标签值数组
 * @param value 观察值
 */
void metrics_histogram_observe(const char* name, const char** label_values, double value);

/**
 * @brief 观察 summary 值
 * @param name 指标名称
 * @param label_values 标签值数组
 * @param value 观察值
 */
void metrics_summary_observe(const char* name, const char** label_values, double value);

/**
 * @brief 获取当前时间戳（纳秒）
 * @return 当前时间戳
 */
uint64_t metrics_get_timestamp_ns(void);

/**
 * @brief 创建指标迭代器
 * @param pattern 匹配模式（NULL 表示所有）
 * @return 迭代器句柄，失败返回 NULL
 */
metric_iterator_t* metrics_iter_create(const char* pattern);

/**
 * @brief 获取迭代器下一个样本
 * @param iter 迭代器
 * @param sample 样本输出
 * @return true 有更多样本，false 已结束
 */
bool metrics_iter_next(metric_iterator_t* iter, metric_sample_t* sample);

/**
 * @brief 销毁迭代器
 * @param iter 迭代器
 */
void metrics_iter_destroy(metric_iterator_t* iter);

/**
 * @brief 导出所有指标为 Prometheus 格式
 * @param buffer 输出缓冲区
 * @param size 缓冲区大小
 * @return 实际写入的字节数
 */
size_t metrics_export_prometheus(char* buffer, size_t size);

/**
 * @brief 导出所有指标为 JSON 格式
 * @param buffer 输出缓冲区
 * @param size 缓冲区大小
 * @return 实际写入的字节数
 */
size_t metrics_export_json(char* buffer, size_t size);

/**
 * @brief 重置所有指标
 */
void metrics_reset(void);

/**
 * @brief 获取指标数量
 * @return 指标数量
 */
size_t metrics_get_count(void);

/**
 * @brief 获取采样间隔
 * @return 采样间隔（毫秒）
 */
uint32_t metrics_get_sampling_interval(void);

/* ============================================================================
 * 预定义指标
 * ============================================================================ */

/* 权限系统指标 */
extern const char* METRIC_PERMISSIONS_TOTAL;
extern const char* METRIC_PERMISSIONS_DURATION_SECONDS;
extern const char* METRIC_PERMISSIONS_CACHE_HITS;
extern const char* METRIC_PERMISSIONS_CACHE_MISSES;

/* 净化器指标 */
extern const char* METRIC_SANITIZER_INPUT_TOTAL;
extern const char* METRIC_SANITIZER_DURATION_SECONDS;
extern const char* METRIC_SANITIZER_REJECTED_TOTAL;

/* 工作台指标 */
extern const char* METRIC_WORKBENCH_EXECUTIONS_TOTAL;
extern const char* METRIC_WORKBENCH_DURATION_SECONDS;
extern const char* METRIC_WORKBENCH_MEMORY_BYTES;
extern const char* METRIC_WORKBENCH_CPU_SECONDS;
extern const char* METRIC_WORKBENCH_OOM_KILLS;

/* 审计日志指标 */
extern const char* METRIC_AUDIT_EVENTS_TOTAL;
extern const char* METRIC_AUDIT_QUEUE_SIZE;
extern const char* METRIC_AUDIT_BYTES_WRITTEN;

/* 错误指标 */
extern const char* METRIC_ERRORS_TOTAL;

/* 系统指标 */
extern const char* METRIC_PROCESS_MEMORY_BYTES;
extern const char* METRIC_PROCESS_CPU_SECONDS;
extern const char* METRIC_THREAD_COUNT;

#ifdef __cplusplus
}
#endif

#endif /* DOMES_METRICS_H */
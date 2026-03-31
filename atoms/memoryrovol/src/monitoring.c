/**
 * @file monitoring.c
 * @brief MemoryRovol 监控与可观测性子系统
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * MemoryRovol 监控子系统提供四层记忆架构的全面可观测性，支持生产级
 * 99.999%可靠性标准。实现指标收集、健康检查、性能分析和预警机制。
 *
 * 核心功能：
 * 1. 分层指标收集：L1-L4各层的读写统计、延迟指标、容量使用
 * 2. 检索性能监控：查询延迟、召回率、缓存命中率
 * 3. 记忆演化追踪：模式挖掘进度、规则生成统计
 * 4. 资源使用监控：内存消耗、磁盘使用、线程状态
 * 5. 健康状态评估：各组件健康度、依赖服务状态
 * 6. 预警与告警：阈值检测、异常通知、自动恢复
 * 7. 分布式追踪：跨层调用链追踪、上下文传播
 */

#include "memoryrovol.h"
#include "manager.h"
#include "layer1_raw.h"
#include "layer2_feature.h"
#include "layer3_structure.h"
#include "layer4_pattern.h"
#include "retrieval.h"
#include "forgetting.h"
#include "agentos.h"
#include "logger.h"
#include "observability.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../commons/utils/memory/include/memory_compat.h"
#include "../../../commons/utils/string/include/string_compat.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <cjson/cJSON.h>

/* ==================== 内部常量定义 ==================== */

/** @brief 最大监控指标数量 */
#define MAX_METRICS 256

/** @brief 监控数据保留时长（秒） */
#define MONITORING_RETENTION_SECONDS 3600

/** @brief 默认监控间隔（毫秒） */
#define DEFAULT_MONITORING_INTERVAL_MS 5000

/** @brief 健康检查超时（毫秒） */
#define HEALTH_CHECK_TIMEOUT_MS 3000

/** @brief 预警阈值：高延迟警告（毫秒） */
#define WARNING_HIGH_LATENCY_MS 1000

/** @brief 预警阈值：错误率警告（百分比） */
#define WARNING_ERROR_RATE_PERCENT 5.0

/** @brief 预警阈值：内存使用警告（百分比） */
#define WARNING_MEMORY_USAGE_PERCENT 80.0

/* ==================== 内部数据结构 ==================== */

/**
 * @brief 监控指标类型
 */
typedef enum {
    METRIC_TYPE_COUNTER = 0,      /**< 计数器（只增不减） */
    METRIC_TYPE_GAUGE,            /**< 仪表盘（可增可减） */
    METRIC_TYPE_HISTOGRAM,        /**< 直方图（分布统计） */
    METRIC_TYPE_SUMMARY           /**< 摘要（分位数统计） */
} metric_type_t;

/**
 * @brief 监控指标定义
 */
typedef struct monitoring_metric {
    char* name;                    /**< 指标名称 */
    metric_type_t type;            /**< 指标类型 */
    char* description;             /**< 指标描述 */
    char* unit;                    /**< 指标单位 */
    union {
        uint64_t counter;          /**< 计数器值 */
        double gauge;              /**< 仪表盘值 */
        struct {
            double sum;            /**< 总和 */
            uint64_t count;       /**< 计数 */
            double* buckets;      /**< 桶数组 */
            size_t bucket_count;  /**< 桶数量 */
        } histogram;               /**< 直方图数据 */
    } value;                     /**< 指标值 */
    uint64_t timestamp_ns;        /**< 最后更新时间戳 */
    struct monitoring_metric* next; /**< 下一个指标 */
} monitoring_metric_t;

/**
 * @brief 层监控数据
 */
typedef struct layer_monitoring_data {
    uint64_t write_count;          /**< 写入次数 */
    uint64_t read_count;           /**< 读取次数 */
    uint64_t delete_count;         /**< 删除次数 */
    uint64_t total_write_bytes;    /**< 总写入字节数 */
    uint64_t total_read_bytes;     /**< 总读取字节数 */
    uint64_t total_write_time_ns;  /**< 总写入耗时 */
    uint64_t total_read_time_ns;   /**< 总读取耗时 */
    uint64_t error_count;          /**< 错误次数 */
    uint64_t current_items;        /**< 当前项目数 */
    uint64_t max_items;            /**< 最大项目数 */
    uint64_t last_cleanup_ns;      /**< 最后清理时间戳 */
} layer_monitoring_data_t;

/**
 * @brief 检索监控数据
 */
typedef struct retrieval_monitoring_data {
    uint64_t query_count;          /**< 查询次数 */
    uint64_t cache_hit_count;      /**< 缓存命中次数 */
    uint64_t total_query_time_ns;  /**< 总查询耗时 */
    uint64_t total_recall_items;   /**< 总召回项目数 */
    uint64_t rerank_count;         /**< 重排序次数 */
    uint64_t mount_count;          /**< 挂载次数 */
    uint64_t attractor_count;      /**< 吸引子调用次数 */
    double avg_precision;          /**< 平均精确度 */
    double avg_recall;             /**< 平均召回度 */
} retrieval_monitoring_data_t;

/**
 * @brief 演化监控数据
 */
typedef struct evolution_monitoring_data {
    uint64_t evolve_count;         /**< 演化次数 */
    uint64_t pattern_mined_count;  /**< 模式挖掘次数 */
    uint64_t rules_generated_count; /**< 规则生成次数 */
    uint64_t total_evolve_time_ns; /**< 总演化耗时 */
    uint64_t last_evolve_ns;       /**< 最后演化时间戳 */
    uint64_t patterns_current;     /**< 当前模式数量 */
    uint64_t rules_current;        /**< 当前规则数量 */
    uint64_t cluster_count;        /**< 聚类数量 */
    uint64_t outlier_count;        /**< 异常点数 */
} evolution_monitoring_data_t;

/**
 * @brief 资源监控数据
 */
typedef struct resource_monitoring_data {
    uint64_t memory_usage_bytes;   /**< 内存使用量 */
    uint64_t disk_usage_bytes;     /**< 磁盘使用量 */
    uint64_t max_memory_bytes;     /**< 最大内存限制 */
    uint64_t max_disk_bytes;       /**< 最大磁盘限制 */
    uint32_t thread_count;         /**< 线程数量 */
    uint32_t active_threads;       /**< 活动线程数 */
    uint64_t total_allocations;    /**< 总分配次数 */
    uint64_t total_deallocations;  /**< 总释放次数 */
    uint64_t open_file_handles;    /**< 打开文件句柄数 */
} resource_monitoring_data_t;

/**
 * @brief 预警规则
 */
typedef struct alert_rule {
    char* name;                    /**< 预警规则名称 */
    char* metric_name;             /**< 指标名称 */
    char* condition;              /**< 条件表达式 */
    char* severity;                /**< 严重程度 */
    char* message_template;        /**< 消息模板 */
    uint64_t last_triggered_ns;    /**< 最后触发时间戳 */
    uint32_t cooldown_seconds;     /**< 冷却时间 */
    struct alert_rule* next;       /**< 下一个规则 */
} alert_rule_t;

/**
 * @brief 监控子系统句柄
 */
struct agentos_memoryrov_monitor {
    agentos_mutex_t* lock;          /**< 线程锁 */

    /* 分层监控数据 */
    layer_monitoring_data_t l1_monitoring;   /**< L1原始层监控数据 */
    layer_monitoring_data_t l2_monitoring;   /**< L2特征层监控数据 */
    layer_monitoring_data_t l3_monitoring;   /**< L3结构层监控数据 */
    layer_monitoring_data_t l4_monitoring;   /**< L4模式层监控数据 */

    /* 子系统监控数据 */
    retrieval_monitoring_data_t retrieval_monitoring;   /**< 检索监控数据 */
    evolution_monitoring_data_t evolution_monitoring;   /**< 演化监控数据 */
    resource_monitoring_data_t resource_monitoring;     /**< 资源监控数据 */

    /* 指标管理 */
    monitoring_metric_t* metrics;            /**< 指标链表 */
    uint32_t metric_count;                   /**< 指标数量 */

    /* 预警管理 */
    alert_rule_t* alert_rules;               /**< 预警规则链表 */
    uint32_t alert_count;                    /**< 预警规则数量 */

    /* 可观测性集成 */
    agentos_observability_t* obs;            /**< 可观测性句柄 */

    /* 配置 */
    uint32_t monitoring_interval_ms;         /**< 监控间隔 */
    uint8_t enabled;                         /**< 是否启用 */
    char* monitor_id;                        /**< 监控器ID */

    /* 时间序列数据 */
    cJSON* timeseries_data;                  /**< 时间序列数据缓存 */
    uint64_t last_export_ns;                 /**< 最后导出时间戳 */
};

/* ==================== 内部工具函数 ==================== */

/**
 * @brief 获取当前时间戳（纳秒�?
 * @return 当前时间�?
 */
static uint64_t get_current_timestamp_ns(void) {
    return agentos_get_monotonic_time_ns();
}

/**
 * @brief 格式化时间戳为字符串
 * @param timestamp_ns 时间戳（纳秒级）
 * @return 格式化后的字符串（需调用者释放）
 */
static char* format_timestamp(uint64_t timestamp_ns) {
    time_t seconds = timestamp_ns / 1000000000;
    struct tm* timeinfo = localtime(&seconds);
    char* buffer = (char*)AGENTOS_MALLOC(64);
    if (buffer) {
        strftime(buffer, 64, "%Y-%m-%d %H:%M:%S", timeinfo);
        char* result = (char*)AGENTOS_MALLOC(128);
        if (result) {
            snprintf(result, 128, "%s.%03llu", buffer,
                    (unsigned long long)(timestamp_ns % 1000000000) / 1000000);
        }
        AGENTOS_FREE(buffer);
        return result;
    }
    return NULL;
}

/**
 * @brief 创建监控指标
 * @param name 指标名称
 * @param type 指标类型
 * @param description 指标描述
 * @param unit 指标单位
 * @return 新指标对象，失败返回NULL
 */
static monitoring_metric_t* create_metric(const char* name, metric_type_t type,
                                          const char* description, const char* unit) {
    if (!name) return NULL;

    monitoring_metric_t* metric = (monitoring_metric_t*)AGENTOS_CALLOC(1, sizeof(monitoring_metric_t));
    if (!metric) {
        AGENTOS_LOG_ERROR("Failed to allocate monitoring metric");
        return NULL;
    }

    metric->name = AGENTOS_STRDUP(name);
    metric->type = type;
    metric->description = description ? AGENTOS_STRDUP(description) : NULL;
    metric->unit = unit ? AGENTOS_STRDUP(unit) : NULL;
    metric->timestamp_ns = get_current_timestamp_ns();
    metric->next = NULL;

    if (!metric->name || (description && !metric->description) || (unit && !metric->unit)) {
        if (metric->name) AGENTOS_FREE(metric->name);
        if (metric->description) AGENTOS_FREE(metric->description);
        if (metric->unit) AGENTOS_FREE(metric->unit);
        AGENTOS_FREE(metric);
        AGENTOS_LOG_ERROR("Failed to duplicate strings for metric");
        return NULL;
    }

    // 初始化指标�?
    switch (type) {
        case METRIC_TYPE_COUNTER:
            metric->value.counter = 0;
            break;
        case METRIC_TYPE_GAUGE:
            metric->value.gauge = 0.0;
            break;
        case METRIC_TYPE_HISTOGRAM:
            metric->value.histogram.sum = 0.0;
            metric->value.histogram.count = 0;
            metric->value.histogram.buckets = NULL;
            metric->value.histogram.bucket_count = 0;
            break;
        case METRIC_TYPE_SUMMARY:
            // 简化实�?
            break;
    }

    return metric;
}

/**
 * @brief 释放监控指标
 * @param metric 指标对象
 */
static void free_metric(monitoring_metric_t* metric) {
    if (!metric) return;

    if (metric->name) AGENTOS_FREE(metric->name);
    if (metric->description) AGENTOS_FREE(metric->description);
    if (metric->unit) AGENTOS_FREE(metric->unit);

    if (metric->type == METRIC_TYPE_HISTOGRAM && metric->value.histogram.buckets) {
        AGENTOS_FREE(metric->value.histogram.buckets);
    }

    AGENTOS_FREE(metric);
}

/**
 * @brief 查找监控指标
 * @param monitor 监控�?
 * @param name 指标名称
 * @return 指标对象，未找到返回NULL
 */
static monitoring_metric_t* find_metric(agentos_memoryrov_monitor_t* monitor, const char* name) {
    if (!monitor || !name) return NULL;

    monitoring_metric_t* metric = monitor->metrics;
    while (metric) {
        if (strcmp(metric->name, name) == 0) {
            return metric;
        }
        metric = metric->next;
    }

    return NULL;
}

/**
 * @brief 更新或创建监控指�?
 * @param monitor 监控�?
 * @param name 指标名称
 * @param type 指标类型
 * @param value 指标�?
 * @param description 指标描述
 * @param unit 指标单位
 * @return AGENTOS_SUCCESS 成功，其他为错误�?
 */
static agentos_error_t update_metric(agentos_memoryrov_monitor_t* monitor,
                                     const char* name, metric_type_t type,
                                     double value, const char* description,
                                     const char* unit) {
    if (!monitor || !name) return AGENTOS_EINVAL;

    agentos_mutex_lock(monitor->lock);

    monitoring_metric_t* metric = find_metric(monitor, name);
    if (!metric) {
        // 创建新指�?
        metric = create_metric(name, type, description, unit);
        if (!metric) {
            agentos_mutex_unlock(monitor->lock);
            return AGENTOS_ENOMEM;
        }

        // 添加到链�?
        metric->next = monitor->metrics;
        monitor->metrics = metric;
        monitor->metric_count++;
    }

    // 更新指标�?
    metric->timestamp_ns = get_current_timestamp_ns();

    switch (type) {
        case METRIC_TYPE_COUNTER:
            metric->value.counter += (uint64_t)value;
            break;
        case METRIC_TYPE_GAUGE:
            metric->value.gauge = value;
            break;
        case METRIC_TYPE_HISTOGRAM:
            // 简化实�?
            metric->value.histogram.sum += value;
            metric->value.histogram.count++;
            break;
        case METRIC_TYPE_SUMMARY:
            // 简化实�?
            break;
    }

    agentos_mutex_unlock(monitor->lock);
    return AGENTOS_SUCCESS;
}

/* ==================== 分层监控函数 ==================== */

/**
 * @brief 更新L1层监控数�?
 * @param monitor 监控�?
 * @param write_bytes 写入字节�?
 * @param read_bytes 读取字节�?
 * @param write_time_ns 写入耗时
 * @param read_time_ns 读取耗时
 * @param is_error 是否错误
 */
static void update_l1_monitoring(agentos_memoryrov_monitor_t* monitor,
                                 uint64_t write_bytes, uint64_t read_bytes,
                                 uint64_t write_time_ns, uint64_t read_time_ns,
                                 int is_error) {
    if (!monitor) return;

    agentos_mutex_lock(monitor->lock);

    if (write_bytes > 0) {
        monitor->l1_monitoring.write_count++;
        monitor->l1_monitoring.total_write_bytes += write_bytes;
        monitor->l1_monitoring.total_write_time_ns += write_time_ns;
    }

    if (read_bytes > 0) {
        monitor->l1_monitoring.read_count++;
        monitor->l1_monitoring.total_read_bytes += read_bytes;
        monitor->l1_monitoring.total_read_time_ns += read_time_ns;
    }

    if (is_error) {
        monitor->l1_monitoring.error_count++;
    }

    agentos_mutex_unlock(monitor->lock);

    // 更新指标
    if (write_bytes > 0) {
        update_metric(monitor, "memoryrov_l1_write_total", METRIC_TYPE_COUNTER, 1,
                     "L1原始层写入总次�?, "count");
        update_metric(monitor, "memoryrov_l1_write_bytes_total", METRIC_TYPE_COUNTER,
                     write_bytes, "L1原始层写入总字节数", "bytes");
        update_metric(monitor, "memoryrov_l1_write_duration_seconds", METRIC_TYPE_HISTOGRAM,
                     write_time_ns / 1e9, "L1原始层写入耗时", "seconds");
    }

    if (read_bytes > 0) {
        update_metric(monitor, "memoryrov_l1_read_total", METRIC_TYPE_COUNTER, 1,
                     "L1原始层读取总次�?, "count");
        update_metric(monitor, "memoryrov_l1_read_bytes_total", METRIC_TYPE_COUNTER,
                     read_bytes, "L1原始层读取总字节数", "bytes");
        update_metric(monitor, "memoryrov_l1_read_duration_seconds", METRIC_TYPE_HISTOGRAM,
                     read_time_ns / 1e9, "L1原始层读取耗时", "seconds");
    }
}

/**
 * @brief 更新L2层监控数�?
 * @param monitor 监控�?
 * @param operation 操作类型�?=添加�?=查询�?=删除�?
 * @param vector_count 向量数量
 * @param operation_time_ns 操作耗时
 * @param is_error 是否错误
 */
static void update_l2_monitoring(agentos_memoryrov_monitor_t* monitor,
                                 int operation, uint32_t vector_count,
                                 uint64_t operation_time_ns, int is_error) {
    if (!monitor) return;

    agentos_mutex_lock(monitor->lock);

    switch (operation) {
        case 0:  // 添加
            monitor->l2_monitoring.write_count++;
            monitor->l2_monitoring.current_items += vector_count;
            monitor->l2_monitoring.total_write_time_ns += operation_time_ns;
            break;
        case 1:  // 查询
            monitor->l2_monitoring.read_count++;
            monitor->l2_monitoring.total_read_time_ns += operation_time_ns;
            break;
        case 2:  // 删除
            monitor->l2_monitoring.delete_count++;
            if (monitor->l2_monitoring.current_items >= vector_count) {
                monitor->l2_monitoring.current_items -= vector_count;
            }
            break;
    }

    if (is_error) {
        monitor->l2_monitoring.error_count++;
    }

    agentos_mutex_unlock(monitor->lock);

    // 更新指标
    const char* operation_name = NULL;
    const char* metric_suffix = NULL;

    switch (operation) {
        case 0:
            operation_name = "add";
            metric_suffix = "add";
            break;
        case 1:
            operation_name = "query";
            metric_suffix = "query";
            break;
        case 2:
            operation_name = "delete";
            metric_suffix = "delete";
            break;
    }

    if (operation_name) {
        char metric_name[128];
        snprintf(metric_name, sizeof(metric_name), "memoryrov_l2_%s_total", metric_suffix);
        update_metric(monitor, metric_name, METRIC_TYPE_COUNTER, 1,
                     "L2特征层操作总次�?, "count");

        snprintf(metric_name, sizeof(metric_name), "memoryrov_l2_%s_duration_seconds", metric_suffix);
        update_metric(monitor, metric_name, METRIC_TYPE_HISTOGRAM,
                     operation_time_ns / 1e9, "L2特征层操作耗时", "seconds");
    }
}

/* ==================== 公共API实现 ==================== */

/**
 * @brief 创建MemoryRovol监控�?
 * @param out_monitor 输出监控器句�?
 * @return agentos_error_t
 */
agentos_error_t agentos_memoryrov_monitor_create(agentos_memoryrov_monitor_t** out_monitor) {
    if (!out_monitor) return AGENTOS_EINVAL;

    agentos_memoryrov_monitor_t* monitor = (agentos_memoryrov_monitor_t*)AGENTOS_CALLOC(1, sizeof(agentos_memoryrov_monitor_t));
    if (!monitor) {
        AGENTOS_LOG_ERROR("Failed to allocate memoryrov monitor");
        return AGENTOS_ENOMEM;
    }

    monitor->lock = agentos_mutex_create();
    if (!monitor->lock) {
        AGENTOS_LOG_ERROR("Failed to create mutex for memoryrov monitor");
        AGENTOS_FREE(monitor);
        return AGENTOS_ENOMEM;
    }

    // 生成唯一ID
    monitor->monitor_id = agentos_generate_uuid();
    if (!monitor->monitor_id) {
        AGENTOS_LOG_WARN("Failed to generate UUID for memoryrov monitor, using default");
        monitor->monitor_id = AGENTOS_STRDUP("memoryrov_monitor_default");
    }

    // 初始化可观测�?
    monitor->obs = agentos_observability_create();
    if (monitor->obs) {
        // 注册核心指标
        agentos_observability_register_metric(monitor->obs, "memoryrov_operations_total",
                                              AGENTOS_METRIC_COUNTER, "MemoryRovol总操作次�?);
        agentos_observability_register_metric(monitor->obs, "memoryrov_errors_total",
                                              AGENTOS_METRIC_COUNTER, "MemoryRovol总错误次�?);
        agentos_observability_register_metric(monitor->obs, "memoryrov_latency_seconds",
                                              AGENTOS_METRIC_HISTOGRAM, "MemoryRovol操作延迟");
        agentos_observability_register_metric(monitor->obs, "memoryrov_memory_usage_bytes",
                                              AGENTOS_METRIC_GAUGE, "MemoryRovol内存使用�?);
        agentos_observability_register_metric(monitor->obs, "memoryrov_disk_usage_bytes",
                                              AGENTOS_METRIC_GAUGE, "MemoryRovol磁盘使用�?);
    }

    // 初始化监控数�?
    memset(&monitor->l1_monitoring, 0, sizeof(layer_monitoring_data_t));
    memset(&monitor->l2_monitoring, 0, sizeof(layer_monitoring_data_t));
    memset(&monitor->l3_monitoring, 0, sizeof(layer_monitoring_data_t));
    memset(&monitor->l4_monitoring, 0, sizeof(layer_monitoring_data_t));
    memset(&monitor->retrieval_monitoring, 0, sizeof(retrieval_monitoring_data_t));
    memset(&monitor->evolution_monitoring, 0, sizeof(evolution_monitoring_data_t));
    memset(&monitor->resource_monitoring, 0, sizeof(resource_monitoring_data_t));

    // 设置默认配置
    monitor->monitoring_interval_ms = DEFAULT_MONITORING_INTERVAL_MS;
    monitor->enabled = 1;
    monitor->metric_count = 0;
    monitor->alert_count = 0;
    monitor->metrics = NULL;
    monitor->alert_rules = NULL;
    monitor->timeseries_data = cJSON_CreateArray();
    monitor->last_export_ns = get_current_timestamp_ns();

    // 添加默认预警规则
    // 这里可以添加一些默认规�?

    *out_monitor = monitor;

    AGENTOS_LOG_INFO("MemoryRovol monitor created: %s", monitor->monitor_id);
    return AGENTOS_SUCCESS;
}

/**
 * @brief 销毁MemoryRovol监控�?
 * @param monitor 监控器句�?
 */
void agentos_memoryrov_monitor_destroy(agentos_memoryrov_monitor_t* monitor) {
    if (!monitor) return;

    AGENTOS_LOG_DEBUG("Destroying MemoryRovol monitor: %s", monitor->monitor_id);

    // 释放可观测性资�?
    if (monitor->obs) {
        agentos_observability_destroy(monitor->obs);
    }

    // 释放指标链表
    monitoring_metric_t* metric = monitor->metrics;
    while (metric) {
        monitoring_metric_t* next = metric->next;
        free_metric(metric);
        metric = next;
    }

    // 释放预警规则链表
    alert_rule_t* rule = monitor->alert_rules;
    while (rule) {
        alert_rule_t* next = rule->next;
        if (rule->name) AGENTOS_FREE(rule->name);
        if (rule->metric_name) AGENTOS_FREE(rule->metric_name);
        if (rule->condition) AGENTOS_FREE(rule->condition);
        if (rule->severity) AGENTOS_FREE(rule->severity);
        if (rule->message_template) AGENTOS_FREE(rule->message_template);
        AGENTOS_FREE(rule);
        rule = next;
    }

    // 释放时间序列数据
    if (monitor->timeseries_data) {
        cJSON_Delete(monitor->timeseries_data);
    }

    // 释放互斥�?
    if (monitor->lock) {
        agentos_mutex_destroy(monitor->lock);
    }

    // 释放ID
    if (monitor->monitor_id) {
        AGENTOS_FREE(monitor->monitor_id);
    }

    AGENTOS_FREE(monitor);
}

/**
 * @brief 记录L1层操�?
 * @param monitor 监控�?
 * @param operation 操作类型�?=写入�?=读取�?=删除�?
 * @param bytes 字节�?
 * @param duration_ns 耗时（纳秒）
 * @param success 是否成功
 * @return agentos_error_t
 */
agentos_error_t agentos_memoryrov_monitor_record_l1(agentos_memoryrov_monitor_t* monitor,
                                                    int operation, uint64_t bytes,
                                                    uint64_t duration_ns, int success) {
    if (!monitor) return AGENTOS_EINVAL;

    if (!monitor->enabled) return AGENTOS_SUCCESS;

    int is_error = !success;

    switch (operation) {
        case 0:  // 写入
            update_l1_monitoring(monitor, bytes, 0, duration_ns, 0, is_error);
            break;
        case 1:  // 读取
            update_l1_monitoring(monitor, 0, bytes, 0, duration_ns, is_error);
            break;
        case 2:  // 删除
            agentos_mutex_lock(monitor->lock);
            monitor->l1_monitoring.delete_count++;
            if (is_error) monitor->l1_monitoring.error_count++;
            agentos_mutex_unlock(monitor->lock);

            update_metric(monitor, "memoryrov_l1_delete_total", METRIC_TYPE_COUNTER, 1,
                         "L1原始层删除总次�?, "count");
            break;
        default:
            return AGENTOS_EINVAL;
    }

    // 更新总操作计�?
    update_metric(monitor, "memoryrov_operations_total", METRIC_TYPE_COUNTER, 1,
                 "MemoryRovol总操作次�?, "count");

    if (is_error) {
        update_metric(monitor, "memoryrov_errors_total", METRIC_TYPE_COUNTER, 1,
                     "MemoryRovol总错误次�?, "count");
    }

    // 记录延迟指标
    update_metric(monitor, "memoryrov_latency_seconds", METRIC_TYPE_HISTOGRAM,
                 duration_ns / 1e9, "MemoryRovol操作延迟", "seconds");

    // 触发可观测�?
    if (monitor->obs) {
        agentos_observability_increment_counter(monitor->obs, "memoryrov_operations_total", 1);
        if (is_error) {
            agentos_observability_increment_counter(monitor->obs, "memoryrov_errors_total", 1);
        }
        agentos_observability_record_histogram(monitor->obs, "memoryrov_latency_seconds",
                                              duration_ns / 1e9);
    }

    return AGENTOS_SUCCESS;
}

/**
 * @brief 记录L2层操�?
 * @param monitor 监控�?
 * @param operation 操作类型�?=添加向量�?=查询向量�?=删除向量�?
 * @param vector_count 向量数量
 * @param duration_ns 耗时（纳秒）
 * @param success 是否成功
 * @return agentos_error_t
 */
agentos_error_t agentos_memoryrov_monitor_record_l2(agentos_memoryrov_monitor_t* monitor,
                                                    int operation, uint32_t vector_count,
                                                    uint64_t duration_ns, int success) {
    if (!monitor) return AGENTOS_EINVAL;

    if (!monitor->enabled) return AGENTOS_SUCCESS;

    int is_error = !success;
    update_l2_monitoring(monitor, operation, vector_count, duration_ns, is_error);

    // 更新总操作计�?
    update_metric(monitor, "memoryrov_operations_total", METRIC_TYPE_COUNTER, 1,
                 "MemoryRovol总操作次�?, "count");

    if (is_error) {
        update_metric(monitor, "memoryrov_errors_total", METRIC_TYPE_COUNTER, 1,
                     "MemoryRovol总错误次�?, "count");
    }

    // 记录延迟指标
    update_metric(monitor, "memoryrov_latency_seconds", METRIC_TYPE_HISTOGRAM,
                 duration_ns / 1e9, "MemoryRovol操作延迟", "seconds");

    // 触发可观测�?
    if (monitor->obs) {
        agentos_observability_increment_counter(monitor->obs, "memoryrov_operations_total", 1);
        if (is_error) {
            agentos_observability_increment_counter(monitor->obs, "memoryrov_errors_total", 1);
        }
        agentos_observability_record_histogram(monitor->obs, "memoryrov_latency_seconds",
                                              duration_ns / 1e9);
    }

    return AGENTOS_SUCCESS;
}

/**
 * @brief 获取监控统计信息
 * @param monitor 监控�?
 * @param out_stats 输出统计JSON字符�?
 * @return agentos_error_t
 */
agentos_error_t agentos_memoryrov_monitor_stats(agentos_memoryrov_monitor_t* monitor,
                                                char** out_stats) {
    if (!monitor || !out_stats) return AGENTOS_EINVAL;

    cJSON* stats_json = cJSON_CreateObject();
    if (!stats_json) return AGENTOS_ENOMEM;

    agentos_mutex_lock(monitor->lock);

    // 基本信息
    cJSON_AddStringToObject(stats_json, "monitor_id", monitor->monitor_id);
    cJSON_AddBoolToObject(stats_json, "enabled", monitor->enabled);
    cJSON_AddNumberToObject(stats_json, "monitoring_interval_ms", monitor->monitoring_interval_ms);
    cJSON_AddNumberToObject(stats_json, "metric_count", monitor->metric_count);
    cJSON_AddNumberToObject(stats_json, "alert_count", monitor->alert_count);

    // L1层统�?
    cJSON* l1_json = cJSON_CreateObject();
    cJSON_AddNumberToObject(l1_json, "write_count", monitor->l1_monitoring.write_count);
    cJSON_AddNumberToObject(l1_json, "read_count", monitor->l1_monitoring.read_count);
    cJSON_AddNumberToObject(l1_json, "delete_count", monitor->l1_monitoring.delete_count);
    cJSON_AddNumberToObject(l1_json, "total_write_bytes", monitor->l1_monitoring.total_write_bytes);
    cJSON_AddNumberToObject(l1_json, "total_read_bytes", monitor->l1_monitoring.total_read_bytes);
    cJSON_AddNumberToObject(l1_json, "error_count", monitor->l1_monitoring.error_count);
    cJSON_AddNumberToObject(l1_json, "current_items", monitor->l1_monitoring.current_items);

    double l1_avg_write_time = monitor->l1_monitoring.write_count > 0 ?
                              (double)monitor->l1_monitoring.total_write_time_ns / monitor->l1_monitoring.write_count / 1e6 : 0.0;
    double l1_avg_read_time = monitor->l1_monitoring.read_count > 0 ?
                             (double)monitor->l1_monitoring.total_read_time_ns / monitor->l1_monitoring.read_count / 1e6 : 0.0;

    cJSON_AddNumberToObject(l1_json, "avg_write_time_ms", l1_avg_write_time);
    cJSON_AddNumberToObject(l1_json, "avg_read_time_ms", l1_avg_read_time);
    cJSON_AddItemToObject(stats_json, "l1_raw_layer", l1_json);

    // L2层统�?
    cJSON* l2_json = cJSON_CreateObject();
    cJSON_AddNumberToObject(l2_json, "write_count", monitor->l2_monitoring.write_count);
    cJSON_AddNumberToObject(l2_json, "read_count", monitor->l2_monitoring.read_count);
    cJSON_AddNumberToObject(l2_json, "delete_count", monitor->l2_monitoring.delete_count);
    cJSON_AddNumberToObject(l2_json, "error_count", monitor->l2_monitoring.error_count);
    cJSON_AddNumberToObject(l2_json, "current_items", monitor->l2_monitoring.current_items);
    cJSON_AddNumberToObject(l2_json, "max_items", monitor->l2_monitoring.max_items);

    double l2_avg_write_time = monitor->l2_monitoring.write_count > 0 ?
                              (double)monitor->l2_monitoring.total_write_time_ns / monitor->l2_monitoring.write_count / 1e6 : 0.0;
    double l2_avg_read_time = monitor->l2_monitoring.read_count > 0 ?
                             (double)monitor->l2_monitoring.total_read_time_ns / monitor->l2_monitoring.read_count / 1e6 : 0.0;

    cJSON_AddNumberToObject(l2_json, "avg_write_time_ms", l2_avg_write_time);
    cJSON_AddNumberToObject(l2_json, "avg_read_time_ms", l2_avg_read_time);
    cJSON_AddItemToObject(stats_json, "l2_feature_layer", l2_json);

    // 检索统�?
    cJSON* retrieval_json = cJSON_CreateObject();
    cJSON_AddNumberToObject(retrieval_json, "query_count", monitor->retrieval_monitoring.query_count);
    cJSON_AddNumberToObject(retrieval_json, "cache_hit_count", monitor->retrieval_monitoring.cache_hit_count);
    cJSON_AddNumberToObject(retrieval_json, "total_query_time_ns", monitor->retrieval_monitoring.total_query_time_ns);
    cJSON_AddNumberToObject(retrieval_json, "total_recall_items", monitor->retrieval_monitoring.total_recall_items);

    double cache_hit_rate = monitor->retrieval_monitoring.query_count > 0 ?
                           (double)monitor->retrieval_monitoring.cache_hit_count / monitor->retrieval_monitoring.query_count * 100.0 : 0.0;
    double avg_query_time = monitor->retrieval_monitoring.query_count > 0 ?
                           (double)monitor->retrieval_monitoring.total_query_time_ns / monitor->retrieval_monitoring.query_count / 1e6 : 0.0;

    cJSON_AddNumberToObject(retrieval_json, "cache_hit_rate_percent", cache_hit_rate);
    cJSON_AddNumberToObject(retrieval_json, "avg_query_time_ms", avg_query_time);
    cJSON_AddNumberToObject(retrieval_json, "avg_precision", monitor->retrieval_monitoring.avg_precision);
    cJSON_AddNumberToObject(retrieval_json, "avg_recall", monitor->retrieval_monitoring.avg_recall);
    cJSON_AddItemToObject(stats_json, "retrieval", retrieval_json);

    // 资源统计
    cJSON* resource_json = cJSON_CreateObject();
    cJSON_AddNumberToObject(resource_json, "memory_usage_bytes", monitor->resource_monitoring.memory_usage_bytes);
    cJSON_AddNumberToObject(resource_json, "disk_usage_bytes", monitor->resource_monitoring.disk_usage_bytes);
    cJSON_AddNumberToObject(resource_json, "max_memory_bytes", monitor->resource_monitoring.max_memory_bytes);

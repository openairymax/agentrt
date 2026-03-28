/**
 * @file monitor_service.h
 * @brief 监控服务接口定义
 * @details 负责系统监控、指标收集、告警管理和日志记录
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_MONITOR_SERVICE_H
#define AGENTOS_MONITOR_SERVICE_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 监控服务配置
 */
typedef struct {
    uint32_t metrics_collection_interval_ms; /**< 指标收集间隔（毫秒） */
    uint32_t health_check_interval_ms;      /**< 健康检查间隔（毫秒） */
    uint32_t log_flush_interval_ms;         /**< 日志刷新间隔（毫秒） */
    uint32_t alert_check_interval_ms;        /**< 告警检查间隔（毫秒） */
    char* log_file_path;                    /**< 日志文件路径 */
    char* metrics_storage_path;             /**< 指标存储路径 */
    bool enable_tracing;                    /**< 是否启用追踪 */
    bool enable_alerting;                   /**< 是否启用告警 */
    // From data intelligence emerges. by spharx
} monitor_config_t;

/**
 * @brief 指标类型
 */
typedef enum {
    METRIC_TYPE_COUNTER,    /**< 计数器 */
    METRIC_TYPE_GAUGE,      /**<  gauge */
    METRIC_TYPE_HISTOGRAM,  /**< 直方图 */
    METRIC_TYPE_SUMMARY,    /**< 摘要 */
    METRIC_TYPE_COUNT
} metric_type_t;

/**
 * @brief 告警级别
 */
typedef enum {
    ALERT_LEVEL_INFO,       /**< 信息 */
    ALERT_LEVEL_WARNING,    /**< 警告 */
    ALERT_LEVEL_ERROR,      /**< 错误 */
    ALERT_LEVEL_CRITICAL,   /**< 严重 */
    ALERT_LEVEL_COUNT
} alert_level_t;

/**
 * @brief 指标信息
 */
typedef struct {
    char* name;             /**< 指标名称 */
    char* description;      /**< 指标描述 */
    metric_type_t type;     /**< 指标类型 */
    char** labels;          /**< 标签数组 */
    size_t label_count;     /**< 标签数量 */
    double value;           /**< 指标值 */
    uint64_t timestamp;     /**< 时间戳 */
} metric_info_t;

/**
 * @brief 告警信息
 */
typedef struct {
    char* alert_id;         /**< 告警 ID */
    char* message;          /**< 告警消息 */
    alert_level_t level;     /**< 告警级别 */
    char* service_name;      /**< 服务名称 */
    char* resource_id;      /**< 资源 ID */
    char** labels;          /**< 标签数组 */
    size_t label_count;     /**< 标签数量 */
    uint64_t timestamp;     /**< 时间戳 */
    bool is_resolved;       /**< 是否已解决 */
} alert_info_t;

/**
 * @brief 日志级别
 */
typedef enum {
    LOG_LEVEL_DEBUG,        /**< 调试 */
    LOG_LEVEL_INFO,         /**< 信息 */
    LOG_LEVEL_WARNING,      /**< 警告 */
    LOG_LEVEL_ERROR,        /**< 错误 */
    LOG_LEVEL_FATAL,        /**< 致命 */
    LOG_LEVEL_COUNT
} log_level_t;

/**
 * @brief 日志信息
 */
typedef struct {
    log_level_t level;      /**< 日志级别 */
    char* message;          /**< 日志消息 */
    char* service_name;      /**< 服务名称 */
    char* file;             /**< 文件名 */
    int line;               /**< 行号 */
    char* function;         /**< 函数名 */
    uint64_t timestamp;     /**< 时间戳 */
    char** context;         /**< 上下文信息 */
    size_t context_count;   /**< 上下文数量 */
} log_info_t;

/**
 * @brief 健康检查结果
 */
typedef struct {
    char* service_name;      /**< 服务名称 */
    bool is_healthy;        /**< 是否健康 */
    char* status_message;    /**< 状态消息 */
    uint64_t timestamp;     /**< 时间戳 */
    int error_code;          /**< 错误码 */
} health_check_result_t;

/**
 * @brief 监控服务句柄
 */
typedef struct monitor_service monitor_service_t;

/**
 * @brief 创建监控服务
 * @param manager 配置信息
 * @param service 输出参数，返回创建的服务句柄
 * @return 0 表示成功，非 0 表示错误码
 */
int monitor_service_create(const monitor_config_t* manager, monitor_service_t** service);

/**
 * @brief 销毁监控服务
 * @param service 服务句柄
 * @return 0 表示成功，非 0 表示错误码
 */
int monitor_service_destroy(monitor_service_t* service);

/**
 * @brief 记录指标
 * @param service 服务句柄
 * @param metric 指标信息
 * @return 0 表示成功，非 0 表示错误码
 */
int monitor_service_record_metric(monitor_service_t* service, const metric_info_t* metric);

/**
 * @brief 记录日志
 * @param service 服务句柄
 * @param log 日志信息
 * @return 0 表示成功，非 0 表示错误码
 */
int monitor_service_log(monitor_service_t* service, const log_info_t* log);

/**
 * @brief 触发告警
 * @param service 服务句柄
 * @param alert 告警信息
 * @return 0 表示成功，非 0 表示错误码
 */
int monitor_service_trigger_alert(monitor_service_t* service, const alert_info_t* alert);

/**
 * @brief 解决告警
 * @param service 服务句柄
 * @param alert_id 告警 ID
 * @return 0 表示成功，非 0 表示错误码
 */
int monitor_service_resolve_alert(monitor_service_t* service, const char* alert_id);

/**
 * @brief 执行健康检查
 * @param service 服务句柄
 * @param service_name 服务名称
 * @param result 输出参数，返回健康检查结果
 * @return 0 表示成功，非 0 表示错误码
 */
int monitor_service_health_check(monitor_service_t* service, const char* service_name, health_check_result_t** result);

/**
 * @brief 获取指标数据
 * @param service 服务句柄
 * @param metric_name 指标名称
 * @param metrics 输出参数，返回指标数据数组
 * @param count 输出参数，返回指标数量
 * @return 0 表示成功，非 0 表示错误码
 */
int monitor_service_get_metrics(monitor_service_t* service, const char* metric_name, metric_info_t*** metrics, size_t* count);

/**
 * @brief 获取告警列表
 * @param service 服务句柄
 * @param alerts 输出参数，返回告警数组
 * @param count 输出参数，返回告警数量
 * @return 0 表示成功，非 0 表示错误码
 */
int monitor_service_get_alerts(monitor_service_t* service, alert_info_t*** alerts, size_t* count);

/**
 * @brief 重载配置
 * @param service 服务句柄
 * @param manager 新的配置信息
 * @return 0 表示成功，非 0 表示错误码
 */
int monitor_service_reload_config(monitor_service_t* service, const monitor_config_t* manager);

/**
 * @brief 生成监控报告
 * @param service 服务句柄
 * @param report 输出参数，返回报告内容
 * @return 0 表示成功，非 0 表示错误码
 */
int monitor_service_generate_report(monitor_service_t* service, char** report);

#endif /* AGENTOS_MONITOR_SERVICE_H */

/**
 * @file domes_monitoring.h
 * @brief 监控集成 - Prometheus / OpenTelemetry
 * @author Spharx
 * @date 2024
 *
 * 设计原则：
 * - 标准协议：支持 Prometheus 和 OpenTelemetry
 * - 低侵入：最小化对核心逻辑的影响
 * - 高可用：本地缓存 + 远程同步
 * - 异步报告：不阻塞主业务逻辑
 *
 * 支持的监控系统：
 * - Prometheus：pull 模式（通过 /metrics 端点）
 * - OpenTelemetry：push 模式（通过 OTLP 协议）
 * - StatsD：传统指标收集
 */

#ifndef DOMES_MONITORING_H
#define DOMES_MONITORING_H

#include "domes_metrics.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 监控后端类型 */
typedef enum monitoring_backend {
    MONITORING_BACKEND_NONE = 0,
    MONITORING_BACKEND_PROMETHEUS,
    MONITORING_BACKEND_OPENTELEMETRY,
    MONITORING_BACKEND_STATSD,
    MONITORING_BACKEND_ALL
} monitoring_backend_t;

/* 监控配置 */
typedef struct monitoring_config {
    monitoring_backend_t backend;

    struct {
        const char* listen_addr;
        uint16_t port;
        const char* endpoint;
        bool enable_tls;
        const char* tls_cert_file;
        const char* tls_key_file;
    } prometheus;

    struct {
        const char* endpoint;
        const char* service_name;
        const char* service_namespace;
        const char* auth_token;
        bool enable_tls;
    } opentelemetry;

    struct {
        const char* host;
        uint16_t port;
        const char* prefix;
    } statsd;

    uint32_t reporting_interval_ms;
    uint32_t buffer_size;
    bool enable_caching;
} monitoring_config_t;

/* 监控状态 */
typedef enum monitoring_status {
    MONITORING_STATUS_STOPPED = 0,
    MONITORING_STATUS_STARTING,
    MONITORING_STATUS_RUNNING,
    MONITORING_STATUS_ERROR,
    MONITORING_STATUS_STOPPING
} monitoring_status_t;

/* 健康检查结果 */
typedef struct health_check_result {
    bool healthy;
    const char* component;
    const char* message;
    uint64_t timestamp_ns;
} health_check_result_t;

/* 监控句柄 */
typedef struct domes_monitoring domes_monitoring_t;

/* 健康检查回调 */
typedef bool (*health_check_fn_t)(void);

/**
 * @brief 创建监控管理器
 * @param config 监控配置（NULL 使用默认配置）
 * @return 监控管理器句柄，失败返回 NULL
 */
domes_monitoring_t* domes_monitoring_create(const monitoring_config_t* config);

/**
 * @brief 销毁监控管理器
 * @param mgr 监控管理器句柄
 */
void domes_monitoring_destroy(domes_monitoring_t* mgr);

/**
 * @brief 启动监控
 * @param mgr 监控管理器句柄
 * @return 0 成功，其他失败
 */
int domes_monitoring_start(domes_monitoring_t* mgr);

/**
 * @brief 停止监控
 * @param mgr 监控管理器句柄
 */
void domes_monitoring_stop(domes_monitoring_t* mgr);

/**
 * @brief 获取监控状态
 * @param mgr 监控管理器句柄
 * @return 监控状态
 */
monitoring_status_t domes_monitoring_get_status(domes_monitoring_t* mgr);

/**
 * @brief 报告指标（推模式）
 * @param mgr 监控管理器句柄
 * @return 0 成功，其他失败
 */
int domes_monitoring_report(domes_monitoring_t* mgr);

/**
 * @brief 导出指标（拉模式）
 * @param mgr 监控管理器句柄
 * @param buffer 输出缓冲区
 * @param size 缓冲区大小
 * @return 实际写入的字节数
 */
size_t domes_monitoring_export(domes_monitoring_t* mgr, char* buffer, size_t size);

/**
 * @brief 注册健康检查
 * @param mgr 监控管理器句柄
 * @param name 检查名称
 * @param callback 检查回调
 * @return 0 成功，其他失败
 */
int domes_monitoring_register_health_check(domes_monitoring_t* mgr,
                                          const char* name,
                                          health_check_fn_t callback);

/**
 * @brief 执行健康检查
 * @param mgr 监控管理器句柄
 * @param results 结果数组
 * @param max_results 最大结果数量
 * @return 实际结果数量
 */
int domes_monitoring_check_health(domes_monitoring_t* mgr,
                                 health_check_result_t* results,
                                 size_t max_results);

/**
 * @brief 获取服务器地址（Prometheus 拉模式）
 * @param mgr 监控管理器句柄
 * @return 地址字符串
 */
const char* domes_monitoring_get_listen_addr(domes_monitoring_t* mgr);

/**
 * @brief 设置指标过滤器
 * @param mgr 监控管理器句柄
 * @param include_patterns 包含模式（NULL 表示全部）
 * @param exclude_patterns 排除模式（NULL 表示无排除）
 * @return 0 成功，其他失败
 */
int domes_monitoring_set_filter(domes_monitoring_t* mgr,
                               const char** include_patterns,
                               const char** exclude_patterns);

/**
 * @brief 获取监控指标数量
 * @param mgr 监控管理器句柄
 * @return 指标数量
 */
size_t domes_monitoring_get_metric_count(domes_monitoring_t* mgr);

/**
 * @brief 获取最后报告时间戳
 * @param mgr 监控管理器句柄
 * @return 时间戳（纳秒）
 */
uint64_t domes_monitoring_get_last_report_time(domes_monitoring_t* mgr);

/**
 * @brief 获取最后错误
 * @param mgr 监控管理器句柄
 * @return 错误信息（NULL 表示无错误）
 */
const char* domes_monitoring_get_last_error(domes_monitoring_t* mgr);

/* ============================================================================
 * 便捷函数
 * ============================================================================ */

/**
 * @brief 创建默认 Prometheus 配置
 * @param port 端口号
 * @return 监控配置（需手动释放）
 */
monitoring_config_t* monitoring_config_create_prometheus(uint16_t port);

/**
 * @brief 创建默认 OpenTelemetry 配置
 * @param endpoint OTLP 端点
 * @param service_name 服务名称
 * @return 监控配置（需手动释放）
 */
monitoring_config_t* monitoring_config_create_opentelemetry(const char* endpoint,
                                                            const char* service_name);

/**
 * @brief 销毁监控配置
 * @param config 监控配置
 */
void monitoring_config_destroy(monitoring_config_t* config);

/**
 * @brief 获取后端类型字符串
 * @param backend 后端类型
 * @return 后端名称
 */
const char* monitoring_backend_string(monitoring_backend_t backend);

/**
 * @brief 获取状态字符串
 * @param status 状态
 * @return 状态名称
 */
const char* monitoring_status_string(monitoring_status_t status);

/**
 * @brief 获取单例监控实例
 * @return 监控实例（未创建返回 NULL）
 */
domes_monitoring_t* domes_monitoring_get_instance(void);

/**
 * @brief 初始化单例监控实例
 * @param config 监控配置（NULL 使用默认配置）
 * @return 0 成功，其他失败
 */
int domes_monitoring_init_instance(const monitoring_config_t* config);

/**
 * @brief 销毁单例监控实例
 */
void domes_monitoring_shutdown_instance(void);

#ifdef __cplusplus
}
#endif

#endif /* DOMES_MONITORING_H */
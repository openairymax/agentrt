/**
 * @file main.c
 * @brief 监控服务主实现
 * @details 实现监控服务的核心功能，包括指标收集、告警管理、日志记录和健康检查
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "monitor_service.h"

/**
 * @brief 监控服务内部结构
 */
struct monitor_service {
    monitor_config_t config;           /**< 配置信息 */
    metric_info_t** metrics;           /**< 指标数据 */
    size_t metric_count;               /**< 指标数量 */
    size_t metric_capacity;            /**< 指标容量 */
    alert_info_t** alerts;             /**< 告警数据 */
    size_t alert_count;                /**< 告警数量 */
    size_t alert_capacity;             /**< 告警容量 */
    char* log_buffer;                  /**< 日志缓冲区 */
    size_t log_buffer_size;            /**< 日志缓冲区大小 */
    bool is_running;                   /**< 服务是否运行 */
};

/**
 * @brief 创建监控服务
 * @param config 配置信息
 * @param service 输出参数，返回创建的服务句柄
 * @return 0 表示成功，非 0 表示错误码
 */
int monitor_service_create(const monitor_config_t* config, monitor_service_t** service) {
    if (!config || !service) {
        return -1;
    }

    monitor_service_t* new_service = (monitor_service_t*)malloc(sizeof(monitor_service_t));
    if (!new_service) {
        return -2;
    }

    // 复制配置
    new_service->config = *config;
    new_service->metrics = NULL;
    new_service->metric_count = 0;
    new_service->metric_capacity = 1024;
    new_service->alerts = NULL;
    new_service->alert_count = 0;
    new_service->alert_capacity = 1024;
    new_service->log_buffer = NULL;
    new_service->log_buffer_size = 0;
    new_service->is_running = true;

    // 分配内存
    new_service->metrics = (metric_info_t**)malloc(sizeof(metric_info_t*) * new_service->metric_capacity);
    if (!new_service->metrics) {
        free(new_service);
        return -2;
    }

    new_service->alerts = (alert_info_t**)malloc(sizeof(alert_info_t*) * new_service->alert_capacity);
    if (!new_service->alerts) {
        free(new_service->metrics);
        free(new_service);
        return -2;
    }

    *service = new_service;
    return 0;
}

/**
 * @brief 销毁监控服务
 * @param service 服务句柄
 * @return 0 表示成功，非 0 表示错误码
 */
int monitor_service_destroy(monitor_service_t* service) {
    if (!service) {
        return -1;
    }

    // 释放指标数据
    for (size_t i = 0; i < service->metric_count; i++) {
        if (service->metrics[i]) {
            if (service->metrics[i]->labels) {
                for (size_t j = 0; j < service->metrics[i]->label_count; j++) {
                    free(service->metrics[i]->labels[j]);
                }
                free(service->metrics[i]->labels);
            }
            free(service->metrics[i]->name);
            free(service->metrics[i]->description);
            free(service->metrics[i]);
        }
    }
    free(service->metrics);

    // 释放告警数据
    for (size_t i = 0; i < service->alert_count; i++) {
        if (service->alerts[i]) {
            if (service->alerts[i]->labels) {
                for (size_t j = 0; j < service->alerts[i]->label_count; j++) {
                    free(service->alerts[i]->labels[j]);
                }
                free(service->alerts[i]->labels);
            }
            free(service->alerts[i]->alert_id);
            free(service->alerts[i]->message);
            free(service->alerts[i]->service_name);
            free(service->alerts[i]->resource_id);
            free(service->alerts[i]);
        }
    }
    free(service->alerts);

    // 释放日志缓冲区
    if (service->log_buffer) {
        free(service->log_buffer);
    }

    free(service);
    return 0;
}

/**
 * @brief 记录指标
 * @param service 服务句柄
 * @param metric 指标信息
 * @return 0 表示成功，非 0 表示错误码
 */
int monitor_service_record_metric(monitor_service_t* service, const metric_info_t* metric) {
    if (!service || !metric) {
        return -1; // 无效参数
    }

    // 验证指标数据
    if (!metric->name || strlen(metric->name) == 0) {
        return -3; // 无效的指标名称
    }

    if (metric->type < 0 || metric->type >= METRIC_TYPE_COUNT) {
        return -4; // 无效的指标类型
    }

    // 检查容量
    if (service->metric_count >= service->metric_capacity) {
        size_t new_capacity = service->metric_capacity * 2;
        metric_info_t** new_metrics = (metric_info_t**)realloc(service->metrics, sizeof(metric_info_t*) * new_capacity);
        if (!new_metrics) {
            return -2; // 内存分配失败
        }
        service->metrics = new_metrics;
        service->metric_capacity = new_capacity;
    }

    // 复制指标数据
    metric_info_t* new_metric = (metric_info_t*)malloc(sizeof(metric_info_t));
    if (!new_metric) {
        return -2; // 内存分配失败
    }

    new_metric->name = strdup(metric->name);
    if (!new_metric->name) {
        free(new_metric);
        return -2; // 内存分配失败
    }

    new_metric->description = metric->description ? strdup(metric->description) : NULL;
    new_metric->type = metric->type;
    new_metric->label_count = metric->label_count;
    new_metric->value = metric->value;
    new_metric->timestamp = metric->timestamp;

    // 复制标签
    if (metric->label_count > 0 && metric->labels) {
        new_metric->labels = (char**)malloc(sizeof(char*) * metric->label_count);
        if (!new_metric->labels) {
            free(new_metric->name);
            if (new_metric->description) {
                free(new_metric->description);
            }
            free(new_metric);
            return -2; // 内存分配失败
        }

        for (size_t i = 0; i < metric->label_count; i++) {
            if (metric->labels[i]) {
                new_metric->labels[i] = strdup(metric->labels[i]);
                if (!new_metric->labels[i]) {
                    // 释放已分配的标签
                    for (size_t j = 0; j < i; j++) {
                        free(new_metric->labels[j]);
                    }
                    free(new_metric->labels);
                    free(new_metric->name);
                    if (new_metric->description) {
                        free(new_metric->description);
                    }
                    free(new_metric);
                    return -2; // 内存分配失败
                }
            } else {
                new_metric->labels[i] = NULL;
            }
        }
    } else {
        new_metric->labels = NULL;
    }

    service->metrics[service->metric_count++] = new_metric;
    return 0;
}

/**
 * @brief 记录日志
 * @param service 服务句柄
 * @param log 日志信息
 * @return 0 表示成功，非 0 表示错误码
 */
int monitor_service_log(monitor_service_t* service, const log_info_t* log) {
    if (!service || !log) {
        return -1;
    }

    // 构建日志消息
    char log_message[1024];
    time_t timestamp = (time_t)(log->timestamp / 1000);
    struct tm* tm_info = localtime(&timestamp);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    const char* level_str[] = {"DEBUG", "INFO", "WARNING", "ERROR", "FATAL"};
    snprintf(log_message, sizeof(log_message), "[%s] [%s] [%s] %s:%d:%s - %s",
             time_str, level_str[log->level], log->service_name,
             log->file, log->line, log->function, log->message);

    // 输出到控制台
    printf("%s\n", log_message);

    // 写入日志文件
    if (service->config.log_file_path) {
        FILE* log_file = fopen(service->config.log_file_path, "a");
        if (log_file) {
            fprintf(log_file, "%s\n", log_message);
            fclose(log_file);
        }
    }

    return 0;
}

/**
 * @brief 触发告警
 * @param service 服务句柄
 * @param alert 告警信息
 * @return 0 表示成功，非 0 表示错误码
 */
int monitor_service_trigger_alert(monitor_service_t* service, const alert_info_t* alert) {
    if (!service || !alert) {
        return -1;
    }

    // 检查容量
    if (service->alert_count >= service->alert_capacity) {
        size_t new_capacity = service->alert_capacity * 2;
        alert_info_t** new_alerts = (alert_info_t**)realloc(service->alerts, sizeof(alert_info_t*) * new_capacity);
        if (!new_alerts) {
            return -2;
        }
        service->alerts = new_alerts;
        service->alert_capacity = new_capacity;
    }

    // 复制告警数据
    alert_info_t* new_alert = (alert_info_t*)malloc(sizeof(alert_info_t));
    if (!new_alert) {
        return -2;
    }

    new_alert->alert_id = strdup(alert->alert_id);
    new_alert->message = strdup(alert->message);
    new_alert->level = alert->level;
    new_alert->service_name = strdup(alert->service_name);
    new_alert->resource_id = strdup(alert->resource_id);
    new_alert->label_count = alert->label_count;
    new_alert->timestamp = alert->timestamp;
    new_alert->is_resolved = alert->is_resolved;

    // 复制标签
    if (alert->label_count > 0 && alert->labels) {
        new_alert->labels = (char**)malloc(sizeof(char*) * alert->label_count);
        if (!new_alert->labels) {
            free(new_alert->alert_id);
            free(new_alert->message);
            free(new_alert->service_name);
            free(new_alert->resource_id);
            free(new_alert);
            return -2;
        }

        for (size_t i = 0; i < alert->label_count; i++) {
            new_alert->labels[i] = strdup(alert->labels[i]);
        }
    } else {
        new_alert->labels = NULL;
    }

    service->alerts[service->alert_count++] = new_alert;
    return 0;
}

/**
 * @brief 解决告警
 * @param service 服务句柄
 * @param alert_id 告警 ID
 * @return 0 表示成功，非 0 表示错误码
 */
int monitor_service_resolve_alert(monitor_service_t* service, const char* alert_id) {
    if (!service || !alert_id) {
        return -1;
    }

    for (size_t i = 0; i < service->alert_count; i++) {
        if (service->alerts[i] && strcmp(service->alerts[i]->alert_id, alert_id) == 0) {
            service->alerts[i]->is_resolved = true;
            return 0;
        }
    }

    return -3; // 告警不存在
}

/**
 * @brief 执行健康检查
 * @param service 服务句柄
 * @param service_name 服务名称
 * @param result 输出参数，返回健康检查结果
 * @return 0 表示成功，非 0 表示错误码
 */
int monitor_service_health_check(monitor_service_t* service, const char* service_name, health_check_result_t** result) {
    if (!service || !service_name || !result) {
        return -1;
    }

    health_check_result_t* new_result = (health_check_result_t*)malloc(sizeof(health_check_result_t));
    if (!new_result) {
        return -2;
    }

    new_result->service_name = strdup(service_name);
    new_result->is_healthy = true;
    new_result->status_message = strdup("Service is healthy");
    new_result->timestamp = (uint64_t)time(NULL) * 1000;
    new_result->error_code = 0;

    // 这里可以添加具体的健康检查逻辑
    // 例如检查服务是否运行、资源使用情况等

    *result = new_result;
    return 0;
}

/**
 * @brief 获取指标数据
 * @param service 服务句柄
 * @param metric_name 指标名称
 * @param metrics 输出参数，返回指标数据数组
 * @param count 输出参数，返回指标数量
 * @return 0 表示成功，非 0 表示错误码
 */
int monitor_service_get_metrics(monitor_service_t* service, const char* metric_name, metric_info_t*** metrics, size_t* count) {
    if (!service || !metrics || !count) {
        return -1;
    }

    // 统计匹配的指标数量
    size_t matched_count = 0;
    for (size_t i = 0; i < service->metric_count; i++) {
        if (service->metrics[i] && (!metric_name || strcmp(service->metrics[i]->name, metric_name) == 0)) {
            matched_count++;
        }
    }

    if (matched_count == 0) {
        *metrics = NULL;
        *count = 0;
        return 0;
    }

    // 分配内存并复制匹配的指标
    metric_info_t** matched_metrics = (metric_info_t**)malloc(sizeof(metric_info_t*) * matched_count);
    if (!matched_metrics) {
        return -2;
    }

    size_t index = 0;
    for (size_t i = 0; i < service->metric_count && index < matched_count; i++) {
        if (service->metrics[i] && (!metric_name || strcmp(service->metrics[i]->name, metric_name) == 0)) {
            matched_metrics[index++] = service->metrics[i];
        }
    }

    *metrics = matched_metrics;
    *count = matched_count;
    return 0;
}

/**
 * @brief 获取告警列表
 * @param service 服务句柄
 * @param alerts 输出参数，返回告警数组
 * @param count 输出参数，返回告警数量
 * @return 0 表示成功，非 0 表示错误码
 */
int monitor_service_get_alerts(monitor_service_t* service, alert_info_t*** alerts, size_t* count) {
    if (!service || !alerts || !count) {
        return -1;
    }

    // 统计未解决的告警数量
    size_t unresolved_count = 0;
    for (size_t i = 0; i < service->alert_count; i++) {
        if (service->alerts[i] && !service->alerts[i]->is_resolved) {
            unresolved_count++;
        }
    }

    if (unresolved_count == 0) {
        *alerts = NULL;
        *count = 0;
        return 0;
    }

    // 分配内存并复制未解决的告警
    alert_info_t** unresolved_alerts = (alert_info_t**)malloc(sizeof(alert_info_t*) * unresolved_count);
    if (!unresolved_alerts) {
        return -2;
    }

    size_t index = 0;
    for (size_t i = 0; i < service->alert_count && index < unresolved_count; i++) {
        if (service->alerts[i] && !service->alerts[i]->is_resolved) {
            unresolved_alerts[index++] = service->alerts[i];
        }
    }

    *alerts = unresolved_alerts;
    *count = unresolved_count;
    return 0;
}

/**
 * @brief 重载配置
 * @param service 服务句柄
 * @param config 新的配置信息
 * @return 0 表示成功，非 0 表示错误码
 */
int monitor_service_reload_config(monitor_service_t* service, const monitor_config_t* config) {
    if (!service || !config) {
        return -1;
    }

    service->config = *config;
    return 0;
}

/**
 * @brief 生成监控报告
 * @param service 服务句柄
 * @param report 输出参数，返回报告内容
 * @return 0 表示成功，非 0 表示错误码
 */
int monitor_service_generate_report(monitor_service_t* service, char** report) {
    if (!service || !report) {
        return -1;
    }

    // 计算报告大小
    size_t report_size = 1024; // 初始大小
    char* report_buffer = (char*)malloc(report_size);
    if (!report_buffer) {
        return -2;
    }

    // 构建报告头部
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    int written = snprintf(report_buffer, report_size, "=== AgentOS 监控报告 ===\n" 
                         "生成时间: %s\n" 
                         "======================\n\n" 
                         "1. 系统状态\n" 
                         "   - 监控服务运行状态: %s\n" 
                         "   - 收集的指标数量: %zu\n" 
                         "   - 未解决的告警数量: %zu\n\n",
                         time_str, service->is_running ? "运行中" : "已停止",
                         service->metric_count, service->alert_count);

    // 添加上下文信息
    written += snprintf(report_buffer + written, report_size - written, "2. 配置信息\n" 
                      "   - 指标收集间隔: %u ms\n" 
                      "   - 健康检查间隔: %u ms\n" 
                      "   - 日志刷新间隔: %u ms\n" 
                      "   - 告警检查间隔: %u ms\n" 
                      "   - 日志文件路径: %s\n" 
                      "   - 指标存储路径: %s\n" 
                      "   - 启用追踪: %s\n" 
                      "   - 启用告警: %s\n\n",
                      service->config.metrics_collection_interval_ms,
                      service->config.health_check_interval_ms,
                      service->config.log_flush_interval_ms,
                      service->config.alert_check_interval_ms,
                      service->config.log_file_path ? service->config.log_file_path : "未设置",
                      service->config.metrics_storage_path ? service->config.metrics_storage_path : "未设置",
                      service->config.enable_tracing ? "是" : "否",
                      service->config.enable_alerting ? "是" : "否");

    // 添加最近的告警信息
    written += snprintf(report_buffer + written, report_size - written, "3. 最近告警\n");
    size_t alert_count = 0;
    for (size_t i = service->alert_count > 5 ? service->alert_count - 5 : 0; i < service->alert_count; i++) {
        if (service->alerts[i]) {
            const char* level_str[] = {"INFO", "WARNING", "ERROR", "CRITICAL"};
            written += snprintf(report_buffer + written, report_size - written,
                               "   - [%s] %s (服务: %s)\n",
                               level_str[service->alerts[i]->level],
                               service->alerts[i]->message,
                               service->alerts[i]->service_name);
            alert_count++;
        }
    }

    if (alert_count == 0) {
        written += snprintf(report_buffer + written, report_size - written, "   - 无告警\n");
    }

    written += snprintf(report_buffer + written, report_size - written, "\n4. 报告结束\n");

    *report = report_buffer;
    return 0;
}

/**
 * @brief 监控服务主函数
 */
int main() {
    // 配置监控服务
    monitor_config_t config = {
        .metrics_collection_interval_ms = 5000,
        .health_check_interval_ms = 10000,
        .log_flush_interval_ms = 30000,
        .alert_check_interval_ms = 5000,
        .log_file_path = "monitor.log",
        .metrics_storage_path = "metrics",
        .enable_tracing = true,
        .enable_alerting = true
    };

    // 创建监控服务
    monitor_service_t* service = NULL;
    int ret = monitor_service_create(&config, &service);
    if (ret != 0) {
        printf("创建监控服务失败: %d\n", ret);
        return ret;
    }

    printf("监控服务已启动\n");

    // 模拟一些操作
    // 1. 记录指标
    metric_info_t metric = {
        .name = "service.cpu_usage",
        .description = "CPU 使用率",
        .type = METRIC_TYPE_GAUGE,
        .labels = NULL,
        .label_count = 0,
        .value = 45.5,
        .timestamp = (uint64_t)time(NULL) * 1000
    };
    monitor_service_record_metric(service, &metric);

    // 2. 记录日志
    log_info_t log = {
        .level = LOG_LEVEL_INFO,
        .message = "监控服务启动成功",
        .service_name = "monit_d",
        .file = __FILE__,
        .line = __LINE__,
        .function = __func__,
        .timestamp = (uint64_t)time(NULL) * 1000,
        .context = NULL,
        .context_count = 0
    };
    monitor_service_log(service, &log);

    // 3. 触发告警
    alert_info_t alert = {
        .alert_id = "alert-001",
        .message = "CPU 使用率过高",
        .level = ALERT_LEVEL_WARNING,
        .service_name = "monit_d",
        .resource_id = "cpu",
        .labels = NULL,
        .label_count = 0,
        .timestamp = (uint64_t)time(NULL) * 1000,
        .is_resolved = false
    };
    monitor_service_trigger_alert(service, &alert);

    // 4. 执行健康检查
    health_check_result_t* health_result = NULL;
    monitor_service_health_check(service, "monit_d", &health_result);
    if (health_result) {
        printf("健康检查结果: %s - %s\n", 
               health_result->is_healthy ? "健康" : "不健康",
               health_result->status_message);
        free(health_result->service_name);
        free(health_result->status_message);
        free(health_result);
    }

    // 5. 生成监控报告
    char* report = NULL;
    monitor_service_generate_report(service, &report);
    if (report) {
        printf("\n监控报告:\n%s\n", report);
        free(report);
    }

    // 6. 解决告警
    monitor_service_resolve_alert(service, "alert-001");

    // 销毁监控服务
    monitor_service_destroy(service);

    printf("监控服务已停止\n");
    return 0;
}

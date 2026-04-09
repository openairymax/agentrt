/**
 * @file test_monitor.c
 * @brief 监控服务单元测试
 * @details 测试监控服务的核心功能，包括指标收集、告警管理、日志记录和健康检�?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../../../../agentos/daemon/monit_d/include/monitor_service.h"

/**
 * @brief 测试服务创建和销�?
 */
int test_service_create_destroy() {
    printf("测试服务创建和销�?..");

    // 配置监控服务
    monitor_config_t manager = {
        .metrics_collection_interval_ms = 5000,
        .health_check_interval_ms = 10000,
        .log_flush_interval_ms = 30000,
        .alert_check_interval_ms = 5000,
        // From data intelligence emerges. by spharx
        .log_file_path = "test_monitor.log",
        .metrics_storage_path = "test_metrics",
        .enable_tracing = true,
        .enable_alerting = true
    };

    // 创建监控服务
    monitor_service_t* service = NULL;
    int ret = monitor_service_create(&manager, &service);
    if (ret != 0) {
        printf("失败: 创建服务返回 %d\n", ret);
        return ret;
    }

    // 销毁监控服�?
    ret = monitor_service_destroy(service);
    if (ret != 0) {
        printf("失败: 销毁服务返�?%d\n", ret);
        return ret;
    }

    printf("成功\n");
    return 0;
}

/**
 * @brief 测试指标记录
 */
int test_record_metric() {
    printf("测试指标记录...");

    // 配置监控服务
    monitor_config_t manager = {
        .metrics_collection_interval_ms = 5000,
        .health_check_interval_ms = 10000,
        .log_flush_interval_ms = 30000,
        .alert_check_interval_ms = 5000,
        .log_file_path = NULL,
        .metrics_storage_path = NULL,
        .enable_tracing = false,
        .enable_alerting = false
    };

    // 创建监控服务
    monitor_service_t* service = NULL;
    int ret = monitor_service_create(&manager, &service);
    if (ret != 0) {
        printf("失败: 创建服务返回 %d\n", ret);
        return ret;
    }

    // 记录指标
    metric_info_t metric = {
        .name = "service.cpu_usage",
        .description = "CPU 使用�?,
        .type = METRIC_TYPE_GAUGE,
        .labels = NULL,
        .label_count = 0,
        .value = 45.5,
        .timestamp = (uint64_t)time(NULL) * 1000
    };

    ret = monitor_service_record_metric(service, &metric);
    if (ret != 0) {
        printf("失败: 记录指标返回 %d\n", ret);
        monitor_service_destroy(service);
        return ret;
    }

    // 获取指标
    metric_info_t** metrics = NULL;
    size_t count = 0;
    ret = monitor_service_get_metrics(service, "service.cpu_usage", &metrics, &count);
    if (ret != 0) {
        printf("失败: 获取指标返回 %d\n", ret);
        monitor_service_destroy(service);
        return ret;
    }

    if (count != 1) {
        printf("失败: 指标数量不正确，期望 1，实�?%zu\n", count);
        if (metrics) {
            free(metrics);
        }
        monitor_service_destroy(service);
        return -1;
    }

    if (strcmp(metrics[0]->name, "service.cpu_usage") != 0) {
        printf("失败: 指标名称不正确\n");
        free(metrics);
        monitor_service_destroy(service);
        return -1;
    }

    free(metrics);
    monitor_service_destroy(service);
    printf("成功\n");
    return 0;
}

/**
 * @brief 测试日志记录
 */
int test_log() {
    printf("测试日志记录...");

    // 配置监控服务
    monitor_config_t manager = {
        .metrics_collection_interval_ms = 5000,
        .health_check_interval_ms = 10000,
        .log_flush_interval_ms = 30000,
        .alert_check_interval_ms = 5000,
        .log_file_path = NULL,
        .metrics_storage_path = NULL,
        .enable_tracing = false,
        .enable_alerting = false
    };

    // 创建监控服务
    monitor_service_t* service = NULL;
    int ret = monitor_service_create(&manager, &service);
    if (ret != 0) {
        printf("失败: 创建服务返回 %d\n", ret);
        return ret;
    }

    // 记录日志
    log_info_t log = {
        .level = LOG_LEVEL_INFO,
        .message = "测试日志消息",
        .service_name = "test_service",
        .file = __FILE__,
        .line = __LINE__,
        .function = __func__,
        .timestamp = (uint64_t)time(NULL) * 1000,
        .context = NULL,
        .context_count = 0
    };

    ret = monitor_service_log(service, &log);
    if (ret != 0) {
        printf("失败: 记录日志返回 %d\n", ret);
        monitor_service_destroy(service);
        return ret;
    }

    monitor_service_destroy(service);
    printf("成功\n");
    return 0;
}

/**
 * @brief 测试告警触发和解�?
 */
int test_alert() {
    printf("测试告警触发和解�?..");

    // 配置监控服务
    monitor_config_t manager = {
        .metrics_collection_interval_ms = 5000,
        .health_check_interval_ms = 10000,
        .log_flush_interval_ms = 30000,
        .alert_check_interval_ms = 5000,
        .log_file_path = NULL,
        .metrics_storage_path = NULL,
        .enable_tracing = false,
        .enable_alerting = true
    };

    // 创建监控服务
    monitor_service_t* service = NULL;
    int ret = monitor_service_create(&manager, &service);
    if (ret != 0) {
        printf("失败: 创建服务返回 %d\n", ret);
        return ret;
    }

    // 触发告警
    alert_info_t alert = {
        .alert_id = "alert-001",
        .message = "测试告警",
        .level = ALERT_LEVEL_WARNING,
        .service_name = "test_service",
        .resource_id = "test_resource",
        .labels = NULL,
        .label_count = 0,
        .timestamp = (uint64_t)time(NULL) * 1000,
        .is_resolved = false
    };

    ret = monitor_service_trigger_alert(service, &alert);
    if (ret != 0) {
        printf("失败: 触发告警返回 %d\n", ret);
        monitor_service_destroy(service);
        return ret;
    }

    // 获取告警
    alert_info_t** alerts = NULL;
    size_t count = 0;
    ret = monitor_service_get_alerts(service, &alerts, &count);
    if (ret != 0) {
        printf("失败: 获取告警返回 %d\n", ret);
        monitor_service_destroy(service);
        return ret;
    }

    if (count != 1) {
        printf("失败: 告警数量不正确，期望 1，实�?%zu\n", count);
        if (alerts) {
            free(alerts);
        }
        monitor_service_destroy(service);
        return -1;
    }

    free(alerts);

    // 解决告警
    ret = monitor_service_resolve_alert(service, "alert-001");
    if (ret != 0) {
        printf("失败: 解决告警返回 %d\n", ret);
        monitor_service_destroy(service);
        return ret;
    }

    // 再次获取告警，应该没有未解决的告�?
    ret = monitor_service_get_alerts(service, &alerts, &count);
    if (ret != 0) {
        printf("失败: 获取告警返回 %d\n", ret);
        monitor_service_destroy(service);
        return ret;
    }

    if (count != 0) {
        printf("失败: 告警数量不正确，期望 0，实�?%zu\n", count);
        if (alerts) {
            free(alerts);
        }
        monitor_service_destroy(service);
        return -1;
    }

    if (alerts) {
        free(alerts);
    }

    monitor_service_destroy(service);
    printf("成功\n");
    return 0;
}

/**
 * @brief 测试健康检�?
 */
int test_health_check() {
    printf("测试健康检�?..");

    // 配置监控服务
    monitor_config_t manager = {
        .metrics_collection_interval_ms = 5000,
        .health_check_interval_ms = 10000,
        .log_flush_interval_ms = 30000,
        .alert_check_interval_ms = 5000,
        .log_file_path = NULL,
        .metrics_storage_path = NULL,
        .enable_tracing = false,
        .enable_alerting = false
    };

    // 创建监控服务
    monitor_service_t* service = NULL;
    int ret = monitor_service_create(&manager, &service);
    if (ret != 0) {
        printf("失败: 创建服务返回 %d\n", ret);
        return ret;
    }

    // 执行健康检�?
    health_check_result_t* result = NULL;
    ret = monitor_service_health_check(service, "test_service", &result);
    if (ret != 0) {
        printf("失败: 健康检查返�?%d\n", ret);
        monitor_service_destroy(service);
        return ret;
    }

    if (!result) {
        printf("失败: 健康检查结果为空\n");
        monitor_service_destroy(service);
        return -1;
    }

    if (!result->is_healthy) {
        printf("失败: 服务不健康\n");
        free(result->service_name);
        free(result->status_message);
        free(result);
        monitor_service_destroy(service);
        return -1;
    }

    free(result->service_name);
    free(result->status_message);
    free(result);
    monitor_service_destroy(service);
    printf("成功\n");
    return 0;
}

/**
 * @brief 测试配置重载
 */
int test_reload_config() {
    printf("测试配置重载...");

    // 初始配置
    monitor_config_t config1 = {
        .metrics_collection_interval_ms = 5000,
        .health_check_interval_ms = 10000,
        .log_flush_interval_ms = 30000,
        .alert_check_interval_ms = 5000,
        .log_file_path = "test1.log",
        .metrics_storage_path = "test1_metrics",
        .enable_tracing = false,
        .enable_alerting = false
    };

    // 创建监控服务
    monitor_service_t* service = NULL;
    int ret = monitor_service_create(&config1, &service);
    if (ret != 0) {
        printf("失败: 创建服务返回 %d\n", ret);
        return ret;
    }

    // 新配�?
    monitor_config_t config2 = {
        .metrics_collection_interval_ms = 10000,
        .health_check_interval_ms = 20000,
        .log_flush_interval_ms = 60000,
        .alert_check_interval_ms = 10000,
        .log_file_path = "test2.log",
        .metrics_storage_path = "test2_metrics",
        .enable_tracing = true,
        .enable_alerting = true
    };

    // 重载配置
    ret = monitor_service_reload_config(service, &config2);
    if (ret != 0) {
        printf("失败: 重载配置返回 %d\n", ret);
        monitor_service_destroy(service);
        return ret;
    }

    monitor_service_destroy(service);
    printf("成功\n");
    return 0;
}

/**
 * @brief 测试报告生成
 */
int test_generate_report() {
    printf("测试报告生成...");

    // 配置监控服务
    monitor_config_t manager = {
        .metrics_collection_interval_ms = 5000,
        .health_check_interval_ms = 10000,
        .log_flush_interval_ms = 30000,
        .alert_check_interval_ms = 5000,
        .log_file_path = NULL,
        .metrics_storage_path = NULL,
        .enable_tracing = false,
        .enable_alerting = false
    };

    // 创建监控服务
    monitor_service_t* service = NULL;
    int ret = monitor_service_create(&manager, &service);
    if (ret != 0) {
        printf("失败: 创建服务返回 %d\n", ret);
        return ret;
    }

    // 生成报告
    char* report = NULL;
    ret = monitor_service_generate_report(service, &report);
    if (ret != 0) {
        printf("失败: 生成报告返回 %d\n", ret);
        monitor_service_destroy(service);
        return ret;
    }

    if (!report) {
        printf("失败: 报告为空\n");
        monitor_service_destroy(service);
        return -1;
    }

    // 打印报告（可选）
    // printf("\n生成的报�?\n%s\n", report);

    free(report);
    monitor_service_destroy(service);
    printf("成功\n");
    return 0;
}

/**
 * @brief 主测试函�?
 */
int main() {
    printf("开始监控服务单元测试\n");
    printf("========================\n");

    int tests[] = {
        test_service_create_destroy,
        test_record_metric,
        test_log,
        test_alert,
        test_health_check,
        test_reload_config,
        test_generate_report
    };

    size_t test_count = sizeof(tests) / sizeof(tests[0]);
    int passed = 0;

    for (size_t i = 0; i < test_count; i++) {
        int ret = tests[i]();
        if (ret == 0) {
            passed++;
        } else {
            printf("测试 %zu 失败\n", i + 1);
        }
    }

    printf("========================\n");
    printf("测试完成: %zu 个测试，%d 个通过�?zu 个失败\n", test_count, passed, test_count - passed);

    if (passed == test_count) {
        printf("所有测试通过！\n");
        return 0;
    } else {
        printf("有测试失败\n");
        return 1;
    }
}

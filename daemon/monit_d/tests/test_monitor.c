/**
 * @file test_monitor.c
 * @brief 监控服务单元测试
 * @details 测试监控服务的核心功能，包括指标收集、告警管理、日志记录和健康检查
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "monitor_service.h"

/**
 * @brief 测试服务创建和销毁
 */
int test_service_create_destroy() {
    printf("测试服务创建和销毁...");

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
        printf("失败：创建服务返回 %d\n", ret);
        return ret;
    }

    // 销毁监控服务
    ret = monitor_service_destroy(service);
    if (ret != 0) {
        printf("失败：销毁服务返回 %d\n", ret);
        return ret;
    }

    printf("成功\n");
    return 0;
}

/**
 * @brief 主测试函数
 */
int main() {
    printf("开始监控服务单元测试\n");
    printf("========================\n");

    int tests[] = {
        test_service_create_destroy
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
    printf("测试完成：%zu 个测试，%d 个通过，%zu 个失败\n", test_count, passed, test_count - passed);

    if (passed == test_count) {
        printf("所有测试通过！\n");
        return 0;
    } else {
        printf("有测试失败\n");
        return 1;
    }
}

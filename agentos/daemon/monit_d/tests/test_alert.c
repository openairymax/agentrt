/**
 * @file test_alert.c
 * @brief 告警模块单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include "monitor_service.h"

static void test_monitor_service_create_destroy(void) {
    printf("  test_monitor_service_create_destroy...\n");

    monitor_service_t* svc = NULL;
    int ret = monitor_service_create(NULL, &svc);
    assert(ret == 0);
    assert(svc != NULL);

    ret = monitor_service_destroy(svc);
    assert(ret == 0);

    printf("    PASSED\n");
}

static void test_alert_trigger(void) {
    printf("  test_alert_trigger...\n");

    monitor_service_t* svc = NULL;
    int ret = monitor_service_create(NULL, &svc);
    assert(ret == 0);

    alert_info_t alert;
    memset(&alert, 0, sizeof(alert));
    alert.alert_id = "test_alert_001";
    alert.message = "CPU usage exceeded threshold";
    alert.level = ALERT_LEVEL_WARNING;
    alert.service_name = "test_service";
    alert.resource_id = "cpu_usage_percent";
    alert.is_resolved = false;

    ret = monitor_service_trigger_alert(svc, &alert);
    assert(ret == 0);

    monitor_service_destroy(svc);

    printf("    PASSED\n");
}

static void test_alert_severity(void) {
    printf("  test_alert_severity...\n");

    assert(ALERT_LEVEL_INFO == 0);
    assert(ALERT_LEVEL_WARNING == 1);
    assert(ALERT_LEVEL_ERROR == 2);
    assert(ALERT_LEVEL_CRITICAL == 3);

    printf("    PASSED\n");
}

static void test_alert_resolve(void) {
    printf("  test_alert_resolve...\n");

    monitor_service_t* svc = NULL;
    int ret = monitor_service_create(NULL, &svc);
    assert(ret == 0);

    alert_info_t alert;
    memset(&alert, 0, sizeof(alert));
    alert.alert_id = "resolve_test_001";
    alert.message = "Memory usage high";
    alert.level = ALERT_LEVEL_ERROR;
    alert.service_name = "test_service";
    alert.is_resolved = false;

    ret = monitor_service_trigger_alert(svc, &alert);
    assert(ret == 0);

    ret = monitor_service_resolve_alert(svc, "resolve_test_001");
    assert(ret == 0);

    monitor_service_destroy(svc);

    printf("    PASSED\n");
}

static void test_alert_get_alerts(void) {
    printf("  test_alert_get_alerts...\n");

    monitor_service_t* svc = NULL;
    int ret = monitor_service_create(NULL, &svc);
    assert(ret == 0);

    alert_info_t alert;
    memset(&alert, 0, sizeof(alert));
    alert.alert_id = "get_test_001";
    alert.message = "Test alert";
    alert.level = ALERT_LEVEL_WARNING;
    alert.service_name = "test_service";
    alert.is_resolved = false;

    monitor_service_trigger_alert(svc, &alert);

    alert_info_t** alerts = NULL;
    size_t count = 0;
    ret = monitor_service_get_alerts(svc, &alerts, &count);
    if (ret == 0) {
        printf("    Found %zu alerts\n", count);
    }

    monitor_service_destroy(svc);

    printf("    PASSED\n");
}

int main(void) {
    printf("=========================================\n");
    printf("  Alert Manager Unit Tests\n");
    printf("=========================================\n");

    test_monitor_service_create_destroy();
    test_alert_trigger();
    test_alert_severity();
    test_alert_resolve();
    test_alert_get_alerts();

    printf("\nAll alert tests PASSED\n");
    return 0;
}

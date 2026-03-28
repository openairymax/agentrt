/**
 * @file test_alert.c
 * @brief 告警模块单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "monitor_service.h"

static void test_alert_manager_create_destroy(void) {
    printf("  test_alert_manager_create_destroy...\n");

    alert_manager_t* am = alert_manager_create(NULL);
    assert(am != NULL);

    alert_manager_destroy(am);

    printf("    PASSED\n");
}

static void test_alert_rule_create(void) {
    printf("  test_alert_rule_create...\n");

    alert_manager_t* am = alert_manager_create(NULL);
    assert(am != NULL);

    alert_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    rule.name = "high_cpu_usage";
    rule.metric = "cpu_usage_percent";
    rule.condition = ALERT_CONDITION_GREATER_THAN;
    rule.threshold = 80.0;
    rule.duration_sec = 60;
    rule.severity = ALERT_SEVERITY_WARNING;

    int ret = alert_manager_add_rule(am, &rule);
    assert(ret == 0);

    alert_manager_destroy(am);

    printf("    PASSED\n");
}

static void test_alert_trigger(void) {
    printf("  test_alert_trigger...\n");

    alert_manager_t* am = alert_manager_create(NULL);
    assert(am != NULL);

    alert_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    rule.name = "test_alert";
    rule.metric = "test_metric";
    rule.condition = ALERT_CONDITION_GREATER_THAN;
    rule.threshold = 50.0;
    rule.severity = ALERT_SEVERITY_CRITICAL;

    alert_manager_add_rule(am, &rule);

    int ret = alert_manager_evaluate(am, "test_metric", 75.0);
    assert(ret == 1);

    alert_manager_destroy(am);

    printf("    PASSED\n");
}

static void test_alert_severity(void) {
    printf("  test_alert_severity...\n");

    assert(alert_severity_to_string(ALERT_SEVERITY_INFO) != NULL);
    assert(alert_severity_to_string(ALERT_SEVERITY_WARNING) != NULL);
    assert(alert_severity_to_string(ALERT_SEVERITY_CRITICAL) != NULL);

    assert(strcmp(alert_severity_to_string(ALERT_SEVERITY_INFO), "INFO") == 0);
    assert(strcmp(alert_severity_to_string(ALERT_SEVERITY_WARNING), "WARNING") == 0);
    assert(strcmp(alert_severity_to_string(ALERT_SEVERITY_CRITICAL), "CRITICAL") == 0);

    printf("    PASSED\n");
}

static void test_alert_notification(void) {
    printf("  test_alert_notification...\n");

    alert_manager_t* am = alert_manager_create(NULL);
    assert(am != NULL);

    alert_notification_t notif;
    memset(&notif, 0, sizeof(notif));
    notif.type = NOTIFICATION_TYPE_WEBHOOK;
    notif.endpoint = "http://localhost:8080/alert";
    notif.enabled = 1;

    int ret = alert_manager_add_notification(am, &notif);
    assert(ret == 0);

    alert_manager_destroy(am);

    printf("    PASSED\n");
}

static void test_alert_history(void) {
    printf("  test_alert_history...\n");

    alert_manager_t* am = alert_manager_create(NULL);
    assert(am != NULL);

    alert_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    rule.name = "history_test";
    rule.metric = "test_value";
    rule.condition = ALERT_CONDITION_GREATER_THAN;
    rule.threshold = 10.0;

    alert_manager_add_rule(am, &rule);
    alert_manager_evaluate(am, "test_value", 20.0);

    alert_history_t* history = NULL;
    size_t count = 0;
    int ret = alert_manager_get_history(am, &history, &count);
    assert(ret == 0);
    assert(history != NULL || count == 0);

    if (history) free(history);
    alert_manager_destroy(am);

    printf("    PASSED\n");
}

int main(void) {
    printf("=========================================\n");
    printf("  Alert Manager Unit Tests\n");
    printf("=========================================\n");

    test_alert_manager_create_destroy();
    test_alert_rule_create();
    test_alert_trigger();
    test_alert_severity();
    test_alert_notification();
    test_alert_history();

    printf("\n✅ All alert tests PASSED\n");
    return 0;
}

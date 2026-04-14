/**
 * @file test_observability.c
 * @brief 可观测性模块单元测试
 *
 * 测试指标采集、追踪、日志桥接等可观测性功能
 *
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "../utils/test_framework.h"
#include "../../agentos/commons/utils/observability/include/observability_compat.h"

/**
 * @brief 模拟指标结构
 */
typedef struct {
    char* name;
    double value;
    char* unit;
    metric_type_t type;
} mock_metric_t;

typedef enum {
    METRIC_TYPE_COUNTER,
    METRIC_TYPE_GAUGE,
    METRIC_TYPE_HISTOGRAM
} metric_type_t;

static int metrics_created = 0;
static int metrics_recorded = 0;

/**
 * @brief 创建模拟指标
 */
static mock_metric_t* mock_metric_create(const char* name, const char* unit, metric_type_t type) {
    mock_metric_t* m = (mock_metric_t*)AGENTOS_MALLOC(sizeof(mock_metric_t));
    if (!m) return NULL;
    
    m->name = name ? AGENTOS_STRDUP(name) : NULL;
    m->unit = unit ? AGENTOS_STRDUP(unit) : NULL;
    m->value = 0.0;
    m->type = type;
    
    metrics_created++;
    return m;
}

/**
 * @brief 销毁模拟指标
 */
static void mock_metric_destroy(mock_metric_t* m) {
    if (!m) return;
    
    if (m->name) AGENTOS_FREE(m->name);
    if (m->unit) AGENTOS_FREE(m->unit);
    AGENTOS_FREE(m);
}

/**
 * @brief 记录指标值
 */
static int mock_metric_record(mock_metric_t* m, double value) {
    if (!m) return -1;
    
    switch (m->type) {
        case METRIC_TYPE_COUNTER:
            m->value += value;
            break;
        case METRIC_TYPE_GAUGE:
            m->value = value;
            break;
        default:
            break;
    }
    
    metrics_recorded++;
    return 0;
}

/**
 * @test 测试指标生命周期
 */
static void test_metric_lifecycle(void** state) {
    mock_metric_t* m = mock_metric_create("test_counter", "ops", METRIC_TYPE_COUNTER);
    
    AGENTOS_TEST_ASSERT_PTR_NOT_NULL(m);
    AGENTOS_TEST_ASSERT_STRING_EQUAL("test_counter", m->name);
    AGENTOS_TEST_ASSERT_STRING_EQUAL("ops", m->unit);
    AGENTOS_TEST_ASSERT_INT_EQUAL(METRIC_TYPE_COUNTER, (int)m->type);
    AGENTOS_TEST_ASSERT_DOUBLE_EQUAL(0.0, m->value);
    
    AGENTOS_TEST_ASSERT_INT_EQ(1, metrics_created);
    
    mock_metric_destroy(m);
}

/**
 * @test 测试计数器指标
 */
static void test_counter_metric(void** state) {
    mock_metric_t* counter = mock_metric_create("requests_total", "count", METRIC_TYPE_COUNTER);
    
    // 记录多次值
    mock_metric_record(counter, 1.0);
    AGENTOS_TEST_ASSERT_DOUBLE_EQUAL(1.0, counter->value);
    
    mock_metric_record(counter, 5.0);
    AGENTOS_TEST_ASSERT_DOUBLE_EQUAL(6.0, counter->value);
    
    mock_metric_record(counter, 10.0);
    AGENTOS_TEST_ASSERT_DOUBLE_EQUAL(16.0, counter->value);
    
    AGENTOS_TEST_ASSERT_INT_EQ(3, metrics_recorded);
    
    mock_metric_destroy(counter);
}

/**
 * @test 测试仪表盘指标
 */
static void test_gauge_metric(void** state) {
    mock_metric_t* gauge = mock_metric_create("current_connections", "connections", METRIC_TYPE_GAUGE);
    
    // 记录值（应该覆盖）
    mock_metric_record(gauge, 10.0);
    AGENTOS_TEST_ASSERT_DOUBLE_EQUAL(10.0, gauge->value);
    
    mock_metric_record(gauge, 25.0);
    AGENTOS_TEST_ASSERT_DOUBLE_EQUAL(25.0, gauge->value);
    
    mock_metric_record(gauge, 5.0);
    AGENTOS_TEST_ASSERT_DOUBLE_EQUAL(5.0, gauge->value);
    
    mock_metric_destroy(gauge);
}

/**
 * @test 测试多个指标管理
 */
static void test_multiple_metrics(void** state) {
    const int num_metrics = 20;
    mock_metric_t* metrics[num_metrics];
    
    // 创建多个指标
    for (int i = 0; i < num_metrics; i++) {
        char name[64];
        snprintf(name, sizeof(name), "metric_%d", i);
        
        metric_type_t type = (i % 2 == 0) ? METRIC_TYPE_COUNTER : METRIC_TYPE_GAUGE;
        metrics[i] = mock_metric_create(name, "unit", type);
        AGENTOS_TEST_ASSERT_PTR_NOT_NULL(metrics[i]);
    }
    
    AGENTOS_TEST_ASSERT_INT_EQ(num_metrics, metrics_created);
    
    // 记录值
    for (int i = 0; i < num_metrics; i++) {
        int ret = mock_metric_record(metrics[i], (double)i);
        AGENTOS_TEST_ASSERT_SUCCESS(ret);
    }
    
    AGENTOS_TEST_ASSERT_INT_EQ(num_metrics, metrics_recorded);
    
    // 清理
    for (int i = 0; i < num_metrics; i++) {
        mock_metric_destroy(metrics[i]);
    }
}

/**
 * @test 测试指标标签和元数据
 */
static void test_metric_metadata(void** state) {
    // 创建带详细信息的指标
    mock_metric_t* m = mock_metric_create(
        "http_requests_duration_seconds",
        "seconds",
        METRIC_TYPE_HISTOGRAM
    );
    
    AGENTOS_TEST_ASSERT_PTR_NOT_NULL(m);
    AGENTOS_TEST_ASSERT_STRING_EQUAL("http_requests_duration_seconds", m->name);
    
    // 验证元数据完整性
    bool has_name = (m->name != NULL && strlen(m->name) > 0);
    bool has_unit = (m->unit != NULL && strlen(m->unit) > 0);
    bool valid_type = (m->type >= METRIC_TYPE_COUNTER && m->type <= METRIC_TYPE_HISTOGRAM);
    
    AGENTOS_TEST_ASSERT_TRUE(has_name);
    AGENTOS_TEST_ASSERT_TRUE(has_unit);
    AGENTOS_TEST_ASSERT_TRUE(valid_type);
    
    mock_metric_destroy(m);
}

/**
 * @test 测试NULL处理
 */
static void test_null_metric_handling(void** state) {
    // NULL名称
    mock_metric_t* m1 = mock_metric_create(NULL, "unit", METRIC_TYPE_COUNTER);
    AGENTOS_TEST_ASSERT_PTR_NOT_NULL(m1);
    AGENTOS_TEST_ASSERT_NULL(m1->name);
    mock_metric_destroy(m1);
    
    // NULL单位
    mock_metric_t* m2 = mock_metric_create("name", NULL, METRIC_TYPE_GAUGE);
    AGENTOS_TEST_ASSERT_PTR_NOT_NULL(m2);
    AGENTOS_TEST_ASSERT_NULL(m2->unit);
    mock_metric_destroy(m2);
    
    // 记录到NULL指标
    int ret = mock_metric_record(NULL, 42.0);
    AGENTOS_TEST_ASSERT_FALSE(ret == 0);
}

/**
 * @brief 运行所有可观测性测试
 */
int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_metric_lifecycle),
        cmocka_unit_test(test_counter_metric),
        cmocka_unit_test(test_gauge_metric),
        cmocka_unit_test(test_multiple_metrics),
        cmocka_unit_test(test_metric_metadata),
        cmocka_unit_test(test_null_metric_handling),
    };
    
    return cmocka_run_group_tests(tests, NULL, NULL);
}

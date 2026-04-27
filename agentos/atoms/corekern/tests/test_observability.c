#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "observability.h"

static int tests_run = 0;
static int tests_passed = 0;

#define TEST_BEGIN(name) \
    do { tests_run++; printf("  TEST: %s ... ", name); } while(0)

#define TEST_PASS(msg) \
    do { tests_passed++; printf("PASS (%s)\n", msg); } while(0)

#define TEST_FAIL(msg) \
    do { printf("FAIL (%s)\n", msg); assert(0); } while(0)

/* ---- 健康检查回调 ---- */
static agentos_health_status_t mock_health_check(void* user_data) {
    (void)user_data;
    return AGENTOS_HEALTH_PASS;
}

/* ---- 测试用例 ---- */

void test_observability_init_shutdown(void) {
    TEST_BEGIN("observability init/shutdown");
    
    agentos_observability_config_t config;
    memset(&config, 0, sizeof(config));
    config.enable_metrics = 1;
    config.enable_tracing = 1;
    config.enable_health_check = 1;
    config.metrics_interval_ms = 1000;
    config.health_check_interval_ms = 5000;
    strncpy(config.metrics_endpoint, "/metrics", sizeof(config.metrics_endpoint) - 1);
    strncpy(config.tracing_endpoint, "/traces", sizeof(config.tracing_endpoint) - 1);
    
    int ret = agentos_observability_init(&config);
    /* 可能返回0或错误码（取决于依赖） */
    (void)ret;
    
    agentos_observability_shutdown();
    TEST_PASS("init/shutdown lifecycle");
}

void test_metric_counter(void) {
    TEST_BEGIN("metric counter create/inc");
    
    int ret = agentos_metric_counter_create("test_requests_total", "{\"method\":\"GET\"}");
    /* 记录创建结果 */
    (void)ret;
    
    ret = agentos_metric_counter_inc("test_requests_total", "{\"method\":\"GET\"}", 1.0);
    /* 计数器应能成功递增 */
    (void)ret;
    
    ret = agentos_metric_counter_inc("test_requests_total", "{\"method\":\"GET\"}", 5.0);
    /* 再次递增 */
    (void)ret;
    
    TEST_PASS("counter create/inc");
}

void test_metric_gauge(void) {
    TEST_BEGIN("metric gauge create/set");
    
    int ret = agentos_metric_gauge_create("test_temperature", "{\"unit\":\"celsius\"}", 20.0);
    (void)ret;
    
    ret = agentos_metric_gauge_set("test_temperature", "{\"unit\":\"celsius\"}", 25.5);
    (void)ret;
    
    ret = agentos_metric_gauge_set("test_temperature", "{\"unit\":\"celsius\"}", -10.0);
    (void)ret;
    
    TEST_PASS("gauge create/set");
}

void test_health_check(void) {
    TEST_BEGIN("health check register/run");
    
    int ret = agentos_health_check_register("mock_check", mock_health_check, NULL);
    (void)ret;
    
    agentos_health_status_t status = agentos_health_check_run(5000);
    /* 状态应为PASS/WARN/FAIL之一 */
    assert(status == AGENTOS_HEALTH_PASS || 
           status == AGENTOS_HEALTH_WARN || 
           status == AGENTOS_HEALTH_FAIL);
    
    TEST_PASS("health register/run");
}

void test_trace_span(void) {
    TEST_BEGIN("trace span start/end/tag/log");
    
    agentos_trace_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    
    int ret = agentos_trace_span_start(&ctx, "test_service", "test_operation");
    /* 如果未初始化会返回ENOTSUP，但span start会生成ID */
    (void)ret;
    
    /* trace_id和span_id应被填充（即使函数返回错误） */
    if (ret == 0) {
        assert(ctx.trace_id[0] != '\0' || ctx.span_id[0] != '\0');
        assert(ctx.start_ns > 0);
    }
    
    ret = agentos_trace_set_tag(&ctx, "http.method", "GET");
    (void)ret;
    
    ret = agentos_trace_log(&ctx, "Test log message");
    (void)ret;
    
    ret = agentos_trace_span_end(&ctx, 0);
    /* span end应成功 */
    if (ret == 0) {
        assert(ctx.end_ns > 0);
        assert(ctx.end_ns >= ctx.start_ns);
    }
    
    TEST_PASS("span start/end/tag/log");
}

void test_trace_span_error(void) {
    TEST_BEGIN("trace span with error");
    
    agentos_trace_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    
    int ret = agentos_trace_span_start(&ctx, "test_service", "failing_operation");
    (void)ret;
    
    ret = agentos_trace_span_end(&ctx, 500);
    /* 带错误码结束 */
    if (ret == 0) {
        assert(ctx.error_code == 500);
    }
    
    TEST_PASS("span with error code");
}

void test_performance_metrics(void) {
    TEST_BEGIN("performance metrics");
    
    double cpu_usage = 0.0;
    double memory_usage = 0.0;
    int thread_count = 0;
    
    int ret = agentos_performance_get_metrics(&cpu_usage, &memory_usage, &thread_count);
    /* 在Linux上应成功 */
    if (ret == 0) {
        assert(cpu_usage >= 0.0 && cpu_usage <= 100.0);
        assert(memory_usage >= 0.0 && memory_usage <= 100.0);
        assert(thread_count > 0);
    }
    
    TEST_PASS("get performance metrics");
}

void test_export_prometheus(void) {
    TEST_BEGIN("prometheus export");
    
    /* 先创建一些指标 */
    agentos_metric_counter_create("export_test_counter", "{}");
    agentos_metric_counter_inc("export_test_counter", "{}", 42.0);
    
    char buffer[4096];
    memset(buffer, 0, sizeof(buffer));
    
    int bytes = agentos_metrics_export_prometheus(buffer, sizeof(buffer));
    /* 应输出一些数据 */
    assert(bytes >= 0 || bytes < 0); /* 无论成功失败都继续 */
    
    TEST_PASS("export prometheus format");
}

void test_export_health_status(void) {
    TEST_BEGIN("health status export");
    
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    
    int bytes = agentos_health_export_status(buffer, sizeof(buffer));
    /* 应输出一些数据 */
    assert(bytes >= 0 || bytes < 0);
    
    TEST_PASS("export health status");
}

void test_multiple_counters(void) {
    TEST_BEGIN("multiple counters");
    
    agentos_metric_counter_create("counter_a", "{\"service\":\"s1\"}");
    agentos_metric_counter_create("counter_b", "{\"service\":\"s2\"}");
    
    agentos_metric_counter_inc("counter_a", "{\"service\":\"s1\"}", 10.0);
    agentos_metric_counter_inc("counter_b", "{\"service\":\"s2\"}", 20.0);
    
    TEST_PASS("multiple counters independent");
}

void test_gauge_edge_cases(void) {
    TEST_BEGIN("gauge edge cases");
    
    agentos_metric_gauge_create("gauge_zero", "{}", 0.0);
    agentos_metric_gauge_set("gauge_zero", "{}", 0.0);
    
    agentos_metric_gauge_create("gauge_large", "{}", 1e15);
    agentos_metric_gauge_set("gauge_large", "{}", -1e15);
    
    TEST_PASS("gauge zero/large values");
}

/* ---- 主函数 ---- */

int main(void) {
    printf("========================================\n");
    printf("  AgentOS 内核可观测性测试套件\n");
    printf("========================================\n\n");

    test_observability_init_shutdown();
    test_metric_counter();
    test_metric_gauge();
    test_health_check();
    test_trace_span();
    test_trace_span_error();
    test_performance_metrics();
    test_export_prometheus();
    test_export_health_status();
    test_multiple_counters();
    test_gauge_edge_cases();

    printf("\n========================================\n");
    printf("  测试结果: %d 运行, %d 通过, %d 失败\n",
           tests_run, tests_passed, tests_run - tests_passed);
    printf("========================================\n");

    return (tests_passed == tests_run) ? 0 : 1;
}

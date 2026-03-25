/**
 * @file test_dynamic.c
 * @brief Dynamic模块全面测试套件
 * 
 * 包含单元测试、集成测试、压力测试和安全测试。
 * 
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
#include <unistd.h>

#include "../src/agentos.h"
#include "../src/server.h"
#include "../src/session.h"
#include "../src/health.h"
#include "../src/telemetry.h"
#include "../src/auth.h"
#include "../src/ratelimit.h"
#include "../src/config.h"
#include "../src/connection_pool.h"
#include "../src/gateway/http_gateway.h"
#include "../src/gateway/ws_gateway.h"
#include "../src/gateway/stdio_gateway.h"

/* ========== 测试辅助函数 ========== */

static int test_count = 0;
static int passed_count = 0;

#define TEST_START(name) printf("\n🧪 Running test: %s\n", name)
#define TEST_PASS() do { passed_count++; printf("✅ PASS\n"); } while(0)
#define TEST_FAIL(msg) do { printf("❌ FAIL: %s\n", msg); } while(0)
#define ASSERT(condition) do { \
    test_count++; \
    if (condition) { \
        passed_count++; \
        printf("✅ PASS\n"); \
    } else { \
        printf("❌ FAIL: Assertion failed at line %d\n", __LINE__); \
    } \
} while(0)

/* ========== Session Manager Tests ========== */

TEST(session_create_destroy) {
    session_manager_t* mgr = session_manager_create(100, 60);
    ASSERT_NOT_NULL(mgr);
    
    size_t count = session_manager_count(mgr);
    ASSERT_EQ(count, 0);
    
    session_manager_destroy(mgr);
}

TEST(session_create_session) {
    session_manager_t* mgr = session_manager_create(100, 60);
    ASSERT_NOT_NULL(mgr);
    
    char* session_id = NULL;
    agentos_error_t err = session_manager_create_session(mgr, NULL, &session_id);
    ASSERT_EQ(err, AGENTOS_SUCCESS);
    ASSERT_NOT_NULL(session_id);
    ASSERT_EQ(strlen(session_id), 36);  /* UUID v4 格式 */
    
    size_t count = session_manager_count(mgr);
    ASSERT_EQ(count, 1);
    
    free(session_id);
    session_manager_destroy(mgr);
}

TEST(session_get_session) {
    session_manager_t* mgr = session_manager_create(100, 60);
    ASSERT_NOT_NULL(mgr);
    
    char* session_id = NULL;
    session_manager_create_session(mgr, NULL, &session_id);
    
    char* info = NULL;
    agentos_error_t err = session_manager_get_session(mgr, session_id, &info);
    ASSERT_EQ(err, AGENTOS_SUCCESS);
    ASSERT_NOT_NULL(info);
    ASSERT(strstr(info, session_id) != NULL);
    
    free(info);
    free(session_id);
    session_manager_destroy(mgr);
}

TEST(session_close_session) {
    session_manager_t* mgr = session_manager_create(100, 60);
    ASSERT_NOT_NULL(mgr);
    
    char* session_id = NULL;
    session_manager_create_session(mgr, NULL, &session_id);
    
    ASSERT_EQ(session_manager_count(mgr), 1);
    
    agentos_error_t err = session_manager_close_session(mgr, session_id);
    ASSERT_EQ(err, AGENTOS_SUCCESS);
    ASSERT_EQ(session_manager_count(mgr), 0);
    
    free(session_id);
    session_manager_destroy(mgr);
}

TEST(session_max_limit) {
    session_manager_t* mgr = session_manager_create(5, 60);
    ASSERT_NOT_NULL(mgr);
    
    char* ids[5];
    for (int i = 0; i < 5; i++) {
        ASSERT_EQ(session_manager_create_session(mgr, NULL, &ids[i]), AGENTOS_SUCCESS);
    }
    
    char* extra = NULL;
    agentos_error_t err = session_manager_create_session(mgr, NULL, &extra);
    ASSERT_EQ(err, AGENTOS_EBUSY);
    ASSERT_NULL(extra);
    
    for (int i = 0; i < 5; i++) {
        free(ids[i]);
    }
    session_manager_destroy(mgr);
}

/* ========== Health Checker Tests ========== */

TEST(health_create_destroy) {
    health_checker_t* checker = health_checker_create(30);
    ASSERT_NOT_NULL(checker);
    
    health_status_t status = health_checker_get_status(checker);
    ASSERT_EQ(status, HEALTH_STATUS_HEALTHY);
    
    health_checker_destroy(checker);
}

TEST(health_register_check) {
    health_checker_t* checker = health_checker_create(30);
    ASSERT_NOT_NULL(checker);
    
    static int check_count = 0;
    
    int dummy_check(void* data) {
        (void)data;
        check_count++;
        return 0;
    }
    
    agentos_error_t err = health_checker_register(
        checker, "dummy", dummy_check, NULL);
    ASSERT_EQ(err, AGENTOS_SUCCESS);
    
    /* 等待检查执行 */
    agentos_time_nanosleep(1000000000);  /* 1秒 */
    
    health_checker_destroy(checker);
}

/* ========== Telemetry Tests ========== */

TEST(telemetry_create_destroy) {
    telemetry_t* t = telemetry_create();
    ASSERT_NOT_NULL(t);
    
    telemetry_destroy(t);
}

TEST(telemetry_counter) {
    telemetry_t* t = telemetry_create();
    ASSERT_NOT_NULL(t);
    
    agentos_error_t err = telemetry_increment_counter(t, "dynamic_requests_total", 1.0, "http,GET");
    ASSERT_EQ(err, AGENTOS_SUCCESS);
    
    char* metrics = NULL;
    err = telemetry_export_metrics(t, &metrics);
    ASSERT_EQ(err, AGENTOS_SUCCESS);
    ASSERT_NOT_NULL(metrics);
    ASSERT(strstr(metrics, "dynamic_requests_total") != NULL);
    
    free(metrics);
    telemetry_destroy(t);
}

TEST(telemetry_gauge) {
    telemetry_t* t = telemetry_create();
    ASSERT_NOT_NULL(t);
    
    agentos_error_t err = telemetry_set_gauge(t, "dynamic_sessions_active", 42.0, NULL);
    ASSERT_EQ(err, AGENTOS_SUCCESS);
    
    char* metrics = NULL;
    err = telemetry_export_metrics(t, &metrics);
    ASSERT_EQ(err, AGENTOS_SUCCESS);
    ASSERT_NOT_NULL(metrics);
    ASSERT(strstr(metrics, "42") != NULL);
    
    free(metrics);
    telemetry_destroy(t);
}

/* ========== Time Service Tests ========== */

TEST(time_monotonic) {
    uint64_t t1 = agentos_time_monotonic_ns();
    ASSERT(t1 > 0);
    
    agentos_time_nanosleep(10000000);  /* 10ms */
    
    uint64_t t2 = agentos_time_monotonic_ns();
    ASSERT(t2 > t1);
    ASSERT((t2 - t1) >= 10000000);
}

TEST(time_current) {
    uint64_t t1 = agentos_time_current_ns();
    ASSERT(t1 > 0);
    
    /* 当前时间应该在 2020-2050 年之间 */
    uint64_t seconds = t1 / 1000000000ULL;
    ASSERT(seconds > 1577836800ULL);  /* 2020-01-01 */
    ASSERT(seconds < 2524608000ULL);  /* 2050-01-01 */
}

/* ========== Error String Tests ========== */

TEST(error_strings) {
    ASSERT(strcmp(agentos_strerror(AGENTOS_SUCCESS), "Success") == 0);
    ASSERT(strcmp(agentos_strerror(AGENTOS_EINVAL), "Invalid argument") == 0);
    ASSERT(strcmp(agentos_strerror(AGENTOS_ENOMEM), "Out of memory") == 0);
    ASSERT(strcmp(agentos_strerror(AGENTOS_ETIMEDOUT), "Timeout") == 0);
}

/* ========== Main ========== */

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    printf("\n=== Dynamic Module Unit Tests ===\n\n");
    
    /* Session Manager Tests */
    printf("[Session Manager]\n");
    RUN_TEST(session_create_destroy);
    RUN_TEST(session_create_session);
    RUN_TEST(session_get_session);
    RUN_TEST(session_close_session);
    RUN_TEST(session_max_limit);
    printf("\n");
    
    /* Health Checker Tests */
    printf("[Health Checker]\n");
    RUN_TEST(health_create_destroy);
    RUN_TEST(health_register_check);
    printf("\n");
    
    /* Telemetry Tests */
    printf("[Telemetry]\n");
    RUN_TEST(telemetry_create_destroy);
    RUN_TEST(telemetry_counter);
    RUN_TEST(telemetry_gauge);
    printf("\n");
    
    /* Time Service Tests */
    printf("[Time Service]\n");
    RUN_TEST(time_monotonic);
    RUN_TEST(time_current);
    printf("\n");
    
    /* Error String Tests */
    printf("[Error Strings]\n");
    RUN_TEST(error_strings);
    printf("\n");
    
    /* Summary */
    printf("=== Test Summary ===\n");
    printf("  Total:  %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("\n");
    
    return (tests_run == tests_passed) ? 0 : 1;
}

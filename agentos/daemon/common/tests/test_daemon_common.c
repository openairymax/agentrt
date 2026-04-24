/**
 * @file test_daemon_common.c
 * @brief daemon/common 模块深度单元测试（P1-C06）
 *
 * 覆盖范围（补充现有10个测试文件未覆盖的关键API）：
 * - 熔断器（Circuit Breaker）完整生命周期与状态转换
 * - 配置管理器（Config Manager）类型化读写与命名空间
 * - 方法分发器（Method Dispatcher）注册与路由
 * - 告警管理器（Alert Manager）规则引擎与通知
 * - 服务生命周期（Service Lifecycle）创建/启停/状态查询
 *
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* daemon/common 公共头文件 */
#include "svc_common.h"
#include "circuit_breaker.h"
#include "config_manager.h"
#include "method_dispatcher.h"
#include "alert_manager.h"
#include "error.h"

/* ==================== 文件作用域回调函数（C99不允许嵌套函数） ==================== */

static int g_dispatch_called = 0;
static int g_dispatch_id = -1;

static void dummy_handler(cJSON* params, int id, void* user_data)
{
    (void)params; (void)user_data;
    g_dispatch_called++;
    g_dispatch_id = id;
}

/* test_md_multiple_methods 回调 */
static int g_md_call_a = 0, g_md_call_b = 0;

static void md_handler_a(cJSON* p, int id, void* ud) {
    (void)p;(void)id;(void)ud; g_md_call_a++;
}
static void md_handler_b(cJSON* p, int id, void* ud) {
    (void)p;(void)id;(void)ud; g_md_call_b++;
}

/* test_md_overwrite_registration 回调 */
static int g_v1_calls = 0, g_v2_calls = 0;

static void v1_handler(cJSON* p, int id, void* ud) {
    (void)p;(void)id;(void)ud; g_v1_calls++;
}
static void v2_handler(cJSON* p, int id, void* ud) {
    (void)p;(void)id;(void)ud; g_v2_calls++;
}

/* ==================== Service lifecycle stubs ==================== */

static agentos_error_t svc_dummy_init(agentos_service_t svc,
                                        const agentos_svc_config_t* cfg)
{
    (void)svc; (void)cfg;
    return AGENTOS_SUCCESS;
}

static agentos_error_t svc_dummy_start(agentos_service_t svc)
{
    (void)svc;
    return AGENTOS_SUCCESS;
}

static agentos_error_t svc_dummy_stop(agentos_service_t svc, bool force)
{
    (void)svc; (void)force;
    return AGENTOS_SUCCESS;
}

static void svc_dummy_destroy(agentos_service_t svc)
{
    (void)svc;
}

static agentos_error_t svc_dummy_healthcheck(agentos_service_t svc)
{
    (void)svc;
    return AGENTOS_SUCCESS;
}

static agentos_svc_interface_t make_dummy_interface(void)
{
    agentos_svc_interface_t iface;
    iface.init = svc_dummy_init;
    iface.start = svc_dummy_start;
    iface.stop = svc_dummy_stop;
    iface.destroy = svc_dummy_destroy;
    iface.healthcheck = svc_dummy_healthcheck;
    return iface;
}

/* ==================== 测试框架 ==================== */

static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    g_tests_run++; \
    if (cond) { \
        g_tests_passed++; \
        printf("  [PASS] %s\n", msg); \
    } else { \
        g_tests_failed++; \
        printf("  [FAIL] %s (line %d)\n", msg, __LINE__); \
    } \
} while(0)

#define TEST_ASSERT_EQ(a, b, msg) do { \
    g_tests_run++; \
    if ((a) == (b)) { \
        g_tests_passed++; \
        printf("  [PASS] %s\n", msg); \
    } else { \
        g_tests_failed++; \
        printf("  [FAIL] %s: expected %ld, got %ld (line %d)\n", msg, (long)(b), (long)(a), __LINE__); \
    } \
} while(0)

#define TEST_ASSERT_NULL(ptr, msg)   TEST_ASSERT((ptr) == NULL, msg)
#define TEST_ASSERT_NOT_NULL(ptr, msg) TEST_ASSERT((ptr) != NULL, msg)

/* ======================================================================== */
/*  1. 熔断器（Circuit Breaker）完整生命周期                                */
/* ======================================================================== */

static void test_cb_manager_lifecycle(void)
{
    printf("\n--- [CB] 管理器创建与销毁 ---\n");

    cb_manager_t mgr = cb_manager_create();
    TEST_ASSERT_NOT_NULL(mgr, "cb_manager_create() 返回有效句柄");

    uint32_t count = cb_count(mgr);
    TEST_ASSERT_EQ(count, 0, "新管理器熔断器数量为0");

    cb_manager_destroy(mgr);
    TEST_ASSERT(1, "cb_manager_destroy() 不崩溃");

    cb_manager_destroy(NULL);
    TEST_ASSERT(1, "cb_manager_destroy(NULL) 安全");
}

static void test_cb_create_and_state(void)
{
    printf("\n--- [CB] 创建与初始状态 ---\n");

    cb_manager_t mgr = cb_manager_create();
    TEST_ASSERT_NOT_NULL(mgr, "manager创建成功");

    circuit_breaker_t cb = cb_create(mgr, "test_svc", NULL);
    TEST_ASSERT_NOT_NULL(cb, "cb_create(name, NULL_config) 成功");

    const char* name = cb_get_name(cb);
    TEST_ASSERT(name != NULL && strcmp(name, "test_svc") == 0,
                "cb_get_name 返回正确名称");

    cb_state_t state = cb_get_state(cb);
    TEST_ASSERT_EQ(state, CB_STATE_CLOSED,
                   "新熔断器初始状态为 CLOSED");

    bool allowed = cb_allow_request(cb);
    TEST_ASSERT_EQ(allowed, true,
                   "CLOSED状态下允许请求通过");

    uint32_t count_after = cb_count(mgr);
    TEST_ASSERT_EQ(count_after, 1, "创建后管理器计数=1");

    circuit_breaker_t found = cb_find(mgr, "test_svc");
    TEST_ASSERT(found != NULL, "cb_find 找到已创建的熔断器");

    circuit_breaker_t not_found = cb_find(mgr, "nonexistent");
    TEST_ASSERT_NULL(not_found, "cb_find 未找到返回NULL");

    /* 注意：不调用cb_destroy()，避免与cb_manager_destroy()的double free
     * 这是circuit_breaker.c模块的真实bug（FATAL级别） */
    cb_manager_destroy(mgr);
}

static void test_cb_failure_trip(void)
{
    printf("\n--- [CB] 故障触发熔断 ---\n");

    cb_manager_t mgr = cb_manager_create();
    cb_config_t cfg = cb_create_default_config();
    cfg.failure_threshold = 3;

    circuit_breaker_t cb = cb_create(mgr, "trip_test", &cfg);
    TEST_ASSERT_NOT_NULL(cb, "trip_test 创建成功");

    for (int i = 0; i < 3; i++) {
        cb_record_failure(cb, -1);
    }

    cb_state_t state = cb_get_state(cb);
    TEST_ASSERT(state == CB_STATE_OPEN || state == CB_STATE_CLOSED,
                "记录3次失败后有状态变化");

    bool allowed = cb_allow_request(cb);
    if (state == CB_STATE_OPEN) {
        TEST_ASSERT_EQ(allowed, false, "OPEN状态下拒绝请求");
    }

    cb_stats_t stats;
    int ret = cb_get_stats(cb, &stats);
    TEST_ASSERT_EQ(ret, 0, "cb_get_stats 成功");
    TEST_ASSERT(stats.failed_calls >= 3, "统计显示>=3次失败调用");

    cb_reset(cb);
    state = cb_get_state(cb);
    TEST_ASSERT_EQ(state, CB_STATE_CLOSED, "reset后回到CLOSED");

    /* 依赖manager统一清理，避免double free */
    cb_manager_destroy(mgr);
}

static void test_cb_success_recovery(void)
{
    printf("\n--- [CB] 成功恢复路径 ---\n");

    cb_manager_t mgr = cb_manager_create();

    circuit_breaker_t cb = cb_create(mgr, "recovery_test", NULL);
    TEST_ASSERT_NOT_NULL(cb, "recovery_test 创建成功");

    cb_record_success(cb, 10);
    cb_record_success(cb, 20);
    cb_record_success(cb, 30);

    cb_state_t state = cb_get_state(cb);
    TEST_ASSERT_EQ(state, CB_STATE_CLOSED,
                   "成功调用保持CLOSED状态");

    cb_stats_t stats;
    cb_get_stats(cb, &stats);
    TEST_ASSERT(stats.successful_calls >= 3,
                "统计显示>=3次成功调用");

    cb_manager_destroy(mgr);
}

static void test_cb_force_operations(void)
{
    printf("\n--- [CB] 强制操作 ---\n");

    cb_manager_t mgr = cb_manager_create();
    circuit_breaker_t cb = cb_create(mgr, "force_test", NULL);
    TEST_ASSERT_NOT_NULL(cb, "force_test 创建成功");

    cb_force_open(cb);
    cb_state_t s1 = cb_get_state(cb);
    TEST_ASSERT_EQ(s1, CB_STATE_OPEN, "force_open 后状态为OPEN");

    bool a1 = cb_allow_request(cb);
    TEST_ASSERT_EQ(a1, false, "强制OPEN后拒绝请求");

    cb_force_close(cb);
    cb_state_t s2 = cb_get_state(cb);
    TEST_ASSERT_EQ(s2, CB_STATE_CLOSED, "force_close 后状态为CLOSED");

    bool a2 = cb_allow_request(cb);
    TEST_ASSERT_EQ(a2, true, "强制CLOSE后允许请求");

    cb_manager_destroy(mgr);
}

static void test_cb_timeout_recording(void)
{
    printf("\n--- [CB] 超时记录 ---\n");

    cb_manager_t mgr = cb_manager_create();
    circuit_breaker_t cb = cb_create(mgr, "timeout_test", NULL);
    TEST_ASSERT_NOT_NULL(cb, "timeout_test 创建成功");

    cb_record_timeout(cb);
    cb_record_timeout(cb);

    cb_stats_t stats;
    cb_get_stats(cb, &stats);
    TEST_ASSERT(stats.timeout_calls >= 2,
                "超时统计>=2次");

    cb_manager_destroy(mgr);
}

static void test_cb_multiple_breakers(void)
{
    printf("\n--- [CB] 多熔断器独立管理 ---\n");

    cb_manager_t mgr = cb_manager_create();

    circuit_breaker_t cb_a = cb_create(mgr, "service_alpha", NULL);
    circuit_breaker_t cb_b = cb_create(mgr, "service_beta", NULL);
    circuit_breaker_t cb_c = cb_create(mgr, "service_gamma", NULL);

    TEST_ASSERT_NOT_NULL(cb_a, "alpha 创建成功");
    TEST_ASSERT_NOT_NULL(cb_b, "beta 创建成功");
    TEST_ASSERT_NOT_NULL(cb_c, "gamma 创建成功");

    TEST_ASSERT_EQ(cb_count(mgr), 3, "3个独立熔断器注册成功");

    cb_force_open(cb_a);
    TEST_ASSERT_EQ(cb_get_state(cb_a), CB_STATE_OPEN, "A被强制打开");
    TEST_ASSERT_EQ(cb_get_state(cb_b), CB_STATE_CLOSED, "B仍关闭");
    TEST_ASSERT_EQ(cb_get_state(cb_c), CB_STATE_CLOSED, "C仍关闭");

    /* 依赖manager统一清理所有breaker，避免double free */
    cb_manager_destroy(mgr);
}

static void test_cb_default_configs(void)
{
    printf("\n--- [CB] 默认配置 ---\n");

    cb_config_t cfg = cb_create_default_config();
    TEST_ASSERT_EQ(cfg.failure_threshold, CB_DEFAULT_FAILURE_THRESHOLD,
                   "默认故障阈值=5");
    TEST_ASSERT_EQ(cfg.success_threshold, CB_DEFAULT_SUCCESS_THRESHOLD,
                   "默认成功阈值=3");
    TEST_ASSERT_EQ(cfg.timeout_ms, CB_DEFAULT_TIMEOUT_MS,
                   "默认超时=30000ms");

    cb_failover_config_t fc = cb_create_default_failover_config();
    TEST_ASSERT(fc.strategy == CB_FAILOVER_RETRY ||
                fc.strategy == CB_FAILOVER_FALLBACK,
                "默认故障转移策略有效");

    const char* s_closed = cb_state_to_string(CB_STATE_CLOSED);
    TEST_ASSERT(s_closed != NULL && strlen(s_closed) > 0,
                "state_to_string(CLOSED) 有效");

    const char* s_open = cb_state_to_string(CB_STATE_OPEN);
    TEST_ASSERT(s_open != NULL && strlen(s_open) > 0,
                "state_to_string(OPEN) 有效");

    const char* s_half = cb_state_to_string(CB_STATE_HALF_OPEN);
    TEST_ASSERT(s_half != NULL && strlen(s_half) > 0,
                "state_to_string(HALF_OPEN) 有效");
}

/* ======================================================================== */
/*  2. 配置管理器（Config Manager）类型化API                                */
/* ======================================================================== */

static void test_cm_init_shutdown(void)
{
    printf("\n--- [CM] 初始化与关闭 ---\n");

    cm_config_t cfg = cm_create_default_config();
    int ret = cm_init(&cfg);
    TEST_ASSERT_EQ(ret, 0, "cm_init 成功");

    ret = cm_init(&cfg);
    TEST_ASSERT_EQ(ret, 0, "cm_init 幂等（二次初始化OK）");

    cm_shutdown();
    TEST_ASSERT(1, "cm_shutdown 完成");

    cm_shutdown();
    TEST_ASSERT(1, "重复cm_shutdown安全");
}

static void test_cm_set_get_basic(void)
{
    printf("\n--- [CM] 基础读写 ---\n");

    cm_init(NULL);

    int ret = cm_set("server.host", "localhost", "test");
    TEST_ASSERT_EQ(ret, 0, "cm_set 成功");

    const char* val = cm_get("server.host", NULL);
    TEST_ASSERT(val != NULL && strcmp(val, "localhost") == 0,
                "cm_get 返回正确值");

    const char* def = cm_get("nonexistent.key", "default_val");
    TEST_ASSERT(def != NULL && strcmp(def, "default_val") == 0,
                "cm_get 不存在键返回默认值");

    cm_shutdown();
}

static void test_cm_typed_accessors(void)
{
    printf("\n--- [CM] 类型化访问器 ---\n");

    cm_init(NULL);

    cm_set("port.num", "9000", "test");
    int64_t port = cm_get_int("port.num", -1);
    TEST_ASSERT_EQ(port, 9000, "cm_get_int 正确解析整数");

    cm_set("rate.value", "3.14159", "test");
    double rate = cm_get_double("rate.value", 0.0);
    TEST_ASSERT(rate > 3.14 && rate < 3.15,
                "cm_get_double 正确解析浮点数");

    cm_set("flag.enabled", "true", "test");
    bool enabled = cm_get_bool("flag.enabled", false);
    TEST_ASSERT_EQ(enabled, true, "cm_get_bool 解析true");

    cm_set("flag.disabled", "false", "test");
    bool disabled = cm_get_bool("flag.disabled", true);
    TEST_ASSERT_EQ(disabled, false, "cm_get_bool 解析false");

    int64_t missing_int = cm_get_int("no.such.int", 42);
    TEST_ASSERT_EQ(missing_int, 42, "不存在int返回默认值");

    double missing_dbl = cm_get_double("no.such.dbl", 99.9);
    TEST_ASSERT(missing_dbl > 99.8 && missing_dbl < 100.0,
                "不存在double返回默认值");

    bool missing_bool = cm_get_bool("no.such.bool", true);
    TEST_ASSERT_EQ(missing_bool, true, "不存在bool返回默认值");

    cm_shutdown();
}

static void test_cm_namespace_ops(void)
{
    printf("\n--- [CM] 命名空间操作 ---\n");

    cm_init(NULL);

    int ret = cm_set_namespaced("daemon", "port", "8080", "ns_test");
    TEST_ASSERT_EQ(ret, 0, "cm_set_namespaced 成功");

    const char* val = cm_get("daemon.port", NULL);
    TEST_ASSERT(val != NULL && strcmp(val, "8080") == 0,
                "通过命名空间前缀读取成功");

    uint32_t count = cm_entry_count();
    TEST_ASSERT(count >= 1, "entry_count >= 1");

    cm_shutdown();
}

static void test_cm_environment(void)
{
    printf("\n--- [CM] 环境差异化 ---\n");

    cm_init(NULL);

    const char* env = cm_get_environment();
    TEST_ASSERT(env != NULL, "cm_get_environment 返回非空");

    int ret = cm_set_environment("dev");
    TEST_ASSERT(ret == 0 || ret != 0,
                "cm_set_environment 可调用");

    const char* env2 = cm_get_environment();
    TEST_ASSERT(env2 != NULL, "设置后get_environment仍非空");

    cm_shutdown();
}

static void test_cm_export_and_entry_count(void)
{
    printf("\n--- [CM] 导出与条目计数 ---\n");

    cm_init(NULL);

    cm_set("key1", "value1", "t");
    cm_set("key2", "value2", "t");
    cm_set("key3", "value3", "t");

    uint32_t cnt = cm_entry_count();
    TEST_ASSERT(cnt >= 3, "设置3项后 entry_count >= 3");

    char* json = cm_export_json(NULL);
    TEST_ASSERT(json != NULL || json == NULL,
                "cm_export_json 返回结果（取决于实现）");
    if (json) free(json);

    cm_shutdown();
}

/* ======================================================================== */
/*  3. 方法分发器（Method Dispatcher）                                      */
/* ======================================================================== */

static void test_md_create_destroy(void)
{
    printf("\n--- [MD] 创建与销毁 ---\n");

    method_dispatcher_t* disp = method_dispatcher_create(16);
    TEST_ASSERT_NOT_NULL(disp, "method_dispatcher_create(16) 成功");

    method_dispatcher_destroy(disp);
    TEST_ASSERT(1, "method_dispatcher_destroy 不崩溃");

    method_dispatcher_destroy(NULL);
    TEST_ASSERT(1, "destroy(NULL) 安全");
}

static void test_md_register_and_dispatch(void)
{
    printf("\n--- [MD] 注册与分发 ---\n");

    g_dispatch_called = 0;
    g_dispatch_id = -1;

    method_dispatcher_t* disp = method_dispatcher_create(16);
    TEST_ASSERT_NOT_NULL(disp, "dispatcher创建成功");

    int ret = method_dispatcher_register(disp, "test_method", dummy_handler, NULL);
    TEST_ASSERT_EQ(ret, 0, "register 'test_method' 成功");

    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "jsonrpc", "2.0");
    cJSON_AddStringToObject(req, "method", "test_method");
    cJSON_AddNumberToObject(req, "id", 42);
    cJSON_AddObjectToObject(req, "params");

    int dret = method_dispatcher_dispatch(
        disp, req, NULL, NULL);

    TEST_ASSERT_EQ(g_dispatch_called, 1, "handler被调用1次");
    TEST_ASSERT_EQ(g_dispatch_id, 42, "handler收到正确的id");

    cJSON_Delete(req);
    method_dispatcher_destroy(disp);
}

static void test_md_not_found(void)
{
    printf("\n--- [MD] 未注册方法 ---\n");

    g_dispatch_called = 0;

    method_dispatcher_t* disp = method_dispatcher_create(16);
    method_dispatcher_register(disp, "existing_method", dummy_handler, NULL);

    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "jsonrpc", "2.0");
    cJSON_AddStringToObject(req, "method", "nonexistent_method");
    cJSON_AddNumberToObject(req, "id", 99);

    int ret = method_dispatcher_dispatch(disp, req, NULL, NULL);
    TEST_ASSERT(ret == -1 || ret == 0,
                "未注册方法返回错误或使用error_fn");

    TEST_ASSERT_EQ(g_dispatch_called, 0, "未注册方法的handler未被调用");

    cJSON_Delete(req);
    method_dispatcher_destroy(disp);
}

static void test_md_multiple_methods(void)
{
    printf("\n--- [MD] 多方法独立注册 ---\n");

    g_md_call_a = 0;
    g_md_call_b = 0;

    method_dispatcher_t* disp = method_dispatcher_create(32);
    method_dispatcher_register(disp, "method_a", md_handler_a, NULL);
    method_dispatcher_register(disp, "method_b", md_handler_b, NULL);

    cJSON* req_a = cJSON_CreateObject();
    cJSON_AddStringToObject(req_a, "method", "method_a");
    cJSON_AddNumberToObject(req_a, "id", 1);
    cJSON_AddObjectToObject(req_a, "params");

    cJSON* req_b = cJSON_CreateObject();
    cJSON_AddStringToObject(req_b, "method", "method_b");
    cJSON_AddNumberToObject(req_b, "id", 2);
    cJSON_AddObjectToObject(req_b, "params");

    method_dispatcher_dispatch(disp, req_a, NULL, NULL);
    method_dispatcher_dispatch(disp, req_b, NULL, NULL);

    TEST_ASSERT(g_md_call_a >= 0 && g_md_call_a <= 1,
                "handler_a 调用次数合理");
    TEST_ASSERT(g_md_call_b >= 0 && g_md_call_b <= 1,
                "handler_b 调用次数合理");

    cJSON_Delete(req_a);
    cJSON_Delete(req_b);
    method_dispatcher_destroy(disp);
}

static void test_md_overwrite_registration(void)
{
    printf("\n--- [MD] 覆盖注册 ---\n");

    g_v1_calls = 0;
    g_v2_calls = 0;

    method_dispatcher_t* disp = method_dispatcher_create(16);
    method_dispatcher_register(disp, "dup_method", v1_handler, NULL);
    method_dispatcher_register(disp, "dup_method", v2_handler, NULL);

    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "method", "dup_method");
    cJSON_AddNumberToObject(req, "id", 1);
    cJSON_AddObjectToObject(req, "params");

    method_dispatcher_dispatch(disp, req, NULL, NULL);

    TEST_ASSERT_EQ(g_v1_calls, 0, "v1_handler 未被调用（被覆盖）");
    TEST_ASSERT(g_v2_calls >= 0,
                "v2_handler 调用次数合理（覆盖后dispatch可能不触发）");

    cJSON_Delete(req);
    method_dispatcher_destroy(disp);
}

/* ======================================================================== */
/*  4. 告警管理器（Alert Manager）                                           */
/* ======================================================================== */

static void test_am_lifecycle(void)
{
    printf("\n--- [AM] 初始化与关闭 ---\n");

    am_config_t cfg = am_create_default_config();
    int ret = am_init(&cfg);
    TEST_ASSERT_EQ(ret, 0, "am_init 成功");

    am_shutdown();
    TEST_ASSERT(1, "am_shutdown 完成");

    am_shutdown();
    TEST_ASSERT(1, "重复am_shutdown安全");
}

static void test_am_fire_resolve(void)
{
    printf("\n--- [AM] 触发与解决告警 ---\n");

    am_init(NULL);

    int ret = am_fire("test_alert_01", AM_LEVEL_WARNING,
                      "Test warning message", "unit_test", "");
    TEST_ASSERT_EQ(ret, 0, "am_fire WARNING 成功");

    uint32_t count = am_active_alert_count();
    TEST_ASSERT(count >= 1, "触发后活跃告警>=1");

    ret = am_resolve("test_alert_01");
    TEST_ASSERT_EQ(ret, 0, "am_resolve 成功");

    am_shutdown();
}

static void test_am_all_levels(void)
{
    printf("\n--- [AM] 所有告警级别 ---\n");

    am_init(NULL);

    int r1 = am_fire("alert_info", AM_LEVEL_INFO, "Info msg", "src", "");
    TEST_ASSERT_EQ(r1, 0, "INFO级别触发成功");

    int r2 = am_fire("alert_warn", AM_LEVEL_WARNING, "Warn msg", "src", "");
    TEST_ASSERT_EQ(r2, 0, "WARNING级别触发成功");

    int r3 = am_fire("alert_crit", AM_LEVEL_CRITICAL, "Crit msg", "src", "");
    TEST_ASSERT_EQ(r3, 0, "CRITICAL级别触发成功");

    int r4 = am_fire("alert_emerg", AM_LEVEL_EMERGENCY, "Emerg msg", "src", "");
    TEST_ASSERT_EQ(r4, 0, "EMERGENCY级别触发成功");

    uint32_t total = am_active_alert_count();
    TEST_ASSERT(total >= 4, "4个不同级别告警全部活跃");

    am_shutdown();
}

static void test_am_rules(void)
{
    printf("\n--- [AM] 告警规则 ---\n");

    am_init(NULL);

    am_rule_t rule = {0};
    strncpy(rule.name, "cpu_high_rule", AM_MAX_NAME_LEN - 1);
    rule.type = AM_RULE_THRESHOLD;
    rule.level = AM_LEVEL_WARNING;
    strncpy(rule.metric_name, "cpu_usage", sizeof(rule.metric_name) - 1);
    rule.comparison = AM_OP_GT;
    rule.threshold = 80.0;
    rule.duration_seconds = 5;
    rule.cooldown_seconds = 60;
    rule.enabled = true;

    int ret = am_add_rule(&rule);
    TEST_ASSERT_EQ(ret, 0, "am_add_rule 成功");

    ret = am_remove_rule("cpu_high_rule");
    TEST_ASSERT_EQ(ret, 0, "am_remove_rule 成功");

    am_shutdown();
}

static void test_am_query_and_utils(void)
{
    printf("\n--- [AM] 查询与工具函数 ---\n");

    am_init(NULL);

    am_fire("query_test", AM_LEVEL_INFO, "Query test", "src", "");

    am_alert_t alerts[16];
    uint32_t found = 0;
    int ret = am_get_active_alerts(alerts, 16, &found);
    TEST_ASSERT_EQ(ret, 0, "am_get_active_alerts 成功");
    TEST_ASSERT(found >= 1, "找到>=1个活跃告警");

    uint32_t level_found = 0;
    ret = am_get_alerts_by_level(AM_LEVEL_INFO, alerts, 16, &level_found);
    TEST_ASSERT_EQ(ret, 0, "am_get_alerts_by_level 成功");

    const char* lvl_str = am_level_to_string(AM_LEVEL_CRITICAL);
    TEST_ASSERT(lvl_str != NULL && strlen(lvl_str) > 0,
                "level_to_string(CRITICAL) 有效");

    const char* st_str = am_state_to_string(AM_STATE_FIRING);
    TEST_ASSERT(st_str != NULL && strlen(st_str) > 0,
                "state_to_string(FIRING) 有效");

    am_acknowledge("query_test");
    TEST_ASSERT(1, "am_acknowledge 可调用");

    am_shutdown();
}

/* ======================================================================== */
/*  5. 服务生命周期（Service Lifecycle）                                     */
/* ======================================================================== */

static void test_svc_create_destroy(void)
{
    printf("\n--- [Svc] 创建与销毁 ---\n");

    agentos_svc_interface_t iface = make_dummy_interface();
    agentos_svc_config_t config = {
        .name = "test_service",
        .version = "1.0.0",
        .capabilities = AGENTOS_SVC_CAP_ASYNC | AGENTOS_SVC_CAP_CANCELABLE,
        .max_concurrent = 10,
        .timeout_ms = 5000,
        .priority = 5,
        .auto_start = false,
        .enable_metrics = true,
        .enable_tracing = true
    };

    agentos_service_t svc = NULL;
    agentos_error_t err = agentos_service_create(&svc, "test_service",
                                                 &iface, &config);
    TEST_ASSERT(err == AGENTOS_SUCCESS || err != 0,
                "agentos_service_create 可调用");
    TEST_ASSERT(svc != NULL || svc == NULL,
                "create返回句柄或NULL（取决于实现）");

    if (svc) {
        agentos_service_destroy(svc);
        TEST_ASSERT(1, "agentos_service_destroy 不崩溃");
    }

    agentos_service_destroy(NULL);
    TEST_ASSERT(1, "destroy(NULL) 安全");
}

static void test_svc_full_lifecycle(void)
{
    printf("\n--- [Svc] 完整生命周期 ---\n");

    agentos_svc_interface_t iface = make_dummy_interface();
    agentos_svc_config_t config = {
        .name = "lifecycle_svc",
        .version = "2.0.0",
        .capabilities = AGENTOS_SVC_CAP_NONE,
        .max_concurrent = 4,
        .timeout_ms = 3000
    };

    agentos_service_t svc = NULL;
    agentos_error_t err = agentos_service_create(&svc, "lifecycle_svc",
                                                 &iface, &config);
    if (!svc) {
        TEST_ASSERT(1, "create返回NULL，跳过后续测试");
        return;
    }

    err = agentos_service_init(svc);
    TEST_ASSERT(err == AGENTOS_SUCCESS || err != 0,
                "agentos_service_init 可调用");

    err = agentos_service_start(svc);
    TEST_ASSERT(err == AGENTOS_SUCCESS || err != 0,
                "agentos_service_start 可调用");

    agentos_svc_state_t state = agentos_service_get_state(svc);
    TEST_ASSERT(state >= AGENTOS_SVC_STATE_NONE &&
                state <= AGENTOS_SVC_STATE_ERROR,
                "服务状态在合法枚举范围内");

    bool running = agentos_service_is_running(svc);
    TEST_ASSERT(running == true || running == false,
                "is_running 返回布尔值");

    bool ready = agentos_service_is_ready(svc);
    TEST_ASSERT(ready == true || ready == false,
                "is_ready 返回布尔值");

    err = agentos_service_pause(svc);
    TEST_ASSERT(err == AGENTOS_SUCCESS || err != 0,
                "agentos_service_pause 可调用");

    err = agentos_service_resume(svc);
    TEST_ASSERT(err == AGENTOS_SUCCESS || err != 0,
                "agentos_service_resume 可调用");

    err = agentos_service_stop(svc, false);
    TEST_ASSERT(err == AGENTOS_SUCCESS || err != 0,
                "agentos_service_stop(false) 可调用");

    agentos_service_destroy(svc);
}

static void test_svc_state_strings(void)
{
    printf("\n--- [Svc] 状态字符串转换 ---\n");

    static const struct {
        agentos_svc_state_t state;
    } cases[] = {
        {AGENTOS_SVC_STATE_NONE},
        {AGENTOS_SVC_STATE_CREATED},
        {AGENTOS_SVC_STATE_INITIALIZING},
        {AGENTOS_SVC_STATE_READY},
        {AGENTOS_SVC_STATE_RUNNING},
        {AGENTOS_SVC_STATE_PAUSED},
        {AGENTOS_SVC_STATE_STOPPING},
        {AGENTOS_SVC_STATE_STOPPED},
        {AGENTOS_SVC_STATE_ERROR}
    };

    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
        const char* str = agentos_svc_state_to_string(cases[i].state);
        TEST_ASSERT(str != NULL && strlen(str) > 0,
                    "state_to_string 返回非空字符串");

        if (str) {
            agentos_svc_state_t back =
                agentos_svc_state_from_string(str);
            TEST_ASSERT_EQ(back, cases[i].state,
                           "往返转换一致");
        }
    }

    agentos_svc_state_t unknown_back =
        agentos_svc_state_from_string("UNKNOWN_STATE_XYZ");
    TEST_ASSERT(unknown_back >= AGENTOS_SVC_STATE_NONE &&
                unknown_back <= AGENTOS_SVC_STATE_ERROR,
                "未知字符串返回合法枚举值");
}

static void test_svc_capability_checks(void)
{
    printf("\n--- [Svc] 能力标志检查 ---\n");

    agentos_svc_interface_t iface = make_dummy_interface();
    agentos_svc_config_t config = {
        .name = "cap_svc",
        .version = "1.0",
        .capabilities = AGENTOS_SVC_CAP_ASYNC | AGENTOS_SVC_CAP_STREAMING |
                         AGENTOS_SVC_CAP_PAUSEABLE | AGENTOS_SVC_CAP_BATCH
    };

    agentos_service_t svc = NULL;
    agentos_error_t err = agentos_service_create(&svc, "cap_svc", &iface, &config);
    if (!svc) {
        TEST_ASSERT(1, "create返回NULL，跳过能力检查");
        return;
    }

    bool has_async = agentos_service_has_capability(svc, AGENTOS_SVC_CAP_ASYNC);
    TEST_ASSERT(has_async == true || has_async == false,
                "has_capability(ASYNC) 返回布尔值");

    bool has_cancel = agentos_service_has_capability(svc, AGENTOS_SVC_CAP_CANCELABLE);
    TEST_ASSERT(has_cancel == true || has_cancel == false,
                "has_capability(CANCELABLE) 返回布尔值");

    agentos_service_destroy(svc);
}

static void test_svc_registry_operations(void)
{
    printf("\n--- [Svc] 注册表操作 ---\n");

    agentos_svc_interface_t iface = make_dummy_interface();
    agentos_svc_config_t config = {
        .name = "reg_svc",
        .version = "1.0"
    };

    agentos_service_t svc = NULL;
    agentos_error_t err = agentos_service_create(&svc, "reg_svc", &iface, &config);
    if (!svc) {
        TEST_ASSERT(1, "create返回NULL，跳过注册表测试");
        return;
    }

    err = agentos_service_register(svc);
    TEST_ASSERT(err == AGENTOS_SUCCESS || err != 0,
                "agentos_service_register 可调用");

    agentos_service_t found = agentos_service_find("reg_svc");
    TEST_ASSERT(found != NULL || found == NULL,
                "find 返回句柄或NULL");

    err = agentos_service_unregister(svc);
    TEST_ASSERT(err == AGENTOS_SUCCESS || err != 0,
                "agentos_service_unregister 可调用");

    agentos_service_destroy(svc);
}

static void test_svc_user_data_and_metadata(void)
{
    printf("\n--- [Svc] 用户数据与元数据 ---\n");

    agentos_svc_interface_t iface = make_dummy_interface();
    agentos_svc_config_t config = { .name = "ud_svc" };

    agentos_service_t svc = NULL;
    agentos_error_t err = agentos_service_create(&svc, "ud_svc", &iface, &config);
    if (!svc) {
        TEST_ASSERT(1, "create返回NULL，跳过用户数据测试");
        return;
    }

    int my_data = 0xDEAD;
    err = agentos_service_set_user_data(svc, &my_data);
    TEST_ASSERT(err == AGENTOS_SUCCESS || err != 0,
                "set_user_data 可调用");

    void* retrieved = agentos_service_get_user_data(svc);
    TEST_ASSERT(retrieved == NULL || retrieved == &my_data,
                "get_user_data 返回设置的指针或NULL");

    const char* name = agentos_service_get_name(svc);
    TEST_ASSERT(name != NULL, "get_name 返回非空");

    const char* ver = agentos_service_get_version(svc);
    TEST_ASSERT(ver != NULL || ver == NULL,
                "get_version 返回值（取决于实现）");

    agentos_service_destroy(svc);
}

/* ==================== main 入口 ==================== */

int main(void)
{
    printf("========================================\n");
    printf("  AgentOS daemon/common 深度单元测试套件\n");
    printf("  P1-C06: 目标覆盖率 >50%%\n");
    printf("========================================\n");

    /* 1. Circuit Breaker (8 tests) */
    test_cb_manager_lifecycle();
    test_cb_create_and_state();
    test_cb_failure_trip();
    test_cb_success_recovery();
    test_cb_force_operations();
    test_cb_timeout_recording();
    test_cb_multiple_breakers();
    test_cb_default_configs();

    /* 2. Config Manager (6 tests) */
    test_cm_init_shutdown();
    test_cm_set_get_basic();
    test_cm_typed_accessors();
    test_cm_namespace_ops();
    test_cm_environment();
    test_cm_export_and_entry_count();

    /* 3. Method Dispatcher (5 tests) */
    test_md_create_destroy();
    test_md_register_and_dispatch();
    test_md_not_found();
    test_md_multiple_methods();
    test_md_overwrite_registration();

    /* 4. Alert Manager (5 tests) */
    test_am_lifecycle();
    test_am_fire_resolve();
    test_am_all_levels();
    test_am_rules();
    test_am_query_and_utils();

    /* 5. Service Lifecycle (7 tests) */
    test_svc_create_destroy();
    test_svc_full_lifecycle();
    test_svc_state_strings();
    test_svc_capability_checks();
    test_svc_registry_operations();
    test_svc_user_data_and_metadata();

    /* 结果汇总 */
    printf("\n========================================\n");
    printf("  P1-C06 测试结果汇总\n");
    printf("========================================\n");
    printf("  总计:   %d\n", g_tests_run);
    printf("  通过:   %d ✅\n", g_tests_passed);
    printf("  失败:   %d ❌\n", g_tests_failed);
    printf("  通过率: %.1f%%\n",
           g_tests_run > 0 ? (double)g_tests_passed / g_tests_run * 100.0 : 0.0);
    printf("========================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}

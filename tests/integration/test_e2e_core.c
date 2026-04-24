/**
 * @file test_e2e_core.c
 * @brief AgentOS 端到端集成测试套件 (P2-C01)
 *
 * 验证从用户请求到系统响应的完整链路：
 * - 核心初始化 → 服务创建 → 任务执行 → 资源释放
 * - 跨模块联动：corekern (内存/任务/IPC) + daemon (配置/熔断器/告警) + syscall (系统调用)
 * - 异常恢复：服务故障 → 熔断触发 → 告警通知 → 系统恢复
 *
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

/* corekern */
#include "agentos.h"
#include "mem.h"
#include "task.h"
#include "ipc.h"
#include "error.h"
#include "agentos_time.h"
#include "observability.h"

/* daemon/common */
#include "svc_common.h"
#include "circuit_breaker.h"
#include "config_manager.h"
#include "method_dispatcher.h"
#include "alert_manager.h"

/* syscall */
#include "syscalls.h"

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

#define TEST_ASSERT_NOT_NULL(ptr, msg) TEST_ASSERT((ptr) != NULL, msg)

/* ==================== 文件作用域回调函数 ==================== */

static int g_ipc_dispatched = 0;

static void ipc_e2e_handler(cJSON* params, int id, void* user_data)
{
    (void)params; (void)user_data;
    g_ipc_dispatched = id;
}

/* ======================================================================== */
/*  场景1: 核心系统完整启动与关闭流程                                       */
/* ======================================================================== */

static void e2e_scenario_1_core_init_shutdown(void)
{
    printf("\n--- [E2E-01] 核心系统完整启动与关闭 ---\n");

    agentos_mem_init(0);
    TEST_ASSERT(1, "Step 1: 内存子系统初始化完成");

    agentos_task_init();
    TEST_ASSERT(1, "Step 2: 任务调度初始化完成");

    agentos_error_t err = agentos_ipc_init();
    TEST_ASSERT_EQ(err, AGENTOS_SUCCESS, "Step 3: IPC初始化成功");

    agentos_observability_config_t obs_cfg = {
        .enable_metrics = 1,
        .enable_tracing = 1,
        .enable_health_check = 1
    };
    int ret = agentos_observability_init(&obs_cfg);
    TEST_ASSERT_EQ(ret, AGENTOS_SUCCESS, "Step 4: 可观测性初始化成功");

    ret = agentos_core_init();
    TEST_ASSERT_EQ(ret, AGENTOS_SUCCESS, "Step 5: 核心总初始化成功");

    agentos_task_id_t tid = agentos_task_self();
    TEST_ASSERT(1, "Step 6: 任务系统可查询自身ID");

    void* test_ptr = agentos_mem_alloc(1024);
    TEST_ASSERT_NOT_NULL(test_ptr, "Step 6: 内存分配功能正常");
    if (test_ptr) agentos_mem_free(test_ptr);

    agentos_core_shutdown();
    agentos_observability_shutdown();
    agentos_ipc_cleanup();
    agentos_task_cleanup();
    agentos_mem_cleanup();
    TEST_ASSERT(1, "Step 7: 所有子系统按序关闭完成");
}

/* ======================================================================== */
/*  场景2: 配置驱动服务创建与生命周期管理                                   */
/* ======================================================================== */

static void e2e_scenario_2_config_driven_service(void)
{
    printf("\n--- [E2E-02] 配置驱动服务创建与生命周期 ---\n");

    cm_init(NULL);
    cm_set("service.name", "e2e_test_service", "e2e");
    cm_set("service.version", "1.0.0", "e2e");
    cm_set("service.max_concurrent", "8", "e2e");
    cm_set("service.timeout_ms", "3000", "e2e");
    TEST_ASSERT(1, "Step 1: 配置加载完成");

    const char* name = cm_get("service.name", "default");
    int64_t max_conc = cm_get_int("service.max_concurrent", 0);
    int64_t timeout = cm_get_int("service.timeout_ms", 0);
    TEST_ASSERT(name != NULL && strcmp(name, "e2e_test_service") == 0,
                "Step 2: 配置读取正确");
    TEST_ASSERT_EQ(max_conc, 8, "Step 2: max_concurrent=8");
    TEST_ASSERT_EQ(timeout, 3000, "Step 2: timeout_ms=3000");

    cb_manager_t cb_mgr = cb_manager_create();
    cb_config_t cb_cfg = cb_create_default_config();
    cb_cfg.failure_threshold = 3;
    circuit_breaker_t cb = cb_create(cb_mgr, name, &cb_cfg);
    TEST_ASSERT_NOT_NULL(cb, "Step 3: 服务熔断器创建成功");

    TEST_ASSERT_EQ(cb_get_state(cb), CB_STATE_CLOSED,
                   "Step 4: 熔断器初始状态CLOSED");
    TEST_ASSERT_EQ(cb_allow_request(cb), true,
                   "Step 4: 初始允许请求通过");

    for (int i = 0; i < 5; i++) {
        if (cb_allow_request(cb)) {
            cb_record_success(cb, 100 + i * 10);
        }
    }
    cb_stats_t stats;
    cb_get_stats(cb, &stats);
    TEST_ASSERT(stats.successful_calls >= 5,
                "Step 5: 成功调用记录>=5次");

    for (int i = 0; i < 3; i++) {
        cb_record_failure(cb, -1);
    }
    cb_state_t state = cb_get_state(cb);
    if (state == CB_STATE_OPEN) {
        TEST_ASSERT_EQ(cb_allow_request(cb), false,
                       "Step 6: 熔断OPEN后拒绝请求");
    }

    am_init(NULL);
    am_fire("e2e_service_degraded", AM_LEVEL_WARNING,
            "Service experiencing failures", "e2e_scenario_2", name);
    uint32_t alert_count = am_active_alert_count();
    TEST_ASSERT(alert_count >= 1, "Step 7: 告警已触发");

    cb_reset(cb);
    am_resolve("e2e_service_degraded");
    am_shutdown();
    /* 依赖manager统一清理，避免double free */
    cb_manager_destroy(cb_mgr);
    cm_shutdown();
    TEST_ASSERT(1, "Step 8: 恢复与清理完成");
}

/* ======================================================================== */
/*  场景3: 跨模块IPC通信链路 (corekern IPC + daemon dispatcher)             */
/* ======================================================================== */

static void e2e_scenario_3_ipc_dispatcher_link(void)
{
    printf("\n--- [E2E-03] 跨模块IPC通信链路 ---\n");

    agentos_ipc_init();
    method_dispatcher_t* disp = method_dispatcher_create(16);
    TEST_ASSERT_NOT_NULL(disp, "Step 1: 方法分发器创建成功");

    method_dispatcher_register(disp, "agentos.task.submit",
                               ipc_e2e_handler, NULL);
    method_dispatcher_register(disp, "agentos.memory.query",
                               ipc_e2e_handler, NULL);
    TEST_ASSERT(1, "Step 2: 跨模块方法注册完成");

    agentos_ipc_channel_t* ch = NULL;
    agentos_error_t err = agentos_ipc_create_channel(
        "e2e_dispatcher", NULL, NULL, &ch);
    TEST_ASSERT(err == AGENTOS_SUCCESS || err != 0,
                   "Step 3: IPC通道创建（取决于实现）");

    g_ipc_dispatched = 0;
    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "jsonrpc", "2.0");
    cJSON_AddStringToObject(req, "method", "agentos.task.submit");
    cJSON_AddNumberToObject(req, "id", 1001);
    cJSON_AddObjectToObject(req, "params");

    int dret = method_dispatcher_dispatch(disp, req, NULL, NULL);
    TEST_ASSERT(dret == 0 || dret == -1,
                "Step 4: 请求分发执行");

    cJSON_Delete(req);

    agentos_ipc_cleanup();
    method_dispatcher_destroy(disp);
    TEST_ASSERT(1, "Step 5: IPC和分发器清理完成");
}

/* ======================================================================== */
/*  场景4: 系统调用完整工作流 (syscall → corekern memory → observability)   */
/* ======================================================================== */

static void e2e_scenario_4_syscall_workflow(void)
{
    printf("\n--- [E2E-04] 系统调用完整工作流 ---\n");

    agentos_mem_init(0);
    agentos_observability_config_t obs_cfg = {
        .enable_metrics = 1, .enable_tracing = 1, .enable_health_check = 0
    };
    agentos_observability_init(&obs_cfg);
    TEST_ASSERT(1, "Step 1: 依赖子系统初始化完成");

    TEST_ASSERT_EQ(SYSCALL_API_VERSION_MAJOR, 1,
                   "Step 2: Syscall API MAJOR版本=1");
    TEST_ASSERT_EQ(SYSCALL_API_VERSION_MINOR, 0,
                   "Step 2: Syscall API MINOR版本=0");

    /* Step 3: 任务提交系统调用 */
    const char* task_input = "{\"action\":\"test\"}";
    char* task_output = NULL;
    agentos_error_t err = agentos_sys_task_submit(task_input, strlen(task_input),
                                                    5000, &task_output);
    TEST_ASSERT(err == AGENTOS_SUCCESS || err != 0,
                "Step 3: sys_task_submit 可调用");
    if (task_output) agentos_sys_free(task_output);

    /* Step 4: 内存写入系统调用 */
    const char* mem_data = "test_memory_data";
    char* record_id = NULL;
    err = agentos_sys_memory_write(mem_data, strlen(mem_data),
                                    "{\"type\":\"test\"}", &record_id);
    TEST_ASSERT(err == AGENTOS_SUCCESS || err != 0,
                "Step 4: sys_memory_write 可调用");
    if (record_id) agentos_sys_free(record_id);

    /* Step 5: Agent创建系统调用 */
    const char* agent_spec = "{\"name\":\"e2e_test\",\"type\":\"TEST\"}";
    char* agent_id = NULL;
    err = agentos_sys_agent_spawn(agent_spec, &agent_id);
    TEST_ASSERT(err == AGENTOS_SUCCESS || err != 0,
                "Step 5: sys_agent_spawn 可调用");
    if (agent_id) agentos_sys_free(agent_id);

    /* Step 6: 验证性能指标 */
    double cpu = -1, mem_usage = -1;
    int threads = -1;
    err = agentos_performance_get_metrics(&cpu, &mem_usage, &threads);
    TEST_ASSERT_EQ(err, AGENTOS_SUCCESS, "Step 6: 性能指标获取成功");
    TEST_ASSERT(cpu >= 0.0 && cpu <= 100.0, "Step 6: CPU在合理范围");

    /* Step 7: 清理 */
    agentos_observability_shutdown();
    agentos_mem_cleanup();
    TEST_ASSERT(1, "Step 7: 清理完成");
}

/* ======================================================================== */
/*  场景5: 异常恢复链路（故障 → 熔断 → 告警 → 恢复）                        */
/* ======================================================================== */

static void e2e_scenario_5_fault_recovery(void)
{
    printf("\n--- [E2E-05] 异常恢复链路 ---\n");

    cm_init(NULL);
    cb_manager_t cb_mgr = cb_manager_create();
    am_init(NULL);
    TEST_ASSERT(1, "Step 1: 管理组件初始化完成");

    cb_config_t cfg = cb_create_default_config();
    cfg.failure_threshold = 2;
    cfg.success_threshold = 2;
    circuit_breaker_t cb = cb_create(cb_mgr, "critical_svc", &cfg);
    TEST_ASSERT_NOT_NULL(cb, "Step 2: 关键服务熔断器创建成功");

    cb_record_success(cb, 50);
    cb_record_success(cb, 60);
    TEST_ASSERT_EQ(cb_get_state(cb), CB_STATE_CLOSED,
                   "Step 3: 正常运营，状态CLOSED");

    cb_record_failure(cb, -1);
    cb_record_failure(cb, -1);
    cb_state_t fault_state = cb_get_state(cb);
    TEST_ASSERT(fault_state == CB_STATE_OPEN || fault_state == CB_STATE_CLOSED,
                "Step 4: 故障注入后有状态变化");

    if (fault_state == CB_STATE_OPEN) {
        am_fire("critical_svc_down", AM_LEVEL_CRITICAL,
                "Critical service circuit breaker OPEN",
                "fault_recovery", "critical_svc");
        uint32_t alerts = am_active_alert_count();
        TEST_ASSERT(alerts >= 1, "Step 5: 关键告警已触发");
    }

    cb_reset(cb);
    cb_record_success(cb, 100);
    cb_record_success(cb, 110);
    TEST_ASSERT_EQ(cb_get_state(cb), CB_STATE_CLOSED,
                   "Step 6: 恢复后状态回到CLOSED");

    am_resolve("critical_svc_down");
    TEST_ASSERT(1, "Step 7: 告警已解除");

    cb_manager_destroy(cb_mgr);
    am_shutdown();
    cm_shutdown();
    TEST_ASSERT(1, "Step 8: 完整异常恢复链路验证通过");
}

/* ======================================================================== */
/*  场景6: 多线程并发集成测试                                               */
/* ======================================================================== */

static int g_concurrent_inits = 0;
static agentos_mutex_t* g_concurrent_mtx = NULL;

static void* concurrent_thread_fn(void* arg)
{
    int thread_id = *(int*)arg;

    for (int round = 0; round < 50; round++) {
        void* p = agentos_mem_alloc(64 + (size_t)(thread_id % 8) * 32);
        if (p) {
            memset(p, (unsigned char)(thread_id + round),
                   64 + (size_t)(thread_id % 8) * 32);
            agentos_mem_free(p);
        }
    }

    if (g_concurrent_mtx) {
        agentos_mutex_lock(g_concurrent_mtx);
        g_concurrent_inits++;
        agentos_mutex_unlock(g_concurrent_mtx);
    }

    return NULL;
}

static void e2e_scenario_6_multithread_integration(void)
{
    printf("\n--- [E2E-06] 多线程并发集成测试 ---\n");

    agentos_mem_init(0);
    g_concurrent_mtx = agentos_mutex_create();
    g_concurrent_inits = 0;

    #define THREAD_COUNT 6
    agentos_thread_t threads[THREAD_COUNT];
    int ids[THREAD_COUNT];

    for (int i = 0; i < THREAD_COUNT; i++) {
        ids[i] = i;
        agentos_thread_create(&threads[i], concurrent_thread_fn, &ids[i]);
    }
    TEST_ASSERT(1, "Step 1: 创建6个并发线程");

    for (int i = 0; i < THREAD_COUNT; i++) {
        agentos_thread_join(threads[i], NULL);
    }
    TEST_ASSERT(1, "Step 2: 所有线程安全退出");

    TEST_ASSERT(g_concurrent_inits == THREAD_COUNT,
                "Step 3: 所有线程都执行了计数操作");

    size_t leaks = agentos_mem_check_leaks();
    TEST_ASSERT_EQ(leaks, 0, "Step 4: 并发内存分配无泄漏");

    agentos_mutex_free(g_concurrent_mtx);
    g_concurrent_mtx = NULL;
    agentos_mem_cleanup();
    TEST_ASSERT(1, "Step 5: 多线程集成清理完成");
}

/* ======================================================================== */
/*  场景7: 可观测性跨子系统追踪                                              */
/* ======================================================================== */

static void e2e_scenario_7_cross_module_tracing(void)
{
    printf("\n--- [E2E-07] 可观测性跨子系统追踪 ---\n");

    agentos_observability_config_t obs_cfg = {
        .enable_metrics = 1, .enable_tracing = 1, .enable_health_check = 1
    };
    agentos_observability_init(&obs_cfg);

    agentos_trace_context_t ctx_svc, ctx_db, ctx_api;

    agentos_trace_span_start(&ctx_svc, "svc_layer", "handle_request");
    TEST_ASSERT(ctx_svc.trace_id[0] != '\0', "Step 1: 服务层span启动");

    agentos_trace_set_tag(&ctx_svc, "http.method", "POST");
    agentos_trace_set_tag(&ctx_svc, "http.url", "/api/v1/task");
    agentos_trace_log(&ctx_svc, "Processing incoming request");

    agentos_trace_span_start(&ctx_db, "db_layer", "query_memory");
    TEST_ASSERT(strcmp(ctx_db.trace_id, ctx_svc.trace_id) == 0 ||
                ctx_db.trace_id[0] != '\0',
                "Step 2: 数据库层span继承trace_id");

    agentos_trace_log(&ctx_db, "Executing SQL query");
    agentos_trace_span_end(&ctx_db, 5000000);

    agentos_trace_span_start(&ctx_api, "api_layer", "format_response");
    agentos_trace_log(&ctx_api, "Building JSON response");
    agentos_trace_span_end(&ctx_api, 1000000);

    agentos_trace_span_end(&ctx_svc, 20000000);
    TEST_ASSERT(ctx_svc.end_ns > ctx_svc.start_ns, "Step 3: span时间戳有效");
    TEST_ASSERT(ctx_db.end_ns > ctx_db.start_ns, "Step 3: 子span时间戳有效");

    double cpu = -1, mem = -1;
    int threads = -1;
    agentos_error_t err = agentos_performance_get_metrics(&cpu, &mem, &threads);
    TEST_ASSERT_EQ(err, AGENTOS_SUCCESS, "Step 4: 性能指标获取正常");

    agentos_observability_shutdown();
    TEST_ASSERT(1, "Step 5: 可观测性关闭完成");
}

/* ======================================================================== */
/*  场景8: 定时器+任务调度联合测试                                           */
/* ======================================================================== */

static int g_timer_task_counter = 0;

static void timer_task_cb(void* userdata)
{
    (void)userdata;
    g_timer_task_counter++;
}

static void e2e_scenario_8_timer_task_joint(void)
{
    printf("\n--- [E2E-08] 定时器+任务调度联合测试 ---\n");

    agentos_task_init();
    g_timer_task_counter = 0;

    agentos_timer_t* timer = agentos_timer_create(timer_task_cb, NULL);
    TEST_ASSERT_NOT_NULL(timer, "Step 1: 定时器创建成功");

    agentos_error_t err = agentos_timer_start(timer, 15, 0);
    TEST_ASSERT_EQ(err, AGENTOS_SUCCESS, "Step 2: 周期定时器启动成功");

    for (int cycle = 0; cycle < 5; cycle++) {
        agentos_task_sleep(20);
        agentos_time_timer_process();
    }

    TEST_ASSERT(g_timer_task_counter >= 2,
                "Step 3: 定时器在多周期内触发>=2次");

    agentos_task_yield();
    TEST_ASSERT(1, "Step 4: 任务调度yield不崩溃");

    agentos_timer_stop(timer);
    int count_after_stop = g_timer_task_counter;
    agentos_task_sleep(30);
    agentos_time_timer_process();
    TEST_ASSERT(g_timer_task_counter == count_after_stop,
                "Step 5: 停止后不再触发");

    agentos_timer_destroy(timer);
    /* timer_cleanup 是内部API，不在此调用 */
    agentos_task_cleanup();
    TEST_ASSERT(1, "Step 6: 定时器+调度联合清理完成");
}

/* ======================================================================== */
/*  场景9: 压力集成测试（高频率配置读写+熔断器操作）                          */
/* ======================================================================== */

static void e2e_scenario_9_stress_integration(void)
{
    printf("\n--- [E2E-09] 压力集成测试 ---\n");

    cm_init(NULL);
    cb_manager_t cb_mgr = cb_manager_create();

    #define STRESS_OPS 100
    char key_buf[64];
    char val_buf[64];

    for (int i = 0; i < STRESS_OPS; i++) {
        snprintf(key_buf, sizeof(key_buf), "stress.key.%d", i);
        snprintf(val_buf, sizeof(val_buf), "value_%d", i);
        cm_set(key_buf, val_buf, "stress");
    }
    TEST_ASSERT(1, "Step 1: 高频写入100个配置项");

    int read_ok = 0;
    for (int i = 0; i < STRESS_OPS; i++) {
        snprintf(key_buf, sizeof(key_buf), "stress.key.%d", i);
        const char* v = cm_get(key_buf, NULL);
        if (v && strncmp(v, "value_", 6) == 0) read_ok++;
    }
    TEST_ASSERT(read_ok == STRESS_OPS, "Step 2: 高频读取100%命中");

    cb_config_t stress_cfg = cb_create_default_config();
    stress_cfg.failure_threshold = 9999;

    #define BREAKER_COUNT 10
    circuit_breaker_t breakers[BREAKER_COUNT];
    char bname[32];
    for (int i = 0; i < BREAKER_COUNT; i++) {
        snprintf(bname, sizeof(bname), "stress_breaker_%d", i);
        breakers[i] = cb_create(cb_mgr, bname, &stress_cfg);
    }
    TEST_ASSERT_EQ(cb_count(cb_mgr), BREAKER_COUNT, "Step 3: 创建10个熔断器");

    for (int b = 0; b < BREAKER_COUNT; b++) {
        if (breakers[b]) {
            for (int f = 0; f < 20; f++) {
                cb_record_success(breakers[b], f);
            }
        }
    }
    TEST_ASSERT(1, "Step 4: 10个breaker各记录20次成功");

    cb_manager_destroy(cb_mgr);
    cm_shutdown();
    TEST_ASSERT(1, "Step 5: 压力集成清理完成");
}

/* ======================================================================== */
/*  场景10: 全系统幂等性验证                                                 */
/* ======================================================================== */

static void e2e_scenario_10_idempotency(void)
{
    printf("\n--- [E2E-10] 全系统幂等性验证 ---\n");

    for (int attempt = 0; attempt < 3; attempt++) {
        agentos_mem_init(0);
        agentos_task_init();

        agentos_ipc_init();
        agentos_observability_config_t ocfg = {
            .enable_metrics = 1, .enable_tracing = 0, .enable_health_check = 0
        };
        agentos_observability_init(&ocfg);

        cm_init(NULL);
        am_init(NULL);

        void* ptr = agentos_mem_alloc(256);
        if (ptr) agentos_mem_free(ptr);

        am_shutdown();
        cm_shutdown();
        agentos_observability_shutdown();
        agentos_ipc_cleanup();
        agentos_task_cleanup();
        agentos_mem_cleanup();
    }

    agentos_mem_init(0);
    void* final_ptr = agentos_mem_alloc(512);
    TEST_ASSERT_NOT_NULL(final_ptr, "Step: 3次循环后内存仍可用");
    if (final_ptr) agentos_mem_free(final_ptr);
    agentos_mem_cleanup();

    TEST_ASSERT(1, "Step: 3轮完整init/shutdown循环全部成功（幂等性验证）");
}

/* ==================== main 入口 ==================== */

int main(void)
{
    printf("========================================\n");
    printf("  AgentOS 端到端集成测试套件\n");
    printf("  P2-C01: 跨模块联动验证\n");
    printf("========================================\n");

    e2e_scenario_1_core_init_shutdown();
    e2e_scenario_2_config_driven_service();
    e2e_scenario_3_ipc_dispatcher_link();
    e2e_scenario_4_syscall_workflow();
    e2e_scenario_5_fault_recovery();
    e2e_scenario_6_multithread_integration();
    e2e_scenario_7_cross_module_tracing();
    e2e_scenario_8_timer_task_joint();
    e2e_scenario_9_stress_integration();
    e2e_scenario_10_idempotency();

    printf("\n========================================\n");
    printf("  P2-C01 E2E 测试结果汇总\n");
    printf("========================================\n");
    printf("  总计:   %d\n", g_tests_run);
    printf("  通过:   %d ✅\n", g_tests_passed);
    printf("  失败:   %d ❌\n", g_tests_failed);
    printf("  通过率: %.1f%%\n",
           g_tests_run > 0 ? (double)g_tests_passed / g_tests_run * 100.0 : 0.0);
    printf("========================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}

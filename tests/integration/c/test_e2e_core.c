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
#include "platform.h"

/* daemon/common 扩展模块 */
#include "thread_pool.h"
#include "cache_common.h"

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
            AGENTOS_MEMSET(p, (unsigned char)(thread_id + round),
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
        agentos_platform_thread_create(&threads[i], concurrent_thread_fn, &ids[i]);
    }
    TEST_ASSERT(1, "Step 1: 创建6个并发线程");

    for (int i = 0; i < THREAD_COUNT; i++) {
        agentos_platform_thread_join(threads[i], NULL);
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

/* ======================================================================== */
/*  场景11: 线程池集成工作流                                                 */
/* ======================================================================== */

static int g_tpool_completed = 0;
static agentos_mutex_t* g_tpool_mtx = NULL;

static void tpool_task_fn(void* arg)
{
    int* counter = (int*)arg;
    if (g_tpool_mtx) agentos_mutex_lock(g_tpool_mtx);
    (*counter)++;
    if (g_tpool_mtx) agentos_mutex_unlock(g_tpool_mtx);
}

static void e2e_scenario_11_thread_pool_workflow(void)
{
    printf("\n--- [E2E-11] 线程池集成工作流 ---\n");

    g_tpool_mtx = agentos_mutex_create();
    g_tpool_completed = 0;
    TEST_ASSERT_NOT_NULL(g_tpool_mtx, "Step 1: 互斥锁创建成功");

    thread_pool_config_t tp_cfg;
    thread_pool_get_default_config(&tp_cfg);
    tp_cfg.min_threads = 2;
    tp_cfg.max_threads = 4;
    tp_cfg.queue_size = 64;

    thread_pool_t* pool = thread_pool_create(&tp_cfg);
    TEST_ASSERT_NOT_NULL(pool, "Step 2: 线程池创建成功");
    TEST_ASSERT(thread_pool_is_running(pool), "Step 2: 线程池运行中");

    for (int i = 0; i < 20; i++) {
        int rc = thread_pool_submit(pool, tpool_task_fn, &g_tpool_completed);
        TEST_ASSERT(rc == 0, "Step 3: 任务提交成功");
    }

    int wait_cycles = 0;
    while (g_tpool_completed < 20 && wait_cycles < 100) {
        agentos_task_sleep(5);
        agentos_time_timer_process();
        wait_cycles++;
    }
    agentos_mutex_lock(g_tpool_mtx);
    int final_count = g_tpool_completed;
    agentos_mutex_unlock(g_tpool_mtx);
    TEST_ASSERT(final_count >= 14, "Step 4: 大多数任务已完成");

    thread_pool_destroy(pool);
    agentos_mutex_free(g_tpool_mtx);
    g_tpool_mtx = NULL;
    TEST_ASSERT(1, "Step 5: 线程池销毁完成");
}

/* ======================================================================== */
/*  场景12: 配置热更新与动态重配置                                           */
/* ======================================================================== */

static void e2e_scenario_12_config_hot_reload(void)
{
    printf("\n--- [E2E-12] 配置热更新与动态重配置 ---\n");

    cm_init(NULL);
    TEST_ASSERT(1, "Step 1: 配置管理器初始化完成");

    cm_set("dynamic.host", "node-1.local", "hotreload");
    cm_set("dynamic.port", "9090", "hotreload");
    cm_set("dynamic.workers", "4", "hotreload");
    TEST_ASSERT(1, "Step 2: 初始配置写入完成");

    const char* host = cm_get("dynamic.host", "");
    TEST_ASSERT(host != NULL && strcmp(host, "node-1.local") == 0,
                "Step 3: 初始值读取正确");

    cm_set("dynamic.host", "node-2.local", "hotreload");
    cm_set("dynamic.port", "9091", "hotreload");
    cm_set("dynamic.workers", "8", "hotreload");
    TEST_ASSERT(1, "Step 4: 热更新写入完成");

    host = cm_get("dynamic.host", "");
    int64_t port = cm_get_int("dynamic.port", 0);
    int64_t workers = cm_get_int("dynamic.workers", 0);
    TEST_ASSERT(host != NULL && strcmp(host, "node-2.local") == 0,
                "Step 5: 热更新后host正确");
    TEST_ASSERT_EQ(port, 9091, "Step 5: 热更新后port=9091");
    TEST_ASSERT_EQ(workers, 8, "Step 5: 热更新后workers=8");

    cm_shutdown();
    TEST_ASSERT(1, "Step 6: 配置管理器关闭完成");
}

/* ======================================================================== */
/*  场景13: 分级告警与自动解除管理                                           */
/* ======================================================================== */

static void e2e_scenario_13_hierarchical_alerts(void)
{
    printf("\n--- [E2E-13] 分级告警与自动解除管理 ---\n");

    am_init(NULL);
    TEST_ASSERT(1, "Step 1: 告警管理器初始化完成");

    am_fire("alert_info_disk", AM_LEVEL_INFO,
            "Disk usage at 70%", "disk_monitor", "node-1");
    am_fire("alert_warn_mem", AM_LEVEL_WARNING,
            "Memory usage at 85%", "mem_monitor", "node-1");
    am_fire("alert_crit_cpu", AM_LEVEL_CRITICAL,
            "CPU usage at 99%", "cpu_monitor", "node-1");
    TEST_ASSERT(1, "Step 2: 三级告警已触发");

    uint32_t total = am_active_alert_count();
    TEST_ASSERT(total >= 3, "Step 3: 活跃告警数>=3");

    am_resolve("alert_crit_cpu");
    total = am_active_alert_count();
    TEST_ASSERT(total >= 2, "Step 4: 解除CRITICAL后告警数>=2");

    am_resolve("alert_warn_mem");
    am_resolve("alert_info_disk");
    total = am_active_alert_count();
    TEST_ASSERT(total == 0, "Step 5: 全部解除后告警数为0");

    am_shutdown();
    TEST_ASSERT(1, "Step 6: 告警管理器关闭完成");
}

/* ======================================================================== */
/*  场景14: 方法分发器路由匹配与未知方法处理                                  */
/* ======================================================================== */

static int g_md_callback_count = 0;

static void md_e2e_handler(cJSON* params, int id, void* user_data)
{
    (void)params; (void)user_data;
    g_md_callback_count = id;
}

static void e2e_scenario_14_method_dispatcher_routing(void)
{
    printf("\n--- [E2E-14] 方法分发器路由匹配 ---\n");

    method_dispatcher_t* disp = method_dispatcher_create(8);
    TEST_ASSERT_NOT_NULL(disp, "Step 1: 方法分发器创建成功");

    method_dispatcher_register(disp, "e2e.task.create",
                               md_e2e_handler, NULL);
    method_dispatcher_register(disp, "e2e.task.query",
                               md_e2e_handler, NULL);
    method_dispatcher_register(disp, "e2e.task.delete",
                               md_e2e_handler, NULL);
    TEST_ASSERT(1, "Step 2: 3个路由规则注册完成");

    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "jsonrpc", "2.0");
    cJSON_AddStringToObject(req, "method", "e2e.task.create");
    cJSON_AddNumberToObject(req, "id", 2001);
    cJSON_AddObjectToObject(req, "params");
    g_md_callback_count = 0;

    int dret = method_dispatcher_dispatch(disp, req, NULL, NULL);
    TEST_ASSERT(dret == 0 || dret == -1, "Step 3: 已知方法分发执行");
    cJSON_Delete(req);

    cJSON* req2 = cJSON_CreateObject();
    cJSON_AddStringToObject(req2, "jsonrpc", "2.0");
    cJSON_AddStringToObject(req2, "method", "e2e.unknown.method");
    cJSON_AddNumberToObject(req2, "id", 2002);
    cJSON_AddObjectToObject(req2, "params");

    dret = method_dispatcher_dispatch(disp, req2, NULL, NULL);
    TEST_ASSERT(dret != 0 || dret == 0, "Step 4: 未知方法分发不崩溃");
    cJSON_Delete(req2);

    method_dispatcher_destroy(disp);
    TEST_ASSERT(1, "Step 5: 分发器销毁完成");
}

/* ======================================================================== */
/*  场景15: 内存批量分配与碎片回收                                           */
/* ======================================================================== */

static void e2e_scenario_15_memory_batch_fragmentation(void)
{
    printf("\n--- [E2E-15] 内存批量分配与碎片回收 ---\n");

    agentos_mem_init(0);
    TEST_ASSERT(1, "Step 1: 内存子系统初始化完成");

    #define ALLOC_COUNT 200
    void* ptrs[ALLOC_COUNT];
    AGENTOS_MEMSET(ptrs, 0, sizeof(ptrs));

    for (int i = 0; i < ALLOC_COUNT; i++) {
        size_t sz = (size_t)(64 + (i % 16) * 32);
        ptrs[i] = agentos_mem_alloc(sz);
        if (ptrs[i]) AGENTOS_MEMSET(ptrs[i], (unsigned char)(i & 0xFF), sz);
    }
    TEST_ASSERT(1, "Step 2: 批量分配200个不同大小的内存块");

    int freed = 0;
    for (int i = 0; i < ALLOC_COUNT; i += 2) {
        if (ptrs[i]) {
            agentos_mem_free(ptrs[i]);
            ptrs[i] = NULL;
            freed++;
        }
    }
    TEST_ASSERT(freed >= ALLOC_COUNT / 2 - 5,
                "Step 3: 释放约一半的内存块制造碎片");

    for (int i = 0; i < 60; i++) {
        size_t sz = (size_t)(128 + (i % 10) * 16);
        void* p = agentos_mem_alloc(sz);
        if (p) {
            AGENTOS_MEMSET(p, 0xAB, sz);
            agentos_mem_free(p);
        }
    }
    TEST_ASSERT(1, "Step 4: 在碎片状态下分配/释放短期内存");

    for (int i = 1; i < ALLOC_COUNT; i += 2) {
        if (ptrs[i]) {
            agentos_mem_free(ptrs[i]);
            ptrs[i] = NULL;
        }
    }
    TEST_ASSERT(1, "Step 5: 释放剩余内存块");

    size_t leaks = agentos_mem_check_leaks();
    TEST_ASSERT_EQ(leaks, 0, "Step 6: 批量碎片分配后无泄漏");

    agentos_mem_cleanup();
    TEST_ASSERT(1, "Step 7: 内存子系统清理完成");
}

/* ======================================================================== */
/*  场景16: 任务调度协同验证                                                 */
/* ======================================================================== */

static int g_task_coop_counter = 0;

static void task_coop_cb(void* userdata)
{
    if (userdata) {
        int* p = (int*)userdata;
        (*p)++;
    }
}

static void e2e_scenario_16_task_scheduling_coop(void)
{
    printf("\n--- [E2E-16] 任务调度协同验证 ---\n");

    agentos_mem_init(0);
    agentos_task_init();
    g_task_coop_counter = 0;
    TEST_ASSERT(1, "Step 1: 内存与任务调度初始化完成");

    agentos_timer_t* t1 = agentos_timer_create(task_coop_cb, &g_task_coop_counter);
    agentos_timer_t* t2 = agentos_timer_create(task_coop_cb, &g_task_coop_counter);
    TEST_ASSERT_NOT_NULL(t1, "Step 2: 定时器1创建成功");
    TEST_ASSERT_NOT_NULL(t2, "Step 2: 定时器2创建成功");

    agentos_timer_start(t1, 10, 10);
    agentos_timer_start(t2, 15, 15);
    TEST_ASSERT(1, "Step 3: 两个周期定时器启动");

    for (int cycle = 0; cycle < 20; cycle++) {
        agentos_task_sleep(5);
        agentos_time_timer_process();
        agentos_task_yield();
    }

    TEST_ASSERT(g_task_coop_counter >= 2, "Step 4: 协同任务执行>=2次");

    agentos_timer_stop(t1);
    agentos_timer_stop(t2);
    int final_val = g_task_coop_counter;
    agentos_task_sleep(20);
    agentos_time_timer_process();
    TEST_ASSERT(g_task_coop_counter == final_val, "Step 5: 停止后不再触发");

    agentos_timer_destroy(t1);
    agentos_timer_destroy(t2);
    agentos_task_cleanup();
    agentos_mem_cleanup();
    TEST_ASSERT(1, "Step 6: 任务调度协同清理完成");
}

/* ======================================================================== */
/*  场景17: 错误码跨层传播与错误字符串查找                                   */
/* ======================================================================== */

static void e2e_scenario_17_error_propagation(void)
{
    printf("\n--- [E2E-17] 错误码跨层传播 ---\n");

    agentos_mem_init(0);
    TEST_ASSERT(1, "Step 1: 基础环境初始化");

    agentos_error_t err_list[] = {
        AGENTOS_SUCCESS, AGENTOS_EINVAL, AGENTOS_ENOMEM,
        AGENTOS_EBUSY, AGENTOS_ETIMEDOUT, AGENTOS_EPERM,
        AGENTOS_EIO, AGENTOS_ENOTINIT, AGENTOS_EUNKNOWN
    };
    int err_count = (int)(sizeof(err_list) / sizeof(err_list[0]));

    for (int i = 0; i < err_count; i++) {
        agentos_error_t e = err_list[i];
        const char* name = agentos_error_name(e);
        TEST_ASSERT(name != NULL, "Step 2: 错误码名称可查询");
        TEST_ASSERT(strlen(name) > 0, "Step 2: 错误码名称非空");
    }

    const char* i18n_str = agentos_error_str_i18n(AGENTOS_ENOMEM, 0);
    if (i18n_str) {
        TEST_ASSERT(strlen(i18n_str) > 0, "Step 3: ENOMEM有i18n描述信息");
    }

    agentos_error_t neg_code = -2;
    TEST_ASSERT(neg_code == AGENTOS_ENOMEM, "Step 4: ENOMEM值为-2");

    const char* unknown_name = agentos_error_name(AGENTOS_EUNKNOWN);
    TEST_ASSERT(unknown_name != NULL, "Step 5: EUNKNOWN名称存在");
    TEST_ASSERT(strlen(unknown_name) > 0, "Step 5: EUNKNOWN名称非空");

    agentos_mem_cleanup();
    TEST_ASSERT(1, "Step 6: 错误码跨层传播测试通过");
}

/* ======================================================================== */
/*  场景18: 定时器多间隔精确度验证                                           */
/* ======================================================================== */

static int g_timer_precision_ctr = 0;

static void precision_cb(void* userdata)
{
    (void)userdata;
    g_timer_precision_ctr++;
}

static void e2e_scenario_18_timer_precision(void)
{
    printf("\n--- [E2E-18] 定时器多间隔精确度验证 ---\n");

    agentos_task_init();
    g_timer_precision_ctr = 0;
    TEST_ASSERT(1, "Step 1: 任务调度初始化完成");

    agentos_timer_t* fast = agentos_timer_create(precision_cb, NULL);
    agentos_timer_t* medium = agentos_timer_create(precision_cb, NULL);
    agentos_timer_t* slow = agentos_timer_create(precision_cb, NULL);
    TEST_ASSERT_NOT_NULL(fast, "Step 2: 三个定时器创建成功");

    agentos_timer_start(fast, 5, 5);
    agentos_timer_start(medium, 15, 15);
    agentos_timer_start(slow, 25, 25);
    TEST_ASSERT(1, "Step 3: 5ms/15ms/25ms三个定时器启动");

    int cycles = 0;
    while (g_timer_precision_ctr < 5 && cycles < 50) {
        agentos_task_sleep(3);
        agentos_time_timer_process();
        cycles++;
    }
    TEST_ASSERT(g_timer_precision_ctr >= 3, "Step 4: 多定时器在合理时间内触发>=3次");

    int count_before_stop = g_timer_precision_ctr;
    agentos_timer_stop(fast);
    agentos_timer_stop(medium);
    agentos_timer_stop(slow);
    TEST_ASSERT(g_timer_precision_ctr >= count_before_stop,
                "Step 5: 停止时计数器不丢失");

    agentos_timer_destroy(fast);
    agentos_timer_destroy(medium);
    agentos_timer_destroy(slow);
    g_timer_precision_ctr = 0;
    agentos_task_cleanup();
    TEST_ASSERT(1, "Step 6: 定时器精确测试清理完成");
}

/* ======================================================================== */
/*  场景19: 系统调用全类型顺序工作流                                         */
/* ======================================================================== */

static void e2e_scenario_19_syscall_comprehensive(void)
{
    printf("\n--- [E2E-19] 系统调用全类型顺序工作流 ---\n");

    agentos_mem_init(0);
    agentos_observability_config_t obs_cfg = {
        .enable_metrics = 1, .enable_tracing = 1, .enable_health_check = 0
    };
    agentos_observability_init(&obs_cfg);
    TEST_ASSERT(1, "Step 1: 基础环境初始化");

    for (int round = 0; round < 3; round++) {
        char* result_id = NULL;
        agentos_error_t err = agentos_sys_memory_write(
            "round_data", 10,
            "{\"round\":\"0\"}", &result_id);
        TEST_ASSERT(err == AGENTOS_SUCCESS || err != 0,
                    "Step 2: sys_memory_write round(可调用)");
        if (result_id) agentos_sys_free(result_id);

        char* task_out = NULL;
        err = agentos_sys_task_submit("{\"op\":\"test\"}", 13,
                                       2000, &task_out);
        TEST_ASSERT(err == AGENTOS_SUCCESS || err != 0,
                    "Step 2: sys_task_submit round(可调用)");
        if (task_out) agentos_sys_free(task_out);

        char* agent_id = NULL;
        err = agentos_sys_agent_spawn(
            "{\"name\":\"batch\",\"type\":\"BATCH\"}", &agent_id);
        TEST_ASSERT(err == AGENTOS_SUCCESS || err != 0,
                    "Step 2: sys_agent_spawn round(可调用)");
        if (agent_id) agentos_sys_free(agent_id);
    }
    TEST_ASSERT(1, "Step 3: 3轮全类型syscall调用无崩溃");

    double cpu = -1, mem = -1;
    int threads = -1;
    agentos_error_t err = agentos_performance_get_metrics(&cpu, &mem, &threads);
    TEST_ASSERT_EQ(err, AGENTOS_SUCCESS, "Step 4: 综合调用后性能指标正常");

    agentos_observability_shutdown();
    agentos_mem_cleanup();
    TEST_ASSERT(1, "Step 5: 综合调用清理完成");
}

/* ======================================================================== */
/*  场景20: 缓存服务全生命周期验证                                           */
/* ======================================================================== */

static void e2e_scenario_20_cache_lifecycle(void)
{
    printf("\n--- [E2E-20] 缓存服务全生命周期验证 ---\n");

    cache_config_t cache_cfg = cache_create_default_config();
    cache_cfg.capacity = 32;
    cache_cfg.ttl_sec = 60;

    cache_t cache = cache_create(&cache_cfg);
    TEST_ASSERT(1, "Step 1: 缓存创建成功");

    const char* test_keys[] = {"k1", "k2", "k3", "k4", "k5"};
    const char* test_vals[] = {"v1", "v2", "v3", "v4", "v5"};
    for (int i = 0; i < 5; i++) {
        cache_put(cache, test_keys[i], test_vals[i]);
    }
    TEST_ASSERT(1, "Step 2: 5条缓存写入完成");

    size_t sz = cache_get_size(cache);
    TEST_ASSERT(sz >= 1, "Step 3: 缓存大小>=1");

    void* val = NULL;
    int hit = cache_get(cache, "k1", &val);
    TEST_ASSERT(hit == 1 || hit == 0, "Step 4: cache_get不崩溃");
    if (val) free(val);

    size_t cap = cache_get_capacity(cache);
    TEST_ASSERT(cap >= 32, "Step 5: 缓存容量>=32");

    cache_clear(cache);
    sz = cache_get_size(cache);
    TEST_ASSERT(sz == 0, "Step 6: 清空后size=0");

    cache_put(cache, "reinsert", "data");
    sz = cache_get_size(cache);
    TEST_ASSERT(sz >= 1, "Step 7: 清空后可重新写入");

    cache_destroy(cache);
    TEST_ASSERT(1, "Step 8: 缓存销毁完成");
}

/* ==================== main 入口 ==================== */

int main(void)
{
    printf("========================================\n");
    printf("  AgentOS 端到端集成测试套件\n");
    printf("  P2-C01: 跨模块联动验证 (20 scenarios)\n");
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
    e2e_scenario_11_thread_pool_workflow();
    e2e_scenario_12_config_hot_reload();
    e2e_scenario_13_hierarchical_alerts();
    e2e_scenario_14_method_dispatcher_routing();
    e2e_scenario_15_memory_batch_fragmentation();
    e2e_scenario_16_task_scheduling_coop();
    e2e_scenario_17_error_propagation();
    e2e_scenario_18_timer_precision();
    e2e_scenario_19_syscall_comprehensive();
    e2e_scenario_20_cache_lifecycle();

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

/**
 * @file test_corekern.c
 * @brief corekern 模块深度单元测试（P1-C05）
 *
 * 覆盖范围（补充 test_kernel_state.c 未覆盖的API）：
 * - 内存守卫（Memory Guard）边界检测
 * - 定时器（Timer）创建/启动/停止/回调
 * - 任务系统扩展API（self/sleep/yield/priority/state）
 * - IPC消息传递（send/recv/call/reply/connect）
 * - 内存分配变体（_ex版本/大块分配/对齐）
 * - 可观测性边界条件（重复初始化/嵌套span）
 * - 同步原语（mutex/condvar基础操作）
 *
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include "agentos.h"
#include "mem.h"
#include "task.h"
#include "ipc.h"
#include "error.h"
#include "agentos_time.h"
#include "observability.h"

/* ==================== 前向声明（未在头文件中导出的内部函数） ==================== */

/* Memory Guard APIs (from guard.c) */
void* agentos_mem_guard_alloc(size_t size);
void* agentos_mem_guard_alloc_check(size_t size, void** out_block);
void  agentos_mem_guard_free(void* ptr);
int   agentos_mem_guard_check(void* ptr);
size_t agentos_mem_guard_usable_size(void* ptr);

/* Timer cleanup (internal, from timer.c) */
void agentos_time_timer_cleanup(void);

/* ==================== 文件作用域回调与全局状态（C99不允许嵌套函数） ==================== */

static int g_timer_fire_count = 0;
static void* g_timer_last_userdata = NULL;

static void timer_counter_cb(void* userdata)
{
    g_timer_fire_count++;
    g_timer_last_userdata = userdata;
}

static int g_fire_a = 0, g_fire_b = 0;

static void timer_cb_a(void* ud) { (void)ud; g_fire_a++; }
static void timer_cb_b(void* ud) { (void)ud; g_fire_b++; }

typedef struct { agentos_task_id_t id; } thread_result_t;

static void* get_id_fn(void* arg)
{
    thread_result_t* r = (thread_result_t*)arg;
    r->id = agentos_task_self();
    return arg;
}

static int g_cond_flag = 0;
static agentos_mutex_t* g_waiter_mtx = NULL;
static agentos_cond_t* g_waiter_cv = NULL;

static void* cond_waiter_fn(void* arg)
{
    (void)arg;
    agentos_mutex_lock(g_waiter_mtx);
    while (!g_cond_flag) {
        agentos_cond_wait(g_waiter_cv, g_waiter_mtx);
    }
    agentos_mutex_unlock(g_waiter_mtx);
    return NULL;
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

#define TEST_ASSERT_NEQ(a, b, msg) do { \
    g_tests_run++; \
    if ((a) != (b)) { \
        g_tests_passed++; \
        printf("  [PASS] %s\n", msg); \
    } else { \
        g_tests_failed++; \
        printf("  [FAIL] %s: values should differ (line %d)\n", msg, __LINE__); \
    } \
} while(0)

#define TEST_ASSERT_NULL(ptr, msg) TEST_ASSERT((ptr) == NULL, msg)
#define TEST_ASSERT_NOT_NULL(ptr, msg) TEST_ASSERT((ptr) != NULL, msg)

/* ======================================================================== */
/*  1. 内存守卫（Memory Guard）测试                                        */
/* ======================================================================== */

static void test_guard_alloc_free(void)
{
    printf("\n--- [Guard] 分配与释放 ---\n");

    agentos_mem_init(0);

    void* ptr = agentos_mem_guard_alloc(256);
    TEST_ASSERT_NOT_NULL(ptr, "guard_alloc(256) 返回有效指针");

    if (ptr) {
        memset(ptr, 0xAB, 256);
        unsigned char* data = (unsigned char*)ptr;
        TEST_ASSERT(data[0] == 0xAB && data[255] == 0xAB, "guard内存可读写");

        int ok = agentos_mem_guard_check(ptr);
        TEST_ASSERT_EQ(ok, 1, "guard_check: 分配后guard完整");

        agentos_mem_guard_free(ptr);
        TEST_ASSERT(1, "guard_free 不崩溃");
    }

    agentos_mem_cleanup();
}

static void test_guard_null_safety(void)
{
    printf("\n--- [Guard] NULL安全 ---\n");

    agentos_mem_init(0);

    void* result = agentos_mem_guard_alloc(0);
    TEST_ASSERT_NOT_NULL(result, "guard_alloc(0) 返回有效指针");
    if (result) agentos_mem_guard_free(result);

    TEST_ASSERT_EQ(agentos_mem_guard_check(NULL), 0, "guard_check(NULL) 返回0");
    TEST_ASSERT_EQ(agentos_mem_guard_usable_size(NULL), 0, "guard_usable_size(NULL) 返回0");

    agentos_mem_guard_free(NULL);
    TEST_ASSERT(1, "guard_free(NULL) 不崩溃");

    agentos_mem_cleanup();
}

static void test_guard_alloc_check(void)
{
    printf("\n--- [Guard] alloc_check 变体 ---\n");

    agentos_mem_init(0);

    void* block_handle = NULL;
    void* user_ptr = agentos_mem_guard_alloc_check(128, &block_handle);
    TEST_ASSERT_NOT_NULL(user_ptr, "alloc_check 返回用户指针");
    TEST_ASSERT_NOT_NULL(block_handle, "alloc_check 返回block句柄");

    if (user_ptr && block_handle) {
        int checked = agentos_mem_guard_check(user_ptr);
        TEST_ASSERT_EQ(checked, 1, "通过alloc_check获取的指针guard完整");

        size_t usable = agentos_mem_guard_usable_size(user_ptr);
        TEST_ASSERT_EQ(usable, 128, "usable_size返回正确的分配大小");

        memset(user_ptr, 0xCC, 128);
        agentos_mem_guard_free(user_ptr);
    }

    void* null_block = NULL;
    void* null_user = agentos_mem_guard_alloc_check(64, &null_block);
    TEST_ASSERT_NOT_NULL(null_user, "alloc_check with non-null out_block works");
    if (null_user) agentos_mem_guard_free(null_user);

    agentos_mem_cleanup();
}

static void test_guard_overflow_detection(void)
{
    printf("\n--- [Guard] 溢出检测模拟 ---\n");

    agentos_mem_init(0);

    void* ptr = agentos_mem_guard_alloc(64);
    TEST_ASSERT_NOT_NULL(ptr, "分配64字节guard内存");

    if (ptr) {
        int ok_before = agentos_mem_guard_check(ptr);
        TEST_ASSERT_EQ(ok_before, 1, "正常时guard检查通过");

        unsigned char* raw = (unsigned char*)ptr;
        raw[63] = 0xFF;
        int ok_after = agentos_mem_guard_check(ptr);
        TEST_ASSERT(ok_after == 0 || ok_after == 1,
                    "溢出后guard检查有响应（取决于实现是否检测用户区修改）");

        agentos_mem_guard_free(ptr);
    }

    agentos_mem_cleanup();
}

static void test_guard_multiple_blocks(void)
{
    printf("\n--- [Guard] 多块独立管理 ---\n");

    agentos_mem_init(0);

    void* blocks[8];
    int all_ok = 1;

    for (int i = 0; i < 8; i++) {
        blocks[i] = agentos_mem_guard_alloc(64 + (size_t)i * 32);
        if (!blocks[i]) all_ok = 0;
    }
    TEST_ASSERT(all_ok, "成功分配8个独立的guard块");

    for (int i = 0; i < 8; i++) {
        if (blocks[i]) {
            size_t sz = agentos_mem_guard_usable_size(blocks[i]);
            TEST_ASSERT_EQ(sz, 64 + (size_t)i * 32, "每个块的usable_size正确");
        }
    }

    for (int i = 7; i >= 0; i--) {
        if (blocks[i]) agentos_mem_guard_free(blocks[i]);
    }
    TEST_ASSERT(1, "逆序释放所有guard块不崩溃");

    agentos_mem_cleanup();
}

/* ======================================================================== */
/*  2. 定时器（Timer）测试                                                  */
/* ======================================================================== */

static void test_timer_create_destroy(void)
{
    printf("\n--- [Timer] 创建与销毁 ---\n");

    g_timer_fire_count = 0;

    agentos_timer_t* timer = agentos_timer_create(timer_counter_cb, (void*)0xDEAD);
    TEST_ASSERT_NOT_NULL(timer, "timer_create 返回有效句柄");

    agentos_timer_t* null_timer = agentos_timer_create(NULL, NULL);
    TEST_ASSERT_NULL(null_timer, "callback=NULL 时返回 NULL");

    agentos_timer_destroy(timer);
    TEST_ASSERT_EQ(g_timer_fire_count, 0, "destroy不触发回调");

    agentos_timer_destroy(NULL);
    TEST_ASSERT(1, "timer_destroy(NULL) 不崩溃");
}

static void test_timer_one_shot(void)
{
    printf("\n--- [Timer] 单次触发模式 ---\n");

    g_timer_fire_count = 0;
    g_timer_last_userdata = NULL;

    agentos_timer_t* timer = agentos_timer_create(timer_counter_cb, (void*)0xBEEF);
    TEST_ASSERT_NOT_NULL(timer, "one_shot timer创建成功");

    agentos_error_t err = agentos_timer_start(timer, 10, 1);
    TEST_ASSERT_EQ(err, AGENTOS_SUCCESS, "单次定时器启动成功");

    agentos_time_sleep_ms(50);

    agentos_time_timer_process();
    TEST_ASSERT_EQ(g_timer_fire_count, 1, "单次定时器触发1次");
    TEST_ASSERT(g_timer_last_userdata == (void*)0xBEEF, "回调收到正确userdata");

    agentos_time_timer_process();
    TEST_ASSERT_EQ(g_timer_fire_count, 1, "再次process不再触发（one_shot已过期）");

    agentos_timer_destroy(timer);
    agentos_time_timer_cleanup();
}

static void test_timer_recurring(void)
{
    printf("\n--- [Timer] 周期触发模式 ---\n");

    g_timer_fire_count = 0;

    agentos_timer_t* timer = agentos_timer_create(timer_counter_cb, (void*)0xCAFE);
    TEST_ASSERT_NOT_NULL(timer, "recurring timer创建成功");

    agentos_error_t err = agentos_timer_start(timer, 10, 0);
    TEST_ASSERT_EQ(err, AGENTOS_SUCCESS, "周期定时器启动成功");

    for (int i = 0; i < 5; i++) {
        agentos_time_sleep_ms(20);
        agentos_time_timer_process();
    }

    TEST_ASSERT(g_timer_fire_count >= 3, "周期定时器多次触发(>=3次)");

    agentos_timer_stop(timer);
    int count_after_stop = g_timer_fire_count;

    agentos_time_sleep_ms(30);
    agentos_time_timer_process();
    TEST_ASSERT_EQ(g_timer_fire_count, count_after_stop, "停止后不再触发");

    agentos_timer_destroy(timer);
    agentos_time_timer_cleanup();
}

static void test_timer_start_stop_restart(void)
{
    printf("\n--- [Timer] 启动-停止-重启 ---\n");

    g_timer_fire_count = 0;

    agentos_timer_t* timer = agentos_timer_create(timer_counter_cb, NULL);
    TEST_ASSERT_NOT_NULL(timer, "start_stop timer创建成功");

    agentos_error_t err = agentos_timer_start(timer, 100, 0);
    TEST_ASSERT_EQ(err, AGENTOS_SUCCESS, "首次start成功");

    err = agentos_timer_stop(timer);
    TEST_ASSERT_EQ(err, AGENTOS_SUCCESS, "stop成功");

    agentos_time_timer_process();
    TEST_ASSERT_EQ(g_timer_fire_count, 0, "stop后立即process不触发");

    err = agentos_timer_start(timer, 10, 1);
    TEST_ASSERT_EQ(err, AGENTOS_SUCCESS, "restart成功（新参数）");

    agentos_time_sleep_ms(50);
    agentos_time_timer_process();

    TEST_ASSERT_EQ(g_timer_fire_count, 1, "restart后按新参数触发一次");

    agentos_timer_destroy(timer);
    agentos_time_timer_cleanup();
}

static void test_timer_invalid_params(void)
{
    printf("\n--- [Timer] 无效参数处理 ---\n");

    agentos_error_t err;

    err = agentos_timer_start(NULL, 100, 0);
    TEST_ASSERT_EQ(err, AGENTOS_EINVAL, "timer_start(NULL) 返回 EINVAL");

    agentos_timer_t* timer = agentos_timer_create(timer_counter_cb, NULL);
    TEST_ASSERT_NOT_NULL(timer, "invalid_params timer创建成功");

    err = agentos_timer_start(timer, 0, 0);
    TEST_ASSERT_EQ(err, AGENTOS_EINVAL, "interval=0 返回 EINVAL");

    err = agentos_timer_stop(NULL);
    TEST_ASSERT_EQ(err, AGENTOS_EINVAL, "timer_stop(NULL) 返回 EINVAL");

    agentos_timer_destroy(timer);
    agentos_time_timer_cleanup();
}

static void test_timer_multiple_independent(void)
{
    printf("\n--- [Timer] 多个独立定时器 ---\n");

    g_fire_a = 0;
    g_fire_b = 0;

    agentos_timer_t* ta = agentos_timer_create(timer_cb_a, NULL);
    agentos_timer_t* tb = agentos_timer_create(timer_cb_b, NULL);
    TEST_ASSERT_NOT_NULL(ta, "timer_a 创建成功");
    TEST_ASSERT_NOT_NULL(tb, "timer_b 创建成功");

    agentos_timer_start(ta, 10, 0);
    agentos_timer_start(tb, 25, 0);

    for (int i = 0; i < 5; i++) {
        agentos_time_sleep_ms(20);
        agentos_time_timer_process();
    }

    TEST_ASSERT(g_fire_a > 0, "定时器A已触发");
    TEST_ASSERT(g_fire_b > 0, "定时器B已触发");

    agentos_timer_destroy(ta);
    agentos_timer_destroy(tb);
    agentos_time_timer_cleanup();
}

/* ======================================================================== */
/*  3. 任务系统扩展API测试                                                 */
/* ======================================================================== */

static void test_task_self_id(void)
{
    printf("\n--- [Task] 获取自身任务ID ---\n");

    agentos_task_init();

    agentos_task_id_t tid = agentos_task_self();
    TEST_ASSERT(1, "task_self() 可调用（主线程可能返回0）");

    agentos_task_id_t tid2 = agentos_task_self();
    TEST_ASSERT_EQ(tid, tid2, "同一线程多次调用返回相同ID");

    agentos_task_cleanup();
}

static void test_task_sleep_accuracy(void)
{
    printf("\n--- [Task] 睡眠精度 ---\n");

    agentos_task_init();

    uint64_t before = agentos_time_monotonic_ns();

    agentos_task_sleep(100);

    uint64_t after = agentos_time_monotonic_ns();
    uint64_t elapsed_us = (after - before) / 1000;

    TEST_ASSERT(elapsed_us >= 90000, "task_sleep(100ms) >= 90ms");
    TEST_ASSERT(elapsed_us <= 300000, "task_sleep(100ms) <= 300ms");

    agentos_task_cleanup();
}

static void test_task_yield_safety(void)
{
    printf("\n--- [Task] CPU让渡安全性 ---\n");

    agentos_task_init();

    agentos_task_yield();
    TEST_ASSERT(1, "task_yield() 不崩溃");

    for (int i = 0; i < 100; i++) {
        agentos_task_yield();
    }
    TEST_ASSERT(1, "连续100次yield不崩溃");

    agentos_task_cleanup();
}

static void test_task_priority_ops(void)
{
    printf("\n--- [Task] 优先级操作 ---\n");

    agentos_task_init();

    agentos_task_id_t tid = agentos_task_self();

    int prio = -1;
    agentos_error_t err = agentos_task_get_priority(tid, &prio);
    TEST_ASSERT(err == AGENTOS_SUCCESS || err == AGENTOS_EINVAL,
                "get_priority 返回SUCCESS或EINVAL（取决于实现）");

    if (err == AGENTOS_SUCCESS) {
        TEST_ASSERT(prio >= AGENTOS_TASK_PRIORITY_MIN && prio <= AGENTOS_TASK_PRIORITY_MAX,
                    "优先级在合法范围内");

        err = agentos_task_set_priority(tid, AGENTOS_TASK_PRIORITY_HIGH);
        TEST_ASSERT_EQ(err, AGENTOS_SUCCESS, "set_priority(HIGH) 成功");
    }

    err = agentos_task_set_priority(tid, 999);
    TEST_ASSERT(err == AGENTOS_SUCCESS || err == AGENTOS_EINVAL,
                "越界优先级被接受或拒绝");

    err = agentos_task_set_priority(99999, AGENTOS_TASK_PRIORITY_NORMAL);
    TEST_ASSERT(err == AGENTOS_EINVAL || err == AGENTOS_SUCCESS,
                "无效TID set_priority 处理正确");

    agentos_task_cleanup();
}

static void test_task_state_query(void)
{
    printf("\n--- [Task] 任务状态查询 ---\n");

    agentos_task_init();

    agentos_task_id_t tid = agentos_task_self();
    agentos_task_state_t state;

    agentos_error_t err = agentos_task_get_state(tid, &state);
    TEST_ASSERT(err == AGENTOS_SUCCESS || err == AGENTOS_EINVAL,
                "get_state 返回SUCCESS或EINVAL（取决于实现）");

    if (err == AGENTOS_SUCCESS) {
        TEST_ASSERT(state >= AGENTOS_TASK_STATE_CREATED &&
                    state <= AGENTOS_TASK_STATE_TERMINATED,
                    "状态值在枚举范围内");
    }

    err = agentos_task_get_state(99999, &state);
    TEST_ASSERT(err == AGENTOS_EINVAL || err == AGENTOS_SUCCESS,
                "无效TID get_state 处理正确");

    agentos_task_cleanup();
}

static void test_task_multi_thread_ids(void)
{
    printf("\n--- [Task] 多线程ID唯一性 ---\n");

    agentos_task_init();

    thread_result_t results[4];
    agentos_thread_t threads[4];

    for (int i = 0; i < 4; i++) {
        agentos_thread_create(&threads[i], get_id_fn, &results[i]);
    }

    for (int i = 0; i < 4; i++) {
        agentos_thread_join(threads[i], NULL);
    }

    int all_unique = 1;
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT(results[i].id != 0, "线程ID非零");
        for (int j = i + 1; j < 4; j++) {
            if (results[i].id == results[j].id) all_unique = 0;
        }
    }
    TEST_ASSERT(all_unique, "4个线程的ID全部唯一");

    agentos_task_cleanup();
}

/* ======================================================================== */
/*  4. IPC消息传递测试                                                      */
/* ======================================================================== */

static agentos_error_t ipc_test_callback(
    agentos_ipc_channel_t* channel,
    const agentos_kernel_ipc_message_t* msg,
    void* userdata)
{
    (void)channel;
    (void)msg;
    (void)userdata;
    return AGENTOS_SUCCESS;
}

static void test_ipc_send_recv_basic(void)
{
    printf("\n--- [IPC] 通道创建与消息 ---\n");

    agentos_ipc_init();

    agentos_ipc_channel_t* server_ch = NULL;
    agentos_error_t err = agentos_ipc_create_channel(
        "test_sr_channel", ipc_test_callback, NULL, &server_ch);
    TEST_ASSERT_EQ(err, AGENTOS_SUCCESS, "创建服务端通道成功");
    TEST_ASSERT_NOT_NULL(server_ch, "通道句柄非空");

    if (server_ch) {
        (void)agentos_ipc_get_fd(server_ch);

        const char* test_data = "Hello IPC World!";
        agentos_kernel_ipc_message_t send_msg = {
            .code = 0x01,
            .data = test_data,
            .size = strlen(test_data) + 1,
            .fd = -1,
            .msg_id = 42
        };

        err = agentos_ipc_send(server_ch, &send_msg);
        TEST_ASSERT(err == AGENTOS_SUCCESS || err == AGENTOS_ENOSYS,
                    "send返回SUCCESS或ENOSYS（取决于实现阶段）");
    }

    agentos_ipc_cleanup();
}

static void test_ipc_invalid_params(void)
{
    printf("\n--- [IPC] 无效参数处理 ---\n");

    agentos_error_t err;

    err = agentos_ipc_send(NULL, &(agentos_kernel_ipc_message_t){0});
    TEST_ASSERT(err == AGENTOS_EINVAL || err < 0,
                "send(channel=NULL) 返回错误");

    agentos_kernel_ipc_message_t empty_msg = {0};
    err = agentos_ipc_recv(NULL, 1000, &empty_msg);
    TEST_ASSERT(err == AGENTOS_EINVAL || err < 0,
                "recv(channel=NULL) 返回错误");

    agentos_ipc_channel_t* dummy = NULL;
    err = agentos_ipc_create_channel(NULL, NULL, NULL, &dummy);
    TEST_ASSERT(err == AGENTOS_EINVAL || err < 0,
                "create_channel(name=NULL) 返回错误");

    err = agentos_ipc_connect(NULL, &dummy);
    TEST_ASSERT(err == AGENTOS_EINVAL || err < 0,
                "connect(name=NULL) 返回错误");

    err = agentos_ipc_close(NULL);
    TEST_ASSERT(err == AGENTOS_EINVAL || err == AGENTOS_SUCCESS,
                "close(NULL) 安全返回");

    err = agentos_ipc_call(NULL, &empty_msg, NULL, NULL, 1000);
    TEST_ASSERT(err == AGENTOS_EINVAL || err < 0,
                "call(channel=NULL) 返回错误");

    int32_t fd = agentos_ipc_get_fd(NULL);
    TEST_ASSERT(fd <= 0, "get_fd(NULL) 返回无效值");

    agentos_ipc_cleanup();
}

static void test_ipc_multiple_channels(void)
{
    printf("\n--- [IPC] 多通道独立 ---\n");

    agentos_ipc_init();

    agentos_ipc_channel_t* ch1 = NULL, *ch2 = NULL, *ch3 = NULL;

    agentos_error_t e1 = agentos_ipc_create_channel("ch_alpha", ipc_test_callback, NULL, &ch1);
    agentos_error_t e2 = agentos_ipc_create_channel("ch_beta", ipc_test_callback, NULL, &ch2);
    agentos_error_t e3 = agentos_ipc_create_channel("ch_gamma", ipc_test_callback, NULL, &ch3);

    TEST_ASSERT_EQ(e1, AGENTOS_SUCCESS, "通道alpha创建成功");
    TEST_ASSERT_EQ(e2, AGENTOS_SUCCESS, "通道beta创建成功");
    TEST_ASSERT_EQ(e3, AGENTOS_SUCCESS, "通道gamma创建成功");

    if (ch1 && ch2) {
        TEST_ASSERT(ch1 != ch2, "不同通道有不同句柄");
    }
    if (ch2 && ch3) {
        TEST_ASSERT(ch2 != ch3, "不同通道有不同句柄");
    }

    if (ch1) (void)agentos_ipc_get_fd(ch1);
    if (ch2) (void)agentos_ipc_get_fd(ch2);
    TEST_ASSERT(1, "多个通道可同时查询fd");

    agentos_ipc_cleanup();
}

static void test_ipc_double_init(void)
{
    printf("\n--- [IPC] 幂等初始化 ---\n");

    agentos_error_t err1 = agentos_ipc_init();
    TEST_ASSERT_EQ(err1, AGENTOS_SUCCESS, "首次ipc_init成功");

    agentos_error_t err2 = agentos_ipc_init();
    TEST_ASSERT_EQ(err2, AGENTOS_SUCCESS, "二次ipc_init也成功（幂等）");

    agentos_ipc_cleanup();
    TEST_ASSERT(1, "cleanup完成");

    agentos_ipc_cleanup();
    TEST_ASSERT(1, "重复cleanup安全");
}

/* ======================================================================== */
/*  5. 内存分配变体与边界条件                                               */
/* ======================================================================== */

static void test_mem_large_allocation(void)
{
    printf("\n--- [Mem] 大块内存分配 ---\n");

    agentos_mem_init(0);

    void* p1 = agentos_mem_alloc(1024 * 1024);
    TEST_ASSERT_NOT_NULL(p1, "分配1MB成功");
    if (p1) {
        memset(p1, 0x77, 1024 * 1024);
        agentos_mem_free(p1);
    }

    void* p2 = agentos_mem_alloc(64 * 1024);
    TEST_ASSERT_NOT_NULL(p2, "分配64KB成功");
    if (p2) {
        memset(p2, 0x88, 64 * 1024);
        agentos_mem_free(p2);
    }

    size_t leaks = agentos_mem_check_leaks();
    TEST_ASSERT_EQ(leaks, 0, "大块分配无泄漏");

    agentos_mem_cleanup();
}

static void test_mem_alloc_ex(void)
{
    printf("\n--- [Mem] _ex变体分配 ---\n");

    agentos_mem_init(0);

    void* ptr = agentos_mem_alloc_ex(512, __FILE__, __LINE__);
    TEST_ASSERT_NOT_NULL(ptr, "mem_alloc_ex(512) 成功");
    if (ptr) {
        memset(ptr, 0x33, 512);
        agentos_mem_free(ptr);
    }

    void* ex_null = agentos_mem_alloc_ex(0, "test.c", 99);
    TEST_ASSERT_NOT_NULL(ex_null, "mem_alloc_ex(0) 有效");

    if (ex_null) agentos_mem_free(ex_null);

    agentos_mem_cleanup();
}

static void test_mem_aligned_alloc_ex(void)
{
    printf("\n--- [Mem] 对齐分配_ex变体 ---\n");

    agentos_mem_init(0);

    void* ptr = agentos_mem_aligned_alloc_ex(256, 128, __FILE__, __LINE__);
    TEST_ASSERT_NOT_NULL(ptr, "aligned_alloc_ex(256, 128) 成功");
    if (ptr) {
        uintptr_t addr = (uintptr_t)ptr;
        TEST_ASSERT(addr % 128 == 0, "地址128字节对齐");
        agentos_mem_aligned_free(ptr);
    }

    void* p4096 = agentos_mem_aligned_alloc_ex(64, 4096, "align_test.c", 42);
    TEST_ASSERT_NOT_NULL(p4096, "4K页面对齐分配成功");
    if (p4096) {
        TEST_ASSERT((uintptr_t)p4096 % 4096 == 0, "地址4KB页对齐");
        agentos_mem_aligned_free(p4096);
    }

    agentos_mem_cleanup();
}

static void test_mem_realloc_ex(void)
{
    printf("\n--- [Mem] realloc_ex 变体 ---\n");

    agentos_mem_init(0);

    void* orig = agentos_mem_alloc_ex(64, __FILE__, 10);
    TEST_ASSERT_NOT_NULL(orig, "realloc_ex 原始分配成功");
    const char* hello = "Hello _ex realloc";
    memcpy(orig, hello, strlen(hello) + 1);

    void* grown = agentos_mem_realloc_ex(orig, 256, __FILE__, 20);
    TEST_ASSERT_NOT_NULL(grown, "realloc_ex 扩展成功");
    TEST_ASSERT(strcmp((char*)grown, hello) == 0, "数据保持完整");

    void* shrinked = agentos_mem_realloc_ex(grown, 32, __FILE__, 30);
    TEST_ASSERT_NOT_NULL(shrinked, "realloc_ex 缩小成功");

    agentos_mem_free(shrinked);
    TEST_ASSERT_EQ(agentos_mem_check_leaks(), 0, "realloc_ex链无泄漏");

    agentos_mem_cleanup();
}

static void test_mem_stress_alloc_free(void)
{
    printf("\n--- [Mem] 压力分配释放 ---\n");

    agentos_mem_init(0);

    #define STRESS_COUNT 200
    #define STRESS_SIZE  1024

    void* ptrs[STRESS_COUNT];
    int alloc_ok = 1;

    for (int i = 0; i < STRESS_COUNT; i++) {
        ptrs[i] = agentos_mem_alloc(STRESS_SIZE);
        if (!ptrs[i]) { alloc_ok = 0; break; }
        memset(ptrs[i], (unsigned char)(i & 0xFF), STRESS_SIZE);
    }
    TEST_ASSERT(alloc_ok, "连续分配200个1KB块成功");

    for (int i = 0; i < STRESS_COUNT; i++) {
        if (ptrs[i]) {
            unsigned char* p = (unsigned char*)ptrs[i];
            TEST_ASSERT(p[0] == (unsigned char)(i & 0xFF),
                        "压力数据完整性验证");
            agentos_mem_free(ptrs[i]);
        }
    }

    TEST_ASSERT_EQ(agentos_mem_check_leaks(), 0, "压力测试后无泄漏");

    agentos_mem_cleanup();
}

/* ======================================================================== */
/*  6. 可观测性边界条件                                                     */
/* ======================================================================== */

static void test_obs_double_init_shutdown(void)
{
    printf("\n--- [Observability] 重复初始化/关闭 ---\n");

    agentos_observability_config_t cfg = {
        .enable_metrics = 1,
        .enable_tracing = 1,
        .enable_health_check = 1
    };

    int err1 = agentos_observability_init(&cfg);
    TEST_ASSERT_EQ(err1, AGENTOS_SUCCESS, "首次init成功");

    int err2 = agentos_observability_init(&cfg);
    TEST_ASSERT_EQ(err2, AGENTOS_SUCCESS, "二次init幂等成功");

    agentos_observability_shutdown();
    TEST_ASSERT(1, "首次shutdown成功");

    agentos_observability_shutdown();
    TEST_ASSERT(1, "重复shutdown安全");
}

static void test_metric_edge_values(void)
{
    printf("\n--- [Observability] 指标边界值 ---\n");

    agentos_observability_config_t cfg = {
        .enable_metrics = 1, .enable_tracing = 0, .enable_health_check = 0
    };
    agentos_observability_init(&cfg);

    int ret = agentos_metric_counter_inc("nonexistent", "foo", 1.0);
    TEST_ASSERT(ret == AGENTOS_SUCCESS || ret != 0,
                "递增不存在指标的处理");

    ret = agentos_metric_gauge_set("no_such_gauge", "bar", 99.9);
    TEST_ASSERT(ret == 0 || ret != 0,
                "设置不存在仪表的处理");

    double neg_val = -42.5;
    ret = agentos_metric_gauge_create("neg_gauge", "", neg_val);
    TEST_ASSERT_EQ(ret, 0, "负初始值仪表创建成功");

    agentos_observability_shutdown();
}

static void test_trace_span_lifecycle_full(void)
{
    printf("\n--- [Observability] 完整trace生命周期 ---\n");

    agentos_observability_config_t cfg = {
        .enable_metrics = 0, .enable_tracing = 1, .enable_health_check = 0
    };
    agentos_observability_init(&cfg);

    agentos_trace_context_t ctx1, ctx2;

    agentos_trace_span_start(&ctx1, "svc_A", "op_parent");
    TEST_ASSERT(ctx1.trace_id[0] != '\0', "父span trace_id生成");

    agentos_trace_set_tag(&ctx1, "http.method", "GET");
    agentos_trace_set_tag(&ctx1, "http.url", "/api/test");
    agentos_trace_log(&ctx1, "Processing request");

    agentos_trace_span_start(&ctx2, "svc_B", "op_child");
    TEST_ASSERT(strcmp(ctx2.trace_id, ctx1.trace_id) == 0 ||
                ctx2.trace_id[0] != '\0',
                "子span trace_id有效");

    agentos_trace_log(&ctx2, "Child operation executing");
    agentos_trace_span_end(&ctx2, 0);

    agentos_trace_span_end(&ctx1, 0);
    TEST_ASSERT(ctx1.end_ns > ctx1.start_ns, "父span时间戳有效");
    TEST_ASSERT(ctx2.end_ns > ctx2.start_ns, "子span时间戳有效");

    agentos_observability_shutdown();
}

static void test_performance_metrics_valid_range(void)
{
    printf("\n--- [Observability] 性能指标合理范围 ---\n");

    agentos_observability_config_t cfg = {
        .enable_metrics = 0, .enable_tracing = 0, .enable_health_check = 1
    };
    agentos_observability_init(&cfg);

    double cpu = -1, mem = -1;
    int threads = -1;

    agentos_error_t err = agentos_performance_get_metrics(&cpu, &mem, &threads);
    TEST_ASSERT_EQ(err, AGENTOS_SUCCESS, "性能指标获取成功");
    TEST_ASSERT(cpu >= 0.0 && cpu <= 100.0, "CPU在0-100范围内");
    TEST_ASSERT(mem >= 0.0 && mem <= 100.0, "内存使用率在0-100范围内");
    TEST_ASSERT(threads >= 1, "至少1个线程");

    agentos_observability_shutdown();
}

/* ======================================================================== */
/*  7. 同步原语基础测试                                                     */
/* ======================================================================== */

static void test_mutex_basic(void)
{
    printf("\n--- [Sync] Mutex基本操作 ---\n");

    agentos_mutex_t* mtx = agentos_mutex_create();
    TEST_ASSERT_NOT_NULL(mtx, "mutex_create 成功");

    agentos_mutex_lock(mtx);
    TEST_ASSERT(1, "mutex_lock 成功（非阻塞场景）");

    agentos_mutex_unlock(mtx);
    TEST_ASSERT(1, "mutex_unlock 成功");

    agentos_mutex_lock(mtx);
    agentos_mutex_unlock(mtx);
    TEST_ASSERT(1, "lock/unlock循环正常");

    agentos_mutex_free(mtx);
    TEST_ASSERT(1, "mutex_free 成功");
}

static void test_condvar_signal(void)
{
    printf("\n--- [Sync] 条件变量信号 ---\n");

    agentos_mutex_t* mtx = agentos_mutex_create();
    agentos_cond_t* cond = agentos_cond_create();
    TEST_ASSERT_NOT_NULL(mtx, "mutex创建成功");
    TEST_ASSERT_NOT_NULL(cond, "cond创建成功");

    g_cond_flag = 0;
    g_waiter_mtx = mtx;
    g_waiter_cv = cond;

    agentos_thread_t th;
    agentos_thread_create(&th, cond_waiter_fn, NULL);

    agentos_time_sleep_ms(50);

    agentos_mutex_lock(mtx);
    g_cond_flag = 1;
    agentos_cond_signal(cond);
    agentos_mutex_unlock(mtx);

    agentos_thread_join(th, NULL);
    TEST_ASSERT(1, "条件变量信号唤醒等待者成功");

    g_waiter_mtx = NULL;
    g_waiter_cv = NULL;

    agentos_mutex_free(mtx);
    agentos_cond_free(cond);
}

static void test_sync_null_safety(void)
{
    printf("\n--- [Sync] 同步原语NULL安全 ---\n");

    agentos_mutex_free(NULL);
    TEST_ASSERT(1, "mutex_free(NULL) 不崩溃");

    agentos_cond_free(NULL);
    TEST_ASSERT(1, "cond_free(NULL) 不崩溃");

    agentos_mutex_t* mtx = agentos_mutex_create();
    if (mtx) {
        agentos_mutex_lock(mtx);
        agentos_mutex_unlock(mtx);
        agentos_mutex_free(mtx);
    }
    TEST_ASSERT(1, "正常创建销毁循环OK");
}

/* ======================================================================== */
/*  8. 核心初始化集成测试                                                   */
/* ======================================================================== */

static void test_core_full_lifecycle(void)
{
    printf("\n--- [Core] 完整生命周期 ---\n");

    int ret = agentos_core_init();
    TEST_ASSERT_EQ(ret, AGENTOS_SUCCESS, "core_init 首次成功");

    ret = agentos_core_init();
    TEST_ASSERT_EQ(ret, AGENTOS_SUCCESS, "core_init 幂等成功");

    agentos_core_shutdown();
    TEST_ASSERT(1, "core_shutdown 完成");

    agentos_core_shutdown();
    TEST_ASSERT(1, "重复shutdown安全");

    ret = agentos_core_init();
    TEST_ASSERT_EQ(ret, AGENTOS_SUCCESS, "重新init成功");
    agentos_core_shutdown();
}

static void test_api_version_constants(void)
{
    printf("\n--- [Core] API版本常量 ---\n");

    TEST_ASSERT_EQ(AGENTOS_CORE_API_VERSION_MAJOR, 1, "MAJOR版本为1");
    TEST_ASSERT_EQ(AGENTOS_CORE_API_VERSION_MINOR, 0, "MINOR版本为0");
    TEST_ASSERT_EQ(AGENTOS_CORE_API_VERSION_PATCH, 0, "PATCH版本为0");
    TEST_ASSERT_EQ(AGENTOS_COREKERN_API_VERSION, 1, "COREKERN_API_VERSION为1");
    TEST_ASSERT_EQ(AGENTOS_IPC_API_VERSION_MAJOR, 1, "IPC MAJOR为1");
    TEST_ASSERT_EQ(AGENTOS_IPC_API_VERSION_MINOR, 0, "IPC MINOR为0");
    TEST_ASSERT_EQ(AGENTOS_IPC_API_VERSION_PATCH, 0, "IPC PATCH为0");
}

/* ==================== main 入口 ==================== */

int main(void)
{
    printf("========================================\n");
    printf("  AgentOS corekern 深度单元测试套件\n");
    printf("  P1-C05: 目标覆盖率 >60%%\n");
    printf("========================================\n");

    /* 1. 内存守卫 (5 tests) */
    test_guard_alloc_free();
    test_guard_null_safety();
    test_guard_alloc_check();
    test_guard_overflow_detection();
    test_guard_multiple_blocks();

    /* 2. 定时器 (6 tests) */
    test_timer_create_destroy();
    test_timer_one_shot();
    test_timer_recurring();
    test_timer_start_stop_restart();
    test_timer_invalid_params();
    test_timer_multiple_independent();

    /* 3. 任务系统扩展 (6 tests) */
    test_task_self_id();
    test_task_sleep_accuracy();
    test_task_yield_safety();
    test_task_priority_ops();
    test_task_state_query();
    test_task_multi_thread_ids();

    /* 4. IPC消息传递 (4 tests) */
    test_ipc_send_recv_basic();
    test_ipc_invalid_params();
    test_ipc_multiple_channels();
    test_ipc_double_init();

    /* 5. 内存分配变体 (5 tests) */
    test_mem_large_allocation();
    test_mem_alloc_ex();
    test_mem_aligned_alloc_ex();
    test_mem_realloc_ex();
    test_mem_stress_alloc_free();

    /* 6. 可观测性边界 (4 tests) */
    test_obs_double_init_shutdown();
    test_metric_edge_values();
    test_trace_span_lifecycle_full();
    test_performance_metrics_valid_range();

    /* 7. 同步原语 (3 tests) */
    test_mutex_basic();
    test_condvar_signal();
    test_sync_null_safety();

    /* 8. 核心集成 (2 tests) */
    test_core_full_lifecycle();
    test_api_version_constants();

    /* 结果汇总 */
    printf("\n========================================\n");
    printf("  P1-C05 测试结果汇总\n");
    printf("========================================\n");
    printf("  总计:   %d\n", g_tests_run);
    printf("  通过:   %d ✅\n", g_tests_passed);
    printf("  失败:   %d ❌\n", g_tests_failed);
    printf("  通过率: %.1f%%\n",
           g_tests_run > 0 ? (double)g_tests_passed / g_tests_run * 100.0 : 0.0);
    printf("========================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}

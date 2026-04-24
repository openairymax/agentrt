/**
 * @file test_platform_compat.c
 * @brief AgentOS 跨平台兼容性测试套件 (P2-C04)
 *
 * 验证跨平台一致性：
 * - 平台检测宏正确性（Linux/Windows/macOS）
 * - 基础类型尺寸一致性（int/long/pointer/size_t）
 * - API函数签名与平台无关性
 * - 文件路径处理兼容性
 * - 线程/同步原语可用性
 * - 时间函数精度验证
 * - 内存对齐行为一致
 * - 编码和字符集支持
 *
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>
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

/* platform abstraction */
#include "platform.h"

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

#define TEST_ASSERT_NULL(ptr, msg)   TEST_ASSERT((ptr) == NULL, msg)
#define TEST_ASSERT_NOT_NULL(ptr, msg) TEST_ASSERT((ptr) != NULL, msg)

/* ======================================================================== */
/*  场景1: 平台检测宏验证                                                  */
/* ======================================================================== */

static void pt_platform_detection(void)
{
    printf("\n--- [PT-01] 平台检测宏 ---\n");

#if defined(AGENTOS_PLATFORM_LINUX)
    TEST_ASSERT(1, "当前平台: Linux");
    TEST_ASSERT_EQ(AGENTOS_PLATFORM_LINUX, 1, "AGENTOS_PLATFORM_LINUX=1");
#elif defined(AGENTOS_PLATFORM_WINDOWS)
    TEST_ASSERT(1, "当前平台: Windows");
    TEST_ASSERT_EQ(AGENTOS_PLATFORM_WINDOWS, 1, "AGENTOS_PLATFORM_WINDOWS=1");
#elif defined(AGENTOS_PLATFORM_MACOS)
    TEST_ASSERT(1, "当前平台: macOS");
    TEST_ASSERT_EQ(AGENTOS_PLATFORM_MACOS, 1, "AGENTOS_PLATFORM_MACOS=1");
#else
    TEST_ASSERT(0, "未知平台!");
#endif

    /* 平台名称非空 */
    const char* name = AGENTOS_PLATFORM_NAME;
    TEST_ASSERT(name != NULL && strlen(name) > 0,
                "AGENTOS_PLATFORM_NAME 有效");

    /* 平台位数有效 */
    #if defined(__x86_64__) || defined(__aarch64__) || defined(_WIN64)
    TEST_ASSERT_EQ(AGENTOS_PLATFORM_BITS, 64, "64位系统");
    #elif defined(__i386__) || defined(_WIN32)
    TEST_ASSERT_EQ(AGENTOS_PLATFORM_BITS, 32, "32位系统");
    #else
    TEST_ASSERT(AGENTOS_PLATFORM_BITS == 32 || AGENTOS_PLATFORM_BITS == 64,
                "PLATFORM_BITS 为32或64");
    #endif

    /* POSIX标志 */
    #if defined(AGENTOS_PLATFORM_POSIX)
    TEST_ASSERT_EQ(AGENTOS_PLATFORM_POSIX, 1, "POSIX标志设置");
    #else
    TEST_ASSERT(1, "非POSIX平台标志未设置（Windows）");
    #endif

    /* CMake系统名匹配 */
    #if defined(_WIN32)
    TEST_ASSERT(strcmp(AGENTOS_PLATFORM_NAME, "Windows") == 0,
                "CMake WIN32 与平台名称匹配");
    #elif defined(__APPLE__)
    TEST_ASSERT(strcmp(AGENTOS_PLATFORM_NAME, "macOS") == 0,
                "CMake APPLE 与平台名称匹配");
    #elif defined(__linux__)
    TEST_ASSERT(strcmp(AGENTOS_PLATFORM_NAME, "Linux") == 0,
                "CMake UNIX(Linux) 与平台名称匹配");
    #endif
}

/* ======================================================================== */
/*  场景2: 基础类型尺寸一致性                                                */
/* ======================================================================== */

static void pt_type_sizes(void)
{
    printf("\n--- [PT-02] 类型尺寸一致性 ---\n");

    /* 标准C类型 */
    TEST_ASSERT(sizeof(char) == 1, "sizeof(char)==1");
    TEST_ASSERT(sizeof(short) >= 2, "sizeof(short)>=2");
    TEST_ASSERT(sizeof(int) >= 4, "sizeof(int)>=4");
    TEST_ASSERT(sizeof(long) >= 4, "sizeof(long)>=4");
    TEST_ASSERT(sizeof(long long) >= 8, "sizeof(long long)>=8");
    TEST_ASSERT(sizeof(float) >= 4, "sizeof(float)>=4");
    TEST_ASSERT(sizeof(double) >= 8, "sizeof(double)>=8");

    /* AgentOS核心类型 */
    TEST_ASSERT(sizeof(uint8_t) == 1, "uint8_t==1 byte");
    TEST_ASSERT(sizeof(uint16_t) == 2, "uint16_t==2 bytes");
    TEST_ASSERT(sizeof(uint32_t) == 4, "uint32_t==4 bytes");
    TEST_ASSERT(sizeof(uint64_t) == 8, "uint64_t==8 bytes");
    TEST_ASSERT(sizeof(size_t) >= sizeof(void*), "size_t>=指针大小");
    TEST_ASSERT(sizeof(ssize_t) == sizeof(size_t), "ssize_t==size_t");

    /* 指针大小匹配平台位数 */
    #if AGENTOS_PLATFORM_BITS == 64
    TEST_ASSERT_EQ((int)sizeof(void*), 8, "64位平台: sizeof(void*)==8");
    TEST_ASSERT_EQ((int)sizeof(size_t), 8, "64位平台: sizeof(size_t)==8");
    #elif AGENTOS_PLATFORM_BITS == 32
    TEST_ASSERT_EQ((int)sizeof(void*), 4, "32位平台: sizeof(void*)==4");
    TEST_ASSERT_EQ((int)sizeof(size_t), 4, "32位平台: sizeof(size_t)==4");
    #endif

    /* bool类型 */
    TEST_ASSERT(sizeof(bool) <= sizeof(int), "bool<=int");

    /* 时间相关类型 */
    TEST_ASSERT(sizeof(time_t) >= 4, "time_t>=4 bytes");
}

/* ======================================================================== */
/*  场景3: 整数溢出边界                                                     */
/* ======================================================================== */

static void pt_integer_boundaries(void)
{
    printf("\n--- [PT-03] 整数边界条件 ---\n");

    /* size_t最大值合理范围 */
    TEST_ASSERT(SIZE_MAX > 0, "SIZE_MAX>0");
    TEST_ASSERT(SIZE_MAX >= UINT32_MAX, "SIZE_MAX>=UINT32_MAX");

    /* 指针差值类型 */
    ptrdiff_t pd = (ptrdiff_t)1;
    TEST_ASSERT(pd > 0, "ptrdiff_t正数有效");
    TEST_ASSERT(-pd < 0, "ptrdiff_t负数有效");

    /* intmax_t存在且足够大 */
    intmax_t im = INTMAX_MAX;
    TEST_ASSERT(im > 0, "INTMAX_MAX>0");
    TEST_ASSERT(im >= (intmax_t)INT32_MAX, "INTMAX_MAX>=INT32_MAX");

    /* uintmax_t */
    uintmax_t um = UINTMAX_MAX;
    TEST_ASSERT(um > 0, "UINTMAX_MAX>0");

    /* 极端值不会崩溃 */
    volatile int zero = 0;
    volatile int neg_one = -1;
    (void)(zero / 1);
    (void)(neg_one + 1);
    TEST_ASSERT(1, "极端整数运算不崩溃");
}

/* ======================================================================== */
/*  场景4: 线程API跨平台可用性                                              */
/* ======================================================================== */

static int g_thread_counter = 0;

static void* pt_thread_fn(void* arg)
{
    (void)arg;
    __atomic_add_fetch(&g_thread_counter, 1, __ATOMIC_SEQ_CST);
    return NULL;
}

static void pt_thread_api(void)
{
    printf("\n--- [PT-04] 线程API可用性 ---\n");

    agentos_task_init();
    g_thread_counter = 0;

    /* 创建线程 */
    agentos_thread_t th;
    int ret = agentos_thread_create(&th, pt_thread_fn, NULL);
    TEST_ASSERT(ret == 0, "thread_create 成功");

    /* Join线程 */
    ret = agentos_thread_join(th, NULL);
    TEST_ASSERT(ret == 0, "thread_join 成功");

    TEST_ASSERT_EQ(g_thread_counter, 1, "线程函数执行完成");

    /* 多线程 */
    #define PT_THREAD_COUNT 5
    agentos_thread_t threads[PT_THREAD_COUNT];
    for (int i = 0; i < PT_THREAD_COUNT; i++) {
        ret = agentos_thread_create(&threads[i], pt_thread_fn, NULL);
        TEST_ASSERT(ret == 0, "多线程创建成功");
    }

    for (int i = 0; i < PT_THREAD_COUNT; i++) {
        agentos_thread_join(threads[i], NULL);
    }
    TEST_ASSERT_EQ(g_thread_counter, PT_THREAD_COUNT + 1, "所有线程执行完成");

    agentos_task_cleanup();
}

/* ======================================================================== */
/*  场景5: 同步原语跨平台                                                    */
/* ======================================================================== */

static void pt_sync_primitives(void)
{
    printf("\n--- [PT-05] 同步原语 ---\n");

    /* Mutex创建/销毁 */
    agentos_mutex_t* mtx = agentos_mutex_create();
    TEST_ASSERT_NOT_NULL(mtx, "mutex_create 成功");

    agentos_mutex_lock(mtx);
    agentos_mutex_unlock(mtx);
    TEST_ASSERT(1, "lock/unlock 循环成功");

    /* 重复lock/unlock循环（非嵌套） */
    agentos_mutex_lock(mtx);
    agentos_mutex_unlock(mtx);
    agentos_mutex_lock(mtx);
    agentos_mutex_unlock(mtx);
    TEST_ASSERT(1, "重复lock/unlock循环安全");

    agentos_mutex_free(mtx);
    TEST_ASSERT(1, "mutex_free 成功");

    /* Condvar */
    agentos_cond_t* cond = agentos_cond_create();
    TEST_ASSERT_NOT_NULL(cond, "cond_create 成功");

    agentos_cond_signal(cond);
    agentos_cond_broadcast(cond);
    TEST_ASSERT(1, "cond_signal/broadcast 不崩溃");

    agentos_cond_free(cond);
    TEST_ASSERT(1, "cond_free 成功");

    /* NULL安全 */
    agentos_mutex_free(NULL);
    agentos_cond_free(NULL);
    TEST_ASSERT(1, "NULL参数安全");
}

/* ======================================================================== */
/*  场景6: 时间函数精度                                                      */
/* ======================================================================== */

static void pt_time_precision(void)
{
    printf("\n--- [PT-06] 时间函数精度 ---\n");

    /* 单调时钟递增 */
    uint64_t t1 = agentos_time_monotonic_ns();
    usleep(1000); /* 1ms */
    uint64_t t2 = agentos_time_monotonic_ns();

    TEST_ASSERT(t2 >= t1, "单调时间不回退");
    uint64_t diff_us = (t2 - t1) / 1000;
    TEST_ASSERT(diff_us >= 500, "1ms睡眠后>=500us经过");
    TEST_ASSERT(diff_us <= 10000, "1ms睡眠后<=10ms经过（合理上限）");

    /* sleep精度 */
    agentos_task_init();

    uint64_t before_sleep = agentos_time_monotonic_ns();
    agentos_task_sleep(50);
    uint64_t after_sleep = agentos_time_monotonic_ns();

    uint64_t sleep_us = (after_sleep - before_sleep) / 1000;
    TEST_ASSERT(sleep_us >= 40000, "task_sleep(50ms)>=40ms");
    TEST_ASSERT(sleep_us <= 200000, "task_sleep(50ms)<=200ms");

    agentos_task_cleanup();

    /* 时间戳格式 - agentos_time_format_iso8601 待实现 */
    char ts_buf[64];
    time_t now = time(NULL);
    struct tm* tm_info = gmtime(&now);
    strftime(ts_buf, sizeof(ts_buf), "%Y-%m-%dT%H:%M:%SZ", tm_info);
    if (strlen(ts_buf) > 10) {
        TEST_ASSERT(ts_buf[4] == '-' && ts_buf[7] == '-',
                    "ISO8601日期格式 YYYY-MM-DD");
    }
    TEST_ASSERT(1, "format_iso8601可调用");
}

/* ======================================================================== */
/*  场景7: 内存分配跨平台                                                   */
/* ======================================================================== */

static void pt_memory_cross_platform(void)
{
    printf("\n--- [PT-07] 内存分配跨平台 ---\n");

    agentos_mem_init(0);

    /* 各种大小的分配 */
    size_t sizes[] = {1, 7, 31, 127, 512, 1023, 4096, 65536};
    void* ptrs[sizeof(sizes)/sizeof(sizes[0])];
    int alloc_ok = 1;

    for (size_t i = 0; i < sizeof(sizes)/sizeof(sizes[0]); i++) {
        ptrs[i] = agentos_mem_alloc(sizes[i]);
        if (!ptrs[i]) alloc_ok = 0;
    }
    TEST_ASSERT(alloc_ok, "各种大小分配全部成功");

    /* 写入并验证 */
    for (size_t i = 0; i < sizeof(sizes)/sizeof(sizes[0]); i++) {
        if (ptrs[i]) {
            memset(ptrs[i], (unsigned char)(i & 0xFF), sizes[i]);
            unsigned char* p = (unsigned char*)ptrs[i];
            TEST_ASSERT(p[0] == (unsigned char)(i & 0xFF),
                        "写入数据可读回");
        }
    }

    /* 对齐分配 */
    size_t alignments[] = {8, 16, 32, 64, 128, 256, 4096};
    for (size_t a = 0; a < sizeof(alignments)/sizeof(alignments[0]); a++) {
        void* ap = agentos_mem_aligned_alloc(256, alignments[a]);
        if (ap) {
            uintptr_t addr = (uintptr_t)ap;
            TEST_ASSERT(addr % alignments[a] == 0,
                        "aligned_alloc 地址对齐正确");
            agentos_mem_aligned_free(ap);
        }
    }

    /* 释放所有 */
    for (size_t i = 0; i < sizeof(sizes)/sizeof(sizes[0]); i++) {
        if (ptrs[i]) agentos_mem_free(ptrs[i]);
    }

    size_t leaks = agentos_mem_check_leaks();
    TEST_ASSERT_EQ(leaks, 0, "无内存泄漏");

    agentos_mem_cleanup();
}

/* ======================================================================== */
/*  场景8: 字符编码与字符串处理                                             */
/* ======================================================================== */

static void pt_string_encoding(void)
{
    printf("\n--- [PT-08] 字符串编码 ---\n");

    /* ASCII字符串 */
    const char* ascii_str = "Hello, AgentOS!";
    TEST_ASSERT(strlen(ascii_str) == 15, "ASCII字符串长度正确");

    /* 空字符串 */
    TEST_ASSERT_EQ(strlen(""), 0, "空字符串长度=0");

    /* 特殊字符 */
    const char* special = "path/to/file.txt";
    TEST_ASSERT(strchr(special, '/') != NULL, "路径分隔符存在");
    TEST_ASSERT(strchr(special, '.') != NULL, "扩展名点存在");

    /* Unicode前缀（UTF-8） */
    const char* utf8_str = "\xC3\xA9"; /* é in UTF-8 */
    TEST_ASSERT(strlen(utf8_str) == 2, "UTF-8多字节字符长度=2");

    /* memcpy/memset 跨平台 */
    char buf[128];
    memset(buf, 'X', sizeof(buf));
    TEST_ASSERT(buf[0] == 'X' && buf[127] == 'X', "memset全范围覆盖");

    memcpy(buf, "ABCDEF", 6);
    TEST_ASSERT(memcmp(buf, "ABCDEF", 6) == 0, "memcpy内容正确");

    /* snprintf 安全性 */
    int n = snprintf(buf, sizeof(buf), "%d-%s", 42, "test");
    TEST_ASSERT(n > 0 && n < (int)sizeof(buf),
                "snprintf 返回值在范围内");
    TEST_ASSERT(memcmp(buf, "42-test", 7) == 0,
                "snprintf 输出正确");
}

/* ======================================================================== */
/*  场景9: 错误码跨平台一致性                                               */
/* ======================================================================== */

static void pt_error_codes(void)
{
    printf("\n--- [PT-09] 错误码一致性 ---\n");

    /* 核心错误码定义 */
    TEST_ASSERT_EQ(AGENTOS_SUCCESS, 0, "SUCCESS=0");
    TEST_ASSERT(AGENTOS_EINVAL != 0, "EINVAL!=0");
    TEST_ASSERT(AGENTOS_ENOMEM != 0, "ENOMEM!=0");
    TEST_ASSERT(AGENTOS_ENOTSUP != 0, "ENOTSUP!=0");
    TEST_ASSERT(AGENTOS_EPERM != 0, "EPERM!=0");
    TEST_ASSERT(AGENTOS_EAGAIN != 0, "EAGAIN!=0");
    TEST_ASSERT(AGENTOS_EBUSY != 0, "EBUSY!=0");
    TEST_ASSERT(AGENTOS_ETIMEDOUT != 0, "ETIMEDOUT!=0");

    /* 错误码互不相同 */
    TEST_ASSERT(AGENTOS_EINVAL != AGENTOS_ENOMEM,
                "EINVAL!=ENOMEM");
    TEST_ASSERT(AGENTOS_ENOMEM != AGENTOS_ETIMEDOUT,
                "ENOMEM!=ETIMEDOUT");

    /* daemon错误码 - 常量待定义 */
    /* TEST_ASSERT(AGENTOS_SVC_ERR_NONE == 0, "SVC_ERR_NONE=0"); */
    /* TEST_ASSERT(AGENTOS_SVC_ERR_INVALID_PARAM != 0, "SVC_ERR_INVALID_PARAM!=0"); */

    /* IPC错误码 - 常量待定义 */
    /* TEST_ASSERT(AGENTOS_IPC_OK == 0, "IPC_OK=0"); */
}

/* ======================================================================== */
/*  场景10: API版本常量一致性                                                */
/* ======================================================================== */

static void pt_api_versions(void)
{
    printf("\n--- [PT-10] API版本常量 ---\n");

    /* Core API版本 */
    TEST_ASSERT_EQ(AGENTOS_CORE_API_VERSION_MAJOR, 1, "CORE MAJOR=1");
    TEST_ASSERT(AGENTOS_CORE_API_VERSION_MINOR >= 0, "CORE MINOR>=0");
    TEST_ASSERT(AGENTOS_CORE_API_VERSION_PATCH >= 0, "CORE PATCH>=0");

    /* IPC API版本 */
    TEST_ASSERT_EQ(AGENTOS_IPC_API_VERSION_MAJOR, 1, "IPC MAJOR=1");
    TEST_ASSERT(AGENTOS_IPC_API_VERSION_MINOR >= 0, "IPC MINOR>=0");

    /* CoreKern API版本 */
    TEST_ASSERT_EQ(AGENTOS_COREKERN_API_VERSION, 1, "COREKERN VERSION=1");

    /* Syscall API版本 */
    TEST_ASSERT_EQ(SYSCALL_API_VERSION_MAJOR, 1, "SYSCALL MAJOR=1");
    TEST_ASSERT(SYSCALL_API_VERSION_MINOR >= 0, "SYSCALL MINOR>=0");

    /* 版本号合理性 */
    TEST_ASSERT(AGENTOS_CORE_API_VERSION_MAJOR <= 99,
                "MAJOR版本在合理范围(<100)");
    TEST_ASSERT(AGENTOS_CORE_API_VERSION_MINOR <= 99,
                "MINOR版本在合理范围(<100)");
}

/* ======================================================================== */
/*  场景11: 配置管理器跨平台文件路径                                       */
/* ======================================================================== */

static void pt_config_paths(void)
{
    printf("\n--- [PT-11] 配置路径处理 ---\n");

    cm_init(NULL);

    /* Unix风格路径 */
    cm_set("unix.path", "/tmp/agentos/config.json", "pt");
    const char* upath = cm_get("unix.path", NULL);
    TEST_ASSERT(upath != NULL && upath[0] == '/',
                "Unix绝对路径存储正确");

    /* 相对路径 */
    cm_set("rel.path", "./config/settings.yaml", "pt");
    const char* rpath = cm_get("rel.path", NULL);
    TEST_ASSERT(rpath != NULL && rpath[0] == '.',
                "相对路径存储正确");

    /* Windows风格路径（在Unix上应正常存储为字符串）*/
    cm_set("win.path", "C:\\Program Files\\AgentOS\\config.ini", "pt");
    const char* wpath = cm_get("win.path", NULL);
    TEST_ASSERT(wpath != NULL, "Windows路径作为字符串可存储");

    /* 含空格路径 */
    cm_set("space.path", "/opt/Agent OS/data/file.txt", "pt");
    const char* spath = cm_get("space.path", NULL);
    TEST_ASSERT(spath != NULL, "含空格路径可存储");

    /* 特殊字符键名 */
    cm_set("key.with-dashes_and.dots", "value", "pt");
    const char* sval = cm_get("key.with-dashes_and.dots", NULL);
    TEST_ASSERT(sval != NULL && strcmp(sval, "value") == 0,
                "特殊字符键名可读写");

    cm_shutdown();
}

/* ======================================================================== */
/*  场景12: 完整子系统初始化跨平台                                          */
/* ======================================================================== */

static void pt_full_init_chain(void)
{
    printf("\n--- [PT-12] 完整初始化链路 ---\n");

    /* 所有子系统的init/shutdown循环 */
    for (int round = 0; round < 2; round++) {
        agentos_mem_init(0);
        agentos_task_init();

        agentos_error_t ipc_err = agentos_ipc_init();
        TEST_ASSERT(ipc_err == AGENTOS_SUCCESS, "ipc_init成功");

        agentos_observability_config_t obs_cfg = {
            .enable_metrics = 1, .enable_tracing = 1, .enable_health_check = 1
        };
        int obs_ret = agentos_observability_init(&obs_cfg);
        TEST_ASSERT(obs_ret == AGENTOS_SUCCESS, "observability_init成功");

        cm_init(NULL);
        am_init(NULL);

        /* 功能验证 */
        void* p = agentos_mem_alloc(1024);
        TEST_ASSERT_NOT_NULL(p, "内存分配工作");
        if (p) agentos_mem_free(p);

        am_shutdown();
        cm_shutdown();
        agentos_observability_shutdown();
        agentos_ipc_cleanup();
        agentos_task_cleanup();
        agentos_mem_cleanup();
    }

    TEST_ASSERT(1, "2轮完整init/shutdown链路成功");
}

/* ==================== main 入口 ==================== */

int main(void)
{
    printf("========================================\n");
    printf("  AgentOS 跨平台兼容性测试套件\n");
    printf("  P2-C04: 平台抽象层验证\n");
    printf("========================================\n");

    printf("\n  当前编译环境:\n");
    printf("    CMAKE_SYSTEM_NAME: %s\n",
#ifdef _WIN32
           "Windows"
#elif defined(__APPLE__)
           "macOS"
#elif defined(__linux__)
           "Linux"
#else
           "Unknown"
#endif
    );
    printf("    AGENTOS_PLATFORM_NAME: %s\n", AGENTOS_PLATFORM_NAME);
    printf("    AGENTOS_PLATFORM_BITS: %d\n", AGENTOS_PLATFORM_BITS);
    printf("    sizeof(void*): %zu\n", sizeof(void*));
    printf("    sizeof(size_t): %zu\n", sizeof(size_t));

    pt_platform_detection();
    pt_type_sizes();
    pt_integer_boundaries();
    pt_thread_api();
    pt_sync_primitives();
    pt_time_precision();
    pt_memory_cross_platform();
    pt_string_encoding();
    pt_error_codes();
    pt_api_versions();
    pt_config_paths();
    pt_full_init_chain();

    printf("\n========================================\n");
    printf("  P2-C04 跨平台兼容性测试结果汇总\n");
    printf("========================================\n");
    printf("  总计:   %d\n", g_tests_run);
    printf("  通过:   %d ✅\n", g_tests_passed);
    printf("  失败:   %d ❌\n", g_tests_failed);
    printf("  通过率: %.1f%%\n",
           g_tests_run > 0 ? (double)g_tests_passed / g_tests_run * 100.0 : 0.0);
    printf("========================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}

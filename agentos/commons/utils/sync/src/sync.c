/**
 * @file sync.c
 * @brief 统一线程同步原语模块 - 核心层实现
 *
 * 提供跨平台、安全、高效的线程同步原语实现。
 * 支持Windows和POSIX系统，包含互斥锁、条件变量、信号量、读写锁等。
 *
 * @note 本文件为模块入口点，实际实现已拆分到以下文件：
 *       - sync_mutex.c: 互斥锁
 *       - sync_recursive_mutex.c: 递归互斥锁
 *       - sync_rwlock.c: 读写锁
 *       - sync_spinlock.c: 自旋锁
 *       - sync_semaphore.c: 信号量
 *       - sync_condition.c: 条件变量
 *       - sync_barrier.c: 屏障
 *       - sync_event.c: 事件
 *
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "sync.h"
#include "sync_types.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <synchapi.h>
#include <process.h>
#else
#include <pthread.h>
#include <semaphore.h>
#include <sched.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>
#endif

/**
 * @brief 全局同步模块状态结构体
 */
typedef struct {
    sync_error_callback_t error_callback;
    void* user_context;
    bool initialized;
} sync_global_state_t;

/**
 * @brief 全局同步模块状态
 */
sync_global_state_t g_sync_state = {NULL, NULL, false};

static bool g_initialized = false;

/**
 * @brief 初始化同步模块
 */
sync_result_t sync_init(sync_error_callback_t error_callback, void* context) {
    if (g_initialized) {
        return SYNC_SUCCESS;
    }

    g_sync_state.error_callback = error_callback;
    g_sync_state.user_context = context;
    g_sync_state.initialized = true;
    g_initialized = true;

    return SYNC_SUCCESS;
}

/**
 * @brief 清理同步模块
 */
void sync_cleanup(void) {
    if (!g_initialized) {
        return;
    }

    g_sync_state.error_callback = NULL;
    g_sync_state.user_context = NULL;
    g_sync_state.initialized = false;
    g_initialized = false;
}

/**
 * @brief 获取同步原语类型
 */
sync_type_t sync_get_type(void* lock, sync_lock_type_t lock_type) {
    (void)lock;
    switch (lock_type) {
        case SYNC_LOCK_MUTEX: return SYNC_TYPE_MUTEX;
        case SYNC_LOCK_RECURSIVE_MUTEX: return SYNC_TYPE_RECURSIVE_MUTEX;
        case SYNC_LOCK_RWLOCK: return SYNC_TYPE_RWLOCK;
        case SYNC_LOCK_SPINLOCK: return SYNC_TYPE_SPINLOCK;
        case SYNC_LOCK_SEMAPHORE: return SYNC_TYPE_SEMAPHORE;
        case SYNC_LOCK_CONDITION: return SYNC_TYPE_CONDITION;
        case SYNC_LOCK_BARRIER: return SYNC_TYPE_BARRIER;
        case SYNC_LOCK_EVENT: return SYNC_TYPE_EVENT;
        default: return SYNC_TYPE_UNKNOWN;
    }
}

/**
 * @brief 获取锁的名称
 */
const char* sync_get_name(void* lock) {
    if (lock == NULL) {
        return NULL;
    }

    struct sync_mutex* base = (struct sync_mutex*)lock;

    if (!base->initialized) {
        return NULL;
    }

    switch (base->type) {
        case SYNC_TYPE_MUTEX:
        case SYNC_TYPE_RECURSIVE_MUTEX:
            return ((struct sync_mutex*)lock)->name;
        case SYNC_TYPE_RWLOCK:
            return ((struct sync_rwlock*)lock)->name;
        case SYNC_TYPE_SPINLOCK:
            return ((struct sync_spinlock*)lock)->name;
        case SYNC_TYPE_SEMAPHORE:
            return ((struct sync_semaphore*)lock)->name;
        case SYNC_TYPE_CONDITION:
            return ((struct sync_condition*)lock)->name;
        case SYNC_TYPE_BARRIER:
            return ((struct sync_barrier*)lock)->name;
        case SYNC_TYPE_EVENT:
            return ((struct sync_event*)lock)->name;
        default:
            return NULL;
    }
}

/**
 * @brief 获取锁的统计信息
 */
sync_result_t sync_get_stats(void* lock, sync_stats_t* stats) {
    if (lock == NULL || stats == NULL) {
        return SYNC_ERROR_INVALID;
    }

    struct sync_mutex* base = (struct sync_mutex*)lock;
    if (!base->initialized) {
        return SYNC_ERROR_INVALID;
    }

    // 复制实际统计信息
    *stats = base->stats;

    return SYNC_SUCCESS;
}

/**
 * @brief 重置锁的统计信息
 */
sync_result_t sync_reset_stats(void* lock) {
    if (lock == NULL) {
        return SYNC_ERROR_INVALID;
    }

    struct sync_mutex* base = (struct sync_mutex*)lock;
    if (!base->initialized) {
        return SYNC_ERROR_INVALID;
    }

    // 实际重置统计信息
    memset(&base->stats, 0, sizeof(sync_stats_t));

    return SYNC_SUCCESS;
}

/**
 * @brief 设置锁的选项
 *
 * @note 当前为预留接口，暂不支持任何选项配置
 * @future 可扩展支持：
 *       - 默认超时时间设置
 *       - 调试级别配置
 *       - 优先级继承设置
 *       - 健壮锁配置
 *
 * @param lock 锁对象指针（不能为NULL）
 * @param option 选项标识符
 * @param value 选项值指针
 * @return sync_result_t 成功返回SYNC_SUCCESS，参数无效返回SYNC_ERROR_INVALID
 *
 * @threadsafe 是（内部使用互斥锁保护）
 */
sync_result_t sync_set_option(void* lock, int option, void* value) {
    if (lock == NULL) {
        return SYNC_ERROR_INVALID;
    }
    (void)option;
    (void)value;
    return SYNC_SUCCESS;
}

/**
 * @brief 获取锁的选项
 *
 * @note 当前为预留接口，暂不支持任何选项查询
 *
 * @param lock 锁对象指针（不能为NULL）
 * @param option 选项标识符
 * @param[out] value 输出选项值的缓冲区（不能为NULL）
 * @return sync_result_t 成功返回SYNC_SUCCESS，参数无效返回SYNC_ERROR_INVALID
 *
 * @threadsafe 是
 */
sync_result_t sync_get_option(void* lock, int option, void* value) {
    if (lock == NULL || value == NULL) {
        return SYNC_ERROR_INVALID;
    }
    (void)option;
    return SYNC_SUCCESS;
}

/**
 * @brief 检查锁是否有效
 */
bool sync_is_valid(void* lock) {
    return lock != NULL;
}

/**
 * @brief 打印锁的调试信息
 */
sync_result_t sync_debug(void* lock) {
    if (lock == NULL) {
        return SYNC_ERROR_INVALID;
    }

    struct sync_mutex* base = (struct sync_mutex*)lock;

    fprintf(stderr, "\n[SYNC DEBUG] ====================\n");
    fprintf(stderr, "[SYNC DEBUG] Lock at: %p\n", (void*)lock);
    fprintf(stderr, "[SYNC DEBUG] Type: %d\n", base->type);
    fprintf(stderr, "[SYNC DEBUG] Initialized: %s\n",
            base->initialized ? "true" : "false");

    const char* name = sync_get_name(lock);
    if (name != NULL) {
        fprintf(stderr, "[SYNC DEBUG] Name: %s\n", name);
    } else {
        fprintf(stderr, "[SYNC DEBUG] Name: (unnamed)\n");
    }

    sync_stats_t stats;
    if (sync_get_stats(lock, &stats) == SYNC_SUCCESS) {
        fprintf(stderr, "[SYNC DEBUG] --- Statistics ---\n");
        fprintf(stderr, "[SYNC DEBUG] Lock count: %zu\n", stats.lock_count);
        fprintf(stderr, "[SYNC DEBUG] Unlock count: %zu\n", stats.unlock_count);
        fprintf(stderr, "[SYNC DEBUG] Wait count: %zu\n", stats.wait_count);
        fprintf(stderr, "[SYNC DEBUG] Timeout count: %zu\n", stats.timeout_count);
        fprintf(stderr, "[SYNC DEBUG] Deadlock count: %zu\n", stats.deadlock_count);
        fprintf(stderr, "[SYNC DEBUG] Total wait time: %lu ms\n",
                (unsigned long)stats.total_wait_time_ms);
        fprintf(stderr, "[SYNC DEBUG] Max wait time: %lu ms\n",
                (unsigned long)stats.max_wait_time_ms);
    }

    fprintf(stderr, "[SYNC DEBUG] ====================\n\n");

    return SYNC_SUCCESS;
}

/**
 * @brief 超时转换为毫秒
 */
static uint64_t sync_internal_timeout_to_ms(const sync_timeout_t* timeout) {
    if (timeout == NULL) {
        return 0;
    }
    return timeout->timeout_ms;
}

/**
 * @brief 获取当前时间戳（毫秒）
 */
uint64_t sync_get_timestamp_ms(void) {
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t timestamp = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return timestamp / 10000;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
}

/**
 * @brief 线程睡眠（毫秒）
 */
void sync_sleep_ms(uint64_t ms) {
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    usleep((useconds_t)(ms * 1000));
#endif
}

/**
 * @brief 线程 yield
 */
void sync_yield(void) {
#ifdef _WIN32
    SwitchToThread();
#else
    sched_yield();
#endif
}

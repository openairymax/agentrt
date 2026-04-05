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
#include "sync_platform.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <synchapi.h>
#include <process.h>
#else
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>
#endif

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
sync_type_t sync_get_type(sync_lock_type_t lock_type) {
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
    return NULL;
}

/**
 * @brief 获取锁的统计信息
 */
sync_result_t sync_get_stats(void* lock, sync_stats_t* stats) {
    if (lock == NULL || stats == NULL) {
        return SYNC_ERROR_INVALID;
    }
    memset(stats, 0, sizeof(sync_stats_t));
    return SYNC_SUCCESS;
}

/**
 * @brief 重置锁的统计信息
 */
sync_result_t sync_reset_stats(void* lock) {
    if (lock == NULL) {
        return SYNC_ERROR_INVALID;
    }
    return SYNC_SUCCESS;
}

/**
 * @brief 设置锁的选项
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
    pthread_yield();
#endif
}

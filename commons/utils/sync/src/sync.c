/**
 * @file sync.c
 * @brief 统一线程同步原语模块 - 核心层实�? * 
 * 提供跨平台、安全、高效的线程同步原语实现�? * 支持Windows和POSIX系统，包含互斥锁、条件变量、信号量、读写锁等�? * 
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "sync.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../utils/memory/include/memory_compat.h"
#include "../../../utils/string/include/string_compat.h"
#include <string.h>
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
 * @defgroup sync_internal 内部实现
 * @{
 */

/**
 * @brief 互斥锁内部结�? */
struct sync_mutex {
#ifdef _WIN32
    CRITICAL_SECTION cs;                 /**< Windows临界�?*/
    bool initialized;                    /**< 是否已初始化 */
#else
    pthread_mutex_t mutex;               /**< POSIX互斥�?*/
    bool initialized;                    /**< 是否已初始化 */
#endif
    char* name;                          /**< 锁名�?*/
    sync_stats_t stats;                  /**< 统计信息 */
    uint64_t last_lock_time;             /**< 上次加锁时间 */
    uint64_t owner_thread_id;            /**< 拥有者线程ID */
};

/**
 * @brief 递归互斥锁内部结�? */
struct sync_recursive_mutex {
#ifdef _WIN32
    CRITICAL_SECTION cs;                 /**< Windows临界区（支持递归�?*/
#else
    pthread_mutex_t mutex;               /**< POSIX递归互斥�?*/
#endif
    bool initialized;                    /**< 是否已初始化 */
    char* name;                          /**< 锁名�?*/
    sync_stats_t stats;                  /**< 统计信息 */
    size_t recursion_count;              /**< 递归计数 */
    uint64_t owner_thread_id;            /**< 拥有者线程ID */
};

/**
 * @brief 读写锁内部结�? */
struct sync_rwlock {
#ifdef _WIN32
    SRWLOCK lock;                        /**< Windows读写�?*/
#else
    pthread_rwlock_t rwlock;             /**< POSIX读写�?*/
#endif
    bool initialized;                    /**< 是否已初始化 */
    char* name;                          /**< 锁名�?*/
    sync_stats_t stats;                  /**< 统计信息 */
};

/**
 * @brief 自旋锁内部结�? */
struct sync_spinlock {
#ifdef _WIN32
    LONG lock;                           /**< Windows自旋�?*/
#else
    pthread_spinlock_t spinlock;         /**< POSIX自旋�?*/
#endif
    bool initialized;                    /**< 是否已初始化 */
    char* name;                          /**< 锁名�?*/
    sync_stats_t stats;                  /**< 统计信息 */
};

/**
 * @brief 信号量内部结�? */
struct sync_semaphore {
#ifdef _WIN32
    HANDLE semaphore;                    /**< Windows信号量句�?*/
#else
    sem_t semaphore;                     /**< POSIX信号�?*/
#endif
    bool initialized;                    /**< 是否已初始化 */
    char* name;                          /**< 锁名�?*/
    sync_stats_t stats;                  /**< 统计信息 */
    unsigned int max_value;              /**< 最大�?*/
};

/**
 * @brief 条件变量内部结构
 */
struct sync_condition {
#ifdef _WIN32
    CONDITION_VARIABLE cond;             /**< Windows条件变量 */
#else
    pthread_cond_t cond;                 /**< POSIX条件变量 */
#endif
    bool initialized;                    /**< 是否已初始化 */
    char* name;                          /**< 锁名�?*/
    sync_stats_t stats;                  /**< 统计信息 */
};

/**
 * @brief 屏障内部结构
 */
struct sync_barrier {
#ifdef _WIN32
    // Windows没有原生屏障，使用条件变量和互斥锁模�?    CRITICAL_SECTION cs;
    CONDITION_VARIABLE cond;
    unsigned int count;
    unsigned int current;
    unsigned int generation;
#else
    pthread_barrier_t barrier;           /**< POSIX屏障 */
#endif
    bool initialized;                    /**< 是否已初始化 */
    char* name;                          /**< 锁名�?*/
    sync_stats_t stats;                  /**< 统计信息 */
};

/**
 * @brief 事件内部结构
 */
struct sync_event {
#ifdef _WIN32
    HANDLE event;                        /**< Windows事件句柄 */
#else
    pthread_cond_t cond;                 /**< POSIX条件变量（用于模拟事件） */
    pthread_mutex_t mutex;
    bool signaled;
    bool manual_reset;
#endif
    bool initialized;                    /**< 是否已初始化 */
    char* name;                          /**< 锁名�?*/
    sync_stats_t stats;                  /**< 统计信息 */
};

/**
 * @brief 模块全局状�? */
typedef struct {
    bool initialized;                    /**< 模块是否已初始化 */
    sync_error_callback_t error_callback; /**< 错误回调函数 */
    void* error_callback_context;        /**< 错误回调上下�?*/
    
    // 死锁检测相关（简化实现）
    bool deadlock_detection_enabled;     /**< 死锁检测是否启�?*/
    size_t max_locks_per_thread;         /**< 每个线程最大锁�?*/
} sync_global_state_t;

/**
 * @brief 全局状态实�? */
static sync_global_state_t g_state = {
    .initialized = false,
    .error_callback = NULL,
    .error_callback_context = NULL,
    .deadlock_detection_enabled = false,
    .max_locks_per_thread = 64
};

/**
 * @brief 获取当前时间戳（毫秒�? * 
 * @return 时间�? */
static uint64_t sync_internal_get_timestamp_ms(void) {
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t ts = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return ts / 10000; // 转换为毫�?#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
}

/**
 * @brief 获取当前线程ID
 * 
 * @return 线程ID
 */
static uint64_t sync_internal_get_thread_id(void) {
#ifdef _WIN32
    return GetCurrentThreadId();
#else
    return (uint64_t)pthread_self();
#endif
}

/**
 * @brief 记录错误
 * 
 * @param[in] result 错误结果
 * @param[in] lock_name 锁名�? */
static void sync_internal_record_error(sync_result_t result, const char* lock_name) {
    if (g_state.error_callback != NULL) {
        g_state.error_callback(result, lock_name, g_state.error_callback_context);
    }
}

/**
 * @brief 分配并复制字符串
 * 
 * @param[in] str 源字符串
 * @return 复制的字符串，失败返回NULL
 */
static char* sync_internal_strdup(const char* str) {
    if (str == NULL) {
        return NULL;
    }
    
    size_t len = strlen(str);
    char* copy = AGENTOS_MALLOC(len + 1);
    if (copy != NULL) {
        memcpy(copy, str, len + 1);
    }
    return copy;
}

/**
 * @brief 更新锁统计信息（加锁�? * 
 * @param[inout] stats 统计信息
 * @param[in] wait_time_ms 等待时间（毫秒）
 */
static void sync_internal_update_stats_lock(sync_stats_t* stats, uint64_t wait_time_ms) {
    if (stats == NULL) {
        return;
    }
    
    stats->lock_count++;
    stats->wait_count++;
    
    if (wait_time_ms > 0) {
        stats->total_wait_time_ms += wait_time_ms;
        if (wait_time_ms > stats->max_wait_time_ms) {
            stats->max_wait_time_ms = wait_time_ms;
        }
    }
}

/**
 * @brief 更新锁统计信息（解锁�? * 
 * @param[inout] stats 统计信息
 */
static void sync_internal_update_stats_unlock(sync_stats_t* stats) {
    if (stats == NULL) {
        return;
    }
    
    stats->unlock_count++;
}

/**
 * @brief 更新锁统计信息（超时�? * 
 * @param[inout] stats 统计信息
 */
static void sync_internal_update_stats_timeout(sync_stats_t* stats) {
    if (stats == NULL) {
        return;
    }
    
    stats->timeout_count++;
}

/**
 * @brief 转换Windows错误码为sync_result_t
 * 
 * @param[in] error Windows错误�? * @return 对应的sync_result_t
 */
#ifdef _WIN32
static sync_result_t sync_internal_win_error_to_result(DWORD error) {
    switch (error) {
        case WAIT_TIMEOUT:
            return SYNC_ERROR_TIMEOUT;
        case ERROR_INVALID_HANDLE:
        case ERROR_INVALID_PARAMETER:
            return SYNC_ERROR_INVALID;
        case ERROR_NOT_ENOUGH_MEMORY:
            return SYNC_ERROR_MEMORY;
        case ERROR_ACCESS_DENIED:
            return SYNC_ERROR_PERMISSION;
        case ERROR_BUSY:
            return SYNC_ERROR_BUSY;
        case ERROR_DEADLOCK:
            return SYNC_ERROR_DEADLOCK;
        default:
            return SYNC_ERROR_UNKNOWN;
    }
}
#endif

/**
 * @brief 转换POSIX错误码为sync_result_t
 * 
 * @param[in] error POSIX错误�? * @return 对应的sync_result_t
 */
#ifndef _WIN32
static sync_result_t sync_internal_posix_error_to_result(int error) {
    switch (error) {
        case ETIMEDOUT:
            return SYNC_ERROR_TIMEOUT;
        case EINVAL:
            return SYNC_ERROR_INVALID;
        case ENOMEM:
            return SYNC_ERROR_MEMORY;
        case EPERM:
            return SYNC_ERROR_PERMISSION;
        case EBUSY:
            return SYNC_ERROR_BUSY;
        case EDEADLK:
            return SYNC_ERROR_DEADLOCK;
        default:
            return SYNC_ERROR_UNKNOWN;
    }
}
#endif

/** @} */ // end of sync_internal

sync_result_t sync_init(sync_error_callback_t error_callback, void* context) {
    if (g_state.initialized) {
        return SYNC_SUCCESS;
    }
    
    g_state.error_callback = error_callback;
    g_state.error_callback_context = context;
    g_state.deadlock_detection_enabled = false;
    g_state.max_locks_per_thread = 64;
    g_state.initialized = true;
    
    return SYNC_SUCCESS;
}

void sync_cleanup(void) {
    if (!g_state.initialized) {
        return;
    }
    
    g_state.error_callback = NULL;
    g_state.error_callback_context = NULL;
    g_state.initialized = false;
}

sync_result_t sync_mutex_create(sync_mutex_t* mutex, const sync_attr_t* attr) {
    if (mutex == NULL) {
        return SYNC_ERROR_INVALID;
    }
    
    struct sync_mutex* m = AGENTOS_CALLOC(1, sizeof(struct sync_mutex));
    if (m == NULL) {
        return SYNC_ERROR_MEMORY;
    }
    
    // 设置名称
    if (attr != NULL && attr->name != NULL) {
        m->name = sync_internal_strdup(attr->name);
    }
    
    // 初始化统计信�?    memset(&m->stats, 0, sizeof(sync_stats_t));
    m->owner_thread_id = 0;
    m->last_lock_time = 0;
    
#ifdef _WIN32
    InitializeCriticalSection(&m->cs);
    m->initialized = true;
#else
    int result = pthread_mutex_init(&m->mutex, NULL);
    if (result != 0) {
        AGENTOS_FREE(m->name);
        AGENTOS_FREE(m);
        return sync_internal_posix_error_to_result(result);
    }
    m->initialized = true;
#endif
    
    *mutex = m;
    return SYNC_SUCCESS;
}

sync_result_t sync_mutex_destroy(sync_mutex_t mutex) {
    if (mutex == NULL) {
        return SYNC_ERROR_INVALID;
    }
    
    if (!mutex->initialized) {
        AGENTOS_FREE(mutex->name);
        AGENTOS_FREE(mutex);
        return SYNC_SUCCESS;
    }
    
#ifdef _WIN32
    DeleteCriticalSection(&mutex->cs);
#else
    pthread_mutex_destroy(&mutex->mutex);
#endif
    
    AGENTOS_FREE(mutex->name);
    AGENTOS_FREE(mutex);
    
    return SYNC_SUCCESS;
}

sync_result_t sync_mutex_lock(sync_mutex_t mutex, const sync_timeout_t* timeout) {
    if (mutex == NULL || !mutex->initialized) {
        return SYNC_ERROR_INVALID;
    }
    
    uint64_t start_time = sync_internal_get_timestamp_ms();
    sync_result_t result = SYNC_SUCCESS;
    
#ifdef _WIN32
    if (timeout == NULL) {
        EnterCriticalSection(&mutex->cs);
    } else {
        DWORD wait_ms = (DWORD)timeout->timeout_ms;
        if (wait_ms == INFINITE) {
            EnterCriticalSection(&mutex->cs);
        } else {
            if (!TryEnterCriticalSection(&mutex->cs)) {
                DWORD wait_result = WaitForSingleObject(mutex->cs.OwningThread, wait_ms);
                if (wait_result == WAIT_TIMEOUT) {
                    sync_internal_update_stats_timeout(&mutex->stats);
                    sync_internal_record_error(SYNC_ERROR_TIMEOUT, mutex->name);
                    return SYNC_ERROR_TIMEOUT;
                } else if (wait_result != WAIT_OBJECT_0) {
                    DWORD error = GetLastError();
                    result = sync_internal_win_error_to_result(error);
                    sync_internal_record_error(result, mutex->name);
                    return result;
                }
                // 现在可以尝试进入临界�?                if (!TryEnterCriticalSection(&mutex->cs)) {
                    sync_internal_record_error(SYNC_ERROR_BUSY, mutex->name);
                    return SYNC_ERROR_BUSY;
                }
            }
        }
    }
#else
    if (timeout == NULL) {
        int rc = pthread_mutex_lock(&mutex->mutex);
        if (rc != 0) {
            result = sync_internal_posix_error_to_result(rc);
            sync_internal_record_error(result, mutex->name);
            return result;
        }
    } else {
        // POSIX互斥锁不支持直接超时，使用pthread_mutex_trylock循环
        uint64_t end_time = start_time + timeout->timeout_ms;
        while (true) {
            int rc = pthread_mutex_trylock(&mutex->mutex);
            if (rc == 0) {
                break; // 成功获取�?            } else if (rc == EBUSY) {
                uint64_t current_time = sync_internal_get_timestamp_ms();
                if (current_time >= end_time) {
                    sync_internal_update_stats_timeout(&mutex->stats);
                    sync_internal_record_error(SYNC_ERROR_TIMEOUT, mutex->name);
                    return SYNC_ERROR_TIMEOUT;
                }
                // 短暂休眠后重�?                sync_sleep(1);
            } else {
                result = sync_internal_posix_error_to_result(rc);
                sync_internal_record_error(result, mutex->name);
                return result;
            }
        }
    }
#endif
    
    uint64_t end_time = sync_internal_get_timestamp_ms();
    uint64_t wait_time = (end_time > start_time) ? (end_time - start_time) : 0;
    
    sync_internal_update_stats_lock(&mutex->stats, wait_time);
    mutex->owner_thread_id = sync_internal_get_thread_id();
    mutex->last_lock_time = end_time;
    
    return SYNC_SUCCESS;
}

sync_result_t sync_mutex_try_lock(sync_mutex_t mutex) {
    if (mutex == NULL || !mutex->initialized) {
        return SYNC_ERROR_INVALID;
    }
    
#ifdef _WIN32
    if (TryEnterCriticalSection(&mutex->cs)) {
        sync_internal_update_stats_lock(&mutex->stats, 0);
        mutex->owner_thread_id = sync_internal_get_thread_id();
        mutex->last_lock_time = sync_internal_get_timestamp_ms();
        return SYNC_SUCCESS;
    } else {
        sync_internal_record_error(SYNC_ERROR_BUSY, mutex->name);
        return SYNC_ERROR_BUSY;
    }
#else
    int rc = pthread_mutex_trylock(&mutex->mutex);
    if (rc == 0) {
        sync_internal_update_stats_lock(&mutex->stats, 0);
        mutex->owner_thread_id = sync_internal_get_thread_id();
        mutex->last_lock_time = sync_internal_get_timestamp_ms();
        return SYNC_SUCCESS;
    } else if (rc == EBUSY) {
        sync_internal_record_error(SYNC_ERROR_BUSY, mutex->name);
        return SYNC_ERROR_BUSY;
    } else {
        sync_result_t result = sync_internal_posix_error_to_result(rc);
        sync_internal_record_error(result, mutex->name);
        return result;
    }
#endif
}

sync_result_t sync_mutex_unlock(sync_mutex_t mutex) {
    if (mutex == NULL || !mutex->initialized) {
        return SYNC_ERROR_INVALID;
    }
    
    // 检查是否由当前线程持有
    uint64_t current_thread_id = sync_internal_get_thread_id();
    if (mutex->owner_thread_id != 0 && mutex->owner_thread_id != current_thread_id) {
        sync_internal_record_error(SYNC_ERROR_PERMISSION, mutex->name);
        return SYNC_ERROR_PERMISSION;
    }
    
#ifdef _WIN32
    LeaveCriticalSection(&mutex->cs);
#else
    int rc = pthread_mutex_unlock(&mutex->mutex);
    if (rc != 0) {
        sync_result_t result = sync_internal_posix_error_to_result(rc);
        sync_internal_record_error(result, mutex->name);
        return result;
    }
#endif
    
    sync_internal_update_stats_unlock(&mutex->stats);
    mutex->owner_thread_id = 0;
    
    return SYNC_SUCCESS;
}

sync_result_t sync_recursive_mutex_create(sync_recursive_mutex_t* mutex, 
                                         const sync_attr_t* attr) {
    if (mutex == NULL) {
        return SYNC_ERROR_INVALID;
    }
    
    struct sync_recursive_mutex* m = AGENTOS_CALLOC(1, sizeof(struct sync_recursive_mutex));
    if (m == NULL) {
        return SYNC_ERROR_MEMORY;
    }
    
    // 设置名称
    if (attr != NULL && attr->name != NULL) {
        m->name = sync_internal_strdup(attr->name);
    }
    
    // 初始化统计信�?    memset(&m->stats, 0, sizeof(sync_stats_t));
    m->recursion_count = 0;
    m->owner_thread_id = 0;
    
#ifdef _WIN32
    InitializeCriticalSection(&m->cs);
    m->initialized = true;
#else
    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE);
    
    int result = pthread_mutex_init(&m->mutex, &mutex_attr);
    pthread_mutexattr_destroy(&mutex_attr);
    
    if (result != 0) {
        AGENTOS_FREE(m->name);
        AGENTOS_FREE(m);
        return sync_internal_posix_error_to_result(result);
    }
    m->initialized = true;
#endif
    
    *mutex = m;
    return SYNC_SUCCESS;
}

sync_result_t sync_recursive_mutex_destroy(sync_recursive_mutex_t mutex) {
    if (mutex == NULL) {
        return SYNC_ERROR_INVALID;
    }
    
    if (!mutex->initialized) {
        AGENTOS_FREE(mutex->name);
        AGENTOS_FREE(mutex);
        return SYNC_SUCCESS;
    }
    
#ifdef _WIN32
    DeleteCriticalSection(&mutex->cs);
#else
    pthread_mutex_destroy(&mutex->mutex);
#endif
    
    AGENTOS_FREE(mutex->name);
    AGENTOS_FREE(mutex);
    
    return SYNC_SUCCESS;
}

sync_result_t sync_recursive_mutex_lock(sync_recursive_mutex_t mutex, 
                                       const sync_timeout_t* timeout) {
    if (mutex == NULL || !mutex->initialized) {
        return SYNC_ERROR_INVALID;
    }
    
    uint64_t start_time = sync_internal_get_timestamp_ms();
    sync_result_t result = SYNC_SUCCESS;
    
    // 检查是否已由当前线程持�?    uint64_t current_thread_id = sync_internal_get_thread_id();
    if (mutex->owner_thread_id == current_thread_id) {
        // 递归加锁
        mutex->recursion_count++;
        sync_internal_update_stats_lock(&mutex->stats, 0);
        return SYNC_SUCCESS;
    }
    
#ifdef _WIN32
    if (timeout == NULL) {
        EnterCriticalSection(&mutex->cs);
    } else {
        // Windows递归锁不支持超时，简化处�?        DWORD wait_ms = (DWORD)timeout->timeout_ms;
        if (wait_ms == INFINITE) {
            EnterCriticalSection(&mutex->cs);
        } else {
            // 尝试进入临界区，带超时（简化实现）
            DWORD start = GetTickCount();
            while (!TryEnterCriticalSection(&mutex->cs)) {
                if (GetTickCount() - start >= wait_ms) {
                    sync_internal_update_stats_timeout(&mutex->stats);
                    sync_internal_record_error(SYNC_ERROR_TIMEOUT, mutex->name);
                    return SYNC_ERROR_TIMEOUT;
                }
                Sleep(1);
            }
        }
    }
#else
    if (timeout == NULL) {
        int rc = pthread_mutex_lock(&mutex->mutex);
        if (rc != 0) {
            result = sync_internal_posix_error_to_result(rc);
            sync_internal_record_error
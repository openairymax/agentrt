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
 * @brief 互斥锁内部结构
 * @note 使用 sync_type_t 类型字段标识锁类型，支持 sync_get_type() 可靠查询
 */
struct sync_mutex {
#ifdef _WIN32
    CRITICAL_SECTION cs;                 /**< Windows临界区 */
    bool initialized;                    /**< 是否已初始化 */
#else
    pthread_mutex_t mutex;               /**< POSIX互斥锁 */
    bool initialized;                    /**< 是否已初始化 */
#endif
    sync_type_t type;                     /**< 锁类型标识 */
    char* name;                          /**< 锁名称 */
    sync_stats_t stats;                  /**< 统计信息 */
    uint64_t last_lock_time;             /**< 上次加锁时间 */
    uint64_t owner_thread_id;            /**< 拥有者线程ID */
};

/**
 * @brief 递归互斥锁内部结构
 * @note 使用 sync_type_t 类型字段标识锁类型，支持 sync_get_type() 可靠查询
 */
struct sync_recursive_mutex {
#ifdef _WIN32
    CRITICAL_SECTION cs;                 /**< Windows临界区（支持递归） */
#else
    pthread_mutex_t mutex;               /**< POSIX递归互斥锁 */
#endif
    bool initialized;                    /**< 是否已初始化 */
    sync_type_t type;                     /**< 锁类型标识 */
    char* name;                          /**< 锁名称 */
    sync_stats_t stats;                  /**< 统计信息 */
    size_t recursion_count;              /**< 递归计数 */
    uint64_t owner_thread_id;            /**< 拥有者线程ID */
};

/**
 * @brief 读写锁内部结构
 * @note 使用 sync_type_t 类型字段标识锁类型，支持 sync_get_type() 可靠查询
 */
struct sync_rwlock {
#ifdef _WIN32
    SRWLOCK lock;                        /**< Windows读写锁 */
#else
    pthread_rwlock_t rwlock;             /**< POSIX读写锁 */
#endif
    bool initialized;                    /**< 是否已初始化 */
    sync_type_t type;                     /**< 锁类型标识 */
    char* name;                          /**< 锁名称 */
    sync_stats_t stats;                  /**< 统计信息 */
};

/**
 * @brief 自旋锁内部结构
 * @note 使用 sync_type_t 类型字段标识锁类型，支持 sync_get_type() 可靠查询
 */
struct sync_spinlock {
#ifdef _WIN32
    LONG lock;                           /**< Windows自旋锁 */
#else
    pthread_spinlock_t spinlock;         /**< POSIX自旋锁 */
#endif
    bool initialized;                    /**< 是否已初始化 */
    sync_type_t type;                     /**< 锁类型标识 */
    char* name;                          /**< 锁名称 */
    sync_stats_t stats;                  /**< 统计信息 */
};

/**
 * @brief 信号量内部结构
 * @note 使用 sync_type_t 类型字段标识锁类型，支持 sync_get_type() 可靠查询
 */
struct sync_semaphore {
#ifdef _WIN32
    HANDLE semaphore;                    /**< Windows信号量句柄 */
#else
    sem_t semaphore;                     /**< POSIX信号量 */
#endif
    bool initialized;                    /**< 是否已初始化 */
    sync_type_t type;                     /**< 锁类型标识 */
    char* name;                          /**< 锁名称 */
    sync_stats_t stats;                  /**< 统计信息 */
    unsigned int max_value;              /**< 最大值 */
};

/**
 * @brief 条件变量内部结构
 * @note 使用 sync_type_t 类型字段标识锁类型，支持 sync_get_type() 可靠查询
 */
struct sync_condition {
#ifdef _WIN32
    CONDITION_VARIABLE cond;             /**< Windows条件变量 */
#else
    pthread_cond_t cond;                 /**< POSIX条件变量 */
#endif
    bool initialized;                    /**< 是否已初始化 */
    sync_type_t type;                   /**< 锁类型标识 */
    char* name;                         /**< 锁名称 */
    sync_stats_t stats;                 /**< 统计信息 */
};

/**
 * @brief 屏障内部结构
 * @note 使用 sync_type_t 类型字段标识锁类型，支持 sync_get_type() 可靠查询
 */
struct sync_barrier {
#ifdef _WIN32
    CRITICAL_SECTION cs;                /**< Windows临界区（模拟屏障） */
    CONDITION_VARIABLE cond;            /**< Windows条件变量 */
    unsigned int count;                  /**< 等待线程数 */
    unsigned int current;                /**< 当前计数 */
    unsigned int generation;             /**< 世代计数 */
#else
    pthread_barrier_t barrier;           /**< POSIX屏障 */
#endif
    bool initialized;                    /**< 是否已初始化 */
    sync_type_t type;                   /**< 锁类型标识 */
    char* name;                         /**< 锁名称 */
    sync_stats_t stats;                 /**< 统计信息 */
};

/**
 * @brief 事件内部结构
 * @note 使用 sync_type_t 类型字段标识锁类型，支持 sync_get_type() 可靠查询
 */
struct sync_event {
#ifdef _WIN32
    HANDLE event;                        /**< Windows事件句柄 */
#else
    pthread_cond_t cond;                 /**< POSIX条件变量（用于模拟事件） */
    pthread_mutex_t mutex;               /**< POSIX互斥锁 */
    bool signaled;                       /**< 信号状态 */
    bool manual_reset;                   /**< 手动重置标志 */
#endif
    bool initialized;                    /**< 是否已初始化 */
    sync_type_t type;                   /**< 锁类型标识 */
    char* name;                         /**< 锁名称 */
    sync_stats_t stats;                 /**< 统计信息 */
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

    // 设置锁类型标识
    m->type = SYNC_TYPE_MUTEX;

    // 设置名称
    if (attr != NULL && attr->name != NULL) {
        m->name = sync_internal_strdup(attr->name);
    }

    // 初始化统计信息
    memset(&m->stats, 0, sizeof(sync_stats_t));
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
        uint64_t end_time = start_time + timeout->timeout_ms;
        while (true) {
            int rc = pthread_mutex_trylock(&mutex->mutex);
            if (rc == 0) {
                break;
            } else if (rc == EBUSY) {
                uint64_t current_time = sync_internal_get_timestamp_ms();
                if (current_time >= end_time) {
                    sync_internal_update_stats_timeout(&mutex->stats);
                    sync_internal_record_error(SYNC_ERROR_TIMEOUT, mutex->name);
                    return SYNC_ERROR_TIMEOUT;
                }
                sync_sleep(1);
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
    mutex->recursion_count = 1;
    
    return SYNC_SUCCESS;
}

sync_result_t sync_recursive_mutex_unlock(sync_recursive_mutex_t mutex) {
    if (mutex == NULL || !mutex->initialized) {
        return SYNC_ERROR_INVALID;
    }
    
    uint64_t current_thread_id = sync_internal_get_thread_id();
    if (mutex->owner_thread_id != current_thread_id) {
        sync_internal_record_error(SYNC_ERROR_PERMISSION, mutex->name);
        return SYNC_ERROR_PERMISSION;
    }
    
    if (mutex->recursion_count > 1) {
        mutex->recursion_count--;
        sync_internal_update_stats_unlock(&mutex->stats);
        return SYNC_SUCCESS;
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
    mutex->recursion_count = 0;
    
    return SYNC_SUCCESS;
}

sync_result_t sync_recursive_mutex_get_count(sync_recursive_mutex_t mutex, size_t* count) {
    if (mutex == NULL || count == NULL) {
        return SYNC_ERROR_INVALID;
    }
    
    *count = mutex->recursion_count;
    return SYNC_SUCCESS;
}

sync_result_t sync_rwlock_create(sync_rwlock_t* rwlock, const sync_attr_t* attr) {
    if (rwlock == NULL) {
        return SYNC_ERROR_INVALID;
    }

    struct sync_rwlock* lock = AGENTOS_CALLOC(1, sizeof(struct sync_rwlock));
    if (lock == NULL) {
        return SYNC_ERROR_MEMORY;
    }

    // 设置锁类型标识
    lock->type = SYNC_TYPE_RWLOCK;

    if (attr != NULL && attr->name != NULL) {
        lock->name = sync_internal_strdup(attr->name);
    }

    memset(&lock->stats, 0, sizeof(sync_stats_t));

#ifdef _WIN32
    InitializeSRWLock(&lock->lock);
    lock->initialized = true;
#else
    int result = pthread_rwlock_init(&lock->rwlock, NULL);
    if (result != 0) {
        AGENTOS_FREE(lock->name);
        AGENTOS_FREE(lock);
        return sync_internal_posix_error_to_result(result);
    }
    lock->initialized = true;
#endif

    *rwlock = lock;
    return SYNC_SUCCESS;
}

sync_result_t sync_rwlock_destroy(sync_rwlock_t rwlock) {
    if (rwlock == NULL) {
        return SYNC_ERROR_INVALID;
    }
    
    if (!rwlock->initialized) {
        AGENTOS_FREE(rwlock->name);
        AGENTOS_FREE(rwlock);
        return SYNC_SUCCESS;
    }
    
#ifdef _WIN32
    // SRWLock 不需要显式销毁
#else
    pthread_rwlock_destroy(&rwlock->rwlock);
#endif
    
    AGENTOS_FREE(rwlock->name);
    AGENTOS_FREE(rwlock);
    
    return SYNC_SUCCESS;
}

sync_result_t sync_rwlock_read_lock(sync_rwlock_t rwlock, const sync_timeout_t* timeout) {
    if (rwlock == NULL || !rwlock->initialized) {
        return SYNC_ERROR_INVALID;
    }
    
    uint64_t start_time = sync_internal_get_timestamp_ms();
    sync_result_t result = SYNC_SUCCESS;
    
#ifdef _WIN32
    if (timeout == NULL) {
        AcquireSRWLockShared(&rwlock->lock);
    } else {
        DWORD wait_ms = (DWORD)timeout->timeout_ms;
        DWORD start = GetTickCount();
        while (!TryAcquireSRWLockShared(&rwlock->lock)) {
            if (GetTickCount() - start >= wait_ms) {
                sync_internal_update_stats_timeout(&rwlock->stats);
                sync_internal_record_error(SYNC_ERROR_TIMEOUT, rwlock->name);
                return SYNC_ERROR_TIMEOUT;
            }
            Sleep(1);
        }
    }
#else
    if (timeout == NULL) {
        int rc = pthread_rwlock_rdlock(&rwlock->rwlock);
        if (rc != 0) {
            result = sync_internal_posix_error_to_result(rc);
            sync_internal_record_error(result, rwlock->name);
            return result;
        }
    } else {
        uint64_t end_time = start_time + timeout->timeout_ms;
        while (true) {
            int rc = pthread_rwlock_tryrdlock(&rwlock->rwlock);
            if (rc == 0) {
                break;
            } else if (rc == EBUSY) {
                uint64_t current_time = sync_internal_get_timestamp_ms();
                if (current_time >= end_time) {
                    sync_internal_update_stats_timeout(&rwlock->stats);
                    sync_internal_record_error(SYNC_ERROR_TIMEOUT, rwlock->name);
                    return SYNC_ERROR_TIMEOUT;
                }
                sync_sleep(1);
            } else {
                result = sync_internal_posix_error_to_result(rc);
                sync_internal_record_error(result, rwlock->name);
                return result;
            }
        }
    }
#endif
    
    uint64_t end_time = sync_internal_get_timestamp_ms();
    uint64_t wait_time = (end_time > start_time) ? (end_time - start_time) : 0;
    sync_internal_update_stats_lock(&rwlock->stats, wait_time);
    
    return SYNC_SUCCESS;
}

sync_result_t sync_rwlock_try_read_lock(sync_rwlock_t rwlock) {
    if (rwlock == NULL || !rwlock->initialized) {
        return SYNC_ERROR_INVALID;
    }
    
#ifdef _WIN32
    if (TryAcquireSRWLockShared(&rwlock->lock)) {
        sync_internal_update_stats_lock(&rwlock->stats, 0);
        return SYNC_SUCCESS;
    }
    return SYNC_ERROR_BUSY;
#else
    int rc = pthread_rwlock_tryrdlock(&rwlock->rwlock);
    if (rc == 0) {
        sync_internal_update_stats_lock(&rwlock->stats, 0);
        return SYNC_SUCCESS;
    } else if (rc == EBUSY) {
        return SYNC_ERROR_BUSY;
    }
    return sync_internal_posix_error_to_result(rc);
#endif
}

sync_result_t sync_rwlock_write_lock(sync_rwlock_t rwlock, const sync_timeout_t* timeout) {
    if (rwlock == NULL || !rwlock->initialized) {
        return SYNC_ERROR_INVALID;
    }
    
    uint64_t start_time = sync_internal_get_timestamp_ms();
    sync_result_t result = SYNC_SUCCESS;
    
#ifdef _WIN32
    if (timeout == NULL) {
        AcquireSRWLockExclusive(&rwlock->lock);
    } else {
        DWORD wait_ms = (DWORD)timeout->timeout_ms;
        DWORD start = GetTickCount();
        while (!TryAcquireSRWLockExclusive(&rwlock->lock)) {
            if (GetTickCount() - start >= wait_ms) {
                sync_internal_update_stats_timeout(&rwlock->stats);
                sync_internal_record_error(SYNC_ERROR_TIMEOUT, rwlock->name);
                return SYNC_ERROR_TIMEOUT;
            }
            Sleep(1);
        }
    }
#else
    if (timeout == NULL) {
        int rc = pthread_rwlock_wrlock(&rwlock->rwlock);
        if (rc != 0) {
            result = sync_internal_posix_error_to_result(rc);
            sync_internal_record_error(result, rwlock->name);
            return result;
        }
    } else {
        uint64_t end_time = start_time + timeout->timeout_ms;
        while (true) {
            int rc = pthread_rwlock_trywrlock(&rwlock->rwlock);
            if (rc == 0) {
                break;
            } else if (rc == EBUSY) {
                uint64_t current_time = sync_internal_get_timestamp_ms();
                if (current_time >= end_time) {
                    sync_internal_update_stats_timeout(&rwlock->stats);
                    sync_internal_record_error(SYNC_ERROR_TIMEOUT, rwlock->name);
                    return SYNC_ERROR_TIMEOUT;
                }
                sync_sleep(1);
            } else {
                result = sync_internal_posix_error_to_result(rc);
                sync_internal_record_error(result, rwlock->name);
                return result;
            }
        }
    }
#endif
    
    uint64_t end_time = sync_internal_get_timestamp_ms();
    uint64_t wait_time = (end_time > start_time) ? (end_time - start_time) : 0;
    sync_internal_update_stats_lock(&rwlock->stats, wait_time);
    
    return SYNC_SUCCESS;
}

sync_result_t sync_rwlock_try_write_lock(sync_rwlock_t rwlock) {
    if (rwlock == NULL || !rwlock->initialized) {
        return SYNC_ERROR_INVALID;
    }
    
#ifdef _WIN32
    if (TryAcquireSRWLockExclusive(&rwlock->lock)) {
        sync_internal_update_stats_lock(&rwlock->stats, 0);
        return SYNC_SUCCESS;
    }
    return SYNC_ERROR_BUSY;
#else
    int rc = pthread_rwlock_trywrlock(&rwlock->rwlock);
    if (rc == 0) {
        sync_internal_update_stats_lock(&rwlock->stats, 0);
        return SYNC_SUCCESS;
    } else if (rc == EBUSY) {
        return SYNC_ERROR_BUSY;
    }
    return sync_internal_posix_error_to_result(rc);
#endif
}

sync_result_t sync_rwlock_unlock(sync_rwlock_t rwlock) {
    if (rwlock == NULL || !rwlock->initialized) {
        return SYNC_ERROR_INVALID;
    }
    
#ifdef _WIN32
    ReleaseSRWLockShared(&rwlock->lock);
#else
    int rc = pthread_rwlock_unlock(&rwlock->rwlock);
    if (rc != 0) {
        sync_result_t result = sync_internal_posix_error_to_result(rc);
        sync_internal_record_error(result, rwlock->name);
        return result;
    }
#endif
    
    sync_internal_update_stats_unlock(&rwlock->stats);
    return SYNC_SUCCESS;
}

sync_result_t sync_spinlock_create(sync_spinlock_t* spinlock, const sync_attr_t* attr) {
    if (spinlock == NULL) {
        return SYNC_ERROR_INVALID;
    }

    struct sync_spinlock* lock = AGENTOS_CALLOC(1, sizeof(struct sync_spinlock));
    if (lock == NULL) {
        return SYNC_ERROR_MEMORY;
    }

    // 设置锁类型标识
    lock->type = SYNC_TYPE_SPINLOCK;

    if (attr != NULL && attr->name != NULL) {
        lock->name = sync_internal_strdup(attr->name);
    }

    memset(&lock->stats, 0, sizeof(sync_stats_t));

#ifdef _WIN32
    lock->lock = 0;
    lock->initialized = true;
#else
    int result = pthread_spin_init(&lock->spinlock, PTHREAD_PROCESS_PRIVATE);
    if (result != 0) {
        AGENTOS_FREE(lock->name);
        AGENTOS_FREE(lock);
        return sync_internal_posix_error_to_result(result);
    }
    lock->initialized = true;
#endif
    
    *spinlock = lock;
    return SYNC_SUCCESS;
}

sync_result_t sync_spinlock_destroy(sync_spinlock_t spinlock) {
    if (spinlock == NULL) {
        return SYNC_ERROR_INVALID;
    }
    
    if (!spinlock->initialized) {
        AGENTOS_FREE(spinlock->name);
        AGENTOS_FREE(spinlock);
        return SYNC_SUCCESS;
    }
    
#ifdef _WIN32
    // Windows 自旋锁不需要显式销毁
#else
    pthread_spin_destroy(&spinlock->spinlock);
#endif
    
    AGENTOS_FREE(spinlock->name);
    AGENTOS_FREE(spinlock);
    
    return SYNC_SUCCESS;
}

sync_result_t sync_spinlock_lock(sync_spinlock_t spinlock) {
    if (spinlock == NULL || !spinlock->initialized) {
        return SYNC_ERROR_INVALID;
    }
    
#ifdef _WIN32
    while (InterlockedCompareExchange(&spinlock->lock, 1, 0) != 0) {
        // 自旋等待
    }
#else
    int rc = pthread_spin_lock(&spinlock->spinlock);
    if (rc != 0) {
        return sync_internal_posix_error_to_result(rc);
    }
#endif
    
    sync_internal_update_stats_lock(&spinlock->stats, 0);
    return SYNC_SUCCESS;
}

sync_result_t sync_spinlock_try_lock(sync_spinlock_t spinlock) {
    if (spinlock == NULL || !spinlock->initialized) {
        return SYNC_ERROR_INVALID;
    }
    
#ifdef _WIN32
    if (InterlockedCompareExchange(&spinlock->lock, 1, 0) == 0) {
        sync_internal_update_stats_lock(&spinlock->stats, 0);
        return SYNC_SUCCESS;
    }
    return SYNC_ERROR_BUSY;
#else
    int rc = pthread_spin_trylock(&spinlock->spinlock);
    if (rc == 0) {
        sync_internal_update_stats_lock(&spinlock->stats, 0);
        return SYNC_SUCCESS;
    } else if (rc == EBUSY) {
        return SYNC_ERROR_BUSY;
    }
    return sync_internal_posix_error_to_result(rc);
#endif
}

sync_result_t sync_spinlock_unlock(sync_spinlock_t spinlock) {
    if (spinlock == NULL || !spinlock->initialized) {
        return SYNC_ERROR_INVALID;
    }
    
#ifdef _WIN32
    InterlockedExchange(&spinlock->lock, 0);
#else
    int rc = pthread_spin_unlock(&spinlock->spinlock);
    if (rc != 0) {
        return sync_internal_posix_error_to_result(rc);
    }
#endif
    
    sync_internal_update_stats_unlock(&spinlock->stats);
    return SYNC_SUCCESS;
}

sync_result_t sync_semaphore_create(sync_semaphore_t* semaphore,
                                   unsigned int initial_value,
                                   unsigned int max_value,
                                   const sync_attr_t* attr) {
    if (semaphore == NULL) {
        return SYNC_ERROR_INVALID;
    }

    struct sync_semaphore* sem = AGENTOS_CALLOC(1, sizeof(struct sync_semaphore));
    if (sem == NULL) {
        return SYNC_ERROR_MEMORY;
    }

    // 设置锁类型标识
    sem->type = SYNC_TYPE_SEMAPHORE;

    if (attr != NULL && attr->name != NULL) {
        sem->name = sync_internal_strdup(attr->name);
    }

    memset(&sem->stats, 0, sizeof(sync_stats_t));
    sem->max_value = (max_value == 0) ? 0xFFFFFFFF : max_value;

#ifdef _WIN32
    sem->semaphore = CreateSemaphore(NULL, initial_value, sem->max_value, NULL);
    if (sem->semaphore == NULL) {
        AGENTOS_FREE(sem->name);
        AGENTOS_FREE(sem);
        return SYNC_ERROR_UNKNOWN;
    }
    sem->initialized = true;
#else
    int result = sem_init(&sem->semaphore, 0, initial_value);
    if (result != 0) {
        AGENTOS_FREE(sem->name);
        AGENTOS_FREE(sem);
        return sync_internal_posix_error_to_result(errno);
    }
    sem->initialized = true;
#endif

    *semaphore = sem;
    return SYNC_SUCCESS;
}

sync_result_t sync_semaphore_destroy(sync_semaphore_t semaphore) {
    if (semaphore == NULL) {
        return SYNC_ERROR_INVALID;
    }
    
    if (!semaphore->initialized) {
        AGENTOS_FREE(semaphore->name);
        AGENTOS_FREE(semaphore);
        return SYNC_SUCCESS;
    }
    
#ifdef _WIN32
    CloseHandle(semaphore->semaphore);
#else
    sem_destroy(&semaphore->semaphore);
#endif
    
    AGENTOS_FREE(semaphore->name);
    AGENTOS_FREE(semaphore);
    
    return SYNC_SUCCESS;
}

sync_result_t sync_semaphore_wait(sync_semaphore_t semaphore, const sync_timeout_t* timeout) {
    if (semaphore == NULL || !semaphore->initialized) {
        return SYNC_ERROR_INVALID;
    }
    
    uint64_t start_time = sync_internal_get_timestamp_ms();
    
#ifdef _WIN32
    DWORD wait_ms = (timeout == NULL) ? INFINITE : (DWORD)timeout->timeout_ms;
    DWORD result = WaitForSingleObject(semaphore->semaphore, wait_ms);
    if (result == WAIT_TIMEOUT) {
        sync_internal_update_stats_timeout(&semaphore->stats);
        sync_internal_record_error(SYNC_ERROR_TIMEOUT, semaphore->name);
        return SYNC_ERROR_TIMEOUT;
    } else if (result != WAIT_OBJECT_0) {
        sync_internal_record_error(SYNC_ERROR_UNKNOWN, semaphore->name);
        return SYNC_ERROR_UNKNOWN;
    }
#else
    if (timeout == NULL) {
        int rc = sem_wait(&semaphore->semaphore);
        if (rc != 0) {
            return sync_internal_posix_error_to_result(errno);
        }
    } else {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout->timeout_ms / 1000;
        ts.tv_nsec += (timeout->timeout_ms % 1000) * 1000000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000;
        }
        int rc = sem_timedwait(&semaphore->semaphore, &ts);
        if (rc != 0) {
            if (errno == ETIMEDOUT) {
                sync_internal_update_stats_timeout(&semaphore->stats);
                sync_internal_record_error(SYNC_ERROR_TIMEOUT, semaphore->name);
                return SYNC_ERROR_TIMEOUT;
            }
            return sync_internal_posix_error_to_result(errno);
        }
    }
#endif
    
    uint64_t end_time = sync_internal_get_timestamp_ms();
    uint64_t wait_time = (end_time > start_time) ? (end_time - start_time) : 0;
    sync_internal_update_stats_lock(&semaphore->stats, wait_time);
    
    return SYNC_SUCCESS;
}

sync_result_t sync_semaphore_try_wait(sync_semaphore_t semaphore) {
    if (semaphore == NULL || !semaphore->initialized) {
        return SYNC_ERROR_INVALID;
    }
    
#ifdef _WIN32
    DWORD result = WaitForSingleObject(semaphore->semaphore, 0);
    if (result == WAIT_TIMEOUT) {
        return SYNC_ERROR_BUSY;
    } else if (result != WAIT_OBJECT_0) {
        return SYNC_ERROR_UNKNOWN;
    }
#else
    int rc = sem_trywait(&semaphore->semaphore);
    if (rc != 0) {
        if (errno == EAGAIN) {
            return SYNC_ERROR_BUSY;
        }
        return sync_internal_posix_error_to_result(errno);
    }
#endif
    
    sync_internal_update_stats_lock(&semaphore->stats, 0);
    return SYNC_SUCCESS;
}

sync_result_t sync_semaphore_post(sync_semaphore_t semaphore) {
    if (semaphore == NULL || !semaphore->initialized) {
        return SYNC_ERROR_INVALID;
    }
    
#ifdef _WIN32
    if (!ReleaseSemaphore(semaphore->semaphore, 1, NULL)) {
        return SYNC_ERROR_UNKNOWN;
    }
#else
    int rc = sem_post(&semaphore->semaphore);
    if (rc != 0) {
        return sync_internal_posix_error_to_result(errno);
    }
#endif
    
    sync_internal_update_stats_unlock(&semaphore->stats);
    return SYNC_SUCCESS;
}

sync_result_t sync_semaphore_get_value(sync_semaphore_t semaphore, unsigned int* value) {
    if (semaphore == NULL || value == NULL || !semaphore->initialized) {
        return SYNC_ERROR_INVALID;
    }

#ifdef _WIN32
    /**
     * @note Windows平台限制说明：
     * Windows的信号量API (WaitForSingleObject/ ReleaseSemaphore) 不提供
     * 获取当前计数器的直接方法。若需要此功能，可考虑：
     * 1. 使用互斥锁 + 计数器模拟（需自行实现）
     * 2. 使用条件变量实现更灵活的同步机制
     * 3. 通过WaitForSingleObject获取等待数间接估算
     */
    *value = 0;
    return SYNC_ERROR_UNKNOWN;
#else
    int val;
    if (sem_getvalue(&semaphore->semaphore, &val) != 0) {
        return sync_internal_posix_error_to_result(errno);
    }
    *value = (unsigned int)val;
    return SYNC_SUCCESS;
#endif
}

sync_result_t sync_condition_create(sync_condition_t* condition, const sync_attr_t* attr) {
    if (condition == NULL) {
        return SYNC_ERROR_INVALID;
    }

    struct sync_condition* cond = AGENTOS_CALLOC(1, sizeof(struct sync_condition));
    if (cond == NULL) {
        return SYNC_ERROR_MEMORY;
    }

    // 设置锁类型标识
    cond->type = SYNC_TYPE_CONDITION;

    if (attr != NULL && attr->name != NULL) {
        cond->name = sync_internal_strdup(attr->name);
    }

    memset(&cond->stats, 0, sizeof(sync_stats_t));

#ifdef _WIN32
    InitializeConditionVariable(&cond->cond);
    cond->initialized = true;
#else
    int result = pthread_cond_init(&cond->cond, NULL);
    if (result != 0) {
        AGENTOS_FREE(cond->name);
        AGENTOS_FREE(cond);
        return sync_internal_posix_error_to_result(result);
    }
    cond->initialized = true;
#endif
    
    *condition = cond;
    return SYNC_SUCCESS;
}

sync_result_t sync_condition_destroy(sync_condition_t condition) {
    if (condition == NULL) {
        return SYNC_ERROR_INVALID;
    }
    
    if (!condition->initialized) {
        AGENTOS_FREE(condition->name);
        AGENTOS_FREE(condition);
        return SYNC_SUCCESS;
    }
    
#ifdef _WIN32
    // Windows 条件变量不需要显式销毁
#else
    pthread_cond_destroy(&condition->cond);
#endif
    
    AGENTOS_FREE(condition->name);
    AGENTOS_FREE(condition);
    
    return SYNC_SUCCESS;
}

sync_result_t sync_condition_wait(sync_condition_t condition, 
                                 sync_mutex_t mutex,
                                 const sync_timeout_t* timeout) {
    if (condition == NULL || mutex == NULL || 
        !condition->initialized || !mutex->initialized) {
        return SYNC_ERROR_INVALID;
    }
    
#ifdef _WIN32
    DWORD wait_ms = (timeout == NULL) ? INFINITE : (DWORD)timeout->timeout_ms;
    if (!SleepConditionVariableCS(&condition->cond, &mutex->cs, wait_ms)) {
        if (GetLastError() == ERROR_TIMEOUT) {
            sync_internal_update_stats_timeout(&condition->stats);
            return SYNC_ERROR_TIMEOUT;
        }
        return SYNC_ERROR_UNKNOWN;
    }
#else
    if (timeout == NULL) {
        int rc = pthread_cond_wait(&condition->cond, &mutex->mutex);
        if (rc != 0) {
            return sync_internal_posix_error_to_result(rc);
        }
    } else {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout->timeout_ms / 1000;
        ts.tv_nsec += (timeout->timeout_ms % 1000) * 1000000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000;
        }
        int rc = pthread_cond_timedwait(&condition->cond, &mutex->mutex, &ts);
        if (rc != 0) {
            if (rc == ETIMEDOUT) {
                sync_internal_update_stats_timeout(&condition->stats);
                return SYNC_ERROR_TIMEOUT;
            }
            return sync_internal_posix_error_to_result(rc);
        }
    }
#endif
    
    sync_internal_update_stats_lock(&condition->stats, 0);
    return SYNC_SUCCESS;
}

sync_result_t sync_condition_signal(sync_condition_t condition) {
    if (condition == NULL || !condition->initialized) {
        return SYNC_ERROR_INVALID;
    }
    
#ifdef _WIN32
    WakeConditionVariable(&condition->cond);
#else
    int rc = pthread_cond_signal(&condition->cond);
    if (rc != 0) {
        return sync_internal_posix_error_to_result(rc);
    }
#endif
    
    return SYNC_SUCCESS;
}

sync_result_t sync_condition_broadcast(sync_condition_t condition) {
    if (condition == NULL || !condition->initialized) {
        return SYNC_ERROR_INVALID;
    }
    
#ifdef _WIN32
    WakeAllConditionVariable(&condition->cond);
#else
    int rc = pthread_cond_broadcast(&condition->cond);
    if (rc != 0) {
        return sync_internal_posix_error_to_result(rc);
    }
#endif
    
    return SYNC_SUCCESS;
}

sync_result_t sync_barrier_create(sync_barrier_t* barrier,
                                 unsigned int count,
                                 const sync_attr_t* attr) {
    if (barrier == NULL || count == 0) {
        return SYNC_ERROR_INVALID;
    }

    struct sync_barrier* b = AGENTOS_CALLOC(1, sizeof(struct sync_barrier));
    if (b == NULL) {
        return SYNC_ERROR_MEMORY;
    }

    // 设置锁类型标识
    b->type = SYNC_TYPE_BARRIER;

    if (attr != NULL && attr->name != NULL) {
        b->name = sync_internal_strdup(attr->name);
    }

    memset(&b->stats, 0, sizeof(sync_stats_t));

#ifdef _WIN32
    InitializeCriticalSection(&b->cs);
    InitializeConditionVariable(&b->cond);
    b->count = count;
    b->current = 0;
    b->generation = 0;
#else
    int result = pthread_barrier_init(&b->barrier, NULL, count);
    if (result != 0) {
        AGENTOS_FREE(b->name);
        AGENTOS_FREE(b);
        return sync_internal_posix_error_to_result(result);
    }
#endif
    b->initialized = true;

    *barrier = b;
    return SYNC_SUCCESS;
}

sync_result_t sync_barrier_destroy(sync_barrier_t barrier) {
    if (barrier == NULL) {
        return SYNC_ERROR_INVALID;
    }
    
    if (!barrier->initialized) {
        AGENTOS_FREE(barrier->name);
        AGENTOS_FREE(barrier);
        return SYNC_SUCCESS;
    }
    
#ifdef _WIN32
    DeleteCriticalSection(&barrier->cs);
#else
    pthread_barrier_destroy(&barrier->barrier);
#endif
    
    AGENTOS_FREE(barrier->name);
    AGENTOS_FREE(barrier);
    
    return SYNC_SUCCESS;
}

sync_result_t sync_barrier_wait(sync_barrier_t barrier, const sync_timeout_t* timeout) {
    if (barrier == NULL || !barrier->initialized) {
        return SYNC_ERROR_INVALID;
    }
    
#ifdef _WIN32
    EnterCriticalSection(&barrier->cs);
    barrier->current++;
    
    if (barrier->current >= barrier->count) {
        barrier->current = 0;
        barrier->generation++;
        WakeAllConditionVariable(&barrier->cond);
        LeaveCriticalSection(&barrier->cs);
        return SYNC_SUCCESS;
    }
    
    unsigned int gen = barrier->generation;
    DWORD wait_ms = (timeout == NULL) ? INFINITE : (DWORD)timeout->timeout_ms;
    
    while (barrier->generation == gen) {
        if (!SleepConditionVariableCS(&barrier->cond, &barrier->cs, wait_ms)) {
            if (GetLastError() == ERROR_TIMEOUT) {
                LeaveCriticalSection(&barrier->cs);
                return SYNC_ERROR_TIMEOUT;
            }
        }
    }
    LeaveCriticalSection(&barrier->cs);
    return SYNC_SUCCESS;
#else
    int rc = pthread_barrier_wait(&barrier->barrier);
    if (rc == PTHREAD_BARRIER_SERIAL_THREAD) {
        return SYNC_SUCCESS;
    } else if (rc != 0) {
        return sync_internal_posix_error_to_result(rc);
    }
    return SYNC_SUCCESS;
#endif
}

sync_result_t sync_barrier_reset(sync_barrier_t barrier, unsigned int new_count) {
    if (barrier == NULL || !barrier->initialized) {
        return SYNC_ERROR_INVALID;
    }
    
#ifdef _WIN32
    EnterCriticalSection(&barrier->cs);
    if (new_count > 0) {
        barrier->count = new_count;
    }
    barrier->current = 0;
    barrier->generation++;
    LeaveCriticalSection(&barrier->cs);
    return SYNC_SUCCESS;
#else
    // POSIX 屏障不支持重置，需要销毁后重建
    (void)new_count;
    return SYNC_ERROR_INVALID;
#endif
}

sync_result_t sync_event_create(sync_event_t* event,
                               bool manual_reset,
                               bool initial_state,
                               const sync_attr_t* attr) {
    if (event == NULL) {
        return SYNC_ERROR_INVALID;
    }

    struct sync_event* e = AGENTOS_CALLOC(1, sizeof(struct sync_event));
    if (e == NULL) {
        return SYNC_ERROR_MEMORY;
    }

    // 设置锁类型标识
    e->type = SYNC_TYPE_EVENT;

    if (attr != NULL && attr->name != NULL) {
        e->name = sync_internal_strdup(attr->name);
    }

    memset(&e->stats, 0, sizeof(sync_stats_t));

#ifdef _WIN32
    e->event = CreateEvent(NULL, manual_reset, initial_state, NULL);
    if (e->event == NULL) {
        AGENTOS_FREE(e->name);
        AGENTOS_FREE(e);
        return SYNC_ERROR_UNKNOWN;
    }
    e->initialized = true;
#else
    pthread_cond_init(&e->cond, NULL);
    pthread_mutex_init(&e->mutex, NULL);
    e->signaled = initial_state;
    e->manual_reset = manual_reset;
    e->initialized = true;
#endif

    *event = e;
    return SYNC_SUCCESS;
}

sync_result_t sync_event_destroy(sync_event_t event) {
    if (event == NULL) {
        return SYNC_ERROR_INVALID;
    }
    
    if (!event->initialized) {
        AGENTOS_FREE(event->name);
        AGENTOS_FREE(event);
        return SYNC_SUCCESS;
    }
    
#ifdef _WIN32
    CloseHandle(event->event);
#else
    pthread_cond_destroy(&event->cond);
    pthread_mutex_destroy(&event->mutex);
#endif
    
    AGENTOS_FREE(event->name);
    AGENTOS_FREE(event);
    
    return SYNC_SUCCESS;
}

sync_result_t sync_event_set(sync_event_t event) {
    if (event == NULL || !event->initialized) {
        return SYNC_ERROR_INVALID;
    }
    
#ifdef _WIN32
    SetEvent(event->event);
#else
    pthread_mutex_lock(&event->mutex);
    event->signaled = true;
    if (event->manual_reset) {
        pthread_cond_broadcast(&event->cond);
    } else {
        pthread_cond_signal(&event->cond);
    }
    pthread_mutex_unlock(&event->mutex);
#endif
    
    return SYNC_SUCCESS;
}

sync_result_t sync_event_reset(sync_event_t event) {
    if (event == NULL || !event->initialized) {
        return SYNC_ERROR_INVALID;
    }
    
#ifdef _WIN32
    ResetEvent(event->event);
#else
    pthread_mutex_lock(&event->mutex);
    event->signaled = false;
    pthread_mutex_unlock(&event->mutex);
#endif
    
    return SYNC_SUCCESS;
}

sync_result_t sync_event_wait(sync_event_t event, const sync_timeout_t* timeout) {
    if (event == NULL || !event->initialized) {
        return SYNC_ERROR_INVALID;
    }
    
#ifdef _WIN32
    DWORD wait_ms = (timeout == NULL) ? INFINITE : (DWORD)timeout->timeout_ms;
    DWORD result = WaitForSingleObject(event->event, wait_ms);
    if (result == WAIT_TIMEOUT) {
        return SYNC_ERROR_TIMEOUT;
    } else if (result != WAIT_OBJECT_0) {
        return SYNC_ERROR_UNKNOWN;
    }
    return SYNC_SUCCESS;
#else
    pthread_mutex_lock(&event->mutex);
    
    while (!event->signaled) {
        if (timeout == NULL) {
            pthread_cond_wait(&event->cond, &event->mutex);
        } else {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += timeout->timeout_ms / 1000;
            ts.tv_nsec += (timeout->timeout_ms % 1000) * 1000000;
            if (ts.tv_nsec >= 1000000000) {
                ts.tv_sec++;
                ts.tv_nsec -= 1000000000;
            }
            int rc = pthread_cond_timedwait(&event->cond, &event->mutex, &ts);
            if (rc == ETIMEDOUT) {
                pthread_mutex_unlock(&event->mutex);
                return SYNC_ERROR_TIMEOUT;
            }
        }
    }
    
    if (!event->manual_reset) {
        event->signaled = false;
    }
    
    pthread_mutex_unlock(&event->mutex);
    return SYNC_SUCCESS;
#endif
}

sync_result_t sync_get_stats(void* lock, sync_stats_t* stats) {
    if (lock == NULL || stats == NULL) {
        return SYNC_ERROR_INVALID;
    }
    
    sync_stats_t* lock_stats = NULL;
    sync_type_t type = sync_get_type(lock);
    
    switch (type) {
        case SYNC_TYPE_MUTEX:
            lock_stats = &((sync_mutex_t)lock)->stats;
            break;
        case SYNC_TYPE_RECURSIVE_MUTEX:
            lock_stats = &((sync_recursive_mutex_t)lock)->stats;
            break;
        case SYNC_TYPE_RWLOCK:
            lock_stats = &((sync_rwlock_t)lock)->stats;
            break;
        case SYNC_TYPE_SPINLOCK:
            lock_stats = &((sync_spinlock_t)lock)->stats;
            break;
        case SYNC_TYPE_SEMAPHORE:
            lock_stats = &((sync_semaphore_t)lock)->stats;
            break;
        case SYNC_TYPE_CONDITION:
            lock_stats = &((sync_condition_t)lock)->stats;
            break;
        case SYNC_TYPE_BARRIER:
            lock_stats = &((sync_barrier_t)lock)->stats;
            break;
        case SYNC_TYPE_EVENT:
            lock_stats = &((sync_event_t)lock)->stats;
            break;
        default:
            return SYNC_ERROR_INVALID;
    }
    
    memcpy(stats, lock_stats, sizeof(sync_stats_t));
    return SYNC_SUCCESS;
}

sync_result_t sync_reset_stats(void* lock) {
    if (lock == NULL) {
        return SYNC_ERROR_INVALID;
    }
    
    sync_stats_t* lock_stats = NULL;
    sync_type_t type = sync_get_type(lock);
    
    switch (type) {
        case SYNC_TYPE_MUTEX:
            lock_stats = &((sync_mutex_t)lock)->stats;
            break;
        case SYNC_TYPE_RECURSIVE_MUTEX:
            lock_stats = &((sync_recursive_mutex_t)lock)->stats;
            break;
        case SYNC_TYPE_RWLOCK:
            lock_stats = &((sync_rwlock_t)lock)->stats;
            break;
        case SYNC_TYPE_SPINLOCK:
            lock_stats = &((sync_spinlock_t)lock)->stats;
            break;
        case SYNC_TYPE_SEMAPHORE:
            lock_stats = &((sync_semaphore_t)lock)->stats;
            break;
        case SYNC_TYPE_CONDITION:
            lock_stats = &((sync_condition_t)lock)->stats;
            break;
        case SYNC_TYPE_BARRIER:
            lock_stats = &((sync_barrier_t)lock)->stats;
            break;
        case SYNC_TYPE_EVENT:
            lock_stats = &((sync_event_t)lock)->stats;
            break;
        default:
            return SYNC_ERROR_INVALID;
    }
    
    memset(lock_stats, 0, sizeof(sync_stats_t));
    return SYNC_SUCCESS;
}

sync_result_t sync_check_deadlock(sync_deadlock_info_t* info, size_t max_info_size) {
    (void)info;
    (void)max_info_size;
    // 简化实现，暂不支持死锁检测
    return SYNC_SUCCESS;
}

sync_result_t sync_set_name(void* lock, const char* name) {
    if (lock == NULL) {
        return SYNC_ERROR_INVALID;
    }
    
    char** lock_name = NULL;
    sync_type_t type = sync_get_type(lock);
    
    switch (type) {
        case SYNC_TYPE_MUTEX:
            lock_name = &((sync_mutex_t)lock)->name;
            break;
        case SYNC_TYPE_RECURSIVE_MUTEX:
            lock_name = &((sync_recursive_mutex_t)lock)->name;
            break;
        case SYNC_TYPE_RWLOCK:
            lock_name = &((sync_rwlock_t)lock)->name;
            break;
        case SYNC_TYPE_SPINLOCK:
            lock_name = &((sync_spinlock_t)lock)->name;
            break;
        case SYNC_TYPE_SEMAPHORE:
            lock_name = &((sync_semaphore_t)lock)->name;
            break;
        case SYNC_TYPE_CONDITION:
            lock_name = &((sync_condition_t)lock)->name;
            break;
        case SYNC_TYPE_BARRIER:
            lock_name = &((sync_barrier_t)lock)->name;
            break;
        case SYNC_TYPE_EVENT:
            lock_name = &((sync_event_t)lock)->name;
            break;
        default:
            return SYNC_ERROR_INVALID;
    }
    
    AGENTOS_FREE(*lock_name);
    *lock_name = sync_internal_strdup(name);
    
    return SYNC_SUCCESS;
}

const char* sync_get_name(void* lock) {
    if (lock == NULL) {
        return NULL;
    }
    
    sync_type_t type = sync_get_type(lock);
    
    switch (type) {
        case SYNC_TYPE_MUTEX:
            return ((sync_mutex_t)lock)->name;
        case SYNC_TYPE_RECURSIVE_MUTEX:
            return ((sync_recursive_mutex_t)lock)->name;
        case SYNC_TYPE_RWLOCK:
            return ((sync_rwlock_t)lock)->name;
        case SYNC_TYPE_SPINLOCK:
            return ((sync_spinlock_t)lock)->name;
        case SYNC_TYPE_SEMAPHORE:
            return ((sync_semaphore_t)lock)->name;
        case SYNC_TYPE_CONDITION:
            return ((sync_condition_t)lock)->name;
        case SYNC_TYPE_BARRIER:
            return ((sync_barrier_t)lock)->name;
        case SYNC_TYPE_EVENT:
            return ((sync_event_t)lock)->name;
        default:
            return NULL;
    }
}

uint64_t sync_get_thread_id(void) {
    return sync_internal_get_thread_id();
}

/**
 * @brief 获取锁类型
 * @param lock 锁对象指针（可以是任意锁类型）
 * @param lock_type 锁的实际类型
 * @return 锁类型标识
 * @note 调用者必须确保 lock 和 lock_type 匹配，否则行为未定义
 */
sync_type_t sync_get_type(void* lock, sync_lock_type_t lock_type) {
    if (lock == NULL) {
        return SYNC_TYPE_UNKNOWN;
    }

    switch (lock_type) {
        case SYNC_LOCK_MUTEX:
            return ((struct sync_mutex*)lock)->type;
        case SYNC_LOCK_RECURSIVE_MUTEX:
            return ((struct sync_recursive_mutex*)lock)->type;
        case SYNC_LOCK_RWLOCK:
            return ((struct sync_rwlock*)lock)->type;
        case SYNC_LOCK_SPINLOCK:
            return ((struct sync_spinlock*)lock)->type;
        case SYNC_LOCK_SEMAPHORE:
            return ((struct sync_semaphore*)lock)->type;
        case SYNC_LOCK_CONDITION:
            return ((struct sync_condition*)lock)->type;
        case SYNC_LOCK_BARRIER:
            return ((struct sync_barrier*)lock)->type;
        case SYNC_LOCK_EVENT:
            return ((struct sync_event*)lock)->type;
        default:
            return SYNC_TYPE_UNKNOWN;
    }
}

void sync_sleep(unsigned int ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

uint64_t sync_get_timestamp_ms(void) {
    return sync_internal_get_timestamp_ms();
}

bool sync_atomic_cas(volatile void* ptr, uintptr_t expected, uintptr_t desired) {
#ifdef _WIN32
    return InterlockedCompareExchangePointer((void* volatile*)ptr, (void*)desired, (void*)expected) == (void*)expected;
#else
    return __atomic_compare_exchange_n((uintptr_t*)ptr, &expected, desired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#endif
}

uintptr_t sync_atomic_add(volatile void* ptr, uintptr_t value) {
#ifdef _WIN32
    return InterlockedExchangeAdd((LONG*)ptr, (LONG)value);
#else
    return __atomic_fetch_add((uintptr_t*)ptr, value, __ATOMIC_SEQ_CST);
#endif
}

uintptr_t sync_atomic_sub(volatile void* ptr, uintptr_t value) {
#ifdef _WIN32
    return InterlockedExchangeAdd((LONG*)ptr, -(LONG)value);
#else
    return __atomic_fetch_sub((uintptr_t*)ptr, value, __ATOMIC_SEQ_CST);
#endif
}

uintptr_t sync_atomic_load(volatile void* ptr) {
#ifdef _WIN32
    return InterlockedExchangeAdd((LONG*)ptr, 0);
#else
    return __atomic_load_n((uintptr_t*)ptr, __ATOMIC_SEQ_CST);
#endif
}

void sync_atomic_store(volatile void* ptr, uintptr_t value) {
#ifdef _WIN32
    InterlockedExchange((LONG*)ptr, (LONG)value);
#else
    __atomic_store_n((uintptr_t*)ptr, value, __ATOMIC_SEQ_CST);
#endif
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

    // 设置锁类型标识
    m->type = SYNC_TYPE_RECURSIVE_MUTEX;

    // 设置名称
    if (attr != NULL && attr->name != NULL) {
        m->name = sync_internal_strdup(attr->name);
    }

    // 初始化统计信息
    memset(&m->stats, 0, sizeof(sync_stats_t));
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
/**
 * @file sync_mutex.c
 * @brief 互斥锁同步原语实现
 * 
 * 提供跨平台的互斥锁实现，支持Windows和POSIX系统
 * 
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "sync.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "sync_platform.h"

/**
 * @addtogroup sync_mutex
 * @{
 */

/**
 * @brief 互斥锁内部结构
 */
struct sync_mutex {
#ifdef _WIN32
    CRITICAL_SECTION cs;
    bool initialized;
#else
    pthread_mutex_t mutex;
    bool initialized;
#endif
    sync_type_t type;
    char* name;
    sync_stats_t stats;
    uint64_t last_lock_time;
    uint64_t owner_thread_id;
};

/**
 * @brief 获取当前时间戳（毫秒）
 */
static uint64_t get_timestamp_ms(void) {
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t ts = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return ts / 10000;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
}

/**
 * @brief 获取当前线程ID
 */
static uint64_t get_thread_id(void) {
#ifdef _WIN32
    return GetCurrentThreadId();
#else
    return (uint64_t)pthread_self();
#endif
}

/**
 * @brief 更新统计信息（加锁）
 */
static void update_stats_lock(sync_stats_t* stats, uint64_t wait_time_ms) {
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
 * @brief 更新统计信息（解锁）
 */
static void update_stats_unlock(sync_stats_t* stats) {
    if (stats == NULL) {
        return;
    }
    
    stats->unlock_count++;
}

/**
 * @brief 创建互斥锁
 */
sync_mutex_t* sync_mutex_create(const char* name) {
    sync_mutex_t* mutex = (sync_mutex_t*)malloc(sizeof(sync_mutex_t));
    if (mutex == NULL) {
        return NULL;
    }
    
    memset(mutex, 0, sizeof(sync_mutex_t));
    
#ifdef _WIN32
    mutex->initialized = false;
    if (InitializeCriticalSectionAndSpinCount(&mutex->cs, 4000)) {
        mutex->initialized = true;
    }
#else
    mutex->initialized = false;
    pthread_mutexattr_t attr;
    if (pthread_mutexattr_init(&attr) == 0) {
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
        if (pthread_mutex_init(&mutex->mutex, &attr) == 0) {
            mutex->initialized = true;
        }
        pthread_mutexattr_destroy(&attr);
    }
#endif
    
    if (!mutex->initialized) {
        free(mutex);
        return NULL;
    }
    
    mutex->type = SYNC_TYPE_MUTEX;
    mutex->name = (name != NULL) ? strdup(name) : NULL;
    mutex->last_lock_time = 0;
    mutex->owner_thread_id = 0;
    memset(&mutex->stats, 0, sizeof(sync_stats_t));
    
    return mutex;
}

/**
 * @brief 销毁互斥锁
 */
void sync_mutex_destroy(sync_mutex_t* mutex) {
    if (mutex == NULL) {
        return;
    }
    
    if (mutex->initialized) {
#ifdef _WIN32
        DeleteCriticalSection(&mutex->cs);
#else
        pthread_mutex_destroy(&mutex->mutex);
#endif
    }
    
    if (mutex->name != NULL) {
        free(mutex->name);
        mutex->name = NULL;
    }
    
    free(mutex);
}

/**
 * @brief 互斥锁加锁
 */
sync_result_t sync_mutex_lock(sync_mutex_t* mutex) {
    if (mutex == NULL || !mutex->initialized) {
        return SYNC_ERROR_INVALID;
    }
    
    uint64_t start_time = get_timestamp_ms();
    int result = 0;
    
#ifdef _WIN32
    EnterCriticalSection(&mutex->cs);
#else
    result = pthread_mutex_lock(&mutex->mutex);
#endif
    
    if (result == 0) {
        uint64_t wait_time = get_timestamp_ms() - start_time;
        update_stats_lock(&mutex->stats, wait_time);
        mutex->last_lock_time = get_timestamp_ms();
        mutex->owner_thread_id = get_thread_id();
        return SYNC_OK;
    } else {
#ifdef _WIN32
        return SYNC_ERROR_UNKNOWN;
#else
        return (result == EDEADLK) ? SYNC_ERROR_DEADLOCK : SYNC_ERROR_UNKNOWN;
#endif
    }
}

/**
 * @brief 互斥锁尝试加锁
 */
sync_result_t sync_mutex_trylock(sync_mutex_t* mutex) {
    if (mutex == NULL || !mutex->initialized) {
        return SYNC_ERROR_INVALID;
    }
    
    int result = 0;
    
#ifdef _WIN32
    if (TryEnterCriticalSection(&mutex->cs)) {
        update_stats_lock(&mutex->stats, 0);
        mutex->last_lock_time = get_timestamp_ms();
        mutex->owner_thread_id = get_thread_id();
        return SYNC_OK;
    } else {
        return SYNC_ERROR_BUSY;
    }
#else
    result = pthread_mutex_trylock(&mutex->mutex);
    if (result == 0) {
        update_stats_lock(&mutex->stats, 0);
        mutex->last_lock_time = get_timestamp_ms();
        mutex->owner_thread_id = get_thread_id();
        return SYNC_OK;
    } else if (result == EBUSY) {
        return SYNC_ERROR_BUSY;
    } else {
        return SYNC_ERROR_UNKNOWN;
    }
#endif
}

/**
 * @brief 互斥锁解锁
 */
sync_result_t sync_mutex_unlock(sync_mutex_t* mutex) {
    if (mutex == NULL || !mutex->initialized) {
        return SYNC_ERROR_INVALID;
    }
    
#ifdef _WIN32
    LeaveCriticalSection(&mutex->cs);
#else
    pthread_mutex_unlock(&mutex->mutex);
#endif
    
    update_stats_unlock(&mutex->stats);
    mutex->last_lock_time = 0;
    mutex->owner_thread_id = 0;
    
    return SYNC_OK;
}

/**
 * @brief 获取互斥锁统计信息
 */
sync_result_t sync_mutex_get_stats(const sync_mutex_t* mutex, sync_stats_t* stats) {
    if (mutex == NULL || stats == NULL) {
        return SYNC_ERROR_INVALID;
    }
    
    memcpy(stats, &mutex->stats, sizeof(sync_stats_t));
    return SYNC_OK;
}

/**
 * @brief 重置互斥锁统计信息
 */
sync_result_t sync_mutex_reset_stats(sync_mutex_t* mutex) {
    if (mutex == NULL) {
        return SYNC_ERROR_INVALID;
    }
    
    memset(&mutex->stats, 0, sizeof(sync_stats_t));
    return SYNC_OK;
}

/**
 * @brief 获取互斥锁类型
 */
sync_type_t sync_mutex_get_type(const sync_mutex_t* mutex) {
    if (mutex == NULL) {
        return SYNC_TYPE_UNKNOWN;
    }
    return mutex->type;
}

/**
 * @brief 获取互斥锁名称
 */
const char* sync_mutex_get_name(const sync_mutex_t* mutex) {
    if (mutex == NULL) {
        return NULL;
    }
    return mutex->name;
}

/** @} */

/**
 * @file task.c
 * @brief 轻量级任务调度实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "task.h"
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#include <synchapi.h>
#else
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#endif

/* ==================== 内部结构定义 ==================== */

#ifdef _WIN32
/* Windows 实现 */
struct agentos_lite_mutex {
    CRITICAL_SECTION cs;
};

struct agentos_lite_cond {
    CONDITION_VARIABLE cv;
};

struct agentos_lite_thread {
    HANDLE handle;
    DWORD id;
};

#else
/* POSIX 实现 */
struct agentos_lite_mutex {
    pthread_mutex_t mutex;
};

struct agentos_lite_cond {
    pthread_cond_t cond;
};

struct agentos_lite_thread {
    pthread_t thread;
};

#endif

/* ==================== 互斥锁实现 ==================== */

/**
 * @brief 创建互斥锁
 */
AGENTOS_LITE_API agentos_lite_mutex_t* agentos_lite_mutex_create(void) {
    agentos_lite_mutex_t* mutex = (agentos_lite_mutex_t*)malloc(sizeof(agentos_lite_mutex_t));
    if (!mutex) return NULL;
    
#ifdef _WIN32
    InitializeCriticalSection(&mutex->cs);
#else
    if (pthread_mutex_init(&mutex->mutex, NULL) != 0) {
        free(mutex);
        return NULL;
    }
#endif
    
    return mutex;
}

/**
 * @brief 销毁互斥锁
 */
AGENTOS_LITE_API void agentos_lite_mutex_destroy(agentos_lite_mutex_t* mutex) {
    if (!mutex) return;
    
#ifdef _WIN32
    DeleteCriticalSection(&mutex->cs);
#else
    pthread_mutex_destroy(&mutex->mutex);
#endif
    
    free(mutex);
}

/**
 * @brief 加锁
 */
AGENTOS_LITE_API agentos_lite_error_t agentos_lite_mutex_lock(agentos_lite_mutex_t* mutex) {
    if (!mutex) return AGENTOS_LITE_EINVAL;
    
#ifdef _WIN32
    EnterCriticalSection(&mutex->cs);
    return AGENTOS_LITE_SUCCESS;
#else
    int ret = pthread_mutex_lock(&mutex->mutex);
    if (ret == 0) return AGENTOS_LITE_SUCCESS;
    if (ret == EDEADLK) return AGENTOS_LITE_EBUSY;
    return AGENTOS_LITE_EINVAL;
#endif
}

/**
 * @brief 尝试加锁
 */
AGENTOS_LITE_API agentos_lite_error_t agentos_lite_mutex_trylock(agentos_lite_mutex_t* mutex) {
    if (!mutex) return AGENTOS_LITE_EINVAL;
    
#ifdef _WIN32
    if (TryEnterCriticalSection(&mutex->cs)) {
        return AGENTOS_LITE_SUCCESS;
    }
    return AGENTOS_LITE_EBUSY;
#else
    int ret = pthread_mutex_trylock(&mutex->mutex);
    if (ret == 0) return AGENTOS_LITE_SUCCESS;
    if (ret == EBUSY) return AGENTOS_LITE_EBUSY;
    return AGENTOS_LITE_EINVAL;
#endif
}

/**
 * @brief 解锁
 */
AGENTOS_LITE_API agentos_lite_error_t agentos_lite_mutex_unlock(agentos_lite_mutex_t* mutex) {
    if (!mutex) return AGENTOS_LITE_EINVAL;
    
#ifdef _WIN32
    LeaveCriticalSection(&mutex->cs);
    return AGENTOS_LITE_SUCCESS;
#else
    int ret = pthread_mutex_unlock(&mutex->mutex);
    if (ret == 0) return AGENTOS_LITE_SUCCESS;
    return AGENTOS_LITE_EINVAL;
#endif
}

/* ==================== 条件变量实现 ==================== */

/**
 * @brief 创建条件变量
 */
AGENTOS_LITE_API agentos_lite_cond_t* agentos_lite_cond_create(void) {
    agentos_lite_cond_t* cond = (agentos_lite_cond_t*)malloc(sizeof(agentos_lite_cond_t));
    if (!cond) return NULL;
    
#ifdef _WIN32
    InitializeConditionVariable(&cond->cv);
#else
    if (pthread_cond_init(&cond->cond, NULL) != 0) {
        free(cond);
        return NULL;
    }
#endif
    
    return cond;
}

/**
 * @brief 销毁条件变量
 */
AGENTOS_LITE_API void agentos_lite_cond_destroy(agentos_lite_cond_t* cond) {
    if (!cond) return;
    
#ifdef _WIN32
    /* Windows条件变量无需销毁 */
#else
    pthread_cond_destroy(&cond->cond);
#endif
    
    free(cond);
}

/**
 * @brief 等待条件变量
 */
AGENTOS_LITE_API agentos_lite_error_t agentos_lite_cond_wait(
    agentos_lite_cond_t* cond, agentos_lite_mutex_t* mutex, uint32_t timeout_ms) {
    if (!cond || !mutex) return AGENTOS_LITE_EINVAL;
    
#ifdef _WIN32
    BOOL success;
    if (timeout_ms == 0) {
        /* 无限等待 */
        success = SleepConditionVariableCS(&cond->cv, &mutex->cs, INFINITE);
    } else {
        success = SleepConditionVariableCS(&cond->cv, &mutex->cs, timeout_ms);
    }
    
    if (success) return AGENTOS_LITE_SUCCESS;
    if (GetLastError() == ERROR_TIMEOUT) return AGENTOS_LITE_ETIMEDOUT;
    return AGENTOS_LITE_EINVAL;
#else
    if (timeout_ms == 0) {
        /* 无限等待 */
        int ret = pthread_cond_wait(&cond->cond, &mutex->mutex);
        if (ret == 0) return AGENTOS_LITE_SUCCESS;
        return AGENTOS_LITE_EINVAL;
    } else {
        /* 超时等待 */
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout_ms / 1000;
        ts.tv_nsec += (timeout_ms % 1000) * 1000000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec += 1;
            ts.tv_nsec -= 1000000000;
        }
        
        int ret = pthread_cond_timedwait(&cond->cond, &mutex->mutex, &ts);
        if (ret == 0) return AGENTOS_LITE_SUCCESS;
        if (ret == ETIMEDOUT) return AGENTOS_LITE_ETIMEDOUT;
        return AGENTOS_LITE_EINVAL;
    }
#endif
}

/**
 * @brief 唤醒一个等待线程
 */
AGENTOS_LITE_API agentos_lite_error_t agentos_lite_cond_signal(agentos_lite_cond_t* cond) {
    if (!cond) return AGENTOS_LITE_EINVAL;
    
#ifdef _WIN32
    WakeConditionVariable(&cond->cv);
#else
    pthread_cond_signal(&cond->cond);
#endif
    
    return AGENTOS_LITE_SUCCESS;
}

/**
 * @brief 唤醒所有等待线程
 */
AGENTOS_LITE_API agentos_lite_error_t agentos_lite_cond_broadcast(agentos_lite_cond_t* cond) {
    if (!cond) return AGENTOS_LITE_EINVAL;
    
#ifdef _WIN32
    WakeAllConditionVariable(&cond->cv);
#else
    pthread_cond_broadcast(&cond->cond);
#endif
    
    return AGENTOS_LITE_SUCCESS;
}

/* ==================== 线程实现 ==================== */

/**
 * @brief 线程入口包装器
 */
#ifdef _WIN32
static DWORD WINAPI thread_wrapper(LPVOID arg) {
    struct {
        void (*entry)(void*);
        void* arg;
    }* params = (typeof(params))arg;
    
    params->entry(params->arg);
    free(params);
    return 0;
}
#else
static void* thread_wrapper(void* arg) {
    struct {
        void (*entry)(void*);
        void* arg;
    }* params = (typeof(params))arg;
    
    params->entry(params->arg);
    free(params);
    return NULL;
}
#endif

/**
 * @brief 创建线程
 */
AGENTOS_LITE_API agentos_lite_error_t agentos_lite_thread_create(
    agentos_lite_thread_t* thread, const agentos_lite_thread_attr_t* attr,
    void (*entry)(void*), void* arg) {
    if (!thread || !entry) return AGENTOS_LITE_EINVAL;
    
    /* 分配参数结构 */
    struct {
        void (*entry)(void*);
        void* arg;
    }* params = (typeof(params))malloc(sizeof(*params));
    if (!params) return AGENTOS_LITE_ENOMEM;
    
    params->entry = entry;
    params->arg = arg;
    
#ifdef _WIN32
    /* Windows 线程创建 */
    DWORD thread_id;
    HANDLE handle = CreateThread(NULL, 0, thread_wrapper, params, 0, &thread_id);
    if (!handle) {
        free(params);
        return AGENTOS_LITE_EINVAL;
    }
    
    thread->handle = handle;
    thread->id = thread_id;
    
    /* 设置线程优先级（简化实现） */
    if (attr && attr->priority != AGENTOS_LITE_TASK_PRIORITY_NORMAL) {
        int win_priority = THREAD_PRIORITY_NORMAL;
        switch (attr->priority) {
            case AGENTOS_LITE_TASK_PRIORITY_LOW: win_priority = THREAD_PRIORITY_BELOW_NORMAL; break;
            case AGENTOS_LITE_TASK_PRIORITY_HIGH: win_priority = THREAD_PRIORITY_ABOVE_NORMAL; break;
            case AGENTOS_LITE_TASK_PRIORITY_REALTIME: win_priority = THREAD_PRIORITY_TIME_CRITICAL; break;
        }
        SetThreadPriority(handle, win_priority);
    }
#else
    /* POSIX 线程创建 */
    pthread_attr_t pthread_attr;
    pthread_attr_init(&pthread_attr);
    
    if (attr) {
        if (attr->stack_size > 0) {
            pthread_attr_setstacksize(&pthread_attr, attr->stack_size);
        }
        
        /* 设置调度策略（简化实现） */
        struct sched_param sched_param = {0};
        int policy = SCHED_OTHER;
        
        switch (attr->priority) {
            case AGENTOS_LITE_TASK_PRIORITY_LOW:
                sched_param.sched_priority = sched_get_priority_min(SCHED_OTHER);
                break;
            case AGENTOS_LITE_TASK_PRIORITY_NORMAL:
                sched_param.sched_priority = 0; /* 默认 */
                break;
            case AGENTOS_LITE_TASK_PRIORITY_HIGH:
                sched_param.sched_priority = sched_get_priority_max(SCHED_OTHER) / 2;
                break;
            case AGENTOS_LITE_TASK_PRIORITY_REALTIME:
                policy = SCHED_RR;
                sched_param.sched_priority = sched_get_priority_max(SCHED_RR) / 2;
                break;
        }
        
        pthread_attr_setschedpolicy(&pthread_attr, policy);
        pthread_attr_setschedparam(&pthread_attr, &sched_param);
    }
    
    int ret = pthread_create(&thread->thread, &pthread_attr, thread_wrapper, params);
    pthread_attr_destroy(&pthread_attr);
    
    if (ret != 0) {
        free(params);
        return AGENTOS_LITE_EINVAL;
    }
    
    /* 设置线程名称（如果支持） */
#ifdef __APPLE__
    if (attr && attr->name) {
        pthread_setname_np(attr->name);
    }
#elif defined(__linux__)
    if (attr && attr->name) {
        pthread_setname_np(thread->thread, attr->name);
    }
#endif
#endif
    
    return AGENTOS_LITE_SUCCESS;
}

/**
 * @brief 等待线程结束
 */
AGENTOS_LITE_API agentos_lite_error_t agentos_lite_thread_join(agentos_lite_thread_t thread) {
#ifdef _WIN32
    if (WaitForSingleObject(thread.handle, INFINITE) == WAIT_OBJECT_0) {
        CloseHandle(thread.handle);
        return AGENTOS_LITE_SUCCESS;
    }
    return AGENTOS_LITE_EINVAL;
#else
    int ret = pthread_join(thread.thread, NULL);
    if (ret == 0) return AGENTOS_LITE_SUCCESS;
    return AGENTOS_LITE_EINVAL;
#endif
}

/**
 * @brief 线程休眠
 */
AGENTOS_LITE_API void agentos_lite_task_sleep(uint32_t ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}
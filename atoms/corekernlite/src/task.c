/**
 * @file task.c
 * @brief AgentOS Lite 任务调度实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * 
 * 提供基于原生线程的任务调度功能：
 * - 互斥锁和条件变量
 * - 线程创建和管理
 * - 任务优先级
 */

#include "../include/task.h"
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
    #include <process.h>
    
    struct agentos_lite_mutex {
        CRITICAL_SECTION cs;
    };
    
    struct agentos_lite_cond {
        CONDITION_VARIABLE cv;
    };
    
    struct agentos_lite_thread {
        HANDLE handle;
        unsigned int id;
    };
    
    typedef struct {
        void (*func)(void*);
        void* arg;
    } thread_wrapper_arg_t;
    
    static unsigned int __stdcall thread_wrapper(void* arg) {
        thread_wrapper_arg_t* wrapper = (thread_wrapper_arg_t*)arg;
        if (wrapper && wrapper->func) {
            wrapper->func(wrapper->arg);
        }
        free(wrapper);
        return 0;
    }
    
#else
    #include <pthread.h>
    #include <unistd.h>
    #include <time.h>
    #include <errno.h>
    #include <sys/time.h>
    
    struct agentos_lite_mutex {
        pthread_mutex_t mutex;
    };
    
    struct agentos_lite_cond {
        pthread_cond_t cond;
    };
    
    struct agentos_lite_thread {
        pthread_t thread;
        int valid;
    };
    
    typedef struct {
        void (*func)(void*);
        void* arg;
    } thread_wrapper_arg_t;
    
    static void* thread_wrapper(void* arg) {
        thread_wrapper_arg_t* wrapper = (thread_wrapper_arg_t*)arg;
        if (wrapper && wrapper->func) {
            wrapper->func(wrapper->arg);
        }
        free(wrapper);
        return NULL;
    }
    
#endif

static int g_task_initialized = 0;

AGENTOS_LITE_API agentos_lite_error_t agentos_lite_task_init(void) {
    if (g_task_initialized) {
        return AGENTOS_LITE_SUCCESS;
    }
    g_task_initialized = 1;
    return AGENTOS_LITE_SUCCESS;
}

AGENTOS_LITE_API void agentos_lite_task_cleanup(void) {
    g_task_initialized = 0;
}

AGENTOS_LITE_API agentos_lite_mutex_t* agentos_lite_mutex_create(void) {
    agentos_lite_mutex_t* mutex = (agentos_lite_mutex_t*)malloc(sizeof(agentos_lite_mutex_t));
    if (!mutex) {
        return NULL;
    }
    
#if defined(_WIN32) || defined(_WIN64)
    InitializeCriticalSection(&mutex->cs);
#else
    if (pthread_mutex_init(&mutex->mutex, NULL) != 0) {
        free(mutex);
        return NULL;
    }
#endif
    
    return mutex;
}

AGENTOS_LITE_API void agentos_lite_mutex_destroy(agentos_lite_mutex_t* mutex) {
    if (!mutex) {
        return;
    }
    
#if defined(_WIN32) || defined(_WIN64)
    DeleteCriticalSection(&mutex->cs);
#else
    pthread_mutex_destroy(&mutex->mutex);
#endif
    
    free(mutex);
}

AGENTOS_LITE_API void agentos_lite_mutex_lock(agentos_lite_mutex_t* mutex) {
    if (!mutex) {
        return;
    }
    
#if defined(_WIN32) || defined(_WIN64)
    EnterCriticalSection(&mutex->cs);
#else
    pthread_mutex_lock(&mutex->mutex);
#endif
}

AGENTOS_LITE_API int agentos_lite_mutex_trylock(agentos_lite_mutex_t* mutex) {
    if (!mutex) {
        return -1;
    }
    
#if defined(_WIN32) || defined(_WIN64)
    return TryEnterCriticalSection(&mutex->cs) ? 0 : -1;
#else
    return pthread_mutex_trylock(&mutex->mutex) == 0 ? 0 : -1;
#endif
}

AGENTOS_LITE_API void agentos_lite_mutex_unlock(agentos_lite_mutex_t* mutex) {
    if (!mutex) {
        return;
    }
    
#if defined(_WIN32) || defined(_WIN64)
    LeaveCriticalSection(&mutex->cs);
#else
    pthread_mutex_unlock(&mutex->mutex);
#endif
}

AGENTOS_LITE_API agentos_lite_cond_t* agentos_lite_cond_create(void) {
    agentos_lite_cond_t* cond = (agentos_lite_cond_t*)malloc(sizeof(agentos_lite_cond_t));
    if (!cond) {
        return NULL;
    }
    
#if defined(_WIN32) || defined(_WIN64)
    InitializeConditionVariable(&cond->cv);
#else
    if (pthread_cond_init(&cond->cond, NULL) != 0) {
        free(cond);
        return NULL;
    }
#endif
    
    return cond;
}

AGENTOS_LITE_API void agentos_lite_cond_destroy(agentos_lite_cond_t* cond) {
    if (!cond) {
        return;
    }
    
#if defined(_WIN32) || defined(_WIN64)
    (void)cond;
#else
    pthread_cond_destroy(&cond->cond);
#endif
    
    free(cond);
}

AGENTOS_LITE_API agentos_lite_error_t agentos_lite_cond_wait(
    agentos_lite_cond_t* cond,
    agentos_lite_mutex_t* mutex,
    uint32_t timeout_ms) {
    if (!cond || !mutex) {
        return AGENTOS_LITE_EINVAL;
    }
    
#if defined(_WIN32) || defined(_WIN64)
    DWORD ms = timeout_ms == 0 ? INFINITE : timeout_ms;
    BOOL result = SleepConditionVariableCS(&cond->cv, &mutex->cs, ms);
    if (!result) {
        return AGENTOS_LITE_ETIMEDOUT;
    }
    return AGENTOS_LITE_SUCCESS;
#else
    if (timeout_ms == 0) {
        return pthread_cond_wait(&cond->cond, &mutex->mutex) == 0 ? 
               AGENTOS_LITE_SUCCESS : AGENTOS_LITE_EIO;
    }
    
    struct timeval now;
    gettimeofday(&now, NULL);
    
    struct timespec ts;
    ts.tv_sec = now.tv_sec + timeout_ms / 1000;
    ts.tv_nsec = now.tv_usec * 1000 + (timeout_ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }
    
    int ret = pthread_cond_timedwait(&cond->cond, &mutex->mutex, &ts);
    if (ret == ETIMEDOUT) {
        return AGENTOS_LITE_ETIMEDOUT;
    }
    return ret == 0 ? AGENTOS_LITE_SUCCESS : AGENTOS_LITE_EIO;
#endif
}

AGENTOS_LITE_API void agentos_lite_cond_signal(agentos_lite_cond_t* cond) {
    if (!cond) {
        return;
    }
    
#if defined(_WIN32) || defined(_WIN64)
    WakeConditionVariable(&cond->cv);
#else
    pthread_cond_signal(&cond->cond);
#endif
}

AGENTOS_LITE_API void agentos_lite_cond_broadcast(agentos_lite_cond_t* cond) {
    if (!cond) {
        return;
    }
    
#if defined(_WIN32) || defined(_WIN64)
    WakeAllConditionVariable(&cond->cv);
#else
    pthread_cond_broadcast(&cond->cond);
#endif
}

AGENTOS_LITE_API agentos_lite_error_t agentos_lite_thread_create(
    agentos_lite_thread_t* thread,
    const agentos_lite_thread_attr_t* attr,
    void (*func)(void*),
    void* arg) {
    if (!thread || !func) {
        return AGENTOS_LITE_EINVAL;
    }
    
    thread_wrapper_arg_t* wrapper = (thread_wrapper_arg_t*)malloc(sizeof(thread_wrapper_arg_t));
    if (!wrapper) {
        return AGENTOS_LITE_ENOMEM;
    }
    wrapper->func = func;
    wrapper->arg = arg;
    
#if defined(_WIN32) || defined(_WIN64)
    unsigned int stack_size = attr && attr->stack_size ? (unsigned int)attr->stack_size : 0;
    thread->handle = (HANDLE)_beginthreadex(NULL, stack_size, thread_wrapper, wrapper, 0, &thread->id);
    if (!thread->handle) {
        free(wrapper);
        return AGENTOS_LITE_EIO;
    }
    
    if (attr && attr->priority >= AGENTOS_LITE_TASK_PRIORITY_MIN && 
        attr->priority <= AGENTOS_LITE_TASK_PRIORITY_MAX) {
        int win_priority = THREAD_PRIORITY_NORMAL;
        if (attr->priority <= AGENTOS_LITE_TASK_PRIORITY_LOW) {
            win_priority = THREAD_PRIORITY_BELOW_NORMAL;
        } else if (attr->priority >= AGENTOS_LITE_TASK_PRIORITY_HIGH) {
            win_priority = THREAD_PRIORITY_ABOVE_NORMAL;
        }
        SetThreadPriority(thread->handle, win_priority);
    }
#else
    pthread_attr_t pthread_attr;
    pthread_attr_init(&pthread_attr);
    
    if (attr && attr->stack_size) {
        pthread_attr_setstacksize(&pthread_attr, attr->stack_size);
    }
    
    int ret = pthread_create(&thread->thread, &pthread_attr, thread_wrapper, wrapper);
    pthread_attr_destroy(&pthread_attr);
    
    if (ret != 0) {
        free(wrapper);
        return AGENTOS_LITE_EIO;
    }
    thread->valid = 1;
    
    if (attr && attr->priority >= AGENTOS_LITE_TASK_PRIORITY_MIN && 
        attr->priority <= AGENTOS_LITE_TASK_PRIORITY_MAX) {
        struct sched_param param;
        param.sched_priority = attr->priority;
        pthread_setschedparam(thread->thread, SCHED_OTHER, &param);
    }
#endif
    
    return AGENTOS_LITE_SUCCESS;
}

AGENTOS_LITE_API agentos_lite_error_t agentos_lite_thread_join(agentos_lite_thread_t thread) {
#if defined(_WIN32) || defined(_WIN64)
    if (!thread.handle) {
        return AGENTOS_LITE_EINVAL;
    }
    WaitForSingleObject(thread.handle, INFINITE);
    CloseHandle(thread.handle);
    return AGENTOS_LITE_SUCCESS;
#else
    if (!thread.valid) {
        return AGENTOS_LITE_EINVAL;
    }
    return pthread_join(thread.thread, NULL) == 0 ? 
           AGENTOS_LITE_SUCCESS : AGENTOS_LITE_EIO;
#endif
}

AGENTOS_LITE_API agentos_lite_task_id_t agentos_lite_task_self(void) {
#if defined(_WIN32) || defined(_WIN64)
    return (agentos_lite_task_id_t)GetCurrentThreadId();
#else
    return (agentos_lite_task_id_t)pthread_self();
#endif
}

AGENTOS_LITE_API void agentos_lite_task_sleep(uint32_t ms) {
#if defined(_WIN32) || defined(_WIN64)
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

AGENTOS_LITE_API void agentos_lite_task_yield(void) {
#if defined(_WIN32) || defined(_WIN64)
    SwitchToThread();
#else
    sched_yield();
#endif
}

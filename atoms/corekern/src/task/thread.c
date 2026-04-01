/**
 * @file thread.c
 * @brief 线程管理辅助函数实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "task.h"
#include "mem.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../commons/utils/memory/include/memory_compat.h"
#include "../../../commons/utils/string/include/string_compat.h"
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

/**
 * @brief 线程内部结构（跨平台包装）
 */
struct agentos_thread {
#ifdef _WIN32
    HANDLE handle;
    DWORD id;
#else
    pthread_t thread;
#endif
    agentos_thread_func_t func;
    void* arg;
    int started;
};

/**
 * @brief 线程函数包装器
 */
#ifdef _WIN32
static unsigned __stdcall thread_wrapper(void* arg) {
#else
static void* thread_wrapper(void* arg) {
#endif
    agentos_thread_t* thread = (agentos_thread_t*)arg;
    if (!thread || !thread->func) {
#ifdef _WIN32
        return 1;
#else
        return NULL;
#endif
    }

    void* result = thread->func(thread->arg);
    return result;
}

/**
 * @brief 创建线程
 */
agentos_error_t agentos_thread_create(
    agentos_thread_t* thread,
    const agentos_thread_attr_t* attr,
    agentos_thread_func_t func,
    void* arg) {
    if (!thread || !func) {
        return AGENTOS_EINVAL;
    }

    thread->func = func;
    thread->arg = arg;
    thread->started = 0;

#ifdef _WIN32
    HANDLE hThread = (HANDLE)_beginthreadex(
        NULL,
        attr ? attr->stack_size : 0,
        thread_wrapper,
        thread,
        0,
        &thread->id
    );

    if (hThread == NULL) {
        return AGENTOS_EAGAIN;
    }

    thread->handle = hThread;
#else
    pthread_attr_t pthread_attr;
    pthread_attr_init(&pthread_attr);

    if (attr) {
        if (attr->stack_size > 0) {
            pthread_attr_setstacksize(&pthread_attr, attr->stack_size);
        }
        pthread_attr_setdetachstate(&pthread_attr,
            attr->detach_state ? PTHREAD_CREATE_DETACHED : PTHREAD_CREATE_JOINABLE);
    }

    int ret = pthread_create(&thread->thread, &pthread_attr, thread_wrapper, thread);
    pthread_attr_destroy(&pthread_attr);

    if (ret != 0) {
        return AGENTOS_EAGAIN;
    }
#endif

    thread->started = 1;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 等待线程结束
 */
agentos_error_t agentos_thread_join(agentos_thread_t thread, void** retval) {
    if (!thread.started) {
        return AGENTOS_EINVAL;
    }

#ifdef _WIN32
    if (WaitForSingleObject(thread.handle, INFINITE) != WAIT_OBJECT_0) {
        return AGENTOS_ERROR;
    }
    CloseHandle(thread.handle);
    if (retval) {
        *retval = NULL;
    }
#else
    void* thread_retval = NULL;
    int ret = pthread_join(thread.thread, &thread_retval);
    if (ret != 0) {
        return AGENTOS_ERROR;
    }
    if (retval) {
        *retval = thread_retval;
    }
#endif

    return AGENTOS_SUCCESS;
}

/**
 * @brief 获取当前线程 ID
 */
agentos_thread_id_t agentos_thread_self(void) {
#ifdef _WIN32
    return (agentos_thread_id_t)GetCurrentThreadId();
#else
    return (agentos_thread_id_t)pthread_self();
#endif
}

/**
 * @brief 线程休眠
 */
void agentos_thread_sleep(uint32_t ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

/**
 * @brief 线程让出 CPU
 */
void agentos_thread_yield(void) {
#ifdef _WIN32
    SwitchToThread();
#else
    sched_yield();
#endif
}

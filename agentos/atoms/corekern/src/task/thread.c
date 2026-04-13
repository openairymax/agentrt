/**
 * @file thread.c
 * @brief 线程管理辅助函数实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "task.h"
#include "mem.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../../commons/utils/memory/include/memory_compat.h"
#include "../../../../commons/utils/string/include/string_compat.h"
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
 * @brief 线程函数包装器
 */
#ifdef _WIN32
static unsigned __stdcall thread_wrapper(void* arg) {
    void (*func)(void*) = (void (*)(void*))((void**)arg)[0];
    void* arg_val = ((void**)arg)[1];
    AGENTOS_FREE(arg);
    func(arg_val);
    return 0;
#else
static void* thread_wrapper(void* arg) {
    void (*func)(void*) = (void (*)(void*))((void**)arg)[0];
    void* arg_val = ((void**)arg)[1];
    AGENTOS_FREE(arg);
    func(arg_val);
    return NULL;
#endif
}

/**
 * @brief 创建线程
 */
agentos_error_t agentos_thread_create(
    agentos_thread_t* thread,
    const agentos_thread_attr_t* attr,
    void (*func)(void*),
    void* arg) {
    if (!thread || !func) {
        return AGENTOS_EINVAL;
    }

#ifdef _WIN32
    void** thread_args = (void**)AGENTOS_MALLOC(2 * sizeof(void*));
    if (!thread_args) {
        return AGENTOS_ENOMEM;
    }
    thread_args[0] = (void*)func;
    thread_args[1] = arg;

    HANDLE hThread = (HANDLE)_beginthreadex(
        NULL,
        (unsigned)(attr ? attr->stack_size : 0),
        thread_wrapper,
        thread_args,
        0,
        NULL
    );

    if (hThread == NULL) {
        AGENTOS_FREE(thread_args);
        return AGENTOS_ERESOURCE;
    }

    *thread = hThread;
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

    void** thread_args = (void**)AGENTOS_MALLOC(2 * sizeof(void*));
    if (!thread_args) {
        pthread_attr_destroy(&pthread_attr);
        return AGENTOS_ENOMEM;
    }
    thread_args[0] = (void*)func;
    thread_args[1] = arg;

    int ret = pthread_create(thread, &pthread_attr, thread_wrapper, thread_args);
    pthread_attr_destroy(&pthread_attr);

    if (ret != 0) {
        AGENTOS_FREE(thread_args);
        return AGENTOS_ERESOURCE;
    }
#endif

    return AGENTOS_SUCCESS;
}

/**
 * @brief 等待线程结束
 */
agentos_error_t agentos_thread_join(agentos_thread_t thread, void** retval) {
#ifdef _WIN32
    if (WaitForSingleObject(thread, INFINITE) != WAIT_OBJECT_0) {
        return AGENTOS_ERESOURCE;
    }
    CloseHandle(thread);
    if (retval) {
        *retval = NULL;
    }
#else
    void* thread_retval = NULL;
    int ret = pthread_join(thread, &thread_retval);
    if (ret != 0) {
        return AGENTOS_ERESOURCE;
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

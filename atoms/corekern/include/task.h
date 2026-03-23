/**
 * @file task.h
 * @brief 任务调度接口（基于系统原生线程）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_TASK_H
#define AGENTOS_TASK_H

#include <stdint.h>
#include <stddef.h>
#include "error.h"
#include "export.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t agentos_task_id_t;

typedef struct agentos_mutex agentos_mutex_t;
typedef struct agentos_cond agentos_cond_t;

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
    typedef HANDLE agentos_thread_t;
#else
    typedef struct agentos_thread agentos_thread_t;
#endif

#define AGENTOS_TASK_PRIORITY_MIN     0
#define AGENTOS_TASK_PRIORITY_LOW     25
#define AGENTOS_TASK_PRIORITY_NORMAL  50
#define AGENTOS_TASK_PRIORITY_HIGH    75
#define AGENTOS_TASK_PRIORITY_MAX     100

typedef enum {
    AGENTOS_TASK_STATE_CREATED,
    AGENTOS_TASK_STATE_READY,
    AGENTOS_TASK_STATE_RUNNING,
    AGENTOS_TASK_STATE_BLOCKED,
    AGENTOS_TASK_STATE_TERMINATED
} agentos_task_state_t;

typedef struct {
    const char* name;
    int priority;
    size_t stack_size;
} agentos_thread_attr_t;

AGENTOS_API agentos_error_t agentos_task_init(void);

AGENTOS_API agentos_mutex_t* agentos_mutex_create(void);

AGENTOS_API void agentos_mutex_destroy(agentos_mutex_t* mutex);

AGENTOS_API void agentos_mutex_lock(agentos_mutex_t* mutex);

AGENTOS_API int agentos_mutex_trylock(agentos_mutex_t* mutex);

AGENTOS_API void agentos_mutex_unlock(agentos_mutex_t* mutex);

AGENTOS_API agentos_cond_t* agentos_cond_create(void);

AGENTOS_API void agentos_cond_destroy(agentos_cond_t* cond);

AGENTOS_API agentos_error_t agentos_cond_wait(
    agentos_cond_t* cond,
    agentos_mutex_t* mutex,
    uint32_t timeout_ms);

AGENTOS_API void agentos_cond_signal(agentos_cond_t* cond);

AGENTOS_API void agentos_cond_broadcast(agentos_cond_t* cond);

AGENTOS_API agentos_error_t agentos_thread_create(
    agentos_thread_t* thread,
    const agentos_thread_attr_t* attr,
    void (*func)(void*),
    void* arg);

AGENTOS_API agentos_error_t agentos_thread_join(agentos_thread_t thread, void** retval);

AGENTOS_API agentos_task_id_t agentos_task_self(void);

AGENTOS_API void agentos_task_sleep(uint32_t ms);

AGENTOS_API void agentos_task_yield(void);

AGENTOS_API agentos_error_t agentos_task_set_priority(agentos_task_id_t tid, int priority);

AGENTOS_API agentos_error_t agentos_task_get_priority(agentos_task_id_t tid, int* out_priority);

AGENTOS_API agentos_error_t agentos_task_get_state(agentos_task_id_t tid, agentos_task_state_t* out_state);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_TASK_H */

/**
 * @file scheduler.c
 * @brief 任务调度器（基于系统原生线程）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "task.h"
#include "mem.h"
#include "time.h"
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)

#include <windows.h>

typedef struct {
    HANDLE handle;
    agentos_task_id_t id;
    char name[64];
    int priority;
    void* (*entry)(void*);
    void* arg;
    void* retval;
    volatile agentos_task_state_t state;
} task_info_t;

static task_info_t* task_table[1024];
static uint32_t task_count = 0;
static agentos_mutex_t* task_table_lock = NULL;
static volatile uint64_t next_task_id = 1;

static uint64_t fetch_add_task_id(void) {
    return InterlockedIncrement64((volatile LONG64*)&next_task_id) - 1;
}

DWORD WINAPI thread_entry(LPVOID param) {
    task_info_t* info = (task_info_t*)param;
    info->state = AGENTOS_TASK_STATE_RUNNING;
    info->retval = info->entry(info->arg);
    info->state = AGENTOS_TASK_STATE_TERMINATED;
    return 0;
}

agentos_error_t agentos_task_init(void) {
    if (!task_table_lock) {
        task_table_lock = agentos_mutex_create();
        if (!task_table_lock) return AGENTOS_ENOMEM;
    }
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_thread_create(
    agentos_thread_t* thread,
    const agentos_thread_attr_t* attr,
    void (*func)(void*),
    void* arg) {

    if (!thread || !func) return AGENTOS_EINVAL;

    task_info_t* info = (task_info_t*)calloc(1, sizeof(task_info_t));
    if (!info) return AGENTOS_ENOMEM;

    info->id = fetch_add_task_id();
    info->entry = func;
    info->arg = arg;
    info->state = AGENTOS_TASK_STATE_CREATED;
    info->priority = AGENTOS_TASK_PRIORITY_NORMAL;

    if (attr) {
        if (attr->name) {
            strncpy(info->name, attr->name, sizeof(info->name) - 1);
            info->name[sizeof(info->name) - 1] = '\0';
        }
        info->priority = attr->priority;
        if (info->priority < AGENTOS_TASK_PRIORITY_MIN ||
            info->priority > AGENTOS_TASK_PRIORITY_MAX) {
            info->priority = AGENTOS_TASK_PRIORITY_NORMAL;
        }
    } else {
        strcpy(info->name, "unnamed");
    }

    info->handle = CreateThread(
        NULL,
        attr ? (DWORD)attr->stack_size : 0,
        thread_entry,
        info,
        0,
        NULL);
    if (!info->handle) {
        free(info);
        return AGENTOS_ENOMEM;
    }

    int win_prio = THREAD_PRIORITY_NORMAL;
    if (info->priority >= AGENTOS_TASK_PRIORITY_HIGH) {
        win_prio = THREAD_PRIORITY_HIGHEST;
    } else if (info->priority <= AGENTOS_TASK_PRIORITY_LOW) {
        win_prio = THREAD_PRIORITY_LOWEST;
    }
    SetThreadPriority(info->handle, win_prio);

    agentos_mutex_lock(task_table_lock);
    if (task_count < 1024) {
        task_table[task_count++] = info;
    }
    agentos_mutex_unlock(task_table_lock);

    *thread = info->handle;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_thread_join(agentos_thread_t thread, void** retval) {
    if (!thread) return AGENTOS_EINVAL;
    WaitForSingleObject(thread, INFINITE);
    if (retval) {
        agentos_mutex_lock(task_table_lock);
        for (uint32_t i = 0; i < task_count; i++) {
            if (task_table[i]->handle == thread) {
                *retval = task_table[i]->retval;
                break;
            }
        }
        agentos_mutex_unlock(task_table_lock);
    }
    return AGENTOS_SUCCESS;
}

static task_info_t* find_task_by_id(agentos_task_id_t tid) {
    agentos_mutex_lock(task_table_lock);
    for (uint32_t i = 0; i < task_count; i++) {
        if (task_table[i]->id == tid) {
            task_info_t* t = task_table[i];
            agentos_mutex_unlock(task_table_lock);
            return t;
        }
    }
    agentos_mutex_unlock(task_table_lock);
    return NULL;
}

agentos_task_id_t agentos_task_self(void) {
    DWORD tid = GetCurrentThreadId();
    agentos_mutex_lock(task_table_lock);
    for (uint32_t i = 0; i < task_count; i++) {
        if (GetThreadId(task_table[i]->handle) == tid) {
            agentos_task_id_t id = task_table[i]->id;
            agentos_mutex_unlock(task_table_lock);
            return id;
        }
    }
    agentos_mutex_unlock(task_table_lock);
    return 0;
}

void agentos_task_sleep(uint32_t ms) {
    Sleep((DWORD)ms);
}

void agentos_task_yield(void) {
    SwitchToThread();
}

agentos_error_t agentos_task_set_priority(agentos_task_id_t tid, int priority) {
    if (priority < AGENTOS_TASK_PRIORITY_MIN || priority > AGENTOS_TASK_PRIORITY_MAX) {
        return AGENTOS_EINVAL;
    }
    task_info_t* info = find_task_by_id(tid);
    if (!info) return AGENTOS_EINVAL;
    info->priority = priority;
    int win_prio = THREAD_PRIORITY_NORMAL;
    if (priority >= AGENTOS_TASK_PRIORITY_HIGH) {
        win_prio = THREAD_PRIORITY_HIGHEST;
    } else if (priority <= AGENTOS_TASK_PRIORITY_LOW) {
        win_prio = THREAD_PRIORITY_LOWEST;
    }
    SetThreadPriority(info->handle, win_prio);
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_task_get_priority(agentos_task_id_t tid, int* out_priority) {
    if (!out_priority) return AGENTOS_EINVAL;
    task_info_t* info = find_task_by_id(tid);
    if (!info) return AGENTOS_EINVAL;
    *out_priority = info->priority;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_task_get_state(agentos_task_id_t tid, agentos_task_state_t* out_state) {
    if (!out_state) return AGENTOS_EINVAL;
    task_info_t* info = find_task_by_id(tid);
    if (!info) return AGENTOS_EINVAL;
    *out_state = info->state;
    return AGENTOS_SUCCESS;
}

#else

#include <pthread.h>
#include <sched.h>
#include <unistd.h>

typedef struct {
    pthread_t handle;
    agentos_task_id_t id;
    char name[64];
    int priority;
    void* (*entry)(void*);
    void* arg;
    void* retval;
    volatile agentos_task_state_t state;
} task_info_t;

static task_info_t* task_table[1024];
static uint32_t task_count = 0;
static agentos_mutex_t* task_table_lock = NULL;
static volatile uint64_t next_task_id = 1;

static uint64_t fetch_add_task_id(void) {
    return __atomic_fetch_add(&next_task_id, 1, __ATOMIC_SEQ_CST);
}

static void* thread_entry(void* param) {
    task_info_t* info = (task_info_t*)param;
    info->state = AGENTOS_TASK_STATE_RUNNING;
    info->retval = info->entry(info->arg);
    info->state = AGENTOS_TASK_STATE_TERMINATED;
    return NULL;
}

agentos_error_t agentos_task_init(void) {
    if (!task_table_lock) {
        task_table_lock = agentos_mutex_create();
        if (!task_table_lock) return AGENTOS_ENOMEM;
    }
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_thread_create(
    agentos_thread_t* thread,
    const agentos_thread_attr_t* attr,
    void (*func)(void*),
    void* arg) {

    if (!thread || !func) return AGENTOS_EINVAL;

    task_info_t* info = (task_info_t*)calloc(1, sizeof(task_info_t));
    if (!info) return AGENTOS_ENOMEM;

    info->id = fetch_add_task_id();
    info->entry = func;
    info->arg = arg;
    info->state = AGENTOS_TASK_STATE_CREATED;
    info->priority = AGENTOS_TASK_PRIORITY_NORMAL;

    if (attr) {
        if (attr->name) {
            strncpy(info->name, attr->name, sizeof(info->name) - 1);
            info->name[sizeof(info->name) - 1] = '\0';
        }
        info->priority = attr->priority;
        if (info->priority < AGENTOS_TASK_PRIORITY_MIN ||
            info->priority > AGENTOS_TASK_PRIORITY_MAX) {
            info->priority = AGENTOS_TASK_PRIORITY_NORMAL;
        }
    } else {
        strcpy(info->name, "unnamed");
    }

    pthread_attr_t pattr;
    pthread_attr_init(&pattr);
    if (attr && attr->stack_size > 0) {
        pthread_attr_setstacksize(&pattr, attr->stack_size);
    }

    int ret = pthread_create(&info->handle, &pattr, thread_entry, info);
    pthread_attr_destroy(&pattr);

    if (ret != 0) {
        free(info);
        return AGENTOS_ENOMEM;
    }

    agentos_mutex_lock(task_table_lock);
    if (task_count < 1024) {
        task_table[task_count++] = info;
    }
    agentos_mutex_unlock(task_table_lock);

    *thread = info->handle;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_thread_join(agentos_thread_t thread, void** retval) {
    if (!thread) return AGENTOS_EINVAL;
    pthread_join(thread, retval);
    return AGENTOS_SUCCESS;
}

static task_info_t* find_task_by_id(agentos_task_id_t tid) {
    agentos_mutex_lock(task_table_lock);
    for (uint32_t i = 0; i < task_count; i++) {
        if (task_table[i]->id == tid) {
            task_info_t* t = task_table[i];
            agentos_mutex_unlock(task_table_lock);
            return t;
        }
    }
    agentos_mutex_unlock(task_table_lock);
    return NULL;
}

agentos_task_id_t agentos_task_self(void) {
    pthread_t self = pthread_self();
    agentos_mutex_lock(task_table_lock);
    for (uint32_t i = 0; i < task_count; i++) {
        if (pthread_equal(task_table[i]->handle, self)) {
            agentos_task_id_t id = task_table[i]->id;
            agentos_mutex_unlock(task_table_lock);
            return id;
        }
    }
    agentos_mutex_unlock(task_table_lock);
    return 0;
}

void agentos_task_sleep(uint32_t ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

void agentos_task_yield(void) {
    sched_yield();
}

agentos_error_t agentos_task_set_priority(agentos_task_id_t tid, int priority) {
    if (priority < AGENTOS_TASK_PRIORITY_MIN || priority > AGENTOS_TASK_PRIORITY_MAX) {
        return AGENTOS_EINVAL;
    }
    task_info_t* info = find_task_by_id(tid);
    if (!info) return AGENTOS_EINVAL;
    info->priority = priority;

    int sched_policy;
    sched_param sp = {0};
    pthread_getschedparam(info->handle, &sched_policy, &sp);
    int min_prio = sched_get_priority_min(sched_policy);
    int max_prio = sched_get_priority_max(sched_policy);
    sp.sched_priority = min_prio + (int)((max_prio - min_prio) * (double)priority / 100.0);
    pthread_setschedparam(info->handle, sched_policy, &sp);

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_task_get_priority(agentos_task_id_t tid, int* out_priority) {
    if (!out_priority) return AGENTOS_EINVAL;
    task_info_t* info = find_task_by_id(tid);
    if (!info) return AGENTOS_EINVAL;
    *out_priority = info->priority;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_task_get_state(agentos_task_id_t tid, agentos_task_state_t* out_state) {
    if (!out_state) return AGENTOS_EINVAL;
    task_info_t* info = find_task_by_id(tid);
    if (!info) return AGENTOS_EINVAL;
    *out_state = info->state;
    return AGENTOS_SUCCESS;
}

#endif

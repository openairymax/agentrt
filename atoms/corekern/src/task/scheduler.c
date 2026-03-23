/**
 * @file scheduler.c
 * @brief 任务调度器（基于系统原生线程）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * 
 * @details
 * 本模块实现跨平台线程管理：
 * - 线程创建、销毁、等待
 * - 线程优先级设置
 * - 线程状态追踪
 * 
 * 使用哈希表索引加速任务查找，从 O(n) 优化到 O(1) 平均时间复杂度。
 */

#include "task.h"
#include "mem.h"
#include "time.h"
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

/* ==================== 配置常量 ==================== */

/** @brief 任务表最大容量 */
#define TASK_TABLE_CAPACITY 4096

/** @brief 哈希表桶数量（质数，减少冲突） */
#define HASH_TABLE_BUCKETS 4093

/* ==================== 哈希表实现 ==================== */

/**
 * @brief 哈希表节点
 */
typedef struct task_hash_node {
    struct task_hash_node* next;
    agentos_task_id_t id;
    void* task_info;
} task_hash_node_t;

/**
 * @brief 简单哈希函数
 * @param id 任务ID
 * @return 哈希桶索引
 */
static inline size_t task_hash(agentos_task_id_t id) {
    return (size_t)(id % HASH_TABLE_BUCKETS);
}

/* ==================== Windows 平台实现 ==================== */

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

/* ==================== 全局状态 ==================== */

/** @brief 任务信息数组 */
static task_info_t* task_table[TASK_TABLE_CAPACITY];

/** @brief 当前任务数量 */
static uint32_t task_count = 0;

/** @brief 任务表互斥锁 */
static agentos_mutex_t* task_table_lock = NULL;

/** @brief 初始化状态标志 */
static _Atomic int scheduler_initialized = ATOMIC_VAR_INIT(0);

/** @brief 下一个任务ID */
static volatile uint64_t next_task_id = 1;

/** @brief ID到任务信息的哈希索引 */
static task_hash_node_t* id_hash_table[HASH_TABLE_BUCKETS];

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 原子获取并递增任务ID
 * @return 新任务ID
 */
static uint64_t fetch_add_task_id(void) {
    return InterlockedIncrement64((volatile LONG64*)&next_task_id) - 1;
}

/**
 * @brief 确保调度器已初始化
 * @return 0 成功，-1 失败
 */
static int ensure_scheduler_initialized(void) {
    int expected = 0;
    
    if (atomic_load(&scheduler_initialized) == 1) {
        return 0;
    }
    
    if (atomic_compare_exchange_strong(&scheduler_initialized, &expected, 2)) {
        task_table_lock = agentos_mutex_create();
        if (!task_table_lock) {
            atomic_store(&scheduler_initialized, 0);
            return -1;
        }
        memset(id_hash_table, 0, sizeof(id_hash_table));
        atomic_store(&scheduler_initialized, 1);
        return 0;
    } else {
        while (atomic_load(&scheduler_initialized) != 1) {
            Sleep(0);
        }
        return 0;
    }
}

/**
 * @brief 向哈希表插入任务
 * @param id 任务ID
 * @param info 任务信息指针
 * @note 必须在持有 task_table_lock 时调用
 */
static void hash_insert_unlocked(agentos_task_id_t id, task_info_t* info) {
    size_t bucket = task_hash(id);
    task_hash_node_t* node = (task_hash_node_t*)malloc(sizeof(task_hash_node_t));
    if (!node) return;
    
    node->id = id;
    node->task_info = info;
    node->next = id_hash_table[bucket];
    id_hash_table[bucket] = node;
}

/**
 * @brief 从哈希表查找任务
 * @param id 任务ID
 * @return 任务信息指针，未找到返回 NULL
 * @note 必须在持有 task_table_lock 时调用
 */
static task_info_t* hash_find_unlocked(agentos_task_id_t id) {
    size_t bucket = task_hash(id);
    task_hash_node_t* node = id_hash_table[bucket];
    
    while (node) {
        if (node->id == id) {
            return (task_info_t*)node->task_info;
        }
        node = node->next;
    }
    
    return NULL;
}

/**
 * @brief 从哈希表移除任务
 * @param id 任务ID
 * @note 必须在持有 task_table_lock 时调用
 */
static void hash_remove_unlocked(agentos_task_id_t id) {
    size_t bucket = task_hash(id);
    task_hash_node_t* node = id_hash_table[bucket];
    task_hash_node_t* prev = NULL;
    
    while (node) {
        if (node->id == id) {
            if (prev) {
                prev->next = node->next;
            } else {
                id_hash_table[bucket] = node->next;
            }
            free(node);
            return;
        }
        prev = node;
        node = node->next;
    }
}

/**
 * @brief 线程入口函数
 * @param param 任务信息指针
 * @return 退出码
 */
static DWORD WINAPI thread_entry(LPVOID param) {
    task_info_t* info = (task_info_t*)param;
    info->state = AGENTOS_TASK_STATE_RUNNING;
    info->retval = info->entry(info->arg);
    info->state = AGENTOS_TASK_STATE_TERMINATED;
    return 0;
}

/* ==================== 公共接口实现 ==================== */

/**
 * @brief 初始化任务调度器
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t agentos_task_init(void) {
    if (ensure_scheduler_initialized() != 0) {
        return AGENTOS_ENOMEM;
    }
    return AGENTOS_SUCCESS;
}

/**
 * @brief 创建线程
 * @param thread 线程句柄输出
 * @param attr 线程属性
 * @param func 线程入口函数
 * @param arg 线程参数
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t agentos_thread_create(
    agentos_thread_t* thread,
    const agentos_thread_attr_t* attr,
    void (*func)(void*),
    void* arg) {

    if (!thread || !func) return AGENTOS_EINVAL;
    
    if (ensure_scheduler_initialized() != 0) {
        return AGENTOS_ENOMEM;
    }

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
    if (task_count < TASK_TABLE_CAPACITY) {
        task_table[task_count++] = info;
        hash_insert_unlocked(info->id, info);
    }
    agentos_mutex_unlock(task_table_lock);

    *thread = info->handle;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 等待线程结束
 * @param thread 线程句柄
 * @param retval 返回值输出
 * @return AGENTOS_SUCCESS 成功
 */
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

/**
 * @brief 根据任务ID查找任务（使用哈希表优化）
 * @param tid 任务ID
 * @return 任务信息指针
 */
static task_info_t* find_task_by_id(agentos_task_id_t tid) {
    if (atomic_load(&scheduler_initialized) != 1) {
        return NULL;
    }
    
    agentos_mutex_lock(task_table_lock);
    task_info_t* info = hash_find_unlocked(tid);
    agentos_mutex_unlock(task_table_lock);
    
    return info;
}

/**
 * @brief 获取当前任务ID
 * @return 当前任务ID
 */
agentos_task_id_t agentos_task_self(void) {
    if (atomic_load(&scheduler_initialized) != 1) {
        return 0;
    }
    
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

/**
 * @brief 线程休眠
 * @param ms 休眠毫秒数
 */
void agentos_task_sleep(uint32_t ms) {
    Sleep((DWORD)ms);
}

/**
 * @brief 线程让出CPU
 */
void agentos_task_yield(void) {
    SwitchToThread();
}

/**
 * @brief 设置任务优先级
 * @param tid 任务ID
 * @param priority 优先级
 * @return AGENTOS_SUCCESS 成功
 */
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

/**
 * @brief 获取任务优先级
 * @param tid 任务ID
 * @param out_priority 优先级输出
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t agentos_task_get_priority(agentos_task_id_t tid, int* out_priority) {
    if (!out_priority) return AGENTOS_EINVAL;
    task_info_t* info = find_task_by_id(tid);
    if (!info) return AGENTOS_EINVAL;
    *out_priority = info->priority;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 获取任务状态
 * @param tid 任务ID
 * @param out_state 状态输出
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t agentos_task_get_state(agentos_task_id_t tid, agentos_task_state_t* out_state) {
    if (!out_state) return AGENTOS_EINVAL;
    task_info_t* info = find_task_by_id(tid);
    if (!info) return AGENTOS_EINVAL;
    *out_state = info->state;
    return AGENTOS_SUCCESS;
}

/* ==================== POSIX 平台实现 ==================== */

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

/* ==================== 全局状态 ==================== */

static task_info_t* task_table[TASK_TABLE_CAPACITY];
static uint32_t task_count = 0;
static agentos_mutex_t* task_table_lock = NULL;
static _Atomic int scheduler_initialized = ATOMIC_VAR_INIT(0);
static volatile uint64_t next_task_id = 1;
static task_hash_node_t* id_hash_table[HASH_TABLE_BUCKETS];

/* ==================== 内部辅助函数 ==================== */

static uint64_t fetch_add_task_id(void) {
    return __atomic_fetch_add(&next_task_id, 1, __ATOMIC_SEQ_CST);
}

static int ensure_scheduler_initialized(void) {
    int expected = 0;
    
    if (atomic_load(&scheduler_initialized) == 1) {
        return 0;
    }
    
    if (atomic_compare_exchange_strong(&scheduler_initialized, &expected, 2)) {
        task_table_lock = agentos_mutex_create();
        if (!task_table_lock) {
            atomic_store(&scheduler_initialized, 0);
            return -1;
        }
        memset(id_hash_table, 0, sizeof(id_hash_table));
        atomic_store(&scheduler_initialized, 1);
        return 0;
    } else {
        while (atomic_load(&scheduler_initialized) != 1) {
            sched_yield();
        }
        return 0;
    }
}

static void hash_insert_unlocked(agentos_task_id_t id, task_info_t* info) {
    size_t bucket = task_hash(id);
    task_hash_node_t* node = (task_hash_node_t*)malloc(sizeof(task_hash_node_t));
    if (!node) return;
    
    node->id = id;
    node->task_info = info;
    node->next = id_hash_table[bucket];
    id_hash_table[bucket] = node;
}

static task_info_t* hash_find_unlocked(agentos_task_id_t id) {
    size_t bucket = task_hash(id);
    task_hash_node_t* node = id_hash_table[bucket];
    
    while (node) {
        if (node->id == id) {
            return (task_info_t*)node->task_info;
        }
        node = node->next;
    }
    
    return NULL;
}

static void hash_remove_unlocked(agentos_task_id_t id) {
    size_t bucket = task_hash(id);
    task_hash_node_t* node = id_hash_table[bucket];
    task_hash_node_t* prev = NULL;
    
    while (node) {
        if (node->id == id) {
            if (prev) {
                prev->next = node->next;
            } else {
                id_hash_table[bucket] = node->next;
            }
            free(node);
            return;
        }
        prev = node;
        node = node->next;
    }
}

static void* thread_entry(void* param) {
    task_info_t* info = (task_info_t*)param;
    info->state = AGENTOS_TASK_STATE_RUNNING;
    info->retval = info->entry(info->arg);
    info->state = AGENTOS_TASK_STATE_TERMINATED;
    return NULL;
}

agentos_error_t agentos_task_init(void) {
    if (ensure_scheduler_initialized() != 0) {
        return AGENTOS_ENOMEM;
    }
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_thread_create(
    agentos_thread_t* thread,
    const agentos_thread_attr_t* attr,
    void (*func)(void*),
    void* arg) {

    if (!thread || !func) return AGENTOS_EINVAL;
    
    if (ensure_scheduler_initialized() != 0) {
        return AGENTOS_ENOMEM;
    }

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
    if (task_count < TASK_TABLE_CAPACITY) {
        task_table[task_count++] = info;
        hash_insert_unlocked(info->id, info);
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
    if (atomic_load(&scheduler_initialized) != 1) {
        return NULL;
    }
    
    agentos_mutex_lock(task_table_lock);
    task_info_t* info = hash_find_unlocked(tid);
    agentos_mutex_unlock(task_table_lock);
    
    return info;
}

agentos_task_id_t agentos_task_self(void) {
    if (atomic_load(&scheduler_initialized) != 1) {
        return 0;
    }
    
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

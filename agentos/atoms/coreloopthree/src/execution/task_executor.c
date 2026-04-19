/**
 * @file task_executor.c
 * @brief 任务执行器 - 精简版 (DEPRECATED)
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 任务执行器负责管理任务的执行生命周期，包括任务调度、状态跟踪、
 * 超时控制和结果收集。基于 task_executor_utils 模块构建。
 *
 * @deprecated 此模块为精简版实现，工作线程未实际创建。
 *            推荐使用 execution/engine.c 中的 agentos_execution_engine_t，
 *            它提供了完整的线程池、哈希表O(1)查找和引用计数TCB管理。
 *            计划在未来版本中移除此文件。
 */

#include "execution.h"
#include "agentos.h"
#include "logger.h"
#include "task_executor_utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* 基础库兼容性层 */
#include "include/memory_compat.h"
#include "string_compat.h"

/* ==================== 常量定义 ==================== */

#define MAX_TASKS 1024
#define MAX_WORKERS 16
#define DEFAULT_TASK_TIMEOUT_MS 30000
#define DEFAULT_MAX_RETRIES 3
#define TASK_QUEUE_CAPACITY 512

/* ==================== 数据结构 ==================== */

/**
 * @brief 任务状态枚举
 */
typedef enum {
    TASK_STATE_PENDING = 0,
    TASK_STATE_QUEUED,
    TASK_STATE_RUNNING,
    TASK_STATE_COMPLETED,
    TASK_STATE_FAILED,
    TASK_STATE_CANCELLED,
    TASK_STATE_TIMEOUT
} task_state_t;

/**
 * @brief 任务队列节点
 */
typedef struct task_queue_node {
    agentos_task_t* task;
    struct task_queue_node* next;
} task_queue_node_t;

/**
 * @brief 优先级队列
 */
typedef struct priority_queue {
    task_queue_node_t* heads[4];
    task_queue_node_t* tails[4];
    uint32_t counts[4];
    uint32_t total_count;
    agentos_mutex_t* lock;
    agentos_cond_t* cond;
} priority_queue_t;

/**
 * @brief 工作线程
 */
typedef struct worker_thread {
    uint64_t worker_id;
    uint64_t tasks_completed;
    uint64_t tasks_failed;
    int is_active;
    agentos_thread_t* thread;
} worker_thread_t;

/**
 * @brief 任务执行器结构
 */
struct agentos_task_executor {
    char* executor_id;
    priority_queue_t* task_queue;
    agentos_task_t* tasks[MAX_TASKS];
    uint32_t task_count;
    worker_thread_t workers[MAX_WORKERS];
    uint32_t worker_count;
    agentos_mutex_t* lock;
    agentos_cond_t* shutdown_cond;
    int shutdown;
    void* obs;
    uint64_t total_submitted;
    uint64_t total_completed;
    uint64_t total_failed;
    uint64_t total_cancelled;
};

/* ==================== 优先级队列实现 ==================== */

static priority_queue_t* create_priority_queue(void) {
    priority_queue_t* queue = (priority_queue_t*)AGENTOS_CALLOC(1, sizeof(priority_queue_t));
    if (!queue) return NULL;

    queue->lock = agentos_mutex_create();
    queue->cond = agentos_cond_create();

    if (!queue->lock || !queue->cond) {
        if (queue->lock) agentos_mutex_destroy(queue->lock);
        if (queue->cond) agentos_cond_destroy(queue->cond);
        AGENTOS_FREE(queue);
        return NULL;
    }

    return queue;
}

static void destroy_priority_queue(priority_queue_t* queue) {
    if (!queue) return;

    for (int i = 0; i < 4; i++) {
        task_queue_node_t* node = queue->heads[i];
        while (node) {
            task_queue_node_t* next = node->next;
            AGENTOS_FREE(node);
            node = next;
        }
    }

    agentos_cond_destroy(queue->cond);
    agentos_mutex_destroy(queue->lock);
    AGENTOS_FREE(queue);
}

static int priority_queue_push(priority_queue_t* queue, agentos_task_t* task, int priority) {
    if (!queue || !task) return -1;

    task_queue_node_t* node = (task_queue_node_t*)AGENTOS_MALLOC(sizeof(task_queue_node_t));
    if (!node) return -1;

    node->task = task;
    node->next = NULL;

    agentos_mutex_lock(queue->lock);

    if (priority < 0) priority = 0;
    if (priority > 3) priority = 3;

    if (queue->tails[priority]) {
        queue->tails[priority]->next = node;
        queue->tails[priority] = node;
    } else {
        queue->heads[priority] = queue->tails[priority] = node;
    }

    queue->counts[priority]++;
    queue->total_count++;

    agentos_cond_signal(queue->cond);
    agentos_mutex_unlock(queue->lock);

    return 0;
}

static agentos_task_t* priority_queue_pop(priority_queue_t* queue, uint32_t timeout_ms) {
    if (!queue) return NULL;

    agentos_mutex_lock(queue->lock);

    while (queue->total_count == 0) {
        if (timeout_ms == 0) {
            agentos_mutex_unlock(queue->lock);
            return NULL;
        }

        if (timeout_ms > 0) {
            agentos_cond_timedwait(queue->cond, queue->lock, (int)timeout_ms);
        } else {
            agentos_cond_wait(queue->cond, queue->lock);
        }
        
        if (timeout_ms > 0) {
            agentos_mutex_unlock(queue->lock);
            return NULL;
        }
    }

    task_queue_node_t* node = NULL;
    for (int i = 3; i >= 0; i--) {
        if (queue->heads[i]) {
            node = queue->heads[i];
            queue->heads[i] = node->next;
            if (!queue->heads[i]) queue->tails[i] = NULL;
            queue->counts[i]--;
            queue->total_count--;
            break;
        }
    }

    agentos_mutex_unlock(queue->lock);

    if (node) {
        agentos_task_t* task = node->task;
        AGENTOS_FREE(node);
        return task;
    }

    return NULL;
}

/* ==================== 执行器管理 ==================== */

agentos_error_t agentos_task_executor_create(const char* executor_id, uint32_t worker_count,
                                            agentos_task_executor_t** out_executor) {
    if (!executor_id || !out_executor) return AGENTOS_EINVAL;
    if (worker_count > MAX_WORKERS) worker_count = MAX_WORKERS;

    agentos_task_executor_t* executor = (agentos_task_executor_t*)AGENTOS_CALLOC(1, sizeof(agentos_task_executor_t));
    if (!executor) return AGENTOS_ENOMEM;

    executor->executor_id = AGENTOS_STRDUP(executor_id);
    executor->lock = agentos_mutex_create();
    executor->shutdown_cond = agentos_cond_create();
    executor->task_queue = create_priority_queue();

    if (!executor->executor_id || !executor->lock || !executor->shutdown_cond || !executor->task_queue) {
        if (executor->executor_id) AGENTOS_FREE(executor->executor_id);
        if (executor->lock) agentos_mutex_destroy(executor->lock);
        if (executor->shutdown_cond) agentos_cond_destroy(executor->shutdown_cond);
        if (executor->task_queue) destroy_priority_queue(executor->task_queue);
        AGENTOS_FREE(executor);
        return AGENTOS_ENOMEM;
    }

    executor->worker_count = worker_count;
    executor->task_count = 0;
    executor->shutdown = 0;

    AGENTOS_LOG_INFO("Task executor created: %s with %u workers", executor_id, worker_count);
    *out_executor = executor;
    return AGENTOS_SUCCESS;
}

void agentos_task_executor_destroy(agentos_task_executor_t* executor) {
    if (!executor) return;

    agentos_task_executor_shutdown(executor);

    if (executor->executor_id) AGENTOS_FREE(executor->executor_id);
    if (executor->lock) agentos_mutex_destroy(executor->lock);
    if (executor->shutdown_cond) agentos_cond_destroy(executor->shutdown_cond);
    if (executor->task_queue) destroy_priority_queue(executor->task_queue);

    AGENTOS_FREE(executor);
    AGENTOS_LOG_INFO("Task executor destroyed");
}

agentos_error_t agentos_task_executor_start(agentos_task_executor_t* executor) {
    if (!executor) return AGENTOS_EINVAL;

    AGENTOS_LOG_WARN("task_executor_start: DEPRECATED - use agentos_execution_create from execution/engine.c for full thread pool support");

    agentos_mutex_lock(executor->lock);
    
    for (uint32_t i = 0; i < executor->worker_count; i++) {
        executor->workers[i].worker_id = i + 1;
        executor->workers[i].is_active = 1;
        executor->workers[i].tasks_completed = 0;
        executor->workers[i].tasks_failed = 0;
        
        /* 精简版：不实际创建工作线程（同步模式） */
        executor->workers[i].thread = NULL;
    }

    agentos_mutex_unlock(executor->lock);
    AGENTOS_LOG_INFO("Task executor started (synchronous mode, no worker threads)");
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_task_executor_shutdown(agentos_task_executor_t* executor) {
    if (!executor) return AGENTOS_EINVAL;

    agentos_mutex_lock(executor->lock);
    executor->shutdown = 1;
    agentos_cond_broadcast(executor->shutdown_cond);
    agentos_mutex_unlock(executor->lock);

    AGENTOS_LOG_INFO("Task executor shutdown initiated");
    return AGENTOS_SUCCESS;
}

/* ==================== 任务管理 ==================== */

agentos_error_t agentos_task_executor_submit(agentos_task_executor_t* executor,
                                            agentos_task_t* task) {
    if (!executor || !task) return AGENTOS_EINVAL;

    agentos_mutex_lock(executor->lock);

    if (executor->task_count >= MAX_TASKS) {
        agentos_mutex_unlock(executor->lock);
        AGENTOS_LOG_ERROR("Task executor full");
        return AGENTOS_EBUSY;
    }

    executor->tasks[executor->task_count++] = task;
    executor->total_submitted++;

    priority_queue_push(executor->task_queue, task, (int)task->priority);

    agentos_mutex_unlock(executor->lock);
    AGENTOS_LOG_DEBUG("Task submitted: %llu", (unsigned long long)task->task_id);
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_task_executor_wait(agentos_task_executor_t* executor,
                                          agentos_task_t* task, uint32_t timeout_ms) {
    if (!executor || !task) return AGENTOS_EINVAL;

    uint64_t start = 0;
    (void)start;
    (void)timeout_ms;

    agentos_mutex_lock(task->lock);
    while (task->state != TASK_STATE_COMPLETED && 
           task->state != TASK_STATE_FAILED &&
           task->state != TASK_STATE_CANCELLED &&
           task->state != TASK_STATE_TIMEOUT) {
        if (timeout_ms > 0) {
            agentos_cond_timedwait(task->cond, task->lock, (int)timeout_ms);
        } else {
            agentos_cond_wait(task->cond, task->lock);
        }
        break; /* 简化实现 */
    }
    agentos_mutex_unlock(task->lock);

    return task->result;
}

agentos_error_t agentos_task_executor_get_stats(agentos_task_executor_t* executor, char** out_stats) {
    if (!executor || !out_stats) return AGENTOS_EINVAL;

    char* stats = (char*)AGENTOS_MALLOC(512);
    if (!stats) return AGENTOS_ENOMEM;

    snprintf(stats, 512,
             "{\"executor_id\":\"%s\",\"submitted\":%llu,\"completed\":%llu,"
             "\"failed\":%llu,\"cancelled\":%llu,\"task_count\":%u}",
             executor->executor_id ? executor->executor_id : "",
             (unsigned long long)executor->total_submitted,
             (unsigned long long)executor->total_completed,
             (unsigned long long)executor->total_failed,
             (unsigned long long)executor->total_cancelled,
             executor->task_count);

    *out_stats = stats;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_task_executor_cancel_task(agentos_task_executor_t* executor, const char* task_id) {
    if (!executor) return AGENTOS_EINVAL;

    agentos_mutex_lock(executor->lock);

    for (uint32_t i = 0; i < executor->task_count; i++) {
        if (executor->tasks[i] && executor->tasks[i]->task_id && task_id &&
            strcmp(executor->tasks[i]->task_id, task_id) == 0) {
            executor->tasks[i]->state = TASK_STATE_CANCELLED;
            executor->total_cancelled++;
            agentos_mutex_unlock(executor->lock);
            AGENTOS_LOG_INFO("Task %s cancelled", task_id);
            return AGENTOS_SUCCESS;
        }
    }

    agentos_mutex_unlock(executor->lock);
    return AGENTOS_NOTFOUND;
}

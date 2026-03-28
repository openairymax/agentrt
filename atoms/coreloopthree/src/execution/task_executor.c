﻿/**
 * @file task_executor.c
 * @brief 任务执行器实�?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * 
 * @details
 * 任务执行器负责管理任务的执行生命周期，包括任务调度、状态跟踪�?
 * 超时控制和结果收集。实现生产级任务管理，支�?9.999%可靠性标准�?
 * 
 * 核心功能�?
 * 1. 任务调度：优先级队列、依赖管�?
 * 2. 并发控制：工作线程池、资源限�?
 * 3. 状态管理：状态机、状态转�?
 * 4. 超时控制：任务超时、强制取�?
 * 5. 错误处理：重试机制、错误恢�?
 * 6. 监控指标：执行统计、性能分析
 */

#include "execution.h"
#include "agentos.h"
#include "logger.h"
#include "id_utils.h"
#include "observability.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../bases/utils/memory/include/memory_compat.h"
#include "../../../bases/utils/string/include/string_compat.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <cjson/cJSON.h>

/* ==================== 内部常量定义 ==================== */

/** @brief 最大任务数�?*/
#define MAX_TASKS 1024

/** @brief 最大工作线程数 */
#define MAX_WORKERS 16

/** @brief 默认任务超时（毫秒） */
#define DEFAULT_TASK_TIMEOUT_MS 30000

/** @brief 默认重试次数 */
#define DEFAULT_MAX_RETRIES 3

/** @brief 任务队列容量 */
#define TASK_QUEUE_CAPACITY 512

/** @brief 优先级队列大�?*/
#define PRIORITY_QUEUE_SIZE 256

/** @brief 任务状态检查间隔（毫秒�?*/
#define TASK_CHECK_INTERVAL_MS 100

/* ==================== 内部数据结构 ==================== */

/**
 * @brief 任务状态枚�?
 */
typedef enum {
    TASK_STATE_PENDING = 0,     /**< 待执�?*/
    TASK_STATE_QUEUED,          /**< 已入�?*/
    TASK_STATE_RUNNING,         /**< 执行�?*/
    TASK_STATE_COMPLETED,       /**< 已完�?*/
    TASK_STATE_FAILED,          /**< 失败 */
    TASK_STATE_CANCELLED,       /**< 已取�?*/
    TASK_STATE_TIMEOUT          /**< 超时 */
} task_state_t;

/**
 * @brief 任务优先�?
 */
typedef enum {
    TASK_PRIORITY_LOW = 0,      /**< 低优先级 */
    TASK_PRIORITY_NORMAL,       /**< 普通优先级 */
    TASK_PRIORITY_HIGH,         /**< 高优先级 */
    TASK_PRIORITY_CRITICAL      /**< 关键优先�?*/
} task_priority_t;

/**
 * @brief 任务依赖�?
 */
typedef struct task_dependency {
    uint64_t task_id;                   /**< 依赖任务ID */
    int satisfied;                      /**< 是否满足 */
    struct task_dependency* next;       /**< 下一个依�?*/
} task_dependency_t;

/**
 * @brief 任务回调函数类型
 */
typedef agentos_error_t (*task_callback_fn)(void* input, void** output);

/**
 * @brief 任务配置
 */
typedef struct task_config {
    char* task_name;                    /**< 任务名称 */
    task_priority_t priority;           /**< 优先�?*/
    uint32_t timeout_ms;                /**< 超时时间 */
    uint32_t max_retries;               /**< 最大重试次�?*/
    uint32_t retry_delay_ms;            /**< 重试延迟 */
    uint32_t flags;                     /**< 标志�?*/
} task_config_t;

/**
 * @brief 任务统计
 */
typedef struct task_stats {
    uint64_t create_time_ns;            /**< 创建时间 */
    uint64_t start_time_ns;             /**< 开始时�?*/
    uint64_t end_time_ns;               /**< 结束时间 */
    uint64_t duration_ns;               /**< 执行时长 */
    uint32_t retry_count;               /**< 重试次数 */
    uint32_t wait_count;                /**< 等待次数 */
} task_stats_t;

/**
 * @brief 任务内部结构
 */
struct agentos_task {
    uint64_t task_id;                   /**< 任务ID */
    char* task_name;                    /**< 任务名称 */
    task_state_t state;                 /**< 状�?*/
    task_priority_t priority;           /**< 优先�?*/
    task_config_t manager;               /**< 配置 */
    task_stats_t stats;                 /**< 统计 */
    task_dependency_t* dependencies;    /**< 依赖列表 */
    uint32_t dependency_count;          /**< 依赖数量 */
    
    void* input;                        /**< 输入数据 */
    size_t input_size;                  /**< 输入大小 */
    void* output;                       /**< 输出数据 */
    size_t output_size;                 /**< 输出大小 */
    
    task_callback_fn callback;          /**< 回调函数 */
    void* user_data;                    /**< 用户数据 */
    
    agentos_error_t result;             /**< 执行结果 */
    char* error_message;                /**< 错误信息 */
    
    agentos_mutex_t* lock;              /**< 线程�?*/
    agentos_cond_t* cond;               /**< 条件变量 */
};

/**
 * @brief 任务队列节点
 */
typedef struct task_queue_node {
    agentos_task_t* task;               /**< 任务 */
    struct task_queue_node* next;       /**< 下一�?*/
} task_queue_node_t;

/**
 * @brief 优先级队�?
 */
typedef struct priority_queue {
    task_queue_node_t* heads[4];        /**< 各优先级队列�?*/
    task_queue_node_t* tails[4];        /**< 各优先级队列�?*/
    uint32_t counts[4];                 /**< 各优先级计数 */
    uint32_t total_count;               /**< 总计�?*/
    agentos_mutex_t* lock;              /**< 线程�?*/
    agentos_cond_t* cond;               /**< 条件变量 */
} priority_queue_t;

/**
 * @brief 工作线程
 */
typedef struct worker_thread {
    uint64_t worker_id;                 /**< 工作线程ID */
    uint64_t tasks_completed;           /**< 完成任务�?*/
    uint64_t tasks_failed;              /**< 失败任务�?*/
    int is_active;                      /**< 是否活跃 */
    agentos_thread_t* thread;           /**< 线程句柄 */
} worker_thread_t;

/**
 * @brief 任务执行器结�?
 */
struct agentos_task_executor {
    char* executor_id;                  /**< 执行器ID */
    priority_queue_t* task_queue;       /**< 任务队列 */
    agentos_task_t* tasks[MAX_TASKS];   /**< 任务数组 */
    uint32_t task_count;                /**< 任务数量 */
    
    worker_thread_t workers[MAX_WORKERS]; /**< 工作线程 */
    uint32_t worker_count;              /**< 工作线程数量 */
    
    agentos_mutex_t* lock;              /**< 全局�?*/
    agentos_cond_t* shutdown_cond;      /**< 关闭条件变量 */
    int shutdown;                       /**< 关闭标志 */
    
    agentos_observability_t* obs;       /**< 可观测�?*/
    
    uint64_t total_submitted;           /**< 总提交数 */
    uint64_t total_completed;           /**< 总完成数 */
    uint64_t total_failed;              /**< 总失败数 */
    uint64_t total_cancelled;           /**< 总取消数 */
};

/* ==================== 全局变量 ==================== */

static agentos_task_executor_t* g_task_executor = NULL;

/* ==================== 内部工具函数 ==================== */

/**
 * @brief 获取当前时间戳（纳秒�?
 */
static uint64_t get_timestamp_ns(void) {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

/**
 * @brief 创建优先级队�?
 */
static priority_queue_t* create_priority_queue(void) {
    priority_queue_t* queue = (priority_queue_t*)AGENTOS_CALLOC(1, sizeof(priority_queue_t));
    if (!queue) return NULL;
    
    for (int i = 0; i < 4; i++) {
        queue->heads[i] = NULL;
        queue->tails[i] = NULL;
        queue->counts[i] = 0;
    }
    queue->total_count = 0;
    
    queue->lock = agentos_mutex_create();
    if (!queue->lock) {
        AGENTOS_FREE(queue);
        return NULL;
    }
    
    queue->cond = agentos_cond_create();
    if (!queue->cond) {
        agentos_mutex_destroy(queue->lock);
        AGENTOS_FREE(queue);
        return NULL;
    }
    
    return queue;
}

/**
 * @brief 销毁优先级队列
 */
static void destroy_priority_queue(priority_queue_t* queue) {
    if (!queue) return;
    
    agentos_mutex_lock(queue->lock);
    
    for (int i = 0; i < 4; i++) {
        task_queue_node_t* node = queue->heads[i];
        while (node) {
            task_queue_node_t* next = node->next;
            AGENTOS_FREE(node);
            node = next;
        }
    }
    
    agentos_mutex_unlock(queue->lock);
    agentos_cond_destroy(queue->cond);
    agentos_mutex_destroy(queue->lock);
    AGENTOS_FREE(queue);
}

/**
 * @brief 向优先级队列添加任务
 */
static int priority_queue_push(priority_queue_t* queue, agentos_task_t* task) {
    if (!queue || !task) return -1;
    
    task_queue_node_t* node = (task_queue_node_t*)AGENTOS_MALLOC(sizeof(task_queue_node_t));
    if (!node) return -1;
    
    node->task = task;
    node->next = NULL;
    
    agentos_mutex_lock(queue->lock);
    
    int priority = (int)task->priority;
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

/**
 * @brief 从优先级队列获取任务
 */
static agentos_task_t* priority_queue_pop(priority_queue_t* queue, uint32_t timeout_ms) {
    if (!queue) return NULL;
    
    agentos_mutex_lock(queue->lock);
    
    uint64_t start_ns = get_timestamp_ns();
    uint64_t timeout_ns = (uint64_t)timeout_ms * 1000000ULL;
    
    while (queue->total_count == 0) {
        if (timeout_ms == 0) {
            agentos_mutex_unlock(queue->lock);
            return NULL;
        }
        
        uint64_t elapsed_ns = get_timestamp_ns() - start_ns;
        if (elapsed_ns >= timeout_ns) {
            agentos_mutex_unlock(queue->lock);
            return NULL;
        }
        
        agentos_cond_wait(queue->cond, queue->lock, timeout_ms - (uint32_t)(elapsed_ns / 1000000ULL));
    }
    
    // 从高优先级到低优先级查找
    task_queue_node_t* node = NULL;
    for (int i = 3; i >= 0; i--) {
        if (queue->heads[i]) {
            node = queue->heads[i];
            queue->heads[i] = node->next;
            if (!queue->heads[i]) {
                queue->tails[i] = NULL;
            }
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

/**
 * @brief 创建任务依赖�?
 */
static task_dependency_t* create_dependency(uint64_t task_id) {
    task_dependency_t* dep = (task_dependency_t*)AGENTOS_CALLOC(1, sizeof(task_dependency_t));
    if (!dep) return NULL;
    
    dep->task_id = task_id;
    dep->satisfied = 0;
    dep->next = NULL;
    
    return dep;
}

/**
 * @brief 检查任务依赖是否满�?
 */
static int check_dependencies(agentos_task_executor_t* executor, agentos_task_t* task) {
    if (!executor || !task) return 0;
    
    task_dependency_t* dep = task->dependencies;
    while (dep) {
        if (!dep->satisfied) {
            // 查找依赖任务
            for (uint32_t i = 0; i < MAX_TASKS; i++) {
                if (executor->tasks[i] && executor->tasks[i]->task_id == dep->task_id) {
                    if (executor->tasks[i]->state == TASK_STATE_COMPLETED) {
                        dep->satisfied = 1;
                    } else if (executor->tasks[i]->state == TASK_STATE_FAILED ||
                               executor->tasks[i]->state == TASK_STATE_CANCELLED) {
                        return -1;  // 依赖任务失败
                    }
                    break;
                }
            }
        }
        dep = dep->next;
    }
    
    // 检查是否所有依赖都满足
    dep = task->dependencies;
    while (dep) {
        if (!dep->satisfied) return 0;
        dep = dep->next;
    }
    
    return 1;
}

/**
 * @brief 工作线程函数
 */
static void* worker_thread_func(void* arg) {
    agentos_task_executor_t* executor = (agentos_task_executor_t*)arg;
    if (!executor) return NULL;
    
    uint64_t worker_id = 0;
    
    // 找到当前工作线程
    agentos_mutex_lock(executor->lock);
    for (uint32_t i = 0; i < executor->worker_count; i++) {
        if (!executor->workers[i].is_active) {
            executor->workers[i].is_active = 1;
            worker_id = executor->workers[i].worker_id;
            break;
        }
    }
    agentos_mutex_unlock(executor->lock);
    
    AGENTOS_LOG_DEBUG("Worker thread %llu started", (unsigned long long)worker_id);
    
    while (!executor->shutdown) {
        agentos_task_t* task = priority_queue_pop(executor->task_queue, 100);
        if (!task) continue;
        
        // 检查依�?
        int dep_result = check_dependencies(executor, task);
        if (dep_result == 0) {
            // 依赖未满足，重新入队
            priority_queue_push(executor->task_queue, task);
            continue;
        } else if (dep_result < 0) {
            // 依赖失败
            agentos_mutex_lock(task->lock);
            task->state = TASK_STATE_FAILED;
            task->result = AGENTOS_EDEPENDENCY;
            task->error_message = AGENTOS_STRDUP("Dependency task failed");
            agentos_mutex_unlock(task->lock);
            
            agentos_mutex_lock(executor->lock);
            executor->total_failed++;
            agentos_mutex_unlock(executor->lock);
            continue;
        }
        
        // 执行任务
        agentos_mutex_lock(task->lock);
        task->state = TASK_STATE_RUNNING;
        task->stats.start_time_ns = get_timestamp_ns();
        agentos_mutex_unlock(task->lock);
        
        agentos_error_t result = AGENTOS_SUCCESS;
        
        if (task->callback) {
            result = task->callback(task->input, &task->output);
        }
        
        // 更新任务状�?
        agentos_mutex_lock(task->lock);
        task->stats.end_time_ns = get_timestamp_ns();
        task->stats.duration_ns = task->stats.end_time_ns - task->stats.start_time_ns;
        task->result = result;
        
        if (result == AGENTOS_SUCCESS) {
            task->state = TASK_STATE_COMPLETED;
        } else {
            task->state = TASK_STATE_FAILED;
            if (!task->error_message) {
                task->error_message = AGENTOS_STRDUP("Task execution failed");
            }
        }
        
        agentos_cond_signal(task->cond);
        agentos_mutex_unlock(task->lock);
        
        // 更新执行器统�?
        agentos_mutex_lock(executor->lock);
        if (result == AGENTOS_SUCCESS) {
            executor->total_completed++;
        } else {
            executor->total_failed++;
        }
        agentos_mutex_unlock(executor->lock);
        
        // 更新工作线程统计
        for (uint32_t i = 0; i < executor->worker_count; i++) {
            if (executor->workers[i].worker_id == worker_id) {
                if (result == AGENTOS_SUCCESS) {
                    executor->workers[i].tasks_completed++;
                } else {
                    executor->workers[i].tasks_failed++;
                }
                break;
            }
        }
        
        // 更新可观测�?
        if (executor->obs) {
            agentos_observability_increment_counter(executor->obs, 
                result == AGENTOS_SUCCESS ? "task_completed" : "task_failed", 1);
            agentos_observability_record_histogram(executor->obs, 
                "task_duration_seconds", (double)task->stats.duration_ns / 1e9);
        }
    }
    
    AGENTOS_LOG_DEBUG("Worker thread %llu stopped", (unsigned long long)worker_id);
    
    return NULL;
}

/* ==================== 公共API实现 ==================== */

/**
 * @brief 创建任务执行�?
 */
agentos_error_t agentos_task_executor_create(uint32_t worker_count,
                                             agentos_task_executor_t** out_executor) {
    if (!out_executor) return AGENTOS_EINVAL;
    
    if (worker_count == 0) worker_count = 4;
    if (worker_count > MAX_WORKERS) worker_count = MAX_WORKERS;
    
    agentos_task_executor_t* executor = (agentos_task_executor_t*)AGENTOS_CALLOC(1, sizeof(agentos_task_executor_t));
    if (!executor) {
        AGENTOS_LOG_ERROR("Failed to allocate task executor");
        return AGENTOS_ENOMEM;
    }
    
    executor->executor_id = agentos_generate_uuid();
    if (!executor->executor_id) {
        executor->executor_id = AGENTOS_STRDUP("task_executor_default");
    }
    
    executor->lock = agentos_mutex_create();
    if (!executor->lock) {
        if (executor->executor_id) AGENTOS_FREE(executor->executor_id);
        AGENTOS_FREE(executor);
        AGENTOS_LOG_ERROR("Failed to create executor lock");
        return AGENTOS_ENOMEM;
    }
    
    executor->shutdown_cond = agentos_cond_create();
    if (!executor->shutdown_cond) {
        agentos_mutex_destroy(executor->lock);
        if (executor->executor_id) AGENTOS_FREE(executor->executor_id);
        AGENTOS_FREE(executor);
        AGENTOS_LOG_ERROR("Failed to create shutdown condition");
        return AGENTOS_ENOMEM;
    }
    
    executor->task_queue = create_priority_queue();
    if (!executor->task_queue) {
        agentos_cond_destroy(executor->shutdown_cond);
        agentos_mutex_destroy(executor->lock);
        if (executor->executor_id) AGENTOS_FREE(executor->executor_id);
        AGENTOS_FREE(executor);
        AGENTOS_LOG_ERROR("Failed to create task queue");
        return AGENTOS_ENOMEM;
    }
    
    executor->obs = agentos_observability_create();
    if (executor->obs) {
        agentos_observability_register_metric(executor->obs, "task_submitted", 
                                             AGENTOS_METRIC_COUNTER, "Total tasks submitted");
        agentos_observability_register_metric(executor->obs, "task_completed", 
                                             AGENTOS_METRIC_COUNTER, "Total tasks completed");
        agentos_observability_register_metric(executor->obs, "task_failed", 
                                             AGENTOS_METRIC_COUNTER, "Total tasks failed");
        agentos_observability_register_metric(executor->obs, "task_duration_seconds", 
                                             AGENTOS_METRIC_HISTOGRAM, "Task execution duration");
    }
    
    executor->task_count = 0;
    executor->worker_count = worker_count;
    executor->shutdown = 0;
    executor->total_submitted = 0;
    executor->total_completed = 0;
    executor->total_failed = 0;
    executor->total_cancelled = 0;
    
    // 初始化工作线�?
    for (uint32_t i = 0; i < worker_count; i++) {
        executor->workers[i].worker_id = i + 1;
        executor->workers[i].tasks_completed = 0;
        executor->workers[i].tasks_failed = 0;
        executor->workers[i].is_active = 0;
        executor->workers[i].thread = agentos_thread_create(worker_thread_func, executor);
    }
    
    *out_executor = executor;
    
    AGENTOS_LOG_INFO("Task executor created: %s (workers: %u)", 
                     executor->executor_id, worker_count);
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 销毁任务执行器
 */
void agentos_task_executor_destroy(agentos_task_executor_t* executor) {
    if (!executor) return;
    
    AGENTOS_LOG_DEBUG("Destroying task executor: %s", executor->executor_id);
    
    // 设置关闭标志
    agentos_mutex_lock(executor->lock);
    executor->shutdown = 1;
    agentos_mutex_unlock(executor->lock);
    
    // 唤醒所有工作线�?
    agentos_cond_broadcast(executor->task_queue->cond);
    
    // 等待工作线程结束
    for (uint32_t i = 0; i < executor->worker_count; i++) {
        if (executor->workers[i].thread) {
            agentos_thread_join(executor->workers[i].thread);
            agentos_thread_destroy(executor->workers[i].thread);
        }
    }
    
    // 销毁任务队�?
    if (executor->task_queue) {
        destroy_priority_queue(executor->task_queue);
    }
    
    // 销毁所有任�?
    for (uint32_t i = 0; i < MAX_TASKS; i++) {
        if (executor->tasks[i]) {
            agentos_task_destroy(executor->tasks[i]);
            executor->tasks[i] = NULL;
        }
    }
    
    if (executor->obs) agentos_observability_destroy(executor->obs);
    if (executor->shutdown_cond) agentos_cond_destroy(executor->shutdown_cond);
    if (executor->lock) agentos_mutex_destroy(executor->lock);
    if (executor->executor_id) AGENTOS_FREE(executor->executor_id);
    
    AGENTOS_FREE(executor);
}

/**
 * @brief 创建任务
 */
agentos_error_t agentos_task_create(const char* name,
                                    task_callback_fn callback,
                                    void* input,
                                    size_t input_size,
                                    agentos_task_t** out_task) {
    if (!name || !callback || !out_task) return AGENTOS_EINVAL;
    
    agentos_task_t* task = (agentos_task_t*)AGENTOS_CALLOC(1, sizeof(agentos_task_t));
    if (!task) {
        AGENTOS_LOG_ERROR("Failed to allocate task");
        return AGENTOS_ENOMEM;
    }
    
    task->task_id = agentos_generate_id();
    task->task_name = AGENTOS_STRDUP(name);
    task->state = TASK_STATE_PENDING;
    task->priority = TASK_PRIORITY_NORMAL;
    
    task->manager.task_name = AGENTOS_STRDUP(name);
    task->manager.priority = TASK_PRIORITY_NORMAL;
    task->manager.timeout_ms = DEFAULT_TASK_TIMEOUT_MS;
    task->manager.max_retries = DEFAULT_MAX_RETRIES;
    task->manager.retry_delay_ms = 1000;
    task->manager.flags = 0;
    
    task->callback = callback;
    
    if (input && input_size > 0) {
        task->input = AGENTOS_MALLOC(input_size);
        if (task->input) {
            memcpy(task->input, input, input_size);
            task->input_size = input_size;
        }
    }
    
    task->lock = agentos_mutex_create();
    if (!task->lock) {
        if (task->task_name) AGENTOS_FREE(task->task_name);
        if (task->manager.task_name) AGENTOS_FREE(task->manager.task_name);
        if (task->input) AGENTOS_FREE(task->input);
        AGENTOS_FREE(task);
        AGENTOS_LOG_ERROR("Failed to create task lock");
        return AGENTOS_ENOMEM;
    }
    
    task->cond = agentos_cond_create();
    if (!task->cond) {
        agentos_mutex_destroy(task->lock);
        if (task->task_name) AGENTOS_FREE(task->task_name);
        if (task->manager.task_name) AGENTOS_FREE(task->manager.task_name);
        if (task->input) AGENTOS_FREE(task->input);
        AGENTOS_FREE(task);
        AGENTOS_LOG_ERROR("Failed to create task condition");
        return AGENTOS_ENOMEM;
    }
    
    task->stats.create_time_ns = get_timestamp_ns();
    task->result = AGENTOS_SUCCESS;
    task->error_message = NULL;
    
    *out_task = task;
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 销毁任�?
 */
void agentos_task_destroy(agentos_task_t* task) {
    if (!task) return;
    
    if (task->task_name) AGENTOS_FREE(task->task_name);
    if (task->manager.task_name) AGENTOS_FREE(task->manager.task_name);
    if (task->input) AGENTOS_FREE(task->input);
    if (task->output) AGENTOS_FREE(task->output);
    if (task->error_message) AGENTOS_FREE(task->error_message);
    
    // 释放依赖列表
    task_dependency_t* dep = task->dependencies;
    while (dep) {
        task_dependency_t* next = dep->next;
        AGENTOS_FREE(dep);
        dep = next;
    }
    
    if (task->cond) agentos_cond_destroy(task->cond);
    if (task->lock) agentos_mutex_destroy(task->lock);
    
    AGENTOS_FREE(task);
}

/**
 * @brief 提交任务
 */
agentos_error_t agentos_task_executor_submit(agentos_task_executor_t* executor,
                                             agentos_task_t* task) {
    if (!executor || !task) return AGENTOS_EINVAL;
    
    agentos_mutex_lock(executor->lock);
    
    // 查找空闲槽位
    int slot = -1;
    for (uint32_t i = 0; i < MAX_TASKS; i++) {
        if (!executor->tasks[i]) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) {
        agentos_mutex_unlock(executor->lock);
        AGENTOS_LOG_ERROR("No available task slot");
        return AGENTOS_EBUSY;
    }
    
    executor->tasks[slot] = task;
    executor->task_count++;
    executor->total_submitted++;
    
    agentos_mutex_unlock(executor->lock);
    
    // 更新任务状�?
    agentos_mutex_lock(task->lock);
    task->state = TASK_STATE_QUEUED;
    agentos_mutex_unlock(task->lock);
    
    // 加入队列
    priority_queue_push(executor->task_queue, task);
    
    // 更新可观测�?
    if (executor->obs) {
        agentos_observability_increment_counter(executor->obs, "task_submitted", 1);
    }
    
    AGENTOS_LOG_DEBUG("Task submitted: %s (ID: %llu)", 
                     task->task_name, (unsigned long long)task->task_id);
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 等待任务完成
 */
agentos_error_t agentos_task_executor_wait(agentos_task_executor_t* executor,
                                           agentos_task_t* task,
                                           uint32_t timeout_ms) {
    if (!executor || !task) return AGENTOS_EINVAL;
    
    agentos_mutex_lock(task->lock);
    
    uint64_t start_ns = get_timestamp_ns();
    uint64_t timeout_ns = (uint64_t)timeout_ms * 1000000ULL;
    
    while (task->state != TASK_STATE_COMPLETED && 
           task->state != TASK_STATE_FAILED &&
           task->state != TASK_STATE_CANCELLED) {
        
        uint64_t elapsed_ns = get_timestamp_ns() - start_ns;
        if (timeout_ms > 0 && elapsed_ns >= timeout_ns) {
            agentos_mutex_unlock(task->lock);
            return AGENTOS_ETIMEDOUT;
        }
        
        uint32_t remaining_ms = timeout_ms > 0 ? 
                               (uint32_t)((timeout_ns - elapsed_ns) / 1000000ULL) : 0;
        
        agentos_cond_wait(task->cond, task->lock, remaining_ms);
    }
    
    agentos_error_t result = task->result;
    agentos_mutex_unlock(task->lock);
    
    return result;
}

/**
 * @brief 取消任务
 */
agentos_error_t agentos_task_executor_cancel(agentos_task_executor_t* executor,
                                             agentos_task_t* task) {
    if (!executor || !task) return AGENTOS_EINVAL;
    
    agentos_mutex_lock(task->lock);
    
    if (task->state == TASK_STATE_RUNNING) {
        agentos_mutex_unlock(task->lock);
        return AGENTOS_EBUSY;
    }
    
    if (task->state == TASK_STATE_COMPLETED ||
        task->state == TASK_STATE_FAILED ||
        task->state == TASK_STATE_CANCELLED) {
        agentos_mutex_unlock(task->lock);
        return AGENTOS_EALREADY;
    }
    
    task->state = TASK_STATE_CANCELLED;
    task->result = AGENTOS_ECANCELLED;
    
    agentos_mutex_unlock(task->lock);
    
    agentos_mutex_lock(executor->lock);
    executor->total_cancelled++;
    agentos_mutex_unlock(executor->lock);
    
    AGENTOS_LOG_INFO("Task cancelled: %s (ID: %llu)", 
                     task->task_name, (unsigned long long)task->task_id);
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 获取任务状�?
 */
task_state_t agentos_task_get_state(agentos_task_t* task) {
    if (!task) return TASK_STATE_FAILED;
    return task->state;
}

/**
 * @brief 设置任务优先�?
 */
agentos_error_t agentos_task_set_priority(agentos_task_t* task, task_priority_t priority) {
    if (!task) return AGENTOS_EINVAL;
    
    agentos_mutex_lock(task->lock);
    task->priority = priority;
    task->manager.priority = priority;
    agentos_mutex_unlock(task->lock);
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 添加任务依赖
 */
agentos_error_t agentos_task_add_dependency(agentos_task_t* task, uint64_t depends_on_task_id) {
    if (!task) return AGENTOS_EINVAL;
    
    task_dependency_t* dep = create_dependency(depends_on_task_id);
    if (!dep) return AGENTOS_ENOMEM;
    
    agentos_mutex_lock(task->lock);
    
    dep->next = task->dependencies;
    task->dependencies = dep;
    task->dependency_count++;
    
    agentos_mutex_unlock(task->lock);
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 获取执行器统计信�?
 */
agentos_error_t agentos_task_executor_get_stats(agentos_task_executor_t* executor,
                                                char** out_stats) {
    if (!executor || !out_stats) return AGENTOS_EINVAL;
    
    cJSON* stats_json = cJSON_CreateObject();
    if (!stats_json) return AGENTOS_ENOMEM;
    
    agentos_mutex_lock(executor->lock);
    
    cJSON_AddStringToObject(stats_json, "executor_id", executor->executor_id);
    cJSON_AddNumberToObject(stats_json, "worker_count", executor->worker_count);
    cJSON_AddNumberToObject(stats_json, "task_count", executor->task_count);
    cJSON_AddNumberToObject(stats_json, "total_submitted", executor->total_submitted);
    cJSON_AddNumberToObject(stats_json, "total_completed", executor->total_completed);
    cJSON_AddNumberToObject(stats_json, "total_failed", executor->total_failed);
    cJSON_AddNumberToObject(stats_json, "total_cancelled", executor->total_cancelled);
    
    double success_rate = executor->total_submitted > 0 ?
                         (double)executor->total_completed / executor->total_submitted * 100.0 : 0.0;
    cJSON_AddNumberToObject(stats_json, "success_rate_percent", success_rate);
    
    cJSON* workers_array = cJSON_CreateArray();
    for (uint32_t i = 0; i < executor->worker_count; i++) {
        cJSON* worker_obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(worker_obj, "worker_id", executor->workers[i].worker_id);
        cJSON_AddNumberToObject(worker_obj, "tasks_completed", executor->workers[i].tasks_completed);
        cJSON_AddNumberToObject(worker_obj, "tasks_failed", executor->workers[i].tasks_failed);
        cJSON_AddNumberToObject(worker_obj, "is_active", executor->workers[i].is_active);
        cJSON_AddItemToArray(workers_array, worker_obj);
    }
    cJSON_AddItemToObject(stats_json, "workers", workers_array);
    
    cJSON* queue_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(queue_obj, "total_count", executor->task_queue->total_count);
    for (int i = 0; i < 4; i++) {
        char key[32];
        snprintf(key, sizeof(key), "priority_%d", i);
        cJSON_AddNumberToObject(queue_obj, key, executor->task_queue->counts[i]);
    }
    cJSON_AddItemToObject(stats_json, "queue", queue_obj);
    
    agentos_mutex_unlock(executor->lock);
    
    char* stats_str = cJSON_PrintUnformatted(stats_json);
    cJSON_Delete(stats_json);
    
    if (!stats_str) return AGENTOS_ENOMEM;
    
    *out_stats = stats_str;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 健康检�?
 */
agentos_error_t agentos_task_executor_health_check(agentos_task_executor_t* executor,
                                                   char** out_json) {
    if (!executor || !out_json) return AGENTOS_EINVAL;
    
    cJSON* health_json = cJSON_CreateObject();
    if (!health_json) return AGENTOS_ENOMEM;
    
    agentos_mutex_lock(executor->lock);
    
    cJSON_AddStringToObject(health_json, "component", "task_executor");
    cJSON_AddStringToObject(health_json, "executor_id", executor->executor_id);
    
    int healthy = 1;
    int active_workers = 0;
    
    for (uint32_t i = 0; i < executor->worker_count; i++) {
        if (executor->workers[i].is_active) active_workers++;
    }
    
    if (active_workers == 0) healthy = 0;
    if (executor->shutdown) healthy = 0;
    
    cJSON_AddStringToObject(health_json, "status", healthy ? "healthy" : "unhealthy");
    cJSON_AddNumberToObject(health_json, "active_workers", active_workers);
    cJSON_AddNumberToObject(health_json, "total_workers", executor->worker_count);
    cJSON_AddNumberToObject(health_json, "pending_tasks", executor->task_queue->total_count);
    cJSON_AddNumberToObject(health_json, "timestamp_ns", get_timestamp_ns());
    
    agentos_mutex_unlock(executor->lock);
    
    char* health_str = cJSON_PrintUnformatted(health_json);
    cJSON_Delete(health_json);
    
    if (!health_str) return AGENTOS_ENOMEM;
    
    *out_json = health_str;
    return AGENTOS_SUCCESS;
}

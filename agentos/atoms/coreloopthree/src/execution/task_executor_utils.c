/**
 * @file task_executor_utils.c
 * @brief 任务执行器工具函数实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "task_executor_utils.h"
#include "agentos.h"
#include "logger.h"
#include <string.h>
#include <time.h>

/* 基础库兼容性层 */
#include <agentos/memory.h>
#include <agentos/string.h>

/* 任务依赖结构 */
typedef struct task_dependency {
    uint64_t task_id;
    int satisfied;
    struct task_dependency* next;
} task_dependency_t;

/* 任务状态枚举 */
typedef enum {
    TASK_STATE_PENDING = 0,
    TASK_STATE_QUEUED,
    TASK_STATE_RUNNING,
    TASK_STATE_COMPLETED,
    TASK_STATE_FAILED,
    TASK_STATE_CANCELLED,
    TASK_STATE_TIMEOUT
} task_state_t;

/* 任务内部结构（简化版） */
struct agentos_task {
    uint64_t task_id;
    char* task_name;
    task_state_t state;
    int priority;
    void* input;
    size_t input_size;
    void* output;
    size_t output_size;
    task_dependency_t* dependencies;
    uint32_t dependency_count;
    agentos_error_t result;
    char* error_message;
    agentos_mutex_t* lock;
    agentos_cond_t* cond;
    uint64_t create_time_ns;
    uint64_t start_time_ns;
    uint64_t end_time_ns;
    uint64_t duration_ns;
    uint32_t retry_count;
    uint32_t wait_count;
};

/* ==================== 工具函数 ==================== */

uint64_t task_get_timestamp_ns(void) {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

uint64_t task_calculate_duration(uint64_t start_ns, uint64_t end_ns) {
    if (end_ns > start_ns) {
        return end_ns - start_ns;
    }
    return 0;
}

char* task_format_error_message(agentos_error_t error, char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) return NULL;

    const char* msg = "Unknown error";
    switch (error) {
        case AGENTOS_SUCCESS: msg = "Success"; break;
        case AGENTOS_EINVAL: msg = "Invalid argument"; break;
        case AGENTOS_ENOMEM: msg = "Out of memory"; break;
        case AGENTOS_ETIMEDOUT: msg = "Timeout"; break;
        case AGENTOS_EDEPENDENCY: msg = "Dependency failed"; break;
        default: break;
    }

    snprintf(buffer, buffer_size, "Error %d: %s", (int)error, msg);
    return buffer;
}

int task_check_dependencies(agentos_task_t* task) {
    if (!task || !task->dependencies) return 1;  /* 无依赖，直接满足 */

    agentos_mutex_lock(task->lock);

    task_dependency_t* dep = task->dependencies;
    while (dep) {
        if (!dep->satisfied) {
            agentos_mutex_unlock(task->lock);
            return 0;  /* 有未满足的依赖 */
        }
        dep = dep->next;
    }

    agentos_mutex_unlock(task->lock);
    return 1;  /* 所有依赖都满足 */
}

agentos_error_t task_add_dependency(agentos_task_t* task, uint64_t depends_on_id) {
    if (!task) return AGENTOS_EINVAL;

    task_dependency_t* dep = (task_dependency_t*)AGENTOS_CALLOC(1, sizeof(task_dependency_t));
    if (!dep) return AGENTOS_ENOMEM;

    dep->task_id = depends_on_id;
    dep->satisfied = 0;
    dep->next = NULL;

    agentos_mutex_lock(task->lock);

    /* 添加到依赖链表头部 */
    dep->next = task->dependencies;
    task->dependencies = dep;
    task->dependency_count++;

    agentos_mutex_unlock(task->lock);

    AGENTOS_LOG_DEBUG("Added dependency on task %llu to task %llu",
                     (unsigned long long)depends_on_id,
                     (unsigned long long)task->task_id);
    return AGENTOS_SUCCESS;
}

void task_clear_dependencies(agentos_task_t* task) {
    if (!task || !task->dependencies) return;

    agentos_mutex_lock(task->lock);

    task_dependency_t* dep = task->dependencies;
    while (dep) {
        task_dependency_t* next = dep->next;
        AGENTOS_FREE(dep);
        dep = next;
    }
    task->dependencies = NULL;
    task->dependency_count = 0;

    agentos_mutex_unlock(task->lock);
}

void task_update_stats(agentos_task_t* task, int state) {
    if (!task) return;

    agentos_mutex_lock(task->lock);

    task->state = (task_state_t)state;

    switch (state) {
        case TASK_STATE_PENDING:
            task->create_time_ns = task_get_timestamp_ns();
            break;
        case TASK_STATE_RUNNING:
            task->start_time_ns = task_get_timestamp_ns();
            break;
        case TASK_STATE_COMPLETED:
        case TASK_STATE_FAILED:
        case TASK_STATE_CANCELLED:
        case TASK_STATE_TIMEOUT:
            task->end_time_ns = task_get_timestamp_ns();
            task->duration_ns = task_calculate_duration(task->start_time_ns, task->end_time_ns);
            break;
        default:
            break;
    }

    agentos_mutex_unlock(task->lock);
}

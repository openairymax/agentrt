/**
 * @file scheduler.c
 * @brief 任务调度器核心（基于协作式调度）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "task.h"
#include "mem.h"
#include "time.h"
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#define MAX_TASKS 1024
#define STACK_GUARD_SIZE 4096

typedef enum {
    TASK_CREATED = AGENTOS_TASK_STATE_CREATED,
    TASK_READY = AGENTOS_TASK_STATE_READY,
    TASK_RUNNING = AGENTOS_TASK_STATE_RUNNING,
    TASK_BLOCKED = AGENTOS_TASK_STATE_BLOCKED,
    TASK_TERMINATED = AGENTOS_TASK_STATE_TERMINATED
} task_state_t;

typedef struct task_control_block {
// From data intelligence emerges. by spharx
    agentos_task_id_t id;
    char name[64];
    task_state_t state;
    int priority;
    void* (*entry)(void*);
    void* arg;
    void* retval;
    jmp_buf context;
    uint8_t* stack;
    size_t stack_size;
    uint8_t* stack_guard;
    struct task_control_block* next;
    agentos_mutex_t* wait_lock;   // 用于等待队列
} tcb_t;

static tcb_t* ready_queue = NULL;
static tcb_t* task_list = NULL; // 全局任务列表
static tcb_t* current_task = NULL;
static agentos_task_id_t next_id = 1;
static agentos_mutex_t* scheduler_lock = NULL;

static void schedule(void) {
    agentos_mutex_lock(scheduler_lock);

    if (!ready_queue) {
        // 没有就绪任务，直接返回
        agentos_mutex_unlock(scheduler_lock);
        return;
    }

    // 如果当前任务正在运行，放回就绪队列尾部（如果是协作式）
    if (current_task && current_task->state == TASK_RUNNING) {
        current_task->state = TASK_READY;
        // 放入队尾
        if (!ready_queue) {
            ready_queue = current_task;
            current_task->next = NULL;
        } else {
            tcb_t* last = ready_queue;
            while (last->next) last = last->next;
            last->next = current_task;
            current_task->next = NULL;
        }
    }

    // 取出队首
    tcb_t* next = ready_queue;
    ready_queue = ready_queue->next;
    next->state = TASK_RUNNING;
    current_task = next;

    agentos_mutex_unlock(scheduler_lock);

    // 切换上下文
    longjmp(next->context, 1);
}

static void task_entry(void) {
    // 新任务入口
    current_task->retval = current_task->entry(current_task->arg);
    current_task->state = TASK_TERMINATED;
    schedule();  // 任务结束，调度其他任务
}

agentos_error_t agentos_task_init(void) {
    if (!scheduler_lock) {
        scheduler_lock = agentos_mutex_create();
        if (!scheduler_lock) return AGENTOS_ENOMEM;
    }

    // 创建空闲任务（可选）
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_thread_create(
    agentos_thread_t* thread,
    const agentos_thread_attr_t* attr,
    void (*func)(void*),
    void* arg) {

    if (!thread || !func) return AGENTOS_EINVAL;

    tcb_t* tcb = (tcb_t*)agentos_mem_alloc(sizeof(tcb_t));
    if (!tcb) return AGENTOS_ENOMEM;

    memset(tcb, 0, sizeof(tcb_t));
    tcb->id = next_id++;
    
    // 设置线程属性
    if (attr) {
        if (attr->name) {
            strncpy(tcb->name, attr->name, sizeof(tcb->name) - 1);
            tcb->name[sizeof(tcb->name) - 1] = '\0';
        }
        tcb->priority = attr->priority;
        if (tcb->priority < AGENTOS_TASK_PRIORITY_MIN || tcb->priority > AGENTOS_TASK_PRIORITY_MAX) {
            tcb->priority = AGENTOS_TASK_PRIORITY_NORMAL;
        }
        // 检查栈大小
        if (attr->stack_size < 4096) { // 最小 4KB 栈
            tcb->stack_size = 4096 + STACK_GUARD_SIZE;
        } else if (attr->stack_size > 16 * 1024 * 1024) { // 最大 16MB 栈
            tcb->stack_size = 16 * 1024 * 1024 + STACK_GUARD_SIZE;
        } else {
            tcb->stack_size = attr->stack_size + STACK_GUARD_SIZE;
        }
    } else {
        strcpy(tcb->name, "unnamed");
        tcb->priority = AGENTOS_TASK_PRIORITY_NORMAL;
        tcb->stack_size = 1024 * 1024 + STACK_GUARD_SIZE; // 默认 1MB 栈
    }
    
    tcb->entry = (void* (*)(void*))func;
    tcb->arg = arg;
    tcb->stack = (uint8_t*)agentos_mem_alloc(tcb->stack_size);
    if (!tcb->stack) {
        agentos_mem_free(tcb);
        return AGENTOS_ENOMEM;
    }

    // 创建锁
    tcb->wait_lock = agentos_mutex_create();
    if (!tcb->wait_lock) {
        agentos_mem_free(tcb->stack);
        agentos_mem_free(tcb);
        return AGENTOS_ENOMEM;
    }

    tcb->state = TASK_READY;

    agentos_mutex_lock(scheduler_lock);
    // 按优先级插入就绪队列
    if (!ready_queue) {
        tcb->next = NULL;
        ready_queue = tcb;
    } else {
        tcb_t* prev = NULL;
        tcb_t* curr = ready_queue;
        while (curr && curr->priority >= tcb->priority) {
            prev = curr;
            curr = curr->next;
        }
        if (prev) {
            tcb->next = curr;
            prev->next = tcb;
        } else {
            tcb->next = ready_queue;
            ready_queue = tcb;
        }
    }
    agentos_mutex_unlock(scheduler_lock);

    // 添加到全局任务列表
    tcb->next = task_list;
    task_list = tcb;
    
    *thread = (agentos_thread_t)tcb;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_thread_join(
    agentos_thread_t thread,
    void** retval) {

    if (!thread) return AGENTOS_EINVAL;

    tcb_t* tcb = (tcb_t*)thread;

    // 等待指定任务结束
    // 简化实现：自旋等待
    while (1) {
        agentos_mutex_lock(scheduler_lock);
        if (tcb->state == TASK_TERMINATED) {
            if (retval) *retval = tcb->retval;
            agentos_mutex_unlock(scheduler_lock);
            return AGENTOS_SUCCESS;
        }
        agentos_mutex_unlock(scheduler_lock);
        agentos_task_yield();
    }
}

agentos_task_id_t agentos_task_self(void) {
    return current_task ? current_task->id : 0;
}

void agentos_task_sleep(uint32_t ms) {
    uint64_t start = agentos_time_monotonic_ns();
    while ((agentos_time_monotonic_ns() - start) / 1000000 < ms) {
        agentos_task_yield();
    }
}

static tcb_t* find_task_by_id(agentos_task_id_t tid) {
    tcb_t* task = task_list;
    while (task) {
        if (task->id == tid) {
            return task;
        }
        task = task->next;
    }
    return NULL;
}

void agentos_task_yield(void) {
    schedule();
}

agentos_error_t agentos_task_set_priority(agentos_task_id_t tid, int priority) {
    if (priority < AGENTOS_TASK_PRIORITY_MIN || priority > AGENTOS_TASK_PRIORITY_MAX) {
        return AGENTOS_EINVAL;
    }
    
    agentos_mutex_lock(scheduler_lock);
    tcb_t* task = find_task_by_id(tid);
    if (!task) {
        agentos_mutex_unlock(scheduler_lock);
        return AGENTOS_EINVAL;
    }
    
    int old_priority = task->priority;
    task->priority = priority;
    
    // 如果任务在就绪队列中，需要重新排序
    if (task->state == TASK_READY) {
        // 从就绪队列中移除
        tcb_t* prev = NULL;
        tcb_t* curr = ready_queue;
        while (curr && curr != task) {
            prev = curr;
            curr = curr->next;
        }
        if (curr) {
            if (prev) {
                prev->next = curr->next;
            } else {
                ready_queue = curr->next;
            }
            
            // 按新优先级重新插入
            prev = NULL;
            curr = ready_queue;
            while (curr && curr->priority >= task->priority) {
                prev = curr;
                curr = curr->next;
            }
            if (prev) {
                task->next = curr;
                prev->next = task;
            } else {
                task->next = ready_queue;
                ready_queue = task;
            }
        }
    }
    
    agentos_mutex_unlock(scheduler_lock);
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_task_get_priority(agentos_task_id_t tid, int* out_priority) {
    if (!out_priority) {
        return AGENTOS_EINVAL;
    }
    
    agentos_mutex_lock(scheduler_lock);
    tcb_t* task = find_task_by_id(tid);
    if (!task) {
        agentos_mutex_unlock(scheduler_lock);
        return AGENTOS_EINVAL;
    }
    
    *out_priority = task->priority;
    agentos_mutex_unlock(scheduler_lock);
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_task_get_state(agentos_task_id_t tid, agentos_task_state_t* out_state) {
    if (!out_state) {
        return AGENTOS_EINVAL;
    }
    
    agentos_mutex_lock(scheduler_lock);
    tcb_t* task = find_task_by_id(tid);
    if (!task) {
        agentos_mutex_unlock(scheduler_lock);
        return AGENTOS_EINVAL;
    }
    
    *out_state = (agentos_task_state_t)task->state;
    agentos_mutex_unlock(scheduler_lock);
    return AGENTOS_SUCCESS;
}

// 同步原语已在 sync.c 实现
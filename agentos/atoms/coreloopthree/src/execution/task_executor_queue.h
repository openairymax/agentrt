/**
 * @file task_executor_queue.h
 * @brief 任务执行器优先级队列接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_TASK_EXECUTOR_QUEUE_H
#define AGENTOS_TASK_EXECUTOR_QUEUE_H

#include "../../../atoms/corekern/include/agentos.h"
#include <stdint.h>

/* 前向声明 */
typedef struct agentos_task agentos_task_t;
typedef struct priority_queue priority_queue_t;

/**
 * @brief 任务优先级
 */
typedef enum {
    TASK_PRIORITY_LOW = 0,      /**< 低优先级 */
    TASK_PRIORITY_NORMAL,       /**< 普通优先级 */
    TASK_PRIORITY_HIGH,         /**< 高优先级 */
    TASK_PRIORITY_CRITICAL      /**< 关键优先级 */
} task_priority_t;

/**
 * @brief 创建优先级队列
 * @return 优先级队列，失败返回 NULL
 */
priority_queue_t* task_queue_create(void);

/**
 * @brief 销毁优先级队列
 * @param queue 优先级队列
 */
void task_queue_destroy(priority_queue_t* queue);

/**
 * @brief 向优先级队列添加任务
 * @param queue 优先级队列
 * @param task 任务
 * @param priority 优先级
 * @return 0 成功，-1 失败
 */
int task_queue_push(priority_queue_t* queue, agentos_task_t* task, task_priority_t priority);

/**
 * @brief 从优先级队列获取任务
 * @param queue 优先级队列
 * @param timeout_ms 超时时间（毫秒）
 * @return 任务，NULL 表示超时或错误
 */
agentos_task_t* task_queue_pop(priority_queue_t* queue, uint32_t timeout_ms);

/**
 * @brief 获取队列中的任务数量
 * @param queue 优先级队列
 * @return 任务数量
 */
uint32_t task_queue_size(priority_queue_t* queue);

/**
 * @brief 检查队列是否为空
 * @param queue 优先级队列
 * @return 1 表示空，0 表示非空
 */
int task_queue_is_empty(priority_queue_t* queue);

#endif /* AGENTOS_TASK_EXECUTOR_QUEUE_H */

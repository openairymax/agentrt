/**
 * @file task_executor_utils.h
 * @brief 任务执行器工具函数接口 - 时间戳、统计、错误处理
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_TASK_EXECUTOR_UTILS_H
#define AGENTOS_TASK_EXECUTOR_UTILS_H

#include "../../../atoms/corekern/include/agentos.h"
#include <stdint.h>
#include <stddef.h>

/* 前向声明 */
typedef struct agentos_task agentos_task_t;

/**
 * @brief 获取时间戳（纳秒）
 * @return 时间戳
 */
uint64_t task_get_timestamp_ns(void);

/**
 * @brief 计算任务执行时长
 * @param start_ns 开始时间
 * @param end_ns 结束时间
 * @return 执行时长（纳秒）
 */
uint64_t task_calculate_duration(uint64_t start_ns, uint64_t end_ns);

/**
 * @brief 格式化错误信息
 * @param error 错误码
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return 格式化后的字符串
 */
char* task_format_error_message(agentos_error_t error, char* buffer, size_t buffer_size);

/**
 * @brief 检查任务依赖是否满足
 * @param task 任务句柄
 * @return 1 表示满足，0 表示不满足
 */
int task_check_dependencies(agentos_task_t* task);

/**
 * @brief 添加任务依赖
 * @param task 任务句柄
 * @param depends_on_id 依赖的任务 ID
 * @return AGENTOS_SUCCESS 成功，其他为错误码
 */
agentos_error_t task_add_dependency(agentos_task_t* task, uint64_t depends_on_id);

/**
 * @brief 清除任务依赖
 * @param task 任务句柄
 */
void task_clear_dependencies(agentos_task_t* task);

/**
 * @brief 更新任务统计信息
 * @param task 任务句柄
 * @param state 任务状态
 */
void task_update_stats(agentos_task_t* task, int state);

#endif /* AGENTOS_TASK_EXECUTOR_UTILS_H */

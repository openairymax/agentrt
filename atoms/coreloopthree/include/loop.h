/**
 * @file loop.h
 * @brief 三层核心运行时主循环接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#ifndef AGENTOS_LOOP_H
#define AGENTOS_LOOP_H

#include "cognition.h"
#include "execution.h"
#include "memory.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明 */
typedef struct agentos_core_loop agentos_core_loop_t;

/**
 * @brief 核心循环配置
 */
typedef struct agentos_loop_config {
    uint32_t cognition_threads;         /**< 认知层线程数 */
    uint32_t execution_threads;         /**< 行动层线程数 */
    uint32_t memory_threads;            /**< 记忆层线程数 */
    uint32_t max_queued_tasks;          /**< 最大排队任务数 */
    uint32_t stats_interval_ms;         /**< 统计输出间隔（毫秒，0表示不输出） */
    agentos_plan_strategy_t* plan_strategy;      /**< 规划策略（可选） */
    agentos_coordinator_strategy_t* coord_strategy; /**< 协同策略（可选） */
    agentos_dispatching_strategy_t* disp_strategy; /**< 调度策略（可选） */
} agentos_loop_config_t;

/**
 * @brief 创建核心循环
 * @param config 配置（可为NULL，使用默认）
 * @param out_loop 输出循环句柄
 * @return agentos_error_t
 */
agentos_error_t agentos_loop_create(
    const agentos_loop_config_t* config,
    agentos_core_loop_t** out_loop);

/**
 * @brief 销毁核心循环
 * @param loop 循环句柄
 */
void agentos_loop_destroy(agentos_core_loop_t* loop);

/**
 * @brief 启动核心循环（阻塞，直到停止）
 * @param loop 循环句柄
 * @return agentos_error_t
 */
agentos_error_t agentos_loop_run(agentos_core_loop_t* loop);

/**
 * @brief 停止核心循环
 * @param loop 循环句柄
 */
void agentos_loop_stop(agentos_core_loop_t* loop);

/**
 * @brief 提交一个用户任务（自然语言输入）
 * @param loop 循环句柄
 * @param input 输入字符串
 * @param input_len 输入长度
 * @param out_task_id 输出任务ID（需释放）
 * @return agentos_error_t
 */
agentos_error_t agentos_loop_submit(
    agentos_core_loop_t* loop,
    const char* input,
    size_t input_len,
    char** out_task_id);

/**
 * @brief 等待任务完成并获取结果
 * @param loop 循环句柄
 * @param task_id 任务ID
 * @param timeout_ms 超时时间（0无限）
 * @param out_result 输出结果（JSON字符串，需释放）
 * @param out_result_len 输出结果长度
 * @return agentos_error_t
 */
agentos_error_t agentos_loop_wait(
    agentos_core_loop_t* loop,
    const char* task_id,
    uint32_t timeout_ms,
    char** out_result,
    size_t* out_result_len);

/**
 * @brief 获取循环内部组件（用于扩展）
 * @param loop 循环句柄
 * @param out_cognition 输出认知引擎指针（可为NULL）
 * @param out_execution 输出执行引擎指针（可为NULL）
 * @param out_memory 输出记忆引擎指针（可为NULL）
 */
void agentos_loop_get_engines(
    agentos_core_loop_t* loop,
    agentos_cognition_engine_t** out_cognition,
    agentos_execution_engine_t** out_execution,
    agentos_memory_engine_t** out_memory);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_LOOP_H */
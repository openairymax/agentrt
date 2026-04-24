/**
 * @file loop.h
 * @brief 三层核心运行时主循环接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_LOOP_H
#define AGENTOS_LOOP_H

// API 版本声明 (MAJOR.MINOR.PATCH)
#define LOOP_API_VERSION_MAJOR 1
#define LOOP_API_VERSION_MINOR 0
#define LOOP_API_VERSION_PATCH 0

// ABI 兼容性声明
// 在相同 MAJOR 版本内保证 ABI 兼容
// 破坏性更改需递增 MAJOR 并发布迁移说明

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
    uint32_t loop_config_cognition_threads;         /**< 认知层线程数 */
    uint32_t loop_config_execution_threads;         /**< 行动层线程数 */
    uint32_t loop_config_memory_threads;            /**< 记忆层线程数 */
    uint32_t loop_config_max_queued_tasks;          /**< 最大排队任务数 */
    uint32_t loop_config_stats_interval_ms;         /**< 统计输出间隔（毫秒，0表示不输出） */
    size_t   loop_config_memory_query_limit;        /**< 记忆检索上限（默认5） */
    uint32_t loop_config_task_timeout_ms;           /**< 任务执行超时（毫秒，默认30000） */
    float    loop_config_memory_importance;         /**< 记忆重要性权重（默认0.7） */
    agentos_plan_strategy_t* loop_config_plan_strategy;      /**< 规划策略（可选） */
    agentos_coordinator_strategy_t* loop_config_coord_strategy; /**< 协同策略（可选） */
    agentos_dispatching_strategy_t* loop_config_disp_strategy; /**< 调度策略（可选） */
} agentos_loop_config_t;

/**
 * @brief 创建核心循环
 *
 * @param manager [in] 配置（可为NULL，使用默认）
 * @param out_loop [out] 输出循环句柄（调用者负责销毁）
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 *
 * @ownership out_loop 由调用者负责通过 agentos_loop_destroy() 释放
 * @threadsafe 否（内部未使用线程安全措施）
 * @reentrant 否
 * @see agentos_loop_destroy()
 */
AGENTOS_API agentos_error_t agentos_loop_create(
    const agentos_loop_config_t* manager,
    agentos_core_loop_t** out_loop);

/**
 * @brief 销毁核心循环
 *
 * @param loop [in] 循环句柄（非NULL）
 *
 * @ownership 释放 loop 及其内部所有资源
 * @threadsafe 否（内部未使用线程安全措施）
 * @reentrant 否
 * @see agentos_loop_create()
 */
AGENTOS_API void agentos_loop_destroy(agentos_core_loop_t* loop);

/**
 * @brief 启动核心循环（阻塞，直到停止）
 *
 * @param loop [in] 循环句柄（非NULL）
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 *
 * @threadsafe 否（内部未使用线程安全措施）
 * @reentrant 否
 * @see agentos_loop_stop()
 */
AGENTOS_API agentos_error_t agentos_loop_run(agentos_core_loop_t* loop);

/**
 * @brief 停止核心循环
 *
 * @param loop [in] 循环句柄（非NULL）
 *
 * @threadsafe 是（内部使用条件变量和互斥锁）
 * @reentrant 否
 * @see agentos_loop_run()
 */
AGENTOS_API void agentos_loop_stop(agentos_core_loop_t* loop);

/**
 * @brief 提交一个用户任务（自然语言输入）
 *
 * @param loop [in] 循环句柄（非NULL）
 * @param input [in] 输入字符串（非NULL）
 * @param input_len [in] 输入长度
 * @param out_task_id [out] 输出任务ID（调用者负责释放）
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 *
 * @ownership out_task_id 由调用者负责释放
 * @threadsafe 是（内部使用队列和互斥锁）
 * @reentrant 否
 * @see agentos_loop_wait()
 */
AGENTOS_API agentos_error_t agentos_loop_submit(
    agentos_core_loop_t* loop,
    const char* input,
    size_t input_len,
    char** out_task_id);

/**
 * @brief 等待任务完成并获取结果
 *
 * @param loop [in] 循环句柄（非NULL）
 * @param task_id [in] 任务ID（非NULL）
 * @param timeout_ms [in] 超时时间（0无限）
 * @param out_result [out] 输出结果（JSON字符串，调用者负责释放）
 * @param out_result_len [out] 输出结果长度
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 *
 * @ownership out_result 由调用者负责释放
 * @threadsafe 是（内部使用条件变量和互斥锁）
 * @reentrant 否
 * @see agentos_loop_submit()
 */
AGENTOS_API agentos_error_t agentos_loop_wait(
    agentos_core_loop_t* loop,
    const char* task_id,
    uint32_t timeout_ms,
    char** out_result,
    size_t* out_result_len);

/**
 * @brief 获取循环内部组件（用于扩展）
 *
 * @param loop [in] 循环句柄（非NULL）
 * @param out_cognition [out] 输出认知引擎指针（可为NULL）
 * @param out_execution [out] 输出执行引擎指针（可为NULL）
 * @param out_memory [out] 输出记忆引擎指针（可为NULL）
 *
 * @ownership 不转移引擎的所有权，调用者不应尝试释放这些引擎
 * @threadsafe 否（内部未使用线程安全措施）
 * @reentrant 否
 */
AGENTOS_API void agentos_loop_get_engines(
    agentos_core_loop_t* loop,
    agentos_cognition_engine_t** out_cognition,
    agentos_execution_engine_t** out_execution,
    agentos_memory_engine_t** out_memory);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_LOOP_H */

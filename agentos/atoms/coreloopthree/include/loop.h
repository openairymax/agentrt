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
typedef struct agentos_checkpoint_stats agentos_checkpoint_stats_t;

/**
 * @brief 核心循环配置
 */
typedef struct agentos_loop_config {
    uint32_t loop_config_cognition_threads; /**< 认知层线程数 */
    uint32_t loop_config_execution_threads; /**< 行动层线程数 */
    uint32_t loop_config_memory_threads;    /**< 记忆层线程数 */
    uint32_t loop_config_max_queued_tasks;  /**< 最大排队任务数 */
    uint32_t loop_config_stats_interval_ms; /**< 统计输出间隔（毫秒，0表示不输出） */
    size_t loop_config_memory_query_limit;  /**< 记忆检索上限（默认5） */
    uint32_t loop_config_task_timeout_ms;   /**< 任务执行超时（毫秒，默认30000） */
    float loop_config_memory_importance;    /**< 记忆重要性权重（默认0.7） */
    agentos_plan_strategy_t *loop_config_plan_strategy;         /**< 规划策略（可选） */
    agentos_coordinator_strategy_t *loop_config_coord_strategy; /**< 协同策略（可选） */
    agentos_dispatching_strategy_t *loop_config_disp_strategy;  /**< 调度策略（可选） */
    uint32_t loop_config_checkpoint_enabled; /**< Checkpoint启用标志（0禁用，1启用） */
    char loop_config_checkpoint_path[256];   /**< Checkpoint存储路径 */
    uint32_t loop_config_checkpoint_interval_ms; /**< Checkpoint保存间隔（毫秒，默认30000） */
    uint32_t loop_config_checkpoint_interval_turns; /**< P1.6: 每N轮触发checkpoint（0禁用轮次触发，默认0） */
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
AGENTOS_API agentos_error_t agentos_loop_create(const agentos_loop_config_t *manager,
                                                agentos_core_loop_t **out_loop);

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
AGENTOS_API void agentos_loop_destroy(agentos_core_loop_t *loop);

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
AGENTOS_API agentos_error_t agentos_loop_run(agentos_core_loop_t *loop);

/**
 * @brief 停止核心循环
 *
 * @param loop [in] 循环句柄（非NULL）
 *
 * @threadsafe 是（内部使用条件变量和互斥锁）
 * @reentrant 否
 * @see agentos_loop_run()
 */
AGENTOS_API void agentos_loop_stop(agentos_core_loop_t *loop);

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
AGENTOS_API agentos_error_t agentos_loop_submit(agentos_core_loop_t *loop, const char *input,
                                                size_t input_len, char **out_task_id);

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
AGENTOS_API agentos_error_t agentos_loop_wait(agentos_core_loop_t *loop, const char *task_id,
                                              uint32_t timeout_ms, char **out_result,
                                              size_t *out_result_len);

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
AGENTOS_API void agentos_loop_get_engines(agentos_core_loop_t *loop,
                                          agentos_cognition_engine_t **out_cognition,
                                          agentos_execution_engine_t **out_execution,
                                          agentos_memory_engine_t **out_memory);

/**
 * @brief 提交持久化长任务（带检查点恢复能力）
 *
 * 与 agentos_loop_submit 不同，此函数在任务计划生成后自动保存检查点，
 * 确保长任务在进程崩溃或重启后可从最近检查点恢复。
 *
 * @param loop [in] 循环句柄（非NULL）
 * @param input [in] 输入字符串（非NULL）
 * @param input_len [in] 输入长度
 * @param session_id [in] 会话ID（可为NULL，自动生成）
 * @param out_task_id [out] 输出任务ID（调用者负责释放）
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 *
 * @ownership out_task_id 由调用者负责释放
 * @threadsafe 是（内部使用互斥锁保护）
 * @reentrant 否
 * @see agentos_loop_restore_task()
 */
AGENTOS_API agentos_error_t agentos_loop_submit_persistent(agentos_core_loop_t *loop,
                                                           const char *input, size_t input_len,
                                                           const char *session_id,
                                                           char **out_task_id);

/**
 * @brief 从检查点恢复长任务
 *
 * 查找指定任务的最新检查点，恢复任务状态并继续执行未完成的节点。
 *
 * @param loop [in] 循环句柄（非NULL）
 * @param task_id [in] 原任务ID（非NULL）
 * @param out_restored_task_id [out] 输出恢复后的新任务ID（调用者负责释放）
 * @return agentos_error_t AGENTOS_SUCCESS 恢复成功，AGENTOS_ENOENT 无检查点
 *
 * @ownership out_restored_task_id 由调用者负责释放
 * @threadsafe 是（内部使用互斥锁保护）
 * @reentrant 否
 * @see agentos_loop_submit_persistent()
 */
AGENTOS_API agentos_error_t agentos_loop_restore_task(agentos_core_loop_t *loop,
                                                      const char *task_id,
                                                      char **out_restored_task_id);

/**
 * @brief 列出可恢复的检查点任务
 *
 * @param loop [in] 循环句柄（非NULL）
 * @param out_task_ids [out] 输出任务ID数组（调用者负责释放数组及每个元素）
 * @param out_count [out] 输出数量
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 *
 * @ownership out_task_ids 由调用者负责释放
 * @threadsafe 是
 * @reentrant 否
 */
AGENTOS_API agentos_error_t agentos_loop_list_checkpoints(agentos_core_loop_t *loop,
                                                          char ***out_task_ids, size_t *out_count);

/**
 * @brief P1.6: 创建任务的完整快照（序列化到文件）
 *
 * 将当前任务状态完整序列化到指定路径的快照文件，包含认知状态、
 * 记忆上下文和工具调用历史。
 *
 * @param loop [in] 循环句柄（非NULL）
 * @param task_id [in] 任务ID（非NULL）
 * @param snapshot_path [in] 快照文件路径（非NULL）
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 *
 * @ownership 无所有权转移
 * @threadsafe 是
 * @reentrant 否
 */
AGENTOS_API agentos_error_t agentos_loop_create_snapshot(agentos_core_loop_t *loop,
                                                         const char *task_id,
                                                         const char *snapshot_path);

/**
 * @brief P1.6: 从快照文件恢复任务
 *
 * 从指定路径的快照文件恢复任务ID，然后通过 agentos_loop_restore_task
 * 恢复完整的任务状态。
 *
 * @param loop [in] 循环句柄（非NULL）
 * @param snapshot_path [in] 快照文件路径（非NULL）
 * @param out_restored_task_id [out] 输出恢复后的新任务ID（调用者负责释放）
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 *
 * @ownership out_restored_task_id 由调用者负责释放
 * @threadsafe 是
 * @reentrant 否
 */
AGENTOS_API agentos_error_t agentos_loop_restore_snapshot(agentos_core_loop_t *loop,
                                                          const char *snapshot_path,
                                                          char **out_restored_task_id);

/**
 * @brief P1.6: 获取 checkpoint 统计信息
 *
 * @param loop [in] 循环句柄（非NULL）
 * @param out_stats [out] 输出统计信息（调用者提供缓冲区）
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 *
 * @ownership out_stats 由调用者管理
 * @threadsafe 是
 * @reentrant 否
 */
AGENTOS_API agentos_error_t agentos_loop_get_checkpoint_stats(agentos_core_loop_t *loop,
                                                              agentos_checkpoint_stats_t *out_stats);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_LOOP_H */

/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file orchestrator.h
 * @brief AgentOS 流程编排器
 *
 * 协调多agent/多skill的流程编排，支持：
 * - Phase 0-4 串行执行管线
 * - 子任务分发与结果聚合
 * - 编排策略（串行/并行/条件/循环）
 * - 超时与熔断集成
 * - 思考链路追踪
 */

#ifndef AGENTOS_ORCHESTRATOR_H
#define AGENTOS_ORCHESTRATOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========== 类型前向声明 ========== */

typedef struct orchestrator_s orchestrator_t;
typedef struct orch_task_s orch_task_t;
typedef struct orch_pipeline_s orch_pipeline_t;
typedef struct llm_service llm_service_t;
typedef struct tool_service tool_service_t;

/* ========== 编排阶段枚举 ========== */

typedef enum {
    ORCH_PHASE_DECOMPOSITION = 0,
    ORCH_PHASE_PLANNING = 1,
    ORCH_PHASE_GENERATION = 2,
    ORCH_PHASE_CRITIQUE = 3,
    ORCH_PHASE_VERIFICATION = 4,
    ORCH_PHASE_AUDIT = 5,
    ORCH_PHASE_ALIGNMENT = 6,
    ORCH_PHASE_MAX
} orch_phase_t;

/* ========== 任务状态 ========== */

typedef enum {
    ORCH_TASK_PENDING = 0,
    ORCH_TASK_RUNNING = 1,
    ORCH_TASK_COMPLETED = 2,
    ORCH_TASK_FAILED = 3,
    ORCH_TASK_CANCELLED = 4,
    ORCH_TASK_TIMEOUT = 5
} orch_task_status_t;

/* ========== 编排策略 ========== */

typedef enum {
    ORCH_STRATEGY_SEQUENTIAL = 0,
    ORCH_STRATEGY_PARALLEL = 1,
    ORCH_STRATEGY_CONDITIONAL = 2,
    ORCH_STRATEGY_LOOP = 3,
    ORCH_STRATEGY_ADAPTIVE = 4
} orch_strategy_t;

/* ========== 回调类型 ========== */

/**
 * @brief Progress callback function type.
 * @param phase Current phase.
 * @param status Task status.
 * @param task_id [in] Task identifier (BORROW - caller must not free, valid for callback scope only).
 * @param user_data [in] User data (BORROW - caller must not free, valid for callback scope only).
 */
typedef void (*orch_progress_cb_t)(orch_phase_t phase, orch_task_status_t status,
                                   const char *task_id, void *user_data);

/**
 * @brief Condition function type.
 * @param context [in] Context string (BORROW - caller must not free, valid for callback scope only).
 * @param user_data [in] User data (BORROW - caller must not free, valid for callback scope only).
 */
typedef bool (*orch_condition_fn_t)(const char *context, void *user_data);

/* ========== 配置 ========== */

typedef struct {
    uint32_t max_subtasks;
    uint32_t timeout_ms;
    uint32_t max_retries;
    uint32_t retry_delay_ms;
    orch_strategy_t default_strategy;
    bool enable_thinking_chain;
    bool enable_metacognition;
    bool enable_circuit_breaker;
    bool enable_critique_loop;
    uint32_t critique_max_rounds;
    float critique_acceptance_threshold;
    float critique_auto_correct_threshold;
} orch_config_t;

/* ========== 任务结果 ========== */

typedef struct {
    char *task_id;
    char *output;
    size_t output_len;
    orch_task_status_t status;
    int error_code;
    uint32_t duration_ms;
    char *thinking_chain_id;
} orch_result_t;

/* ========== 管线定义 ========== */

typedef struct {
    orch_phase_t phase;
    const char *agent_id;
    const char *skill_id;
    const char *input;
    orch_strategy_t strategy;
    uint32_t timeout_ms;
    orch_condition_fn_t condition_fn;
    void *condition_data;
} orch_pipeline_step_t;

/* ========== 生命周期 ========== */

/**
 * @brief Create a new orchestrator instance.
 * @param config [in] Configuration (BORROW - not stored, copied internally).
 * @return New orchestrator handle (OWNER - caller must call orchestrator_destroy).
 */
orchestrator_t *orchestrator_create(const orch_config_t *config);

/**
 * @brief Destroy an orchestrator instance.
 * @param orch [in] Orchestrator handle (TRANSFER - function takes ownership and frees).
 */
void orchestrator_destroy(orchestrator_t *orch);

/* ========== 管线操作 ========== */

/**
 * @brief Create a new pipeline.
 * @param orch [in] Orchestrator handle (BORROW - caller retains ownership).
 * @param name [in] Pipeline name (BORROW - not stored, copied internally).
 * @return New pipeline handle (OWNER - caller must call orchestrator_pipeline_destroy).
 */
orch_pipeline_t *orchestrator_pipeline_create(orchestrator_t *orch, const char *name);

/**
 * @brief Destroy a pipeline.
 * @param pipeline [in] Pipeline handle (TRANSFER - function takes ownership and frees).
 */
void orchestrator_pipeline_destroy(orch_pipeline_t *pipeline);

/**
 * @brief Add a step to a pipeline.
 * @param pipeline [in] Pipeline handle (BORROW - caller retains ownership).
 * @param step [in] Step definition (BORROW - copied internally, not stored by pointer).
 * @return 0 on success, non-zero on failure.
 */
int orchestrator_pipeline_add_step(orch_pipeline_t *pipeline, const orch_pipeline_step_t *step);

/* ========== 执行 ========== */

/**
 * @brief Execute an orchestration run.
 * @param orch [in] Orchestrator handle (BORROW - caller retains ownership).
 * @param input [in] Input string (BORROW - not stored, copied internally).
 * @param out_results [out] Results array (OWNER - caller must call orchestrator_result_free on each, then free the array).
 * @param out_count [out] Number of results (BORROW - caller provides buffer, function writes to it).
 * @return 0 on success, non-zero on failure.
 */
int orchestrator_execute(orchestrator_t *orch, const char *input, orch_result_t **out_results,
                         size_t *out_count);

/**
 * @brief Execute a pipeline.
 * @param orch [in] Orchestrator handle (BORROW - caller retains ownership).
 * @param pipeline [in] Pipeline handle (BORROW - caller retains ownership).
 * @param input [in] Input string (BORROW - not stored, copied internally).
 * @param out_results [out] Results array (OWNER - caller must call orchestrator_result_free on each, then free the array).
 * @param out_count [out] Number of results (BORROW - caller provides buffer, function writes to it).
 * @return 0 on success, non-zero on failure.
 */
int orchestrator_execute_pipeline(orchestrator_t *orch, orch_pipeline_t *pipeline,
                                  const char *input, orch_result_t **out_results,
                                  size_t *out_count);

/**
 * @brief Execute a single phase.
 * @param orch [in] Orchestrator handle (BORROW - caller retains ownership).
 * @param phase Phase to execute.
 * @param input [in] Input string (BORROW - not stored, copied internally).
 * @param out_result [out] Result output (OWNER - caller must call orchestrator_result_free).
 * @return 0 on success, non-zero on failure.
 */
int orchestrator_execute_phase(orchestrator_t *orch, orch_phase_t phase, const char *input,
                               orch_result_t **out_result);

/* ========== 进度回调 ========== */

/**
 * @brief Set progress callback.
 * @param orch [in] Orchestrator handle (BORROW - caller retains ownership).
 * @param callback [in] Callback function (BORROW - not stored by pointer, copied internally).
 * @param user_data [in] User data passed to callback (BORROW - caller retains ownership, must remain valid).
 */
void orchestrator_set_progress_callback(orchestrator_t *orch, orch_progress_cb_t callback,
                                        void *user_data);

/* ========== C-L06: Orchestrator → CoreLoopThree 连接线 ========== */

/**
 * @brief C-L06: 将 llm_d 服务注入到编排器下的 CoreLoopThree 认知引擎
 *
 * 编排器持有认知引擎，通过此函数将 LLM 服务句柄传递到认知引擎，
 * 使认知循环 Phase 2 能够调用外部 LLM 服务。
 *
 * @param orch [in] Orchestrator handle (BORROW - caller retains ownership).
 * @param llm_svc [in] LLM service handle (BORROW - orchestrator does not take ownership, caller manages lifecycle).
 *
 * @ownership llm_svc: BORROW
 */
void orchestrator_set_cognition_llm_service(orchestrator_t *orch, llm_service_t *llm_svc);

/**
 * @brief C-L06: 将 tool_d 服务注入到编排器下的 CoreLoopThree 认知引擎
 *
 * 编排器持有认知引擎，通过此函数将工具服务句柄传递到认知引擎，
 * 使编排器的任务分发可以路由工具调用。
 *
 * @param orch [in] Orchestrator handle (BORROW - caller retains ownership).
 * @param tool_svc [in] Tool service handle (BORROW - orchestrator does not take ownership, caller manages lifecycle).
 *
 * @ownership tool_svc: BORROW
 */
void orchestrator_set_cognition_tool_service(orchestrator_t *orch, tool_service_t *tool_svc);

/* ========== 查询 ========== */

/**
 * @brief Get task status.
 * @param orch [in] Orchestrator handle (BORROW - caller retains ownership).
 * @param task_id [in] Task identifier (BORROW - not stored, copied internally).
 * @return Task status enum.
 */
orch_task_status_t orchestrator_get_task_status(orchestrator_t *orch, const char *task_id);

/**
 * @brief Get task result.
 * @param orch [in] Orchestrator handle (BORROW - caller retains ownership).
 * @param task_id [in] Task identifier (BORROW - not stored, copied internally).
 * @return Task result (BORROW - belongs to orchestrator, do not free; call orchestrator_result_free on a copy if needed).
 */
orch_result_t *orchestrator_get_result(orchestrator_t *orch, const char *task_id);

/**
 * @brief Free a result structure.
 * @param result [in] Result pointer (TRANSFER - function takes ownership and frees).
 */
void orchestrator_result_free(orch_result_t *result);

/**
 * @brief Get count of active tasks.
 * @param orch [in] Orchestrator handle (BORROW - caller retains ownership).
 * @return Number of active tasks.
 */
uint32_t orchestrator_active_count(orchestrator_t *orch);

/* ========== 控制 ========== */

/**
 * @brief Cancel a task.
 * @param orch [in] Orchestrator handle (BORROW - caller retains ownership).
 * @param task_id [in] Task identifier (BORROW - not stored, copied internally).
 * @return 0 on success, non-zero on failure.
 */
int orchestrator_cancel(orchestrator_t *orch, const char *task_id);

/**
 * @brief Cancel all tasks.
 * @param orch [in] Orchestrator handle (BORROW - caller retains ownership).
 * @return 0 on success, non-zero on failure.
 */
int orchestrator_cancel_all(orchestrator_t *orch);

/* ========== 全局资源清理 ========== */

/**
 * @brief 销毁全局静态 mutex（g_orch_bus_mutex, g_align_mutex）
 *
 * 应在进程退出时调用，确保所有懒初始化的全局 mutex 被正确销毁。
 */
void orchestrator_global_cleanup(void);

/* ========== 默认配置 ========== */

static inline void orch_config_get_defaults(orch_config_t *cfg)
{
    cfg->max_subtasks = 16;
    cfg->timeout_ms = 60000;
    cfg->max_retries = 3;
    cfg->retry_delay_ms = 100;
    cfg->default_strategy = ORCH_STRATEGY_SEQUENTIAL;
    cfg->enable_thinking_chain = true;
    cfg->enable_metacognition = true;
    cfg->enable_circuit_breaker = true;
    cfg->enable_critique_loop = true;
    cfg->critique_max_rounds = 3;
    cfg->critique_acceptance_threshold = 0.7f;
    cfg->critique_auto_correct_threshold = 0.5f;
}

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_ORCHESTRATOR_H */

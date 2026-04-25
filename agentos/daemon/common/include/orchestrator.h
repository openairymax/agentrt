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

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========== 类型前向声明 ========== */

typedef struct orchestrator_s orchestrator_t;
typedef struct orch_task_s orch_task_t;
typedef struct orch_pipeline_s orch_pipeline_t;

/* ========== 编排阶段枚举 ========== */

typedef enum {
    ORCH_PHASE_DECOMPOSITION = 0,
    ORCH_PHASE_PLANNING      = 1,
    ORCH_PHASE_GENERATION    = 2,
    ORCH_PHASE_VERIFICATION  = 3,
    ORCH_PHASE_AUDIT         = 4,
    ORCH_PHASE_ALIGNMENT     = 5,
    ORCH_PHASE_MAX
} orch_phase_t;

/* ========== 任务状态 ========== */

typedef enum {
    ORCH_TASK_PENDING    = 0,
    ORCH_TASK_RUNNING    = 1,
    ORCH_TASK_COMPLETED  = 2,
    ORCH_TASK_FAILED     = 3,
    ORCH_TASK_CANCELLED  = 4,
    ORCH_TASK_TIMEOUT    = 5
} orch_task_status_t;

/* ========== 编排策略 ========== */

typedef enum {
    ORCH_STRATEGY_SEQUENTIAL  = 0,
    ORCH_STRATEGY_PARALLEL    = 1,
    ORCH_STRATEGY_CONDITIONAL = 2,
    ORCH_STRATEGY_LOOP        = 3,
    ORCH_STRATEGY_ADAPTIVE    = 4
} orch_strategy_t;

/* ========== 回调类型 ========== */

typedef void (*orch_progress_cb_t)(orch_phase_t phase,
                                   orch_task_status_t status,
                                   const char* task_id,
                                   void* user_data);

typedef bool (*orch_condition_fn_t)(const char* context, void* user_data);

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
} orch_config_t;

/* ========== 任务结果 ========== */

typedef struct {
    char* task_id;
    char* output;
    size_t output_len;
    orch_task_status_t status;
    int error_code;
    uint32_t duration_ms;
    char* thinking_chain_id;
} orch_result_t;

/* ========== 管线定义 ========== */

typedef struct {
    orch_phase_t phase;
    const char* agent_id;
    const char* skill_id;
    const char* input;
    orch_strategy_t strategy;
    uint32_t timeout_ms;
    orch_condition_fn_t condition_fn;
    void* condition_data;
} orch_pipeline_step_t;

/* ========== 生命周期 ========== */

orchestrator_t* orchestrator_create(const orch_config_t* config);

void orchestrator_destroy(orchestrator_t* orch);

/* ========== 管线操作 ========== */

orch_pipeline_t* orchestrator_pipeline_create(orchestrator_t* orch,
                                              const char* name);

void orchestrator_pipeline_destroy(orch_pipeline_t* pipeline);

int orchestrator_pipeline_add_step(orch_pipeline_t* pipeline,
                                   const orch_pipeline_step_t* step);

/* ========== 执行 ========== */

int orchestrator_execute(orchestrator_t* orch,
                         const char* input,
                         orch_result_t** out_results,
                         size_t* out_count);

int orchestrator_execute_pipeline(orchestrator_t* orch,
                                  orch_pipeline_t* pipeline,
                                  const char* input,
                                  orch_result_t** out_results,
                                  size_t* out_count);

int orchestrator_execute_phase(orchestrator_t* orch,
                               orch_phase_t phase,
                               const char* input,
                               orch_result_t** out_result);

/* ========== 进度回调 ========== */

void orchestrator_set_progress_callback(orchestrator_t* orch,
                                        orch_progress_cb_t callback,
                                        void* user_data);

/* ========== 查询 ========== */

orch_task_status_t orchestrator_get_task_status(orchestrator_t* orch,
                                                const char* task_id);

orch_result_t* orchestrator_get_result(orchestrator_t* orch,
                                       const char* task_id);

void orchestrator_result_free(orch_result_t* result);

uint32_t orchestrator_active_count(orchestrator_t* orch);

/* ========== 控制 ========== */

int orchestrator_cancel(orchestrator_t* orch, const char* task_id);

int orchestrator_cancel_all(orchestrator_t* orch);

/* ========== 默认配置 ========== */

static inline void orch_config_get_defaults(orch_config_t* cfg) {
    cfg->max_subtasks = 16;
    cfg->timeout_ms = 60000;
    cfg->max_retries = 3;
    cfg->retry_delay_ms = 100;
    cfg->default_strategy = ORCH_STRATEGY_SEQUENTIAL;
    cfg->enable_thinking_chain = true;
    cfg->enable_metacognition = true;
    cfg->enable_circuit_breaker = true;
}

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_ORCHESTRATOR_H */

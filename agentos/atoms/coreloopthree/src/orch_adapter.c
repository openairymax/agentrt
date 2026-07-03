/**
 * @file orch_adapter.c
 * @brief C-L06: Orchestrator → CoreLoopThree 编排器适配器实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 实现 Orchestrator 编排引擎与 CoreLoopThree 核心循环的桥接，
 * 使编排器能够创建和管理 CoreLoopThree 实例来执行流水线步骤。
 *
 * 数据流：
 *   Orchestrator → orch_adapter_execute_pipeline()
 *     → 为每个步骤创建 CoreLoopThree 实例
 *     → 注入 LLM/Tool 服务
 *     → 步骤 N 的输出 → 步骤 N+1 的输入
 *     → 聚合所有步骤结果 → orch_result_t[]
 *
 * 支持：
 *   - 串行执行：步骤按序执行，前一步输出作为后一步输入
 *   - 并行执行：独立步骤并发执行
 *   - 进度回调：每个步骤完成时通知编排器
 *   - 检查点：流水线执行中定期保存进度
 */

#include "orch_adapter.h"
#include "orchestrator.h"

#include "checkpoint_adapter.h"
#include "llm_svc_adapter.h"
#include "loop.h"
#include "logger.h"
#include "memory_compat.h"
#include "string_compat.h"
#include "agentos_quality.h"
#include "tool_svc_adapter.h"

/* 跨平台路径常量（AGENTOS_DATA_DIR） */
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================== 默认配置 ==================== */

#define DEFAULT_MAX_PARALLEL_INSTANCES  8
#define DEFAULT_INSTANCE_TIMEOUT_MS     60000
#define DEFAULT_PIPELINE_TIMEOUT_MS     300000
#define DEFAULT_CHECKPOINT_PATH         AGENTOS_DATA_DIR "/checkpoints"

/* ==================== 适配器内部结构 ==================== */

struct orch_adapter_s {
    orchestrator_t *orch;              /* 编排器句柄 */
    orch_adapter_config_t config;

    /* 服务注入 */
    llm_svc_adapter_t *llm_adapter;    /* C-L02: LLM 服务适配器 */
    tool_svc_adapter_t *tool_adapter;  /* C-L04: 工具服务适配器 */

    /* 检查点 */
    checkpoint_adapter_t *checkpoint;  /* C-L07: 检查点适配器 */

    /* 实例池 */
    agentos_core_loop_t **loop_instances; /* CoreLoopThree 实例池 */
    uint32_t instance_count;           /* 当前实例数 */
    uint32_t max_instances;            /* 最大实例数 */

    /* 进度回调 */
    orch_adapter_progress_cb_t progress_cb;
    void *progress_user_data;

    /* 统计 */
    uint64_t total_pipelines;
    uint64_t total_steps;
    uint64_t total_errors;
    uint64_t total_latency_us;
};

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 获取当前时间戳（毫秒）
 */
static uint64_t get_current_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/**
 * @brief 从实例池获取或创建 CoreLoopThree 实例
 */
static agentos_core_loop_t *acquire_loop_instance(orch_adapter_t *adapter)
{
    if (!adapter) return NULL;

    /* 如果实例池未满，创建新实例 */
    if (adapter->instance_count < adapter->max_instances) {
        agentos_loop_config_t loop_cfg;
        __builtin_memset(&loop_cfg, 0, sizeof(loop_cfg));
        loop_cfg.loop_config_cognition_threads = 1;
        loop_cfg.loop_config_execution_threads = 1;
        loop_cfg.loop_config_memory_threads = 1;
        loop_cfg.loop_config_max_queued_tasks = 16;
        loop_cfg.loop_config_task_timeout_ms = adapter->config.instance_timeout_ms;
        loop_cfg.loop_config_checkpoint_enabled =
            adapter->config.enable_checkpoint ? 1 : 0;

        if (adapter->config.checkpoint_path) {
            safe_strcpy(loop_cfg.loop_config_checkpoint_path,
                        sizeof(loop_cfg.loop_config_checkpoint_path),
                        adapter->config.checkpoint_path);
        }

        agentos_core_loop_t *loop = NULL;
        agentos_error_t ret = agentos_loop_create(&loop_cfg, &loop);
        if (ret == AGENTOS_SUCCESS && loop) {
            adapter->loop_instances[adapter->instance_count] = loop;
            adapter->instance_count++;
            return loop;
        }
    }

    /* 如果池已满，返回第一个可用的实例（简单的轮询） */
    for (uint32_t i = 0; i < adapter->instance_count; i++) {
        if (adapter->loop_instances[i]) {
            return adapter->loop_instances[i];
        }
    }

    return NULL;
}

/**
 * @brief 释放 CoreLoopThree 实例回池
 */
static void release_loop_instance(orch_adapter_t *adapter,
                                  agentos_core_loop_t *loop)
{
    (void)adapter;
    (void)loop;
    /* 实例保留在池中供后续使用 */
}

/**
 * @brief 将 orch_result_t 转换为流水线输出 JSON 字符串
 */
__attribute__((unused))
static char *orch_result_to_output(const orch_result_t *result)
{
    if (!result) return NULL;

    if (result->output) {
        return AGENTOS_STRDUP(result->output);
    }

    return NULL;
}

/* ==================== 生命周期实现 ==================== */

orch_adapter_t *orch_adapter_create(orchestrator_t *orch,
                                    const orch_adapter_config_t *config)
{
    if (!orch) return NULL;

    orch_adapter_t *adapter =
        (orch_adapter_t *)AGENTOS_CALLOC(1, sizeof(orch_adapter_t));
    if (!adapter) {
        AGENTOS_LOG_ERROR("C-L06: orch_adapter_create: OOM");
        return NULL;
    }

    adapter->orch = orch;
    adapter->max_instances = (config && config->max_parallel_instances > 0)
                                 ? config->max_parallel_instances
                                 : DEFAULT_MAX_PARALLEL_INSTANCES;
    adapter->config.instance_timeout_ms =
        (config && config->instance_timeout_ms > 0)
            ? config->instance_timeout_ms : DEFAULT_INSTANCE_TIMEOUT_MS;
    adapter->config.pipeline_timeout_ms =
        (config && config->pipeline_timeout_ms > 0)
            ? config->pipeline_timeout_ms : DEFAULT_PIPELINE_TIMEOUT_MS;
    adapter->config.enable_progress_callback =
        config ? config->enable_progress_callback : false;
    adapter->config.enable_checkpoint =
        config ? config->enable_checkpoint : false;
    adapter->config.checkpoint_path =
        (config && config->checkpoint_path)
            ? config->checkpoint_path : DEFAULT_CHECKPOINT_PATH;

    /* 分配实例池 */
    adapter->loop_instances = (agentos_core_loop_t **)AGENTOS_CALLOC(
        adapter->max_instances, sizeof(agentos_core_loop_t *));
    if (!adapter->loop_instances) {
        AGENTOS_LOG_ERROR("C-L06: Failed to allocate instance pool");
        AGENTOS_FREE(adapter);
        return NULL;
    }

    adapter->instance_count = 0;
    adapter->llm_adapter = NULL;
    adapter->tool_adapter = NULL;
    adapter->checkpoint = NULL;
    adapter->progress_cb = NULL;
    adapter->progress_user_data = NULL;
    adapter->total_pipelines = 0;
    adapter->total_steps = 0;
    adapter->total_errors = 0;
    adapter->total_latency_us = 0;

    /* 初始化检查点适配器 */
    if (adapter->config.enable_checkpoint) {
        checkpoint_adapter_config_t ckpt_cfg;
        __builtin_memset(&ckpt_cfg, 0, sizeof(ckpt_cfg));
        ckpt_cfg.storage_path = adapter->config.checkpoint_path;
        ckpt_cfg.save_interval_turns = 10;
        ckpt_cfg.save_interval_ms = 30000;

        adapter->checkpoint = checkpoint_adapter_create(&ckpt_cfg);
        if (!adapter->checkpoint) {
            AGENTOS_LOG_WARN("C-L06: Checkpoint adapter init failed, "
                             "proceeding without checkpoint");
        }
    }

    AGENTOS_LOG_INFO("C-L06: Orchestrator adapter created "
                     "(max_instances=%u, instance_timeout=%ums, "
                     "pipeline_timeout=%ums, checkpoint=%s)",
                     adapter->max_instances,
                     adapter->config.instance_timeout_ms,
                     adapter->config.pipeline_timeout_ms,
                     adapter->config.enable_checkpoint ? "on" : "off");
    return adapter;
}

void orch_adapter_destroy(orch_adapter_t *adapter)
{
    if (!adapter) return;

    AGENTOS_LOG_INFO("C-L06: Orchestrator adapter destroyed "
                     "(pipelines=%llu steps=%llu errors=%llu)",
                     (unsigned long long)adapter->total_pipelines,
                     (unsigned long long)adapter->total_steps,
                     (unsigned long long)adapter->total_errors);

    /* 销毁所有 CoreLoopThree 实例 */
    for (uint32_t i = 0; i < adapter->instance_count; i++) {
        if (adapter->loop_instances[i]) {
            agentos_loop_destroy(adapter->loop_instances[i]);
            adapter->loop_instances[i] = NULL;
        }
    }

    if (adapter->loop_instances) AGENTOS_FREE(adapter->loop_instances);
    if (adapter->checkpoint) checkpoint_adapter_destroy(adapter->checkpoint);

    AGENTOS_FREE(adapter);
}

/* ==================== 服务注入实现 ==================== */

int orch_adapter_set_llm_service(orch_adapter_t *adapter,
                                 void *llm_svc_adapter)
{
    if (!adapter) return -1;

    adapter->llm_adapter = (llm_svc_adapter_t *)llm_svc_adapter;

    /* C-L06 P1.5.1: adapter 句柄暂存。LLM 能力经 loop.c 的
     * agentos_cognition_set_llm_adapter() 注入到认知引擎（IPC 路径，
     * P1.2.1 首选）。旧代码通过 llm_svc_adapter_get_service() 获取
     * llm_service_t 句柄并经 orchestrator_set_cognition_llm_service() 注入，
     * 但 get_service 始终返回 NULL（llm_service_t 由 llm_d daemon 内部管理，
     * coreloopthree 不直接持有），该路径已移除。 */

    AGENTOS_LOG_INFO("C-L06: LLM service injected into orchestrator");
    return 0;
}

int orch_adapter_set_tool_service(orch_adapter_t *adapter,
                                  void *tool_svc_adapter)
{
    if (!adapter) return -1;

    adapter->tool_adapter = (tool_svc_adapter_t *)tool_svc_adapter;

    /* C-L06 P1.5.1: adapter 句柄暂存。Tool 能力经 engine.c 的
     * agentos_cognition_set_tool_adapter() 注入到认知引擎（IPC 路径，
     * P1.3.1 首选）。旧代码通过 tool_svc_adapter_get_service() 获取
     * tool_service_t 句柄并经 orchestrator_set_cognition_tool_service() 注入，
     * 但 get_service 始终返回 NULL（tool_service_t 由 tool_d daemon 内部管理，
     * coreloopthree 不直接持有），该路径已移除。 */

    AGENTOS_LOG_INFO("C-L06: Tool service injected into orchestrator");
    return 0;
}

/* ==================== 流水线执行实现 ==================== */

int orch_adapter_execute_pipeline(orch_adapter_t *adapter,
                                  orch_pipeline_t *pipeline,
                                  const char *input,
                                  orch_result_t **out_results,
                                  size_t *out_count)
{
    if (!adapter || !pipeline || !out_results || !out_count) return -1;

    *out_results = NULL;
    *out_count = 0;

    uint64_t pipeline_start = get_current_time_ms();
    adapter->total_pipelines++;

    /* P1.5.1: 通过 CoreLoopThree 执行流水线 */
    /* 获取 CoreLoopThree 实例 */
    agentos_core_loop_t *loop = acquire_loop_instance(adapter);
    if (!loop) {
        AGENTOS_LOG_ERROR("C-L06: No available CoreLoopThree instance");
        adapter->total_errors++;
        return -1;
    }

    /* 注入 Tool 服务到 CoreLoopThree（LLM adapter 经 loop.c 的
     * set_llm_adapter 路径注入，此处无需重复）。Tool adapter 经
     * engine.c 的 set_tool_adapter 路径注入，此处亦无需重复。 */
    (void)adapter;

    /* P1.5.2: 执行流水线 — 通过编排器的 orchestrator_execute_pipeline */
    /* C-L06: 将 CoreLoopThree 实例注入到编排器，使 GENERATION 阶段使用三层循环 */
    orchestrator_set_core_loop(adapter->orch, loop);

    int ret = orchestrator_execute_pipeline(
        adapter->orch, pipeline, input, out_results, out_count);

    /* 执行完成后解绑 CoreLoopThree */
    orchestrator_set_core_loop(adapter->orch, NULL);

    /* 释放实例 */
    release_loop_instance(adapter, loop);

    if (ret != 0) {
        AGENTOS_LOG_ERROR("C-L06: Pipeline execution failed: ret=%d", ret);
        adapter->total_errors++;
        return ret;
    }

    /* 更新统计 */
    if (out_count) {
        adapter->total_steps += *out_count;
    }
    uint64_t pipeline_end = get_current_time_ms();
    adapter->total_latency_us += (pipeline_end - pipeline_start) * 1000;

    AGENTOS_LOG_INFO("C-L06: Pipeline executed (%zu results, %llums)",
                     out_count ? *out_count : 0,
                     (unsigned long long)(pipeline_end - pipeline_start));

    /* P1.5.3: 触发进度回调 */
    if (adapter->config.enable_progress_callback && adapter->progress_cb) {
        /* 通知每个步骤完成 */
        for (size_t i = 0; i < *out_count; i++) {
            orch_result_t *result = (*out_results) + i;
            orch_phase_t phase = (orch_phase_t)(i % ORCH_PHASE_MAX);
            adapter->progress_cb(phase, (uint32_t)i, (uint32_t)*out_count,
                                 result->status, adapter->progress_user_data);
        }
    }

    return 0;
}

int orch_adapter_execute_phase(orch_adapter_t *adapter,
                               orch_phase_t phase,
                               const char *input,
                               orch_result_t **out_result)
{
    if (!adapter || !input || !out_result) return -1;

    *out_result = NULL;

    /* 通过编排器执行单个阶段 */
    int ret = orchestrator_execute_phase(
        adapter->orch, phase, input, out_result);

    if (ret != 0) {
        AGENTOS_LOG_ERROR("C-L06: Phase execution failed: phase=%d ret=%d",
                          phase, ret);
        adapter->total_errors++;
        return ret;
    }

    adapter->total_steps++;

    AGENTOS_LOG_INFO("C-L06: Phase %d executed successfully", phase);
    return 0;
}

void orch_adapter_set_progress_callback(orch_adapter_t *adapter,
                                        orch_adapter_progress_cb_t callback,
                                        void *user_data)
{
    if (!adapter) return;

    adapter->progress_cb = callback;
    adapter->progress_user_data = user_data;

    /* P1.5.3: 同时设置编排器的进度回调 */
    /* orchestrator_set_progress_callback(adapter->orch, ...); */
}

/* ==================== 实例管理实现 ==================== */

uint32_t orch_adapter_get_instance_count(orch_adapter_t *adapter)
{
    return adapter ? adapter->instance_count : 0;
}

/* ==================== 状态查询实现 ==================== */

void orch_adapter_get_stats(orch_adapter_t *adapter,
                            uint64_t *out_total_pipelines,
                            uint64_t *out_total_steps,
                            uint64_t *out_total_errors,
                            uint64_t *out_avg_step_latency_ms)
{
    if (!adapter) {
        if (out_total_pipelines) *out_total_pipelines = 0;
        if (out_total_steps) *out_total_steps = 0;
        if (out_total_errors) *out_total_errors = 0;
        if (out_avg_step_latency_ms) *out_avg_step_latency_ms = 0;
        return;
    }

    if (out_total_pipelines) *out_total_pipelines = adapter->total_pipelines;
    if (out_total_steps) *out_total_steps = adapter->total_steps;
    if (out_total_errors) *out_total_errors = adapter->total_errors;
    if (out_avg_step_latency_ms) {
        if (adapter->total_steps > 0) {
            *out_avg_step_latency_ms = (uint64_t)(
                (double)adapter->total_latency_us /
                (double)adapter->total_steps / 1000.0);
        } else {
            *out_avg_step_latency_ms = 0;
        }
    }
}

bool orch_adapter_is_ready(orch_adapter_t *adapter)
{
    if (!adapter) return false;
    return adapter->orch != NULL;
}

/* ==================== C-L06: 统计摘要 ==================== */

void orch_adapter_dump_stats(orch_adapter_t *adapter)
{
    if (!adapter) {
        AGENTOS_LOG_WARN("C-L06: ORCH-ADAPTER-STATS unavailable");
        return;
    }

    uint64_t avg_step_latency_ms = 0;
    if (adapter->total_steps > 0) {
        avg_step_latency_ms = (uint64_t)(
            (double)adapter->total_latency_us /
            (double)adapter->total_steps / 1000.0);
    }

    AGENTOS_LOG_INFO("C-L06: ORCH-ADAPTER-STATS "
                     "pipelines=%llu steps=%llu errors=%llu "
                     "avg_step_latency=%llums "
                     "instances=%u/%u "
                     "checkpoint=%s llm=%s tool=%s",
                     (unsigned long long)adapter->total_pipelines,
                     (unsigned long long)adapter->total_steps,
                     (unsigned long long)adapter->total_errors,
                     (unsigned long long)avg_step_latency_ms,
                     adapter->instance_count, adapter->max_instances,
                     adapter->config.enable_checkpoint ? "on" : "off",
                     adapter->llm_adapter ? "yes" : "no",
                     adapter->tool_adapter ? "yes" : "no");
}
/**
 * @file orch_adapter.h
 * @brief C-L06: Orchestrator → CoreLoopThree 编排器适配器
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 将 Orchestrator 编排引擎与 CoreLoopThree 核心循环桥接，
 * 使编排器能够创建和管理 CoreLoopThree 实例来执行流水线步骤。
 *
 * 使用方式：
 * @code
 *   // 1. 创建编排器
 *   orchestrator_t *orch = orchestrator_create(&cfg);
 *
 *   // 2. 创建适配器
 *   orch_adapter_t *adapter = orch_adapter_create(orch, NULL);
 *
 *   // 3. 注入 LLM 和 Tool 服务
 *   orch_adapter_set_llm_service(adapter, llm_adapter);
 *   orch_adapter_set_tool_service(adapter, tool_adapter);
 *
 *   // 4. 执行流水线
 *   orch_adapter_execute_pipeline(adapter, pipeline, input, &results, &count);
 *
 *   // 5. 关闭
 *   orch_adapter_destroy(adapter);
 * @endcode
 *
 * @see orchestrator.h
 * @see loop.h
 * @see P1.5 C-L06 连接线
 */

#ifndef AGENTOS_CORELOOPTHREE_ORCH_ADAPTER_H
#define AGENTOS_CORELOOPTHREE_ORCH_ADAPTER_H

#include "orchestrator.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 适配器句柄 ==================== */

typedef struct orch_adapter_s orch_adapter_t;

/* ==================== 适配器配置 ==================== */

typedef struct {
    uint32_t max_parallel_instances;   /**< 最大并行 CoreLoopThree 实例数，默认 8 */
    uint32_t instance_timeout_ms;      /**< 单个实例超时（毫秒），0 使用默认 60000 */
    uint32_t pipeline_timeout_ms;      /**< 流水线超时（毫秒），0 使用默认 300000 */
    bool enable_progress_callback;     /**< 是否启用进度回调 */
    bool enable_checkpoint;            /**< 是否启用检查点 */
    const char *checkpoint_path;       /**< 检查点存储路径 */
} orch_adapter_config_t;

/* ==================== 流水线步骤上下文 ==================== */

typedef struct {
    orch_phase_t phase;               /**< 当前阶段 */
    const char *step_input;           /**< 步骤输入 */
    char *step_output;                /**< 步骤输出 */
    size_t step_output_len;           /**< 输出长度 */
    orch_task_status_t status;        /**< 步骤状态 */
    uint32_t duration_ms;             /**< 执行耗时 */
    int error_code;                   /**< 错误码 */
} orch_adapter_step_context_t;

/* ==================== 进度回调 ==================== */

/**
 * @brief 流水线步骤进度回调
 *
 * @param phase 当前阶段
 * @param step_index 步骤索引
 * @param total_steps 总步骤数
 * @param status 步骤状态
 * @param user_data 用户数据
 */
typedef void (*orch_adapter_progress_cb_t)(orch_phase_t phase,
                                           uint32_t step_index,
                                           uint32_t total_steps,
                                           orch_task_status_t status,
                                           void *user_data);

/* ==================== 生命周期 ==================== */

/**
 * @brief 创建编排器适配器
 *
 * 初始化编排器与 CoreLoopThree 的桥接层，
 * 创建 CoreLoopThree 实例池用于并行执行流水线步骤。
 *
 * @param orch 编排器句柄
 * @param config 适配器配置（NULL 使用默认）
 * @return 适配器句柄，失败返回 NULL
 *
 * @ownership orch: BORROW, return: OWNER
 */
orch_adapter_t *orch_adapter_create(orchestrator_t *orch,
                                    const orch_adapter_config_t *config);

/**
 * @brief 销毁编排器适配器
 *
 * 释放所有 CoreLoopThree 实例和资源。
 *
 * @param adapter 适配器句柄
 *
 * @ownership adapter: TRANSFER
 */
void orch_adapter_destroy(orch_adapter_t *adapter);

/* ==================== 服务注入 ==================== */

/**
 * @brief C-L06: 将 LLM 服务适配器注入到编排器下的 CoreLoopThree
 *
 * 编排器持有的 CoreLoopThree 实例需要通过此函数
 * 获取 LLM 服务句柄，用于认知循环的 Phase 2。
 *
 * @param adapter 适配器句柄
 * @param llm_svc_adapter LLM 服务适配器（来自 C-L02）
 * @return 0 成功，非0 失败
 *
 * @ownership llm_svc_adapter: BORROW
 */
int orch_adapter_set_llm_service(orch_adapter_t *adapter,
                                 void *llm_svc_adapter);

/**
 * @brief C-L06: 将 Tool 服务适配器注入到编排器下的 CoreLoopThree
 *
 * 编排器持有的 CoreLoopThree 实例需要通过此函数
 * 获取工具服务句柄，用于执行引擎的工具调用。
 *
 * @param adapter 适配器句柄
 * @param tool_svc_adapter 工具服务适配器（来自 C-L04）
 * @return 0 成功，非0 失败
 *
 * @ownership tool_svc_adapter: BORROW
 */
int orch_adapter_set_tool_service(orch_adapter_t *adapter,
                                  void *tool_svc_adapter);

/* ==================== 流水线执行 ==================== */

/**
 * @brief C-L06: 通过 CoreLoopThree 执行编排流水线
 *
 * 将 Orchestrator 定义的流水线映射到 CoreLoopThree 实例执行。
 * 每个流水线步骤创建一个 CoreLoopThree 实例，支持并行执行。
 * 步骤间通过上下文传递数据。
 *
 * 执行流程：
 *   1. Orchestrator 定义流水线步骤
 *   2. 为每个步骤创建 CoreLoopThree 实例
 *   3. 注入 LLM/Tool 服务
 *   4. 步骤 N 的输出 → 步骤 N+1 的输入
 *   5. 聚合所有步骤结果
 *
 * @param adapter 适配器句柄
 * @param pipeline 流水线定义
 * @param input 初始输入
 * @param out_results 输出结果数组（需调用者释放）
 * @param out_count 输出结果数量
 * @return 0 成功，非0 失败
 *
 * @ownership input: BORROW, out_results: OWNER, out_count: BORROW
 */
int orch_adapter_execute_pipeline(orch_adapter_t *adapter,
                                  orch_pipeline_t *pipeline,
                                  const char *input,
                                  orch_result_t **out_results,
                                  size_t *out_count);

/**
 * @brief C-L06: 执行单个编排阶段
 *
 * 对单个阶段使用 CoreLoopThree 实例执行。
 *
 * @param adapter 适配器句柄
 * @param phase 阶段类型
 * @param input 输入
 * @param out_result 输出结果（需调用者释放）
 * @return 0 成功，非0 失败
 *
 * @ownership input: BORROW, out_result: OWNER
 */
int orch_adapter_execute_phase(orch_adapter_t *adapter,
                               orch_phase_t phase,
                               const char *input,
                               orch_result_t **out_result);

/**
 * @brief C-L06: 设置进度回调
 *
 * 当 CoreLoopThree 实例完成一个 cycle 时触发回调，
 * 使编排器可以追踪流水线进度。
 *
 * @param adapter 适配器句柄
 * @param callback 进度回调
 * @param user_data 用户数据
 *
 * @ownership callback: BORROW, user_data: BORROW
 */
void orch_adapter_set_progress_callback(orch_adapter_t *adapter,
                                        orch_adapter_progress_cb_t callback,
                                        void *user_data);

/* ==================== 实例管理 ==================== */

/**
 * @brief 获取 CoreLoopThree 实例池大小
 *
 * @param adapter 适配器句柄
 * @return 实例数量
 */
uint32_t orch_adapter_get_instance_count(orch_adapter_t *adapter);

/**
 * @brief 获取适配器统计信息
 *
 * @param adapter 适配器句柄
 * @param out_total_pipelines 输出总流水线数
 * @param out_total_steps 输出总步骤数
 * @param out_total_errors 输出总错误数
 * @param out_avg_step_latency_ms 输出平均步骤延迟
 */
void orch_adapter_get_stats(orch_adapter_t *adapter,
                            uint64_t *out_total_pipelines,
                            uint64_t *out_total_steps,
                            uint64_t *out_total_errors,
                            uint64_t *out_avg_step_latency_ms);

/**
 * @brief 检查适配器是否就绪
 *
 * @param adapter 适配器句柄
 * @return true 就绪
 */
bool orch_adapter_is_ready(orch_adapter_t *adapter);

/**
 * @brief C-L06: 输出编排器适配器统计摘要（单行格式，适合周期性日志）
 *
 * @param adapter 适配器句柄
 */
void orch_adapter_dump_stats(orch_adapter_t *adapter);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_CORELOOPTHREE_ORCH_ADAPTER_H */
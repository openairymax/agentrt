/**
 * @file cognition.h
 * @brief 认知层公共接口定义
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_COGNITION_H
#define AGENTOS_COGNITION_H

// API 版本声明 (MAJOR.MINOR.PATCH)
#define COGNITION_API_VERSION_MAJOR 1
#define COGNITION_API_VERSION_MINOR 0
#define COGNITION_API_VERSION_PATCH 0

// ABI 兼容性声明
// 在相同 MAJOR 版本内保证 ABI 兼容
// 破坏性更改需递增 MAJOR 并发布迁移说明

#include "agentos.h"
#include "llm_service.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明 */
typedef struct agentos_cognition_engine agentos_cognition_engine_t;
typedef struct agentos_memory_engine agentos_memory_engine_t;
typedef struct agentos_memory_provider agentos_memory_provider_t;
typedef struct agentos_intent agentos_intent_t;
typedef struct agentos_task_plan agentos_task_plan_t;
typedef struct agentos_plan_strategy agentos_plan_strategy_t;
typedef struct agentos_coordinator_strategy agentos_coordinator_strategy_t;
typedef struct agentos_dispatching_strategy agentos_dispatching_strategy_t;
typedef struct agentos_intent_parser agentos_intent_parser_t;
typedef struct agentos_task_checkpoint agentos_task_checkpoint_t;
typedef struct llm_service llm_service_t;
typedef struct llm_svc_adapter_s llm_svc_adapter_t;
typedef struct tool_service tool_service_t;
typedef struct tool_svc_adapter_s tool_svc_adapter_t;

/**
 * @brief 反馈回调函数类型
 * @param level 反馈级别：0=实时，1=轮次内，2=跨轮次
 * @param module [in] 模块名称 (BORROW - caller must not free, valid for callback scope only).
 * @param event [in] 事件类型 (BORROW - caller must not free, valid for callback scope only).
 * @param data [in] 反馈数据（JSON格式） (BORROW - caller must not free, valid for callback scope only).
 * @param data_len 数据长度
 * @param user_data [in] 用户数据 (BORROW - caller must not free, valid for callback scope only).
 */
typedef void (*agentos_feedback_callback_t)(int level, const char *module, const char *event,
                                            const char *data, size_t data_len, void *user_data);

/**
 * @brief 认知引擎配置
 */
typedef struct agentos_cognition_config {
    uint32_t cognition_default_timeout_ms;         /**< 默认任务超时（毫秒） */
    uint32_t cognition_max_retries;                /**< 最大重试次数 */
    agentos_feedback_callback_t feedback_callback; /**< 反馈回调函数（可选） */
    void *feedback_user_data;                      /**< 反馈回调用户数据 */
} agentos_cognition_config_t;

/**
 * @brief 意图结构，表示解析后的用户输入
 */
typedef struct agentos_intent {
    char *intent_raw_text;  /**< 原始输入文本 */
    size_t intent_raw_len;  /**< 原始文本长度 */
    char *intent_goal;      /**< 提取的核心目标 */
    size_t intent_goal_len; /**< 目标长度 */
    uint32_t intent_flags;  /**< 标志位（如紧急、复杂等） */
    void *intent_context;   /**< 附加上下文（需由上层释放） */
} agentos_intent_t;

/**
 * @brief 任务计划节点（DAG中的一个节点）
 */
typedef struct agentos_task_node {
    char *task_node_id;             /**< 任务ID */
    size_t task_node_id_len;        /**< ID长度 */
    char *task_node_handler_name;   /**< 处理器名称（工具名/Agent名） */
    char *task_node_goal;           /**< 任务目标描述 */
    char *task_node_agent_role;     /**< 需要的Agent角色 */
    size_t task_node_role_len;      /**< 角色长度 */
    char **task_node_depends_on;    /**< 依赖的任务ID数组 */
    size_t task_node_depends_count; /**< 依赖数量 */
    uint32_t task_node_timeout_ms;  /**< 超时时间 */
    uint8_t task_node_priority;     /**< 优先级 */
    void *task_node_input;          /**< 输入数据（由策略管理） */
    void *task_node_output;         /**< 输出数据 */
} agentos_task_node_t;

/**
 * @brief 任务计划（DAG）
 */
typedef struct agentos_task_plan {
    char *task_plan_id;                    /**< 计划ID */
    size_t task_plan_id_len;               /**< ID长度 */
    agentos_task_node_t **task_plan_nodes; /**< 节点数组 */
    size_t task_plan_node_count;           /**< 节点数量 */
    char **task_plan_entry_points;         /**< 入口点节点ID数组 */
    size_t task_plan_entry_count;          /**< 入口点数量 */
} agentos_task_plan_t;

/* ==================== 规划策略接口 ==================== */

/**
 * @brief 规划策略函数类型，根据意图生成任务计划
 * @param intent [in] 解析后的意图 (BORROW - caller retains ownership).
 * @param context [in] 上下文 (BORROW - caller retains ownership).
 * @param out_plan [out] 输出计划 (OWNER - caller must call agentos_task_plan_free).
 * @return agentos_error_t
 */
typedef agentos_error_t (*agentos_plan_func_t)(const agentos_intent_t *intent, void *context,
                                               agentos_task_plan_t **out_plan);

/**
 * @brief 规划策略释放函数
 * @param strategy [in] 策略对象 (TRANSFER - function takes ownership and frees).
 */
typedef void (*agentos_plan_destroy_t)(agentos_plan_strategy_t *strategy);

/**
 * @brief 规划策略对象
 */
struct agentos_plan_strategy {
    agentos_plan_func_t plan;       /**< 规划函数 */
    agentos_plan_destroy_t destroy; /**< 释放函数 */
    void *data;                     /**< 私有数据 */
};

/* ==================== 协同策略接口 ==================== */

/**
 * @brief 协同策略函数类型，协调多个模型输出
 * @param prompts [in] 多个模型的输入提示 (BORROW - caller retains ownership, valid for function scope only).
 * @param count 模型数量
 * @param context [in] 上下文 (BORROW - caller retains ownership).
 * @param out_result [out] 输出协调结果 (OWNER - caller must free).
 * @return agentos_error_t
 */
typedef agentos_error_t (*agentos_coordinate_func_t)(const char **prompts, size_t count,
                                                     void *context, char **out_result);

/**
 * @brief 协同策略释放函数
 * @param strategy [in] 策略对象 (TRANSFER - function takes ownership and frees).
 */
typedef void (*agentos_coordinator_destroy_t)(agentos_coordinator_strategy_t *strategy);

/**
 * @brief 协同策略对象
 */
struct agentos_coordinator_strategy {
    agentos_coordinate_func_t coordinate;
    agentos_coordinator_destroy_t destroy;
    void *data;
};

/* ==================== 调度策略接口 ==================== */

/**
 * @brief 调度策略函数类型，从候选Agent中选择最合适的一个
 * @param task [in] 待分配的任务节点 (BORROW - caller retains ownership).
 * @param candidates [in] 候选Agent信息数组 (BORROW - caller retains ownership, valid for function scope only).
 * @param count 候选数量
 * @param context [in] 上下文 (BORROW - caller retains ownership).
 * @param out_agent_id [out] 输出选中的Agent ID (OWNER - caller must free).
 * @return agentos_error_t
 */
typedef agentos_error_t (*agentos_dispatch_func_t)(const agentos_task_node_t *task,
                                                   const void **candidates, size_t count,
                                                   void *context, char **out_agent_id);

/**
 * @brief 调度策略释放函数
 * @param strategy [in] 策略对象 (TRANSFER - function takes ownership and frees).
 */
typedef void (*agentos_dispatch_destroy_t)(agentos_dispatching_strategy_t *strategy);

/**
 * @brief 调度策略对象
 */
struct agentos_dispatching_strategy {
    agentos_dispatch_func_t dispatch;
    agentos_dispatch_destroy_t destroy;
    void *data;
};

/* ==================== 认知引擎接口 ==================== */

/**
 * @brief 创建认知引擎（使用默认配置）
 *
 * @param plan_strategy [in] 规划策略（可选，若为NULL则使用默认策略） (TRANSFER - engine takes ownership if non-NULL, will call destroy on shutdown).
 * @param coord_strategy [in] 协同策略（可选） (TRANSFER - engine takes ownership if non-NULL, will call destroy on shutdown).
 * @param disp_strategy [in] 调度策略（可选） (TRANSFER - engine takes ownership if non-NULL, will call destroy on shutdown).
 * @param out_engine [out] 输出引擎句柄 (OWNER - caller must call agentos_cognition_destroy).
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 *
 * @ownership out_engine: OWNER
 * @threadsafe 否（内部未使用线程安全措施）
 * @reentrant 否
 *
 * @concurrency 并发合约：
 * - 该函数不是线程安全的，不应在多个线程同时调用
 * - 调用者应确保在单个线程中创建认知引擎
 * - 创建完成后，引擎实例可以被多个线程使用，但需要外部同步
 *
 * @see agentos_cognition_create_ex_take(), agentos_cognition_destroy()
 */
/* _take: caller transfers ownership */
AGENTOS_API agentos_error_t agentos_cognition_create_take(agentos_plan_strategy_t *plan_strategy,
                                                          agentos_coordinator_strategy_t *coord_strategy,
                                                          agentos_dispatching_strategy_t *disp_strategy,
                                                          agentos_cognition_engine_t **out_engine);

/**
 * @brief 创建认知引擎（带配置）
 *
 * @param manager [in] 配置（若为NULL使用默认） (BORROW - not stored, copied internally).
 * @param plan_strategy [in] 规划策略（可选） (TRANSFER - engine takes ownership if non-NULL, will call destroy on shutdown).
 * @param coord_strategy [in] 协同策略（可选） (TRANSFER - engine takes ownership if non-NULL, will call destroy on shutdown).
 * @param disp_strategy [in] 调度策略（可选） (TRANSFER - engine takes ownership if non-NULL, will call destroy on shutdown).
 * @param out_engine [out] 输出引擎句柄 (OWNER - caller must call agentos_cognition_destroy).
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 *
 * @ownership out_engine: OWNER
 * @threadsafe 否（内部未使用线程安全措施）
 * @reentrant 否
 *
 * @concurrency 并发合约：
 * - 该函数不是线程安全的，不应在多个线程同时调用
 * - 调用者应确保在单个线程中创建认知引擎
 * - 创建完成后，引擎实例可以被多个线程使用，但需要外部同步
 *
 * @see agentos_cognition_create_take(), agentos_cognition_destroy()
 */
/* _take: caller transfers ownership */
AGENTOS_API agentos_error_t agentos_cognition_create_ex_take(
    const agentos_cognition_config_t *manager, agentos_plan_strategy_t *plan_strategy,
    agentos_coordinator_strategy_t *coord_strategy, agentos_dispatching_strategy_t *disp_strategy,
    agentos_cognition_engine_t **out_engine);

/**
 * @brief 销毁认知引擎
 *
 * @param engine [in] 引擎句柄（非NULL） (TRANSFER - function takes ownership and frees).
 *
 * @ownership engine: TRANSFER
 * @threadsafe 否（内部未使用线程安全措施）
 * @reentrant 否
 *
 * @concurrency 并发合约：
 * - 该函数不是线程安全的，不应在多个线程同时调用
 * - 调用者应确保在所有使用引擎的线程都已完成操作后再调用此函数
 * - 销毁后，引擎实例不应再被任何线程使用
 *
 * @see agentos_cognition_create_take(), agentos_cognition_create_ex_take()
 */
AGENTOS_API void agentos_cognition_destroy(agentos_cognition_engine_t *engine);

/**
 * @brief 设置回退规划策略
 *
 * @param engine [in] 认知引擎（非NULL） (BORROW - caller retains ownership).
 * @param fallback [in] 回退策略（可为NULL） (BORROW - engine does not take ownership, caller manages lifecycle).
 *
 * @ownership fallback: BORROW
 * @threadsafe 否（内部未使用线程安全措施）
 * @reentrant 否
 */
AGENTOS_API void agentos_cognition_set_fallback_plan(agentos_cognition_engine_t *engine,
                                                     agentos_plan_strategy_t *fallback);

/**
 * @brief P1.14: 设置主规划策略
 *
 * 替换引擎当前的主规划策略。旧策略非NULL时将调用其 destroy 释放。
 *
 * @param engine [in] 认知引擎（非NULL） (BORROW - caller retains ownership).
 * @param strategy [in] 新规划策略（可为NULL） (TRANSFER - engine takes ownership if non-NULL, will call destroy on shutdown).
 *
 * @ownership strategy: TRANSFER (if non-NULL), BORROW (if NULL)
 * @threadsafe 是（内部使用 mutex 保护）
 * @reentrant 否
 */
AGENTOS_API void agentos_cognition_set_plan_strategy(agentos_cognition_engine_t *engine,
                                                      agentos_plan_strategy_t *strategy);

/**
 * @brief P1.14: 设置协同策略
 *
 * 替换引擎当前的协同策略。旧策略非NULL时将调用其 destroy 释放。
 *
 * @param engine [in] 认知引擎（非NULL） (BORROW - caller retains ownership).
 * @param strategy [in] 新协同策略（可为NULL） (TRANSFER - engine takes ownership if non-NULL, will call destroy on shutdown).
 *
 * @ownership strategy: TRANSFER (if non-NULL), BORROW (if NULL)
 * @threadsafe 是（内部使用 mutex 保护）
 * @reentrant 否
 */
AGENTOS_API void agentos_cognition_set_coord_strategy(agentos_cognition_engine_t *engine,
                                                       agentos_coordinator_strategy_t *strategy);

/**
 * @brief P1.14: 设置调度策略
 *
 * 替换引擎当前的调度策略。旧策略非NULL时将调用其 destroy 释放。
 *
 * @param engine [in] 认知引擎（非NULL） (BORROW - caller retains ownership).
 * @param strategy [in] 新调度策略（可为NULL） (TRANSFER - engine takes ownership if non-NULL, will call destroy on shutdown).
 *
 * @ownership strategy: TRANSFER (if non-NULL), BORROW (if NULL)
 * @threadsafe 是（内部使用 mutex 保护）
 * @reentrant 否
 */
AGENTOS_API void agentos_cognition_set_disp_strategy(agentos_cognition_engine_t *engine,
                                                      agentos_dispatching_strategy_t *strategy);

/**
 * @brief 处理用户输入，生成任务计划
 *
 * @param engine [in] 认知引擎（非NULL） (BORROW - caller retains ownership).
 * @param input [in] 原始输入字符串（非NULL） (BORROW - not stored, copied internally).
 * @param input_len [in] 输入长度
 * @param out_plan [out] 输出任务计划 (OWNER - caller must call agentos_task_plan_free).
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 *
 * @ownership out_plan: OWNER
 * @threadsafe 否（内部未使用线程安全措施）
 * @reentrant 否
 *
 * @concurrency 并发合约：
 * - 该函数不是线程安全的，不应在多个线程同时调用
 * - 调用者应确保在单个线程中处理输入，或使用外部同步机制
 * - 处理过程中，引擎实例不应被其他线程修改
 *
 * @see agentos_task_plan_free()
 */
AGENTOS_API agentos_error_t agentos_cognition_process(agentos_cognition_engine_t *engine,
                                                      const char *input, size_t input_len,
                                                      agentos_task_plan_t **out_plan);

/**
 * @brief 释放任务计划
 *
 * @param plan [in] 要释放的计划（可为NULL） (TRANSFER - function takes ownership and frees).
 *
 * @ownership plan: TRANSFER
 * @threadsafe 否（内部未使用线程安全措施）
 * @reentrant 否
 * @see agentos_cognition_process()
 */
AGENTOS_API void agentos_task_plan_free(agentos_task_plan_t *plan);

/**
 * @brief 设置认知引擎的全局上下文
 *
 * @param engine [in] 引擎句柄（非NULL） (BORROW - caller retains ownership).
 * @param context [in] 上下文指针（可为NULL） (TRANSFER - if destroy is provided, engine takes ownership and will call destroy on shutdown).
 * @param destroy [in] 上下文释放函数（可为NULL）
 *
 * @ownership context: TRANSFER (if destroy provided), BORROW (if destroy is NULL)
 * @threadsafe 否（内部未使用线程安全措施）
 * @reentrant 否
 */
/* _take: caller transfers ownership */
AGENTOS_API void agentos_cognition_set_context_take(agentos_cognition_engine_t *engine, void *context,
                                                    void (*destroy)(void *));

/**
 * @brief Set memory engine for cognition engine.
 * @param engine [in] Cognition engine handle (BORROW - caller retains ownership).
 * @param memory [in] Memory engine handle (BORROW - engine does not take ownership, caller manages lifecycle).
 *
 * @ownership memory: BORROW
 */
AGENTOS_API void agentos_cognition_set_memory(agentos_cognition_engine_t *engine,
                                              agentos_memory_engine_t *memory);

/**
 * @brief C-L12: Set memory provider from MemoryRovol bridge.
 *
 * Sets the memory provider directly from the bridge, enabling
 * provider switching (builtin ↔ MemoryRovol) at runtime.
 *
 * @param engine [in] Cognition engine handle (BORROW).
 * @param provider [in] Memory provider from memoryrovol_bridge_get_provider() (BORROW).
 *
 * @ownership provider: BORROW
 */
AGENTOS_API void agentos_cognition_set_memory_provider(agentos_cognition_engine_t *engine,
                                                       agentos_memory_provider_t *provider);

/**
 * @brief C-L02: Set LLM service for cognition engine.
 *
 * Inject llm_d daemon's LLM service into CoreLoopThree cognitive loop.
 * When set, Phase 2 (Streaming Critical Loop) will route generation
 * requests through this service instead of local-only processing.
 *
 * @param engine [in] Cognition engine handle (BORROW - caller retains ownership).
 * @param llm_svc [in] LLM service handle (BORROW - engine does not take ownership, caller manages lifecycle).
 *
 * @ownership llm_svc: BORROW
 */
AGENTOS_API void agentos_cognition_set_llm_service(agentos_cognition_engine_t *engine,
                                                    llm_service_t *llm_svc);

/**
 * @brief C-L02 P1.2.1: Set LLM IPC adapter for cognition engine (preferred path).
 *
 * Inject the llm_svc_adapter that communicates with llm_d via IPC Bus.
 * This is the preferred integration path for P1.2 — when set, the cognition
 * engine will route LLM requests through the IPC adapter instead of calling
 * llm_service_complete() directly. Falls back to direct llm_service_t if no
 * adapter is set.
 *
 * @param engine [in] Cognition engine handle (BORROW - caller retains ownership).
 * @param adapter [in] LLM service adapter handle (BORROW - engine does not take ownership, caller manages lifecycle).
 *
 * @ownership adapter: BORROW
 */
AGENTOS_API void agentos_cognition_set_llm_adapter(agentos_cognition_engine_t *engine,
                                                    llm_svc_adapter_t *adapter);

/**
 * @brief C-L02 P1.2.2: Enable async streaming LLM callback.
 *
 * When enabled, LLM responses are delivered via streaming callback
 * (llm_service_complete_stream) instead of blocking sync complete.
 * The callback receives token chunks in real-time; the cognition engine
 * assembles the full response before continuing the pipeline.
 *
 * @param engine [in] Cognition engine handle (BORROW).
 * @param enabled Non-zero to enable streaming.
 * @param callback Streaming chunk callback (BORROW).
 * @param user_data User data for callback (BORROW).
 */
AGENTOS_API void agentos_cognition_set_llm_streaming(agentos_cognition_engine_t *engine,
                                                     int enabled,
                                                     llm_stream_callback_t callback,
                                                     void *user_data);

/**
 * @brief C-L04: Set tool service for cognition engine.
 *
 * Inject tool_d daemon's tool service into CoreLoopThree cognitive loop.
 * When set, task execution can route tool invocations through this service.
 *
 * @param engine [in] Cognition engine handle (BORROW - caller retains ownership).
 * @param tool_svc [in] Tool service handle (BORROW - engine does not take ownership, caller manages lifecycle).
 *
 * @ownership tool_svc: BORROW
 */
AGENTOS_API void agentos_cognition_set_tool_service(agentos_cognition_engine_t *engine,
                                                     tool_service_t *tool_svc);

/**
 * @brief C-L04 P1.3.1: Set tool IPC adapter for cognition engine (preferred path).
 *
 * Inject the tool_svc_adapter that communicates with tool_d via IPC Bus.
 * This is the preferred integration path for P1.3 — when set, the cognition
 * engine will route tool execution requests through the IPC adapter instead
 * of calling tool_service_execute() directly. Falls back to direct
 * tool_service_t if no adapter is set.
 *
 * @param engine [in] Cognition engine handle (BORROW - caller retains ownership).
 * @param adapter [in] Tool service adapter handle (BORROW - engine does not take ownership, caller manages lifecycle).
 *
 * @ownership adapter: BORROW
 */
AGENTOS_API void agentos_cognition_set_tool_adapter(agentos_cognition_engine_t *engine,
                                                     tool_svc_adapter_t *adapter);

/**
 * @brief C-L04: Get tool service from cognition engine.
 *
 * Retrieve the tool service handle for dispatching tool tasks.
 *
 * @param engine [in] Cognition engine handle (BORROW - caller retains ownership).
 * @return Tool service handle (BORROW - belongs to engine, do not free), NULL if not set.
 *
 * @ownership return: BORROW
 */
AGENTOS_API tool_service_t *agentos_cognition_get_tool_service(
    agentos_cognition_engine_t *engine);

/**
 * @brief C-L07: Enable checkpoint auto-save in cognition processing.
 *
 * When enabled, the cognition engine will automatically save task plan
 * checkpoints between each processing phase (0-4), allowing fault
 * recovery and task resumption.
 *
 * @param engine [in] Cognition engine handle (BORROW - caller retains ownership).
 * @param enable 1 to enable auto-checkpoint, 0 to disable.
 * @param session_id [in] Session identifier for checkpoint grouping (BORROW - copied internally).
 *
 * @ownership session_id: BORROW
 */
AGENTOS_API void agentos_cognition_enable_checkpoint(agentos_cognition_engine_t *engine,
                                                      int enable, const char *session_id);

/**
 * @brief C-L07: Save a manual checkpoint of the current task plan state.
 *
 * Saves the current task plan state as a named checkpoint.
 * Called automatically between phases when auto-checkpoint is enabled.
 *
 * @param engine [in] Cognition engine handle (BORROW - caller retains ownership).
 * @param sequence_num Checkpoint sequence number.
 * @param phase_name [in] Descriptive phase name for the checkpoint (BORROW - copied internally).
 * @return 0 on success, non-zero on failure.
 *
 * @ownership phase_name: BORROW
 */
int agentos_cognition_save_checkpoint(agentos_cognition_engine_t *engine,
                                       uint64_t sequence_num, const char *phase_name);

/**
 * @brief 获取认知引擎的当前统计信息
 *
 * @param engine [in] 引擎句柄（非NULL） (BORROW - caller retains ownership).
 * @param out_stats [out] 输出统计字符串 (OWNER - caller must free).
 * @param out_len [out] 输出长度
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 *
 * @ownership out_stats: OWNER
 * @threadsafe 否（内部未使用线程安全措施）
 * @reentrant 否
 */
AGENTOS_API agentos_error_t agentos_cognition_stats(agentos_cognition_engine_t *engine,
                                                    char **out_stats, size_t *out_len);

/**
 * @brief 获取认知引擎健康状态
 *
 * @param engine [in] 认知引擎句柄（非NULL） (BORROW - caller retains ownership).
 * @param out_json [out] 输出 JSON 状态字符串 (OWNER - caller must free).
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 *
 * @ownership out_json: OWNER
 * @threadsafe 否（内部未使用线程安全措施）
 * @reentrant 否
 */
AGENTOS_API agentos_error_t agentos_cognition_health_check(agentos_cognition_engine_t *engine,
                                                           char **out_json);

/* ==================== 意图解析器接口 ==================== */

/**
 * @brief 创建意图解析器
 * @param out_parser [out] 输出解析器句柄 (OWNER - caller must call agentos_intent_parser_destroy).
 * @return agentos_error_t
 *
 * @ownership out_parser: OWNER
 */
AGENTOS_API agentos_error_t agentos_intent_parser_create(agentos_intent_parser_t **out_parser);

/**
 * @brief 销毁意图解析器
 * @param parser [in] 解析器句柄 (TRANSFER - function takes ownership and frees).
 *
 * @ownership parser: TRANSFER
 */
AGENTOS_API void agentos_intent_parser_destroy(agentos_intent_parser_t *parser);

/**
 * @brief 解析用户输入，提取意图
 * @param parser [in] 解析器 (BORROW - caller retains ownership).
 * @param input [in] 用户输入文本 (BORROW - not stored, copied internally).
 * @param input_len 输入长度
 * @param out_intent [out] 输出意图结构 (OWNER - caller must call agentos_intent_free).
 * @return agentos_error_t
 *
 * @ownership out_intent: OWNER
 */
AGENTOS_API agentos_error_t agentos_intent_parser_parse(agentos_intent_parser_t *parser,
                                                        const char *input, size_t input_len,
                                                        agentos_intent_t **out_intent);

/**
 * @brief 释放意图结构
 * @param intent [in] 意图结构 (TRANSFER - function takes ownership and frees).
 *
 * @ownership intent: TRANSFER
 */
AGENTOS_API void agentos_intent_free(agentos_intent_t *intent);

/**
 * @brief 添加自定义意图规则
 * @param parser [in] 解析器 (BORROW - caller retains ownership).
 * @param pattern [in] 模式字符串 (BORROW - not stored, copied internally).
 * @param intent_name [in] 意图名称 (BORROW - not stored, copied internally).
 * @param confidence 置信度
 * @param flags 标志位
 * @return agentos_error_t
 *
 * @ownership pattern: BORROW, intent_name: BORROW
 */
AGENTOS_API agentos_error_t agentos_intent_parser_add_rule(agentos_intent_parser_t *parser,
                                                           const char *pattern,
                                                           const char *intent_name,
                                                           float confidence, uint32_t flags);

/**
 * @brief 获取解析器统计信息
 * @param parser [in] 解析器 (BORROW - caller retains ownership).
 * @param out_stats [out] 输出统计JSON字符串 (OWNER - caller must free).
 * @return agentos_error_t
 *
 * @ownership out_stats: OWNER
 */
AGENTOS_API agentos_error_t agentos_intent_parser_stats(agentos_intent_parser_t *parser,
                                                        char **out_stats);

/**
 * @brief 重置解析器统计信息
 * @param parser [in] 解析器 (BORROW - caller retains ownership).
 *
 * @ownership parser: BORROW
 */
AGENTOS_API void agentos_intent_parser_reset_stats(agentos_intent_parser_t *parser);

/**
 * @brief 健康检查
 * @param parser [in] 解析器 (BORROW - caller retains ownership).
 * @param out_json [out] 输出健康状态JSON (OWNER - caller must free).
 * @return agentos_error_t
 *
 * @ownership out_json: OWNER
 */
AGENTOS_API agentos_error_t agentos_intent_parser_health_check(agentos_intent_parser_t *parser,
                                                               char **out_json);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_COGNITION_H */

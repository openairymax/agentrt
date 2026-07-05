/**
 * @file loop.c
 * @brief 三层核心运行时主循环实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "loop.h"

#include "agentrt.h"
#include "agentrt_dirent.h"
#include "arena.h"
#include "atomic_compat.h"
#include "check.h"
#include "checkpoint.h"
#include "cognition.h"
#include "execution.h"
#include "llm_svc_adapter.h"
#include "logging_compat.h"
#include "manager_adapter.h"
#include "memoryrovol_bridge.h"
#include "orch_adapter.h"
#include "tool_svc_adapter.h"
#include "memory.h"
#include "memory_common.h"
#include "memory_compat.h"
#include "platform.h"
#include "string_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "error.h"
#include "error_compat.h"

#define ATM_RET_ERR(c) \
    do { agentrt_error_push_ex((c), __FILE__, __LINE__, __func__, "%s", agentrt_error_str(c)); return (c); } while(0)


/**
 * @brief 核心循环结构体
 */
struct agentrt_core_loop {
    agentrt_cognition_engine_t *cognition;
    agentrt_execution_engine_t *execution;
    agentrt_memory_engine_t *memory;
    agentrt_loop_config_t manager;
    atomic_int running;
    atomic_int stop_requested;
    atomic_int task_pending;
    agentrt_mutex_t *lock;
    agentrt_cond_t *cond;

    /* P1.19.4: Arena 分配器用于请求处理路径中的短生命周期分配 */
    agentrt_arena_t *arena;

    /* C-L02: llm_d → CoreLoopThree — IPC adapter for LLM requests */
    llm_svc_adapter_t *llm_adapter;

    /* P2.7: agentrt_llm_service_t — 注入到 reactive/reflective planner，
     * LLM 可用时激活 LLM 规划路径，不可用时走规则降级。borrowed adapter 来自 llm_adapter。 */
    struct agentrt_llm_service *llm_svc;

    /* P0.20.7: reflective fallback 规划策略（BORROW 注入 cognition engine，由 loop 销毁）。
     * 主 plan_strat（reactive）失败时由 engine.c L973 调用 fallback_strat->plan()，
     * 实现反思式重规划。BORROW 语义：cognition engine destroy 时不销毁此对象，
     * 由 loop 在 cleanup/destroy 时显式调用 strat->destroy()。 */
    agentrt_plan_strategy_t *fallback_plan_strat;

    /* C-L04: tool_d → CoreLoopThree — IPC adapter for tool execution */
    tool_svc_adapter_t *tool_adapter;

    /* C-L12: CoreLoopThree → MemoryRovol — 内存提供商桥接 */
    memoryrovol_bridge_t *memory_bridge;

    /* C-L01: Manager → CoreLoopThree — 配置管理适配器 */
    agentrt_manager_adapter_t *manager_adapter;

    /* C-L06: Orchestrator → CoreLoopThree — 编排器适配器 */
    orch_adapter_t *orch_adapter;

    int checkpoint_initialized;
    uint64_t last_checkpoint_time_ms;
    uint64_t checkpoint_seq;
    uint64_t loop_turn_count;          /* P1.6: 循环轮次计数器 */
    uint64_t loop_checkpoint_last_turn; /* P1.6: 上次触发轮次检查点的轮次 */
    char current_task_id[128];
    char current_session_id[128];
    char **completed_node_ids;
    size_t completed_node_count;
    size_t completed_node_capacity;
    char *persistent_original_input;
    char **tool_call_history;           /* P1.6.2: 工具调用历史 */
    size_t tool_call_history_count;
    size_t tool_call_history_capacity;

    /* W18: taskflow_advanced DAG 工作流引擎（用于复杂工作流编排） */
    taskflow_engine_t *taskflow_engine;
};

/* 辅助函数声明 - 用于重构降低圈复杂度 */
static agentrt_error_t validate_loop_parameters(const agentrt_loop_config_t *manager,
                                                agentrt_core_loop_t **out_loop);
static agentrt_core_loop_t *allocate_loop_memory(void);
static agentrt_error_t initialize_loop_resources(agentrt_core_loop_t *loop,
                                                 const agentrt_loop_config_t *manager);
static agentrt_error_t create_loop_engines(agentrt_core_loop_t *loop);
static void cleanup_loop_resources(agentrt_core_loop_t *loop);
static void free_loop_memory(agentrt_core_loop_t *loop);

/* P1.14/P2.7: 前向声明 reactive 规划器工厂 + LLM service 生命周期 —
 * 不直接包含 planner/strategy.h 以避免 llm_client.h 中的
 * agentrt_llm_config_t 与 yaml_loader.h 类型冲突。
 * 使用 create_default 规避 config 类型冲突。
 * reactive 规划器接受 agentrt_llm_service_t* 参数（planner 侧类型 struct agentrt_llm_service），
 * 传 NULL 走 REACTIVE_RULES 关键词匹配降级路径；
 * P2.7 注入真实 LLM service 后，LLM 可用时走 LLM 规划路径。
 *
 * P0.20.7: 前向声明 reflective 规划器工厂 — 作为 fallback_plan 注入 cognition engine。
 * reflective 接受 (llm, memory_engine) 双参数：LLM 可用时走反思式重规划（5 阶段管线），
 * LLM 不可用时 llm_build_dynamic_plan 返回 ESERVICE（不崩溃，安全降级）。
 * memory_engine 用于历史经验检索（0.1.1 暂不深度集成，仅存储引用）。
 * 注意：fallback_plan 是 BORROW 语义（engine 不销毁），由 loop 管理生命周期。 */
struct agentrt_llm_service;
agentrt_plan_strategy_t *agentrt_plan_reactive_create(struct agentrt_llm_service *llm);
agentrt_plan_strategy_t *agentrt_plan_reflective_create(struct agentrt_llm_service *llm,
                                                        agentrt_memory_engine_t *memory_engine);
agentrt_error_t agentrt_llm_service_create_default(struct agentrt_llm_service **out_service);
void agentrt_llm_service_destroy(struct agentrt_llm_service *service);
agentrt_error_t agentrt_llm_service_set_adapter(struct agentrt_llm_service *service,
                                                void *adapter);

/* 提取的辅助函数 - 降低 agentrt_loop_submit 圈复杂度 */
static char *build_enhanced_input(const char *input, size_t input_len,
                                  agentrt_memory_record_t **records, size_t record_count,
                                  size_t max_memories);
static void free_memories(agentrt_memory_record_t **records, size_t record_count);
static void loop_checkpoint_auto_hook(const char *task_id, const char *state_json, void *user_data);
static void add_completed_node(agentrt_core_loop_t *loop, const char *node_id);
static void clear_completed_nodes(agentrt_core_loop_t *loop);
static agentrt_error_t save_incremental_checkpoint(agentrt_core_loop_t *loop, const char *task_id,
                                                   const char *session_id, const char *node_id);
static void add_tool_call_history(agentrt_core_loop_t *loop, const char *tool_name, const char *tool_input);
static void clear_tool_call_history(agentrt_core_loop_t *loop);
static char *build_rich_checkpoint_state(agentrt_core_loop_t *loop, const char *task_id,
                                         const char *extra_json);

/* 默认配置 */
static void init_default_config(agentrt_loop_config_t *manager)
{
    __builtin_memset(manager, 0, sizeof(agentrt_loop_config_t));
    manager->loop_config_cognition_threads = 4;
    manager->loop_config_execution_threads = 8;
    manager->loop_config_memory_threads = 2;
    manager->loop_config_max_queued_tasks = 1000;
    manager->loop_config_stats_interval_ms = 60000;
    manager->loop_config_memory_query_limit = 5;
    manager->loop_config_task_timeout_ms = 30000;
    manager->loop_config_memory_importance = 0.7f;
    manager->loop_config_checkpoint_enabled = 0;
    snprintf(manager->loop_config_checkpoint_path, sizeof(manager->loop_config_checkpoint_path),
             "./data/checkpoints");
    manager->loop_config_checkpoint_interval_ms = 30000;
    manager->loop_config_checkpoint_interval_turns = 0;
}

/* ==================== 辅助函数实现 - 用于降低圈复杂度 ==================== */

/**
 * @brief 验证循环创建参数
 * @param manager 配置参数指针（可为 NULL）
 * @param out_loop 输出循环指针
 * @return 错误码，成功返回 AGENTRT_SUCCESS
 */
static agentrt_error_t validate_loop_parameters(const agentrt_loop_config_t *manager,
                                                agentrt_core_loop_t **out_loop)
{
    CHECK_NULL(out_loop);

    if (manager) {
        if (manager->loop_config_cognition_threads > 1024 ||
            manager->loop_config_execution_threads > 1024 ||
            manager->loop_config_memory_threads > 1024) {
            AGENTRT_ERROR(AGENTRT_EINVAL, "failed to validate loop config: thread count exceeds max 1024");
        }

        if (manager->loop_config_max_queued_tasks == 0 ||
            manager->loop_config_max_queued_tasks > 100000) {
            AGENTRT_ERROR(AGENTRT_EINVAL, "failed to validate loop config: max_queued_tasks out of range");
        }

        if (manager->loop_config_stats_interval_ms > 3600000) {
            AGENTRT_ERROR(AGENTRT_EINVAL, "failed to validate loop config: stats_interval_ms exceeds 1 hour");
        }
    }

    return AGENTRT_SUCCESS;
}

static agentrt_core_loop_t *allocate_loop_memory(void)
{
    agentrt_core_loop_t *loop =
        (agentrt_core_loop_t *)AGENTRT_CALLOC(1, sizeof(agentrt_core_loop_t));
    if (loop) {
        __builtin_memset(loop, 0, sizeof(agentrt_core_loop_t));
    }
    return loop;
}

/**
 * @brief 初始化循环资源（互斥锁和条件变量）
 * @param loop 循环结构体指针
 * @param manager 配置参数指针（可为 NULL）
 * @return 错误码，成功返回 AGENTRT_SUCCESS
 */
static agentrt_error_t initialize_loop_resources(agentrt_core_loop_t *loop,
                                                 const agentrt_loop_config_t *manager)
{
    if (manager) {
        __builtin_memcpy(&loop->manager, manager, sizeof(agentrt_loop_config_t));
    } else {
        init_default_config(&loop->manager);
    }

    loop->lock = agentrt_mutex_create();
    if (!loop->lock)
        ATM_RET_ERR(AGENTRT_ENOMEM);

    loop->cond = agentrt_cond_create();
    if (!loop->cond) {
        agentrt_mutex_free(loop->lock);
        ATM_RET_ERR(AGENTRT_ENOMEM);
    }

    return AGENTRT_SUCCESS;
}

/**
 * @brief 创建循环引擎（认知、执行、记忆）
 * @param loop 循环结构体指针
 * @return 错误码，成功返回 AGENTRT_SUCCESS
 */
static agentrt_error_t create_loop_engines(agentrt_core_loop_t *loop)
{
    agentrt_error_t err;

    err = agentrt_cognition_create_ex_take(NULL, loop->manager.loop_config_plan_strategy,
                                      loop->manager.loop_config_coord_strategy,
                                      loop->manager.loop_config_disp_strategy, &loop->cognition);

    if (err != AGENTRT_SUCCESS)
        return err;

    /* C-L02: Create and wire LLM IPC adapter (moved before planner creation for P2.7) */
    {
        llm_svc_adapter_config_t adapter_cfg;
        __builtin_memset(&adapter_cfg, 0, sizeof(adapter_cfg));
        adapter_cfg.llm_d_service_name = "llm_d";
        adapter_cfg.channel_name = "coreloopthree-llm";
        adapter_cfg.request_timeout_ms = loop->manager.loop_config_task_timeout_ms > 0
                                             ? loop->manager.loop_config_task_timeout_ms
                                             : 30000;
        adapter_cfg.enable_streaming = true;

        loop->llm_adapter = llm_svc_adapter_create(&adapter_cfg);
        if (loop->llm_adapter) {
            agentrt_cognition_set_llm_adapter(loop->cognition, loop->llm_adapter);
            AGENTRT_LOG_INFO("C-L02: LLM IPC adapter wired to cognition engine");
        } else {
            AGENTRT_LOG_WARN("C-L02: Failed to create LLM IPC adapter, "
                             "cognition will use direct LLM service if available");
        }
    }

    /* P2.7: 创建 agentrt_llm_service_t 并注入 IPC adapter，激活 LLM 调用路径。
     * llm_svc 传给 reactive/reflective planner：LLM 可用时走 LLM 规划路径，
     * 不可用时（adapter 未连接 / llm_d 未运行）走 REACTIVE_RULES 关键词匹配降级路径。
     * llm_svc 所有权由 loop 管理，在 cleanup 时 destroy。 */
    if (loop->llm_adapter) {
        agentrt_error_t svc_err = agentrt_llm_service_create_default(&loop->llm_svc);
        if (svc_err == AGENTRT_SUCCESS && loop->llm_svc) {
            agentrt_llm_service_set_adapter(loop->llm_svc, (void *)loop->llm_adapter);
            AGENTRT_LOG_INFO("P2.7: LLM service created and adapter injected "
                             "(svc=%p)", (void *)loop->llm_svc);
        } else {
            AGENTRT_LOG_WARN("P2.7: Failed to create LLM service (err=%d), "
                             "planner will use rule-based fallback", (int)svc_err);
            loop->llm_svc = NULL;
        }
    }

    /* P1.14/P2.7: 注入 plan_strategy — reactive 规划器。
     * P2.7: 传入 llm_svc（非 NULL 时激活 LLM 规划路径），NULL 时走规则降级。
     * 注意：set_plan_strategy 是 TRANSFER 语义，引擎在 destroy 时会调用 strategy->destroy。 */
    {
        agentrt_plan_strategy_t *plan_strat = agentrt_plan_reactive_create(loop->llm_svc);
        if (plan_strat) {
            agentrt_cognition_set_plan_strategy(loop->cognition, plan_strat);
            AGENTRT_LOG_INFO("P1.14/P2.7: reactive plan_strategy injected "
                             "(llm_svc=%p available=%d)",
                             (void *)loop->llm_svc,
                             loop->llm_svc ? 1 : 0);
        } else {
            AGENTRT_LOG_ERROR("P1.14: Failed to create reactive plan_strategy, "
                              "cognition pipeline will fall back to no planning");
        }
    }

    err = agentrt_execution_create(loop->manager.loop_config_execution_threads > 0
                                       ? loop->manager.loop_config_execution_threads
                                       : 8,
                                   &loop->execution);

    if (err != AGENTRT_SUCCESS)
        return err;

    err = agentrt_memory_create(NULL, &loop->memory);
    if (err != AGENTRT_SUCCESS)
        return err;

    agentrt_cognition_set_memory(loop->cognition, loop->memory);

    /* P0.20.7: 注入 reflective 作为 fallback 规划策略（BORROW 语义）。
     * 主 plan_strat（reactive）在 engine.c L966 规划失败时，engine.c L973 调用
     * fallback_strat->plan() 进行反思式重规划（5 阶段管线：分解→LLM 生成→审计→
     * 对齐→fallback_plan 构建）。LLM 不可用时 reflective 返回 ESERVICE（安全降级，
     * 不崩溃），此时 fallback 路径无效果但不会引入故障。
     * 注意：set_fallback_plan 是 BORROW 语义，cognition engine destroy 时不销毁
     * fallback 对象，由 loop 在 cleanup/destroy 时显式销毁。 */
    loop->fallback_plan_strat = agentrt_plan_reflective_create(loop->llm_svc, loop->memory);
    if (loop->fallback_plan_strat) {
        agentrt_cognition_set_fallback_plan(loop->cognition, loop->fallback_plan_strat);
        AGENTRT_LOG_INFO("P0.20.7: reflective fallback_plan injected into cognition "
                         "(llm_svc=%p available=%d, memory=%p)",
                         (void *)loop->llm_svc,
                         loop->llm_svc ? 1 : 0,
                         (void *)loop->memory);
    } else {
        AGENTRT_LOG_WARN("P0.20.7: reflective fallback_plan creation failed (non-fatal, "
                         "cognition will fail-fast on primary planning errors)");
    }

    /* C-L04: Create and wire tool IPC adapter */
    {
        tool_svc_adapter_config_t tool_cfg;
        __builtin_memset(&tool_cfg, 0, sizeof(tool_cfg));
        tool_cfg.tool_d_service_name = "tool_d";
        tool_cfg.channel_name = "coreloopthree-tool";
        tool_cfg.request_timeout_ms = loop->manager.loop_config_task_timeout_ms > 0
                                          ? loop->manager.loop_config_task_timeout_ms
                                          : 60000;
        tool_cfg.enable_approval = (loop->manager.loop_config_checkpoint_enabled != 0);

        loop->tool_adapter = tool_svc_adapter_create(&tool_cfg);
        if (loop->tool_adapter) {
            agentrt_cognition_set_tool_adapter(loop->cognition, loop->tool_adapter);
            AGENTRT_LOG_INFO("C-L04: Tool IPC adapter wired to cognition engine"
                             " (approval=%s)",
                             tool_cfg.enable_approval ? "on" : "off");
        } else {
            AGENTRT_LOG_WARN("C-L04: Failed to create tool IPC adapter, "
                             "cognition will use direct tool service if available");
        }
    }

    /* C-L12: Create and wire MemoryRovol bridge */
    {
        memoryrovol_bridge_config_t bridge_cfg;
        __builtin_memset(&bridge_cfg, 0, sizeof(bridge_cfg));
        bridge_cfg.provider_name = "MemoryRovol";
        bridge_cfg.provider_version = "v0.1.1";
        bridge_cfg.enable_l1_raw = true;
        bridge_cfg.enable_l2_feature = true;
        bridge_cfg.enable_l3_structure = true;
        bridge_cfg.enable_l4_pattern = true;
        bridge_cfg.enable_forgetting = true;
        bridge_cfg.enable_faiss = true;
        bridge_cfg.enable_async_ops = true;
        bridge_cfg.enable_llm_integration = true;
        bridge_cfg.query_default_limit = 10;
        bridge_cfg.sync_interval_ms = 5000;

        loop->memory_bridge = memoryrovol_bridge_create(&bridge_cfg);
        if (loop->memory_bridge) {
            agentrt_memory_provider_t *provider =
                memoryrovol_bridge_get_provider(loop->memory_bridge);
            if (provider) {
                agentrt_cognition_set_memory_provider(loop->cognition, provider);

                /* P3.11-C9: 将 bridge provider 注入到 memory engine。
                 * 此前 memory engine 在 create 时通过 agentrt_memory_provider_get_active()
                 * 获取 builtin provider，bridge 的 L2 向量/L3 关系检索能力从未被
                 * thinking_chain prepopulate（走 memory engine 路径）使用。
                 * 注入后，memory engine 与 cognition engine 共用同一 bridge provider，
                 * 消除双 memory 接口的数据流断裂。provider 为 borrowed 语义，
                 * memory engine destroy 时不销毁（由 bridge 管理生命周期）。 */
                if (loop->memory) {
                    agentrt_error_t sp_err = agentrt_memory_set_provider(loop->memory, provider);
                    if (sp_err == AGENTRT_SUCCESS) {
                        AGENTRT_LOG_INFO("P3.11-C9: bridge provider injected into "
                                         "memory engine (unified memory path)");
                    } else {
                        AGENTRT_LOG_WARN("P3.11-C9: Failed to inject bridge provider "
                                         "into memory engine (err=%d), thinking_chain "
                                         "prepopulate will use builtin provider",
                                         (int)sp_err);
                    }
                }

                AGENTRT_LOG_INFO("C-L12: MemoryRovol bridge wired to cognition engine "
                                 "(provider=%s v%s, L1=%d L2=%d L3=%d L4=%d)",
                                 provider->name ? provider->name : "?",
                                 provider->version ? provider->version : "?",
                                 provider->capabilities.l1_raw,
                                 provider->capabilities.l2_feature,
                                 provider->capabilities.l3_structure,
                                 provider->capabilities.l4_pattern);
            } else {
                AGENTRT_LOG_WARN("C-L12: Bridge created but provider unavailable");
            }
        } else {
            AGENTRT_LOG_WARN("C-L12: Failed to create MemoryRovol bridge, "
                             "cognition will use builtin memory provider if available");
            loop->memory_bridge = NULL;
        }
    }

    return AGENTRT_SUCCESS;
}

/**
 * @brief 释放循环结构体内存（支持内存池和传统分配）
 * @param loop 循环结构体指针
 */
static void free_loop_memory(agentrt_core_loop_t *loop)
{
    if (loop == NULL) {
        return;
    }

    AGENTRT_FREE(loop);
}

/**
 * @brief 清理循环资源（反向释放所有资源）
 * @param loop 循环结构体指针
 */
static void cleanup_loop_resources(agentrt_core_loop_t *loop)
{
    if (!loop)
        return;

    /* P0.20.7: 先销毁 fallback_plan_strat（BORROW 语义，engine 不销毁）。
     * 必须在 cognition destroy 之前销毁，避免 cognition destroy 过程中
     * 通过 fallback_plan_strat 指针访问已释放资源（防御性编程）。 */
    if (loop->fallback_plan_strat) {
        if (loop->fallback_plan_strat->destroy) {
            loop->fallback_plan_strat->destroy(loop->fallback_plan_strat);
        } else {
            AGENTRT_FREE(loop->fallback_plan_strat);
        }
        loop->fallback_plan_strat = NULL;
    }

    if (loop->memory) {
        agentrt_memory_destroy(loop->memory);
        loop->memory = NULL;
    }

    if (loop->execution) {
        agentrt_execution_destroy(loop->execution);
        loop->execution = NULL;
    }

    if (loop->cognition) {
        agentrt_cognition_destroy(loop->cognition);
        loop->cognition = NULL;
    }

    /* C-L02: Clean up LLM IPC adapter */
    if (loop->llm_adapter) {
        llm_svc_adapter_destroy(loop->llm_adapter);
        loop->llm_adapter = NULL;
    }

    /* P2.7: Clean up LLM service (adapter is borrowed, not destroyed here) */
    if (loop->llm_svc) {
        agentrt_llm_service_destroy(loop->llm_svc);
        loop->llm_svc = NULL;
    }

    /* C-L04: Clean up tool IPC adapter */
    if (loop->tool_adapter) {
        tool_svc_adapter_destroy(loop->tool_adapter);
        loop->tool_adapter = NULL;
    }

    /* C-L12: Clean up MemoryRovol bridge */
    if (loop->memory_bridge) {
        memoryrovol_bridge_destroy(loop->memory_bridge);
        loop->memory_bridge = NULL;
    }

    if (loop->cond) {
        agentrt_cond_free(loop->cond);
        loop->cond = NULL;
    }

    if (loop->lock) {
        agentrt_mutex_free(loop->lock);
        loop->lock = NULL;
    }

    free_loop_memory(loop);
}

/**
 * @brief 释放记忆结果数组
 * @param memories 记忆数组指针
 * @param record_count 记录数量
 */
__attribute__((unused))
static void free_memories(agentrt_memory_record_t **records, size_t record_count)
{
    if (!records)
        return;
    AGENTRT_FREE(records);
}

/**
 * @brief 构建增强输入（带记忆上下文）
 * @param input 原始输入
 * @param input_len 原始输入长度
 * @param memories 记忆数组
 * @param memory_count 记忆数量
 * @return 增强后的输入字符串，需调用者释放；失败返回NULL
 */
static char *build_enhanced_input(const char *input, size_t input_len,
                                  agentrt_memory_record_t **records, size_t record_count,
                                  size_t max_memories)
{
    if (!input || record_count == 0 || !records) return NULL;

    size_t total_len = input_len + 1024;
    for (size_t i = 0; i < record_count; i++) {
        if (records[i] && records[i]->memory_record_data) {
            total_len += records[i]->memory_record_data_len + 64;
        }
    }

    /* P1.19.4: 使用 Arena 分配短生命周期内存（如果可用），否则回退到 malloc */
    char *enhanced_input = (char *)AGENTRT_ARENA_ALLOC(total_len);
    if (!enhanced_input) return NULL;

    size_t pos = 0;
    pos += snprintf(enhanced_input + pos, total_len - pos, "[上下文增强]\n相关记忆数量：%zu\n\n",
                    record_count);

    for (size_t i = 0; i < record_count && i < max_memories; i++) {
        if (records[i] && records[i]->memory_record_data) {
            pos += snprintf(enhanced_input + pos, total_len - pos, "记忆 %zu: %.*s\n", i + 1,
                            (int)records[i]->memory_record_data_len,
                            (const char *)records[i]->memory_record_data);
        }
    }

    pos += snprintf(enhanced_input + pos, total_len - pos, "\n[用户输入]\n%.*s", (int)input_len,
                    input);

    return enhanced_input;
}

/* ==================== 公共 API 函数实现 ==================== */

AGENTRT_API agentrt_error_t agentrt_loop_create(const agentrt_loop_config_t *manager,
                                                agentrt_core_loop_t **out_loop)
{
    agentrt_error_t err;
    agentrt_core_loop_t *loop = NULL;

    AGENTRT_LOG_INFO("CoreLoopThree: agentrt_loop_create START");

    err = validate_loop_parameters(manager, out_loop);
    if (err != AGENTRT_SUCCESS)
        return err;

    loop = allocate_loop_memory();
    if (!loop)
        ATM_RET_ERR(AGENTRT_ENOMEM);

    err = initialize_loop_resources(loop, manager);
    if (err != AGENTRT_SUCCESS) {
        cleanup_loop_resources(loop);
        return err;
    }

    err = create_loop_engines(loop);
    if (err != AGENTRT_SUCCESS) {
        cleanup_loop_resources(loop);
        return err;
    }

    loop->running = 0;
    loop->stop_requested = 0;
    loop->checkpoint_initialized = 0;
    loop->last_checkpoint_time_ms = 0;
    loop->checkpoint_seq = 0;
    loop->loop_turn_count = 0;
    loop->loop_checkpoint_last_turn = 0;
    __builtin_memset(loop->current_task_id, 0, sizeof(loop->current_task_id));
    __builtin_memset(loop->current_session_id, 0, sizeof(loop->current_session_id));
    loop->completed_node_ids = NULL;
    loop->completed_node_count = 0;
    loop->completed_node_capacity = 0;
    loop->persistent_original_input = NULL;
    loop->tool_call_history = NULL;
    loop->tool_call_history_count = 0;
    loop->tool_call_history_capacity = 0;

    /* C-L01 + C-L06: 外部适配器初始化（通过 setter 函数注入） */
    loop->manager_adapter = NULL;
    loop->orch_adapter = NULL;

    /* P1.19.4: 创建 Arena 分配器（64KB chunk，无限制）用于请求处理短生命周期分配 */
    loop->arena = arena_create(ARENA_DEFAULT_CHUNK_SIZE, 0);
    if (!loop->arena) {
        AGENTRT_LOG_WARN("CoreLoopThree: Arena creation failed, falling back to malloc");
    } else {
        AGENTRT_LOG_INFO("CoreLoopThree: Arena created (chunk_size=%zu) for request-level allocations",
                         (size_t)ARENA_DEFAULT_CHUNK_SIZE);
    }

    /* W18: 创建 taskflow_advanced DAG 工作流引擎，用于复杂工作流编排
     * （条件分支/并行汇聚/循环迭代等 DAG 模式） */
    loop->taskflow_engine = taskflow_engine_create();
    if (!loop->taskflow_engine) {
        AGENTRT_LOG_ERROR("CoreLoopThree: taskflow_engine_create failed");
        cleanup_loop_resources(loop);
        return AGENTRT_ENOMEM;
    }
    AGENTRT_LOG_INFO("CoreLoopThree: taskflow_advanced DAG engine initialized");

    if (loop->manager.loop_config_checkpoint_enabled) {
        const char *cp_path = loop->manager.loop_config_checkpoint_path;
        if (cp_path[0] == '\0') {
            cp_path = "./data/checkpoints";
        }
        agentrt_error_t cp_err = agentrt_checkpoint_init(cp_path);
        if (cp_err == AGENTRT_SUCCESS) {
            loop->checkpoint_initialized = 1;
            agentrt_checkpoint_set_auto_hook(loop_checkpoint_auto_hook, loop,
                                             loop->manager.loop_config_checkpoint_interval_ms);
            AGENTRT_LOG_INFO("Checkpoint subsystem initialized: %s", cp_path);
        } else {
            AGENTRT_LOG_WARN("Checkpoint init failed (err=%d), persistence disabled", cp_err);
        }
    }

    *out_loop = loop;
    return AGENTRT_SUCCESS;
}

AGENTRT_API void agentrt_loop_destroy(agentrt_core_loop_t *loop)
{
    if (!loop)
        return;

    if (loop->running) {
        agentrt_loop_stop(loop);
    }

    if (loop->checkpoint_initialized) {
        agentrt_checkpoint_cleanup(86400, 100);
        agentrt_checkpoint_shutdown();
        loop->checkpoint_initialized = 0;
    }

    clear_completed_nodes(loop);

    if (loop->persistent_original_input) {
        AGENTRT_FREE(loop->persistent_original_input);
        loop->persistent_original_input = NULL;
    }

    /* P1.6.2: 清理工具调用历史 */
    if (loop->tool_call_history) {
        for (size_t i = 0; i < loop->tool_call_history_count; i++) {
            AGENTRT_FREE(loop->tool_call_history[i]);
        }
        AGENTRT_FREE(loop->tool_call_history);
        loop->tool_call_history = NULL;
        loop->tool_call_history_count = 0;
        loop->tool_call_history_capacity = 0;
    }

    /* P1.19.4: 销毁 Arena 分配器 */
    if (loop->arena) {
        arena_stats_t astats;
        arena_get_stats(loop->arena, &astats);
        AGENTRT_LOG_INFO("CoreLoopThree: Arena destroy (allocs=%" PRIu64 ", chunks=%zu, total=%zu, reset=%" PRIu64 ")",
                         astats.alloc_count, astats.chunk_count, astats.total_chunk_bytes, astats.reset_count);
        arena_destroy(loop->arena);
        loop->arena = NULL;
    }

    /* C-L02: 销毁 LLM IPC 适配器 */
    if (loop->llm_adapter) {
        llm_svc_adapter_destroy(loop->llm_adapter);
        loop->llm_adapter = NULL;
    }

    /* P2.7: 销毁 LLM service（adapter 为 borrowed，不在此销毁） */
    if (loop->llm_svc) {
        agentrt_llm_service_destroy(loop->llm_svc);
        loop->llm_svc = NULL;
    }

    /* C-L04: 销毁工具 IPC 适配器 */
    if (loop->tool_adapter) {
        tool_svc_adapter_destroy(loop->tool_adapter);
        loop->tool_adapter = NULL;
    }

    /* P3.11-C9: memory engine 必须在 bridge 之前 destroy。
     * memory engine 的 provider 是 borrowed（指向 bridge 的 active provider），
     * 若 bridge 先 destroy 会释放 provider，memory engine destroy 时访问 provider->name
     * 导致 use-after-free。交换顺序：memory engine 先 destroy（不销毁 borrowed provider），
     * bridge 后 destroy（销毁 provider）。 */
    if (loop->memory) {
        agentrt_memory_destroy(loop->memory);
        loop->memory = NULL;
    }

    /* P0.20.7: 销毁 fallback_plan_strat（BORROW 语义，engine 不销毁）。
     * 必须在 cognition destroy 之前销毁，避免 cognition destroy 过程中
     * 通过 fallback_plan_strat 指针访问已释放资源（防御性编程）。
     * 注意：此处 memory 已 destroy，但 fallback_plan_strat 内部 ctx->memory_engine
     * 仅存储引用（reflective_plan_cleanup 不访问 memory_engine），因此安全。 */
    if (loop->fallback_plan_strat) {
        if (loop->fallback_plan_strat->destroy) {
            loop->fallback_plan_strat->destroy(loop->fallback_plan_strat);
        } else {
            AGENTRT_FREE(loop->fallback_plan_strat);
        }
        loop->fallback_plan_strat = NULL;
    }

    /* C-L12: 销毁 MemoryRovol 桥接器 */
    if (loop->memory_bridge) {
        memoryrovol_bridge_destroy(loop->memory_bridge);
        loop->memory_bridge = NULL;
    }
    if (loop->execution) {
        agentrt_execution_destroy(loop->execution);
        loop->execution = NULL;
    }
    if (loop->cognition) {
        agentrt_cognition_destroy(loop->cognition);
        loop->cognition = NULL;
    }
    /* W18: 销毁 taskflow_advanced DAG 工作流引擎 */
    if (loop->taskflow_engine) {
        taskflow_engine_destroy(loop->taskflow_engine);
        loop->taskflow_engine = NULL;
    }
    if (loop->cond) {
        agentrt_cond_free(loop->cond);
        loop->cond = NULL;
    }
    if (loop->lock) {
        agentrt_mutex_free(loop->lock);
        loop->lock = NULL;
    }

    free_loop_memory(loop);
}

AGENTRT_API agentrt_error_t agentrt_loop_run(agentrt_core_loop_t *loop)
{
    CHECK_NULL(loop);

    agentrt_mutex_lock(loop->lock);
    loop->running = 1;
    loop->stop_requested = 0;
    agentrt_mutex_unlock(loop->lock);

    AGENTRT_LOG_INFO("CoreLoopThree: agentrt_loop_run STARTED");

    uint64_t last_auto_checkpoint_ms = 0;
    uint32_t checkpoint_interval = loop->manager.loop_config_checkpoint_interval_ms;
    if (checkpoint_interval == 0)
        checkpoint_interval = 30000;

    uint64_t last_stats_log_ms = 0;
    uint64_t last_bridge_stats_ms = 0; /* C-L12: bridge stats timer */
    uint64_t last_manager_stats_ms = 0; /* C-L01: manager adapter stats timer */
    uint64_t last_orch_stats_ms = 0;    /* C-L06: orchestrator adapter stats timer */

    while (1) {
        agentrt_mutex_lock(loop->lock);
        while (!loop->stop_requested && !loop->task_pending) {
            agentrt_cond_timedwait(loop->cond, loop->lock, 50);
        }
        if (loop->stop_requested) {
            loop->running = 0;
            loop->task_pending = 0;
            agentrt_cond_broadcast(loop->cond);
            agentrt_mutex_unlock(loop->lock);
            AGENTRT_LOG_INFO("CoreLoopThree: agentrt_loop_run STOPPED (total_turns=%" PRIu64 ")",
                             loop->loop_turn_count);
            break;
        }
        if (loop->task_pending) {
            loop->task_pending = 0;
            AGENTRT_LOG_DEBUG("CoreLoopThree: task pending flag consumed (turn=%" PRIu64 ")",
                              loop->loop_turn_count);
        }
        agentrt_mutex_unlock(loop->lock);

        /* P1.6.1: 时间基检查点触发 */
        if (loop->checkpoint_initialized && loop->current_task_id[0] != '\0' &&
            checkpoint_interval > 0) {
            uint64_t now_ms = agentrt_time_ms();
            if (last_auto_checkpoint_ms == 0 ||
                (now_ms - last_auto_checkpoint_ms) >= checkpoint_interval) {
                agentrt_error_t cp_err = agentrt_checkpoint_trigger_auto(loop->current_task_id);
                if (cp_err == AGENTRT_SUCCESS) {
                    last_auto_checkpoint_ms = now_ms;
                    AGENTRT_LOG_DEBUG("CoreLoopThree: time-based auto-checkpoint triggered "
                                      "(interval=%ums, turn=%" PRIu64 ")",
                                      checkpoint_interval, loop->loop_turn_count);
                }
            }
        }

        /* P1.6.1: 轮次基检查点触发 */
        if (loop->checkpoint_initialized &&
            loop->manager.loop_config_checkpoint_interval_turns > 0 &&
            loop->current_task_id[0] != '\0') {
            uint32_t interval_turns = loop->manager.loop_config_checkpoint_interval_turns;
            if (loop->loop_turn_count - loop->loop_checkpoint_last_turn >= interval_turns) {
                agentrt_error_t cp_err = agentrt_checkpoint_trigger_auto(loop->current_task_id);
                if (cp_err == AGENTRT_SUCCESS) {
                    loop->loop_checkpoint_last_turn = loop->loop_turn_count;
                    AGENTRT_LOG_INFO("CoreLoopThree: turn-based checkpoint triggered "
                                     "(interval=%u turns, total_turns=%" PRIu64 ")",
                                     interval_turns, loop->loop_turn_count);
                }
            }
        }

        /* P1.6: 定期输出 checkpoint 统计信息 */
        if (loop->checkpoint_initialized && loop->manager.loop_config_stats_interval_ms > 0) {
            uint64_t now_ms = agentrt_time_ms();
            if (last_stats_log_ms == 0 ||
                (now_ms - last_stats_log_ms) >= loop->manager.loop_config_stats_interval_ms) {
                agentrt_checkpoint_stats_t stats;
                if (agentrt_checkpoint_get_stats(&stats) == AGENTRT_SUCCESS) {
                    AGENTRT_LOG_INFO("CoreLoopThree: checkpoint stats "
                                     "(total=%" PRIu64 ", success=%" PRIu64 ", fail=%" PRIu64
                                     ", restores=%" PRIu64 ", avg_size=%" PRIu64 "B, "
                                     "turns=%" PRIu64 ")",
                                     stats.total_checkpoints, stats.successful_checkpoints,
                                     stats.failed_checkpoints, stats.total_restore_ops,
                                     stats.avg_checkpoint_size, loop->loop_turn_count);
                }
                last_stats_log_ms = now_ms;
            }
        }

        /* C-L12: 定期输出 MemoryRovol 桥接器统计 */
        if (loop->memory_bridge && loop->manager.loop_config_stats_interval_ms > 0) {
            uint64_t now_ms = agentrt_time_ms();
            if (last_bridge_stats_ms == 0 ||
                (now_ms - last_bridge_stats_ms) >= loop->manager.loop_config_stats_interval_ms) {
                memoryrovol_bridge_dump_stats(loop->memory_bridge);
                last_bridge_stats_ms = now_ms;
            }
        }

        /* C-L01: 定期输出 Manager 适配器统计 */
        if (loop->manager_adapter && loop->manager.loop_config_stats_interval_ms > 0) {
            uint64_t now_ms = agentrt_time_ms();
            if (last_manager_stats_ms == 0 ||
                (now_ms - last_manager_stats_ms) >= loop->manager.loop_config_stats_interval_ms) {
                manager_adapter_dump_stats(loop->manager_adapter);
                last_manager_stats_ms = now_ms;
            }
        }

        /* C-L06: 定期输出 Orchestrator 适配器统计 */
        if (loop->orch_adapter && loop->manager.loop_config_stats_interval_ms > 0) {
            uint64_t now_ms = agentrt_time_ms();
            if (last_orch_stats_ms == 0 ||
                (now_ms - last_orch_stats_ms) >= loop->manager.loop_config_stats_interval_ms) {
                orch_adapter_dump_stats(loop->orch_adapter);
                last_orch_stats_ms = now_ms;
            }
        }
    }

    return AGENTRT_SUCCESS;
}

AGENTRT_API void agentrt_loop_stop(agentrt_core_loop_t *loop)
{
    if (!loop)
        return;

    agentrt_mutex_lock(loop->lock);
    loop->stop_requested = 1;
    while (loop->running) {
        agentrt_cond_wait(loop->cond, loop->lock);
    }
    agentrt_mutex_unlock(loop->lock);
}

AGENTRT_API agentrt_error_t agentrt_loop_submit(agentrt_core_loop_t *loop, const char *input,
                                                size_t input_len, char **out_task_id)
{
    if (!loop || !input || !out_task_id)
        AGENTRT_ERROR(AGENTRT_EINVAL, "failed to submit loop task: null loop, input, or out_task_id");
    if (!loop->cognition || !loop->execution || !loop->memory)
        ATM_RET_ERR(AGENTRT_ENOTINIT);

    AGENTRT_LOG_INFO("CoreLoopThree: agentrt_loop_submit START (input_len=%zu)", input_len);

    /* P1.6.1: 递增轮次计数器 */
    agentrt_mutex_lock(loop->lock);
    loop->loop_turn_count++;
    agentrt_mutex_unlock(loop->lock);

    /* P1.19.4: 设置当前线程的 Arena 用于请求级短生命周期分配 */
    agentrt_arena_t *prev_arena = agentrt_arena_get_current();
    if (loop->arena) {
        agentrt_arena_set_current(loop->arena);
    }

    /* 步骤 1: 从记忆中检索相关上下文 */
    agentrt_memory_query_t query = {0};
    query.memory_query_text = (char *)input;
    query.memory_query_text_len = input_len;
    query.memory_query_limit = 5;
    query.memory_query_include_raw = 1;
    agentrt_memory_result_ext_t *result = NULL;
    agentrt_error_t err = agentrt_memory_query(loop->memory, &query, &result);

    size_t memory_count = 0;
    agentrt_memory_record_t **memories = NULL;
    if (err == AGENTRT_SUCCESS && result && result->memory_result_count > 0) {
        memory_count = result->memory_result_count;
        /* P1.19.4: 使用 Arena 分配短生命周期 memories 数组 */
        memories = (agentrt_memory_record_t **)AGENTRT_ARENA_ALLOC(memory_count *
                                                              sizeof(agentrt_memory_record_t *));
        if (memories) {
            for (size_t i = 0; i < memory_count; i++) {
                memories[i] = result->memory_result_items[i]->memory_result_item_record;
            }
            AGENTRT_LOG_DEBUG("CoreLoopThree: memory query returned %zu records", memory_count);
        } else {
            memory_count = 0;
        }
    }

    /* 步骤 2: 构建增强输入（如果有相关记忆） */
    char *enhanced_input = NULL;
    if (err == AGENTRT_SUCCESS && memory_count > 0) {
        enhanced_input = build_enhanced_input(input, input_len, memories, memory_count,
                                              loop->manager.loop_config_memory_query_limit);
    }

    /* 释放记忆结果（无论是否构建了增强输入） */
    if (result)
        agentrt_memory_result_free(result);
    /* memories 数组由 Arena 管理，无需单独释放 */

    /* 步骤 3: 认知层处理（带上下文增强） */
    const char *process_input = enhanced_input ? enhanced_input : input;
    size_t process_len = enhanced_input ? strlen(enhanced_input) : input_len;

    agentrt_task_plan_t *plan = NULL;
    err = agentrt_cognition_process(loop->cognition, process_input, process_len, &plan);
    if (err != AGENTRT_SUCCESS) {
        AGENTRT_LOG_WARN("CoreLoopThree: cognition process FAILED (err=%d)", err);
        goto submit_cleanup;
    }

    if (!plan || plan->task_plan_node_count == 0) {
        agentrt_task_plan_free(plan);
        AGENTRT_LOG_WARN("CoreLoopThree: empty or null task plan");
        AGENTRT_ERROR(AGENTRT_EINVAL, "failed to submit loop task: empty or null task plan");
        /* 不会到达这里，AGENTRT_ERROR 会 return */
    }

    AGENTRT_LOG_INFO("CoreLoopThree: plan created (nodes=%zu)", plan->task_plan_node_count);

    /* 步骤 4: 执行层按计划节点提交任务 */
    agentrt_error_t first_err = AGENTRT_SUCCESS;
    char *saved_task_id = NULL;
    for (size_t i = 0; i < plan->task_plan_node_count; i++) {
        agentrt_task_node_t *node = plan->task_plan_nodes[i];
        if (!node)
            continue;

        agentrt_task_t task;
        __builtin_memset(&task, 0, sizeof(task));
        task.task_input = node->task_node_input ? node->task_node_input : (void *)process_input;
        task.task_timeout_ms = node->task_node_timeout_ms > 0
                                   ? node->task_node_timeout_ms
                                   : loop->manager.loop_config_task_timeout_ms;

        char *node_task_id = NULL;
        err = agentrt_execution_submit(loop->execution, &task, &node_task_id);
        if (err != AGENTRT_SUCCESS && first_err == AGENTRT_SUCCESS) {
            first_err = err;
        }
        if (i == 0 && node_task_id && !saved_task_id) {
            saved_task_id = AGENTRT_STRDUP(node_task_id);
        }
        if (node_task_id)
            AGENTRT_FREE(node_task_id);
    }

    if (first_err == AGENTRT_SUCCESS && out_task_id) {
        *out_task_id = saved_task_id ? saved_task_id : AGENTRT_STRDUP("task-unknown");
    }

    if (first_err == AGENTRT_SUCCESS) {
        agentrt_mutex_lock(loop->lock);
        loop->task_pending = 1;
        agentrt_cond_signal(loop->cond);
        agentrt_mutex_unlock(loop->lock);
        AGENTRT_LOG_INFO("CoreLoopThree: agentrt_loop_submit OK (task_id=%s, nodes=%zu)",
                         saved_task_id ? saved_task_id : "task-unknown",
                         plan->task_plan_node_count);
    } else {
        AGENTRT_LOG_WARN("CoreLoopThree: agentrt_loop_submit FAILED (err=%d)", first_err);
    }

    /* 清理临时资源 */
    agentrt_task_plan_free(plan);

submit_cleanup:
    /* P1.19.4: 重置 Arena，释放所有请求级短生命周期内存 */
    if (loop->arena) {
        arena_reset(loop->arena);
    }
    /* 恢复之前的 Arena */
    agentrt_arena_set_current(prev_arena);

    return first_err;
}

AGENTRT_API agentrt_error_t agentrt_loop_wait(agentrt_core_loop_t *loop, const char *task_id,
                                              uint32_t timeout_ms, char **out_result,
                                              size_t *out_result_len)
{
    if (!loop || !task_id || !out_result || !out_result_len)
        ATM_RET_ERR(AGENTRT_EINVAL);
    if (!loop->execution || !loop->memory)
        ATM_RET_ERR(AGENTRT_ENOTINIT);

    AGENTRT_LOG_DEBUG("CoreLoopThree: agentrt_loop_wait (task_id=%s, timeout=%u)", task_id, timeout_ms);

    /* 等待执行完成 */
    agentrt_task_t *result_task = NULL;
    agentrt_error_t err =
        agentrt_execution_wait(loop->execution, task_id, timeout_ms, &result_task);

    if (err == AGENTRT_SUCCESS && result_task) {
        if (result_task->task_output) {
            size_t len = 0;
            const char *output = (const char *)result_task->task_output;
            while (output[len] != '\0')
                len++;
            *out_result = AGENTRT_STRDUP(output);
            *out_result_len = len;
        } else {
            *out_result = AGENTRT_STRDUP("");
            *out_result_len = 0;
        }

        if (!*out_result) {
            agentrt_task_free(result_task);
            ATM_RET_ERR(AGENTRT_ENOMEM);
        }

        if (*out_result_len > 0) {
            agentrt_memory_record_t record = {0};
            record.memory_record_data = *out_result;
            record.memory_record_data_len = *out_result_len;
            record.memory_record_type = AGENTRT_MEMTYPE_TEXT;
            record.memory_record_importance = loop->manager.loop_config_memory_importance;
            char *new_record_id = NULL;
            agentrt_error_t store_err = agentrt_memory_write(loop->memory, &record, &new_record_id);
            if (new_record_id) {
                AGENTRT_FREE(new_record_id);
                new_record_id = NULL;
            }
            if (store_err != AGENTRT_SUCCESS) {
                AGENTRT_LOG_WARN("Failed to store execution result to memory: %d", store_err);
            } else {
                AGENTRT_LOG_INFO("Successfully stored execution result to memory");
            }
        }

        agentrt_task_free(result_task);

        if (!*out_result)
            ATM_RET_ERR(AGENTRT_ENOMEM);
    }

    return err;
}

AGENTRT_API void agentrt_loop_get_engines(agentrt_core_loop_t *loop,
                                          agentrt_cognition_engine_t **out_cognition,
                                          agentrt_execution_engine_t **out_execution,
                                          agentrt_memory_engine_t **out_memory)
{
    if (!loop)
        return;

    if (out_cognition)
        *out_cognition = loop->cognition;
    if (out_execution)
        *out_execution = loop->execution;
    if (out_memory)
        *out_memory = loop->memory;
}

static uint64_t get_time_ms(void)
{
    return agentrt_time_ms();
}

static void generate_task_id(char *buf, size_t buf_size)
{
    snprintf(buf, buf_size, "task-%016lx", (unsigned long)(agentrt_time_ns() & 0xFFFFFFFF));
}

static void loop_checkpoint_auto_hook(const char *task_id, const char *state_json, void *user_data)
{
    agentrt_core_loop_t *loop = (agentrt_core_loop_t *)user_data;
    if (!loop || !loop->checkpoint_initialized || !task_id)
        return;

    char state[8192];
    if (state_json && state_json[0]) {
        snprintf(state, sizeof(state), "%s", state_json);
    } else {
        /* P1.6.2: 使用 build_rich_checkpoint_state 构建丰富的 checkpoint 状态 */
        char *rich_state = build_rich_checkpoint_state(loop, task_id, NULL);
        if (rich_state) {
            snprintf(state, sizeof(state), "%s", rich_state);
            AGENTRT_FREE(rich_state);
        } else {
            /* 回退到简单状态 */
            snprintf(state, sizeof(state),
                     "{\"state\":\"auto\",\"task_id\":\"%s\",\"session_id\":\"%s\","
                     "\"turn_count\":%" PRIu64 "}",
                     task_id, loop->current_session_id, loop->loop_turn_count);
        }
    }

    loop->checkpoint_seq++;
    agentrt_task_checkpoint_t *checkpoint = NULL;
    agentrt_error_t err = agentrt_checkpoint_create(
        task_id, loop->current_session_id, loop->checkpoint_seq, state, loop->completed_node_ids,
        loop->completed_node_count, NULL, 0, &checkpoint);
    if (err == AGENTRT_SUCCESS && checkpoint) {
        err = agentrt_checkpoint_save(checkpoint);
        if (err == AGENTRT_SUCCESS) {
            AGENTRT_LOG_INFO("Auto-checkpoint saved for task %s (seq=%lu, completed=%zu, "
                             "turns=%" PRIu64 ", tools=%zu)",
                             task_id, (unsigned long)loop->checkpoint_seq,
                             loop->completed_node_count, loop->loop_turn_count,
                             loop->tool_call_history_count);
        } else {
            AGENTRT_LOG_WARN("Auto-checkpoint save FAILED for task %s (err=%d)", task_id, err);
        }
        agentrt_checkpoint_destroy(checkpoint);
    }
    loop->last_checkpoint_time_ms = get_time_ms();
}

static agentrt_error_t save_plan_checkpoint(agentrt_core_loop_t *loop,
                                            const agentrt_task_plan_t *plan, const char *task_id,
                                            const char *session_id, const char *original_input)
{
    if (!loop->checkpoint_initialized)
        ATM_RET_ERR(AGENTRT_ENOTINIT);
    if (!plan || !task_id)
        AGENTRT_ERROR(AGENTRT_EINVAL, "failed to save plan checkpoint: null plan or task_id");

    size_t completed_count = 0;
    size_t pending_count = plan->task_plan_node_count;
    char **completed_nodes = NULL;
    char **pending_nodes = NULL;

    if (pending_count > 0) {
        /* P1.19.4: 使用 Arena 分配短生命周期 pending_nodes 数组 */
        pending_nodes = (char **)AGENTRT_ARENA_ALLOC(pending_count * sizeof(char *));
        if (!pending_nodes)
            ATM_RET_ERR(AGENTRT_ENOMEM);
        for (size_t i = 0; i < pending_count; i++) {
            if (plan->task_plan_nodes[i] && plan->task_plan_nodes[i]->task_node_id) {
                /* STRDUP 用于 checkpoint 持久化，不能使用 Arena（reset 后会失效） */
                pending_nodes[i] = AGENTRT_STRDUP(plan->task_plan_nodes[i]->task_node_id);
            }
        }
    }

    char state_json[8192];
    const char *safe_input = original_input ? original_input : "";
    int json_len = snprintf(state_json, sizeof(state_json),
                            "{\"plan_id\":\"%s\",\"node_count\":%zu,\"session_id\":\"%s\","
                            "\"original_input\":\"%.4096s\"}",
                            plan->task_plan_id ? plan->task_plan_id : "",
                            plan->task_plan_node_count, session_id ? session_id : "", safe_input);

    if (json_len < 0 || json_len >= (int)sizeof(state_json)) {
        if (pending_nodes) {
            for (size_t i = 0; i < pending_count; i++)
                AGENTRT_FREE(pending_nodes[i]);
            AGENTRT_FREE(pending_nodes);
        }
        ATM_RET_ERR(AGENTRT_EOVERFLOW);
    }

    loop->checkpoint_seq++;
    agentrt_task_checkpoint_t *checkpoint = NULL;
    agentrt_error_t err = agentrt_checkpoint_create(
        task_id, session_id ? session_id : "default", loop->checkpoint_seq, state_json,
        completed_nodes, completed_count, pending_nodes, pending_count, &checkpoint);

    if (pending_nodes) {
        for (size_t i = 0; i < pending_count; i++)
            AGENTRT_FREE(pending_nodes[i]);
        AGENTRT_FREE(pending_nodes);
        pending_nodes = NULL;
    }

    if (err != AGENTRT_SUCCESS || !checkpoint) {
        AGENTRT_LOG_WARN("Failed to create checkpoint for task %s: %d", task_id, err);
        return err;
    }

    err = agentrt_checkpoint_save(checkpoint);
    if (err != AGENTRT_SUCCESS) {
        AGENTRT_LOG_WARN("Failed to save checkpoint for task %s: %d", task_id, err);
    } else {
        AGENTRT_LOG_INFO("Checkpoint saved for task %s (seq=%lu)", task_id,
                         (unsigned long)loop->checkpoint_seq);
    }

    agentrt_checkpoint_destroy(checkpoint);
    loop->last_checkpoint_time_ms = get_time_ms();
    return err;
}

AGENTRT_API agentrt_error_t agentrt_loop_submit_persistent(agentrt_core_loop_t *loop,
                                                           const char *input, size_t input_len,
                                                           const char *session_id,
                                                           char **out_task_id)
{
    if (!loop || !input || !out_task_id)
        ATM_RET_ERR(AGENTRT_EINVAL);
    if (!loop->cognition || !loop->execution || !loop->memory)
        ATM_RET_ERR(AGENTRT_ENOTINIT);

    AGENTRT_LOG_INFO("CoreLoopThree: agentrt_loop_submit_persistent START (input_len=%zu, session=%s)",
                     input_len, session_id ? session_id : "auto");

    /* P1.6.1: 递增轮次计数器 */
    agentrt_mutex_lock(loop->lock);
    loop->loop_turn_count++;
    agentrt_mutex_unlock(loop->lock);

    char task_id_buf[128];
    generate_task_id(task_id_buf, sizeof(task_id_buf));

    agentrt_mutex_lock(loop->lock);
    snprintf(loop->current_task_id, sizeof(loop->current_task_id), "%s", task_id_buf);
    if (session_id) {
        snprintf(loop->current_session_id, sizeof(loop->current_session_id), "%s", session_id);
    } else {
        snprintf(loop->current_session_id, sizeof(loop->current_session_id), "sess-%016lx",
                 (unsigned long)(agentrt_time_ns() & 0xFFFFFFFF));
    }
    loop->checkpoint_seq = 0;
    clear_completed_nodes(loop);
    clear_tool_call_history(loop);
    agentrt_mutex_unlock(loop->lock);

    /* P1.19.4: 设置当前线程的 Arena 用于请求级短生命周期分配 */
    agentrt_arena_t *prev_arena = agentrt_arena_get_current();
    if (loop->arena) {
        agentrt_arena_set_current(loop->arena);
    }

    agentrt_memory_query_t query = {0};
    query.memory_query_text = (char *)input;
    query.memory_query_text_len = input_len;
    query.memory_query_limit = 5;
    query.memory_query_include_raw = 1;
    agentrt_memory_result_ext_t *result = NULL;
    agentrt_error_t err = agentrt_memory_query(loop->memory, &query, &result);

    size_t memory_count = 0;
    agentrt_memory_record_t **memories = NULL;
    if (err == AGENTRT_SUCCESS && result && result->memory_result_count > 0) {
        memory_count = result->memory_result_count;
        /* P1.19.4: 使用 Arena 分配短生命周期 memories 数组 */
        memories = (agentrt_memory_record_t **)AGENTRT_ARENA_ALLOC(memory_count *
                                                              sizeof(agentrt_memory_record_t *));
        if (memories) {
            for (size_t i = 0; i < memory_count; i++) {
                memories[i] = result->memory_result_items[i]->memory_result_item_record;
            }
        } else {
            memory_count = 0;
        }
    }

    char *enhanced_input = NULL;
    if (err == AGENTRT_SUCCESS && memory_count > 0) {
        enhanced_input = build_enhanced_input(input, input_len, memories, memory_count,
                                              loop->manager.loop_config_memory_query_limit);
    }

    if (result)
        agentrt_memory_result_free(result);
    /* memories 数组由 Arena 管理，无需单独释放 */

    const char *process_input = enhanced_input ? enhanced_input : input;
    size_t process_len = enhanced_input ? strlen(enhanced_input) : input_len;

    agentrt_task_plan_t *plan = NULL;
    err = agentrt_cognition_process(loop->cognition, process_input, process_len, &plan);
    if (err != AGENTRT_SUCCESS) {
        AGENTRT_LOG_WARN("CoreLoopThree: persistent cognition process FAILED (err=%d)", err);
        goto persistent_cleanup;
    }

    if (!plan || plan->task_plan_node_count == 0) {
        agentrt_task_plan_free(plan);
        AGENTRT_LOG_WARN("CoreLoopThree: persistent empty or null task plan");
        goto persistent_cleanup;
    }

    AGENTRT_LOG_INFO("CoreLoopThree: persistent plan created (nodes=%zu)", plan->task_plan_node_count);

    if (loop->checkpoint_initialized) {
        save_plan_checkpoint(loop, plan, task_id_buf, loop->current_session_id, process_input);

        if (loop->persistent_original_input) {
            AGENTRT_FREE(loop->persistent_original_input);
        }
        loop->persistent_original_input = AGENTRT_STRDUP(process_input);
    }

    agentrt_error_t first_err = AGENTRT_SUCCESS;
    for (size_t i = 0; i < plan->task_plan_node_count; i++) {
        agentrt_task_node_t *node = plan->task_plan_nodes[i];
        if (!node)
            continue;

        agentrt_task_t task;
        __builtin_memset(&task, 0, sizeof(task));
        task.task_input = node->task_node_input ? node->task_node_input : (void *)process_input;
        task.task_timeout_ms = node->task_node_timeout_ms > 0
                                   ? node->task_node_timeout_ms
                                   : loop->manager.loop_config_task_timeout_ms;

        char *node_task_id = NULL;
        err = agentrt_execution_submit(loop->execution, &task, &node_task_id);
        if (err != AGENTRT_SUCCESS && first_err == AGENTRT_SUCCESS) {
            first_err = err;
        }
        if (node_task_id)
            AGENTRT_FREE(node_task_id);

        if (err == AGENTRT_SUCCESS && loop->checkpoint_initialized && node->task_node_id) {
            add_completed_node(loop, node->task_node_id);
            /* P1.6.2: 记录工具调用历史 */
            add_tool_call_history(loop, node->task_node_id,
                                  node->task_node_input ? (const char *)node->task_node_input : NULL);
            save_incremental_checkpoint(loop, task_id_buf, loop->current_session_id,
                                        node->task_node_id);
        }
    }

    if (first_err == AGENTRT_SUCCESS) {
        *out_task_id = AGENTRT_STRDUP(task_id_buf);
        if (!*out_task_id)
            first_err = AGENTRT_ENOMEM;

        agentrt_mutex_lock(loop->lock);
        loop->task_pending = 1;
        agentrt_cond_signal(loop->cond);
        agentrt_mutex_unlock(loop->lock);
        AGENTRT_LOG_INFO("CoreLoopThree: agentrt_loop_submit_persistent OK (task_id=%s, nodes=%zu)",
                         task_id_buf, plan->task_plan_node_count);
    } else {
        AGENTRT_LOG_WARN("CoreLoopThree: agentrt_loop_submit_persistent FAILED (err=%d)", first_err);
    }

    agentrt_task_plan_free(plan);

persistent_cleanup:
    /* P1.19.4: 重置 Arena，释放所有请求级短生命周期内存 */
    if (loop->arena) {
        arena_reset(loop->arena);
    }
    /* 恢复之前的 Arena */
    agentrt_arena_set_current(prev_arena);

    return first_err;
}

AGENTRT_API agentrt_error_t agentrt_loop_restore_task(agentrt_core_loop_t *loop,
                                                      const char *task_id,
                                                      char **out_restored_task_id)
{
    if (!loop || !task_id || !out_restored_task_id)
        ATM_RET_ERR(AGENTRT_EINVAL);
    if (!loop->checkpoint_initialized)
        ATM_RET_ERR(AGENTRT_ENOTINIT);
    if (!loop->cognition || !loop->execution)
        ATM_RET_ERR(AGENTRT_ENOTINIT);

    AGENTRT_LOG_INFO("CoreLoopThree: agentrt_loop_restore_task (task_id=%s)", task_id);

    agentrt_task_checkpoint_t **checkpoints = NULL;
    size_t cp_count = 0;
    agentrt_error_t err = agentrt_checkpoint_list(task_id, &checkpoints, &cp_count);
    if (err != AGENTRT_SUCCESS || cp_count == 0) {
        if (checkpoints) {
            for (size_t i = 0; i < cp_count; i++) {
                agentrt_checkpoint_destroy(checkpoints[i]);
            }
            AGENTRT_FREE(checkpoints);
        }
        ATM_RET_ERR(AGENTRT_ENOENT);
    }

    agentrt_task_checkpoint_t *latest = NULL;
    uint64_t latest_seq = 0;
    for (size_t i = 0; i < cp_count; i++) {
        if (checkpoints[i] && checkpoints[i]->sequence_num > latest_seq) {
            latest_seq = checkpoints[i]->sequence_num;
            latest = checkpoints[i];
        }
    }

    if (!latest || !latest->state_json) {
        for (size_t i = 0; i < cp_count; i++) {
            agentrt_checkpoint_destroy(checkpoints[i]);
        }
        AGENTRT_FREE(checkpoints);
        ATM_RET_ERR(AGENTRT_ENOENT);
    }

    bool is_valid = false;
    agentrt_checkpoint_verify(latest, &is_valid);
    if (!is_valid) {
        AGENTRT_LOG_WARN("Checkpoint for task %s seq %lu failed verification", task_id,
                         (unsigned long)latest_seq);
        for (size_t i = 0; i < cp_count; i++) {
            agentrt_checkpoint_destroy(checkpoints[i]);
        }
        AGENTRT_FREE(checkpoints);
        ATM_RET_ERR(AGENTRT_EIO);
    }

    char restored_id[128];
    snprintf(restored_id, sizeof(restored_id), "task-%s-restored-%016lx", task_id,
             (unsigned long)(agentrt_time_ns() & 0xFFFFFFFF));

    agentrt_mutex_lock(loop->lock);
    snprintf(loop->current_task_id, sizeof(loop->current_task_id), "%s", restored_id);
    snprintf(loop->current_session_id, sizeof(loop->current_session_id), "%s", latest->session_id);
    loop->checkpoint_seq = latest->sequence_num;
    clear_completed_nodes(loop);
    for (size_t i = 0; i < latest->completed_count; i++) {
        if (latest->completed_nodes && latest->completed_nodes[i])
            add_completed_node(loop, latest->completed_nodes[i]);
    }
    agentrt_mutex_unlock(loop->lock);

    char *recovered_input = NULL;
    if (loop->persistent_original_input) {
        AGENTRT_FREE(loop->persistent_original_input);
        loop->persistent_original_input = NULL;
    }

    if (latest->state_json) {
        const char *input_tag = "\"original_input\":\"";
        char *input_pos = strstr(latest->state_json, input_tag);
        if (input_pos) {
            input_pos += strlen(input_tag);
            char *input_end = strchr(input_pos, '\"');
            if (input_end && input_end > input_pos) {
                size_t input_len = (size_t)(input_end - input_pos);
                recovered_input = (char *)AGENTRT_MALLOC(input_len + 1);
                if (recovered_input) {
                    __builtin_memcpy(recovered_input, input_pos, input_len);
                    recovered_input[input_len] = '\0';
                    loop->persistent_original_input = AGENTRT_STRDUP(recovered_input);
                }
            }
        }
    }

    for (size_t i = 0; i < latest->pending_count; i++) {
        if (!latest->pending_nodes || !latest->pending_nodes[i])
            continue;

        agentrt_task_t task;
        __builtin_memset(&task, 0, sizeof(task));
        task.task_input =
            recovered_input ? (void *)recovered_input : (void *)latest->pending_nodes[i];
        task.task_timeout_ms = loop->manager.loop_config_task_timeout_ms;

        char *node_task_id = NULL;
        err = agentrt_execution_submit(loop->execution, &task, &node_task_id);
        if (err != AGENTRT_SUCCESS) {
            AGENTRT_LOG_WARN("Failed to resubmit pending node %s: %d", latest->pending_nodes[i],
                             err);
        }
        if (node_task_id)
            AGENTRT_FREE(node_task_id);
    }

    if (recovered_input) {
        AGENTRT_FREE(recovered_input);
        recovered_input = NULL;
    }

    *out_restored_task_id = AGENTRT_STRDUP(restored_id);

    agentrt_mutex_lock(loop->lock);
    loop->task_pending = 1;
    agentrt_cond_signal(loop->cond);
    agentrt_mutex_unlock(loop->lock);

    AGENTRT_LOG_INFO("Restored task %s from checkpoint seq %lu (%zu pending nodes)", task_id,
                     (unsigned long)latest_seq, latest->pending_count);

    for (size_t i = 0; i < cp_count; i++) {
        agentrt_checkpoint_destroy(checkpoints[i]);
    }
    AGENTRT_FREE(checkpoints);

    return AGENTRT_SUCCESS;
}

AGENTRT_API agentrt_error_t agentrt_loop_list_checkpoints(agentrt_core_loop_t *loop,
                                                          char ***out_task_ids, size_t *out_count)
{
    if (!loop || !out_task_ids || !out_count)
        ATM_RET_ERR(AGENTRT_EINVAL);
    if (!loop->checkpoint_initialized)
        ATM_RET_ERR(AGENTRT_ENOTINIT);

    *out_count = 0;
    *out_task_ids = NULL;

    const char *cp_dir = loop->manager.loop_config_checkpoint_path;
    if (cp_dir[0] == '\0')
        cp_dir = "./data/checkpoints";

    DIR *dir = opendir(cp_dir);
    if (!dir)
        return AGENTRT_SUCCESS;

    size_t capacity = 16;
    char **ids = (char **)AGENTRT_CALLOC(capacity, sizeof(char *));
    if (!ids) {
        closedir(dir);
        ATM_RET_ERR(AGENTRT_ENOMEM);
    }

    size_t count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        size_t nlen = strlen(name);

        if (nlen < 6 || strcmp(name + nlen - 5, ".json") != 0)
            continue;

        if (strncmp(name, "checkpoint_", 11) != 0)
            continue;

        char tid[128] = {0};
        size_t tid_len = nlen - 5 - 11;
        if (tid_len >= sizeof(tid))
            tid_len = sizeof(tid) - 1;
        __builtin_memcpy(tid, name + 11, tid_len);
        tid[tid_len] = '\0';

        int dup = 0;
        for (size_t j = 0; j < count; j++) {
            if (ids[j] && strcmp(ids[j], tid) == 0) {
                dup = 1;
                break;
            }
        }
        if (dup)
            continue;

        if (count >= capacity) {
            capacity *= 2;
            char **new_ids = (char **)AGENTRT_REALLOC(ids, capacity * sizeof(char *));
            if (!new_ids) {
                for (size_t j = 0; j < count; j++)
                    AGENTRT_FREE(ids[j]);
                AGENTRT_FREE(ids);
                closedir(dir);
                ATM_RET_ERR(AGENTRT_ENOMEM);
            }
            ids = new_ids;
            __builtin_memset(ids + count, 0, (capacity - count) * sizeof(char *));
        }

        ids[count] = AGENTRT_STRDUP(tid);
        if (ids[count])
            count++;
    }
    closedir(dir);

    *out_task_ids = ids;
    *out_count = count;
    return AGENTRT_SUCCESS;
}

static void add_completed_node(agentrt_core_loop_t *loop, const char *node_id)
{
    if (!loop || !node_id)
        return;

    if (loop->completed_node_count >= loop->completed_node_capacity) {
        size_t new_cap =
            loop->completed_node_capacity == 0 ? 16 : loop->completed_node_capacity * 2;
        char **new_arr =
            (char **)AGENTRT_REALLOC(loop->completed_node_ids, new_cap * sizeof(char *));
        if (!new_arr)
            return;
        loop->completed_node_ids = new_arr;
        __builtin_memset(loop->completed_node_ids + loop->completed_node_capacity, 0,
               (new_cap - loop->completed_node_capacity) * sizeof(char *));
        loop->completed_node_capacity = new_cap;
    }

    loop->completed_node_ids[loop->completed_node_count] = AGENTRT_STRDUP(node_id);
    if (loop->completed_node_ids[loop->completed_node_count])
        loop->completed_node_count++;
}

static void clear_completed_nodes(agentrt_core_loop_t *loop)
{
    if (!loop)
        return;

    if (loop->completed_node_ids) {
        for (size_t i = 0; i < loop->completed_node_count; i++) {
            AGENTRT_FREE(loop->completed_node_ids[i]);
        }
        AGENTRT_FREE(loop->completed_node_ids);
        loop->completed_node_ids = NULL;
    }
    loop->completed_node_count = 0;
    loop->completed_node_capacity = 0;
}

static agentrt_error_t save_incremental_checkpoint(agentrt_core_loop_t *loop, const char *task_id,
                                                   const char *session_id, const char *node_id)
{
    if (!loop || !loop->checkpoint_initialized || !task_id)
        ATM_RET_ERR(AGENTRT_EINVAL);

    char state_json[8192];
    int json_len = snprintf(state_json, sizeof(state_json),
                            "{\"type\":\"incremental\",\"task_id\":\"%s\","
                            "\"session_id\":\"%s\",\"completed_node\":\"%s\","
                            "\"total_completed\":%zu}",
                            task_id, session_id ? session_id : "", node_id ? node_id : "",
                            loop->completed_node_count);

    if (json_len <= 0 || (size_t)json_len >= sizeof(state_json))
        ATM_RET_ERR(AGENTRT_EOVERFLOW);

    loop->checkpoint_seq++;
    agentrt_task_checkpoint_t *checkpoint = NULL;
    agentrt_error_t err = agentrt_checkpoint_create(
        task_id, session_id ? session_id : "default", loop->checkpoint_seq, state_json,
        loop->completed_node_ids, loop->completed_node_count, NULL, 0, &checkpoint);

    if (err == AGENTRT_SUCCESS && checkpoint) {
        err = agentrt_checkpoint_save(checkpoint);
        if (err == AGENTRT_SUCCESS) {
            AGENTRT_LOG_INFO(
                "Incremental checkpoint saved for task %s node %s (seq=%lu, completed=%zu)",
                task_id, node_id ? node_id : "", (unsigned long)loop->checkpoint_seq,
                loop->completed_node_count);
        }
        agentrt_checkpoint_destroy(checkpoint);
    }

    loop->last_checkpoint_time_ms = get_time_ms();
    return err;
}

/* ==================== P1.6.2: 工具调用历史管理 ==================== */

static void add_tool_call_history(agentrt_core_loop_t *loop, const char *tool_name, const char *tool_input)
{
    if (!loop || !tool_name)
        return;

    if (loop->tool_call_history_count >= loop->tool_call_history_capacity) {
        size_t new_cap =
            loop->tool_call_history_capacity == 0 ? 8 : loop->tool_call_history_capacity * 2;
        char **new_arr =
            (char **)AGENTRT_REALLOC(loop->tool_call_history, new_cap * sizeof(char *));
        if (!new_arr)
            return;
        loop->tool_call_history = new_arr;
        __builtin_memset(loop->tool_call_history + loop->tool_call_history_capacity, 0,
               (new_cap - loop->tool_call_history_capacity) * sizeof(char *));
        loop->tool_call_history_capacity = new_cap;
    }

    /* 格式: "tool_name(input_truncated)" */
    char entry[512];
    size_t input_len = tool_input ? strlen(tool_input) : 0;
    const char *truncated = (input_len > 64) ? "..." : "";
    snprintf(entry, sizeof(entry), "%s(%.64s%s)", tool_name,
             tool_input ? tool_input : "", truncated);

    loop->tool_call_history[loop->tool_call_history_count] = AGENTRT_STRDUP(entry);
    if (loop->tool_call_history[loop->tool_call_history_count])
        loop->tool_call_history_count++;

    AGENTRT_LOG_DEBUG("CoreLoopThree: tool call recorded: %s (total=%zu)",
                      tool_name, loop->tool_call_history_count);
}

static void clear_tool_call_history(agentrt_core_loop_t *loop)
{
    if (!loop || !loop->tool_call_history)
        return;

    for (size_t i = 0; i < loop->tool_call_history_count; i++) {
        AGENTRT_FREE(loop->tool_call_history[i]);
    }
    AGENTRT_FREE(loop->tool_call_history);
    loop->tool_call_history = NULL;
    loop->tool_call_history_count = 0;
    loop->tool_call_history_capacity = 0;
}

/* ==================== P1.6.2: 丰富的 checkpoint 状态构建 ==================== */

static char *build_rich_checkpoint_state(agentrt_core_loop_t *loop, const char *task_id,
                                         const char *extra_json)
{
    if (!loop || !task_id)
        return NULL;

    /* 工具调用历史 JSON */
    char tool_json[4096] = "";
    size_t tpos = 0;
    if (loop->tool_call_history_count > 0) {
        tpos += snprintf(tool_json + tpos, sizeof(tool_json) - tpos, "\"tool_call_history\":[");
        for (size_t i = 0; i < loop->tool_call_history_count && tpos < sizeof(tool_json) - 128; i++) {
            if (loop->tool_call_history[i]) {
                tpos += snprintf(tool_json + tpos, sizeof(tool_json) - tpos,
                                 "%s\"%s\"", i > 0 ? "," : "", loop->tool_call_history[i]);
            }
        }
        snprintf(tool_json + tpos, sizeof(tool_json) - tpos, "],");
    }

    /* 构建完整状态 JSON */
    size_t state_size = 8192 + (extra_json ? strlen(extra_json) : 0);
    char *state = (char *)AGENTRT_MALLOC(state_size);
    if (!state)
        return NULL;

    int written = snprintf(state, state_size,
                           "{\"type\":\"checkpoint\",\"task_id\":\"%s\","
                           "\"session_id\":\"%s\","
                           "\"checkpoint_seq\":%lu,"
                           "\"turn_count\":%" PRIu64 ","
                           "\"completed_nodes\":%zu,"
                           "%s"
                           "\"cognition_state\":\"active\","
                           "\"memory_context\":\"%zu_memory_records\","
                           "%s}",
                           task_id, loop->current_session_id,
                           (unsigned long)loop->checkpoint_seq,
                           loop->loop_turn_count,
                           loop->completed_node_count,
                           tool_json,
                           loop->completed_node_count,
                           extra_json ? extra_json : "");

    if (written <= 0 || (size_t)written >= state_size) {
        AGENTRT_FREE(state);
        return NULL;
    }

    return state;
}

/* ==================== P1.6.3: 快照 API 实现 ==================== */

AGENTRT_API agentrt_error_t agentrt_loop_create_snapshot(agentrt_core_loop_t *loop,
                                                         const char *task_id,
                                                         const char *snapshot_path)
{
    if (!loop || !task_id || !snapshot_path)
        ATM_RET_ERR(AGENTRT_EINVAL);
    if (!loop->checkpoint_initialized)
        ATM_RET_ERR(AGENTRT_ENOTINIT);

    AGENTRT_LOG_INFO("CoreLoopThree: creating snapshot for task %s (path=%s)",
                     task_id, snapshot_path);

    agentrt_error_t err = agentrt_snapshot_create(task_id, snapshot_path);
    if (err == AGENTRT_SUCCESS) {
        AGENTRT_LOG_INFO("CoreLoopThree: snapshot created OK (task=%s, path=%s, "
                         "turns=%" PRIu64 ", completed=%zu)",
                         task_id, snapshot_path, loop->loop_turn_count,
                         loop->completed_node_count);
    } else {
        AGENTRT_LOG_WARN("CoreLoopThree: snapshot creation FAILED (task=%s, err=%d)",
                         task_id, err);
    }

    return err;
}

AGENTRT_API agentrt_error_t agentrt_loop_restore_snapshot(agentrt_core_loop_t *loop,
                                                          const char *snapshot_path,
                                                          char **out_restored_task_id)
{
    if (!loop || !snapshot_path || !out_restored_task_id)
        ATM_RET_ERR(AGENTRT_EINVAL);
    if (!loop->checkpoint_initialized)
        ATM_RET_ERR(AGENTRT_ENOTINIT);

    AGENTRT_LOG_INFO("CoreLoopThree: restoring from snapshot (path=%s)", snapshot_path);

    *out_restored_task_id = NULL;

    /* 从快照文件恢复 task_id */
    char *task_id = NULL;
    agentrt_error_t err = agentrt_snapshot_restore(snapshot_path, &task_id);
    if (err != AGENTRT_SUCCESS || !task_id) {
        AGENTRT_LOG_WARN("CoreLoopThree: snapshot restore FAILED (path=%s, err=%d)",
                         snapshot_path, err);
        if (task_id)
            AGENTRT_FREE(task_id);
        return err == AGENTRT_SUCCESS ? AGENTRT_EIO : err;
    }

    AGENTRT_LOG_INFO("CoreLoopThree: snapshot task_id recovered: %s", task_id);

    /* 通过 restore_task 恢复完整状态 */
    err = agentrt_loop_restore_task(loop, task_id, out_restored_task_id);
    AGENTRT_FREE(task_id);
    task_id = NULL;

    if (err == AGENTRT_SUCCESS) {
        AGENTRT_LOG_INFO("CoreLoopThree: snapshot restored OK (new_task=%s)",
                         *out_restored_task_id ? *out_restored_task_id : "unknown");
    } else {
        AGENTRT_LOG_WARN("CoreLoopThree: snapshot restore via restore_task FAILED (err=%d)", err);
    }

    return err;
}

AGENTRT_API agentrt_error_t agentrt_loop_get_checkpoint_stats(agentrt_core_loop_t *loop,
                                                              agentrt_checkpoint_stats_t *out_stats)
{
    if (!loop || !out_stats)
        ATM_RET_ERR(AGENTRT_EINVAL);
    if (!loop->checkpoint_initialized)
        ATM_RET_ERR(AGENTRT_ENOTINIT);

    return agentrt_checkpoint_get_stats(out_stats);
}

/* ================================================================
 * C-L01 + C-L06: 外部适配器注入
 * ================================================================ */

AGENTRT_API void agentrt_loop_set_manager_adapter(agentrt_core_loop_t *loop,
                                                   agentrt_manager_adapter_t *adapter)
{
    if (!loop) return;
    loop->manager_adapter = adapter;
    AGENTRT_LOG_INFO("C-L01: Manager adapter %s to CoreLoopThree",
                     adapter ? "attached" : "detached");
}

AGENTRT_API void agentrt_loop_set_orch_adapter(agentrt_core_loop_t *loop,
                                                orch_adapter_t *adapter)
{
    if (!loop) return;
    loop->orch_adapter = adapter;
    AGENTRT_LOG_INFO("C-L06: Orchestrator adapter %s to CoreLoopThree",
                     adapter ? "attached" : "detached");
}

/* ================================================================
 * W18: taskflow_advanced DAG 工作流集成实现
 *
 * 设计说明：
 * - CoreLoopThree 内部持有 taskflow_engine_t*，在 loop_create 时创建、loop_destroy 时销毁
 * - agentrt_loop_submit_dag 接受结构化 taskflow_workflow_t，注册并启动执行
 * - agentrt_loop_dag_wait 阻塞直到执行完成，返回输出 JSON
 * - agentrt_loop_dag_status 查询执行状态与进度
 *
 * 注意：taskflow_engine_get_execution 返回内部指针（非副本），
 *       不可调用 taskflow_execution_destroy（会释放引擎内部字段导致 use-after-free），
 *       此处仅读取字段并复制。
 * ================================================================ */

AGENTRT_API agentrt_error_t agentrt_loop_dag_register_handler(
    agentrt_core_loop_t *loop, const char *name, taskflow_task_handler_t handler,
    void *user_data)
{
    if (!loop || !name || !handler)
        ATM_RET_ERR(AGENTRT_EINVAL);
    if (!loop->taskflow_engine)
        ATM_RET_ERR(AGENTRT_ENOTINIT);

    int rc = taskflow_engine_register_handler(loop->taskflow_engine, name, handler, user_data);
    if (rc != 0) {
        AGENTRT_LOG_ERROR("CoreLoopThree: dag_register_handler '%s' failed (rc=%d)", name, rc);
        return AGENTRT_EINVAL;
    }
    AGENTRT_LOG_INFO("CoreLoopThree: DAG handler '%s' registered", name);
    return AGENTRT_SUCCESS;
}

AGENTRT_API agentrt_error_t agentrt_loop_submit_dag(agentrt_core_loop_t *loop,
                                                     const taskflow_workflow_t *workflow,
                                                     const char *input_json,
                                                     char **out_execution_id)
{
    if (!loop || !workflow || !out_execution_id)
        ATM_RET_ERR(AGENTRT_EINVAL);
    if (!loop->taskflow_engine)
        ATM_RET_ERR(AGENTRT_ENOTINIT);

    *out_execution_id = NULL;

    /* 注册工作流定义（taskflow_engine_register_workflow 内部复制节点和边） */
    int rc = taskflow_engine_register_workflow(loop->taskflow_engine, workflow);
    if (rc != 0) {
        AGENTRT_LOG_ERROR("CoreLoopThree: dag_register_workflow '%s' failed (rc=%d)",
                         workflow->id, rc);
        return AGENTRT_EINVAL;
    }

    /* 启动工作流执行 */
    char *exec_id = NULL;
    rc = taskflow_engine_start(loop->taskflow_engine, workflow->id, input_json, &exec_id);
    if (rc != 0 || !exec_id) {
        AGENTRT_LOG_ERROR("CoreLoopThree: dag_start workflow '%s' failed (rc=%d)",
                         workflow->id, rc);
        return AGENTRT_EINVAL;
    }

    *out_execution_id = exec_id;
    AGENTRT_LOG_INFO("CoreLoopThree: DAG workflow '%s' submitted (exec_id=%s)",
                     workflow->id, exec_id);
    return AGENTRT_SUCCESS;
}

AGENTRT_API agentrt_error_t agentrt_loop_dag_wait(agentrt_core_loop_t *loop,
                                                   const char *execution_id,
                                                   uint32_t timeout_ms,
                                                   char **out_result_json)
{
    if (!loop || !execution_id)
        ATM_RET_ERR(AGENTRT_EINVAL);
    if (!loop->taskflow_engine)
        ATM_RET_ERR(AGENTRT_ENOTINIT);

    (void)timeout_ms;  /* taskflow_engine_run_to_completion 不支持超时参数，
                        * 0.1.1 版本阻塞等待完成；超时功能留待后续版本 */

    /* 先查询当前执行状态。taskflow_engine_start 对单节点工作流会同步执行 handler
     * 并将状态直接置为 COMPLETED/FAILED，此时 run_to_completion 会因 "state != RUNNING"
     * 返回 IO 错误。因此 dag_wait 需先检查状态：已完成（COMPLETED/FAILED/CANCELED/
     * SKIPPED）则直接获取结果返回，仍在 RUNNING/PENDING 才调用 run_to_completion。 */
    taskflow_execution_t *exec_peek = NULL;
    int rc = taskflow_engine_get_execution(loop->taskflow_engine, execution_id, &exec_peek);
    if (rc != 0 || !exec_peek) {
        AGENTRT_LOG_ERROR("CoreLoopThree: dag_wait '%s' execution not found (rc=%d)",
                         execution_id, rc);
        return AGENTRT_ENOENT;
    }

    taskflow_state_t cur_state = exec_peek->state;
    if (cur_state == TASKFLOW_STATE_RUNNING || cur_state == TASKFLOW_STATE_PENDING ||
        cur_state == TASKFLOW_STATE_READY || cur_state == TASKFLOW_STATE_WAITING ||
        cur_state == TASKFLOW_STATE_RETRYING) {
        /* 仍在执行中，阻塞等待完成 */
        rc = taskflow_engine_run_to_completion(loop->taskflow_engine, execution_id);
        if (rc != 0) {
            AGENTRT_LOG_ERROR("CoreLoopThree: dag_wait '%s' run_to_completion failed (rc=%d)",
                             execution_id, rc);
            return AGENTRT_EINVAL;
        }
    }

    /* 获取执行结果（get_execution 返回内部指针，仅读取不 destroy） */
    if (out_result_json) {
        *out_result_json = NULL;
        taskflow_execution_t *exec = NULL;
        rc = taskflow_engine_get_execution(loop->taskflow_engine, execution_id, &exec);
        if (rc == 0 && exec) {
            *out_result_json = AGENTRT_STRDUP(exec->output_json ? exec->output_json : "{}");
        }
    }

    return AGENTRT_SUCCESS;
}

AGENTRT_API agentrt_error_t agentrt_loop_dag_status(agentrt_core_loop_t *loop,
                                                     const char *execution_id,
                                                     char **out_state,
                                                     double *out_progress)
{
    if (!loop || !execution_id)
        ATM_RET_ERR(AGENTRT_EINVAL);
    if (!loop->taskflow_engine)
        ATM_RET_ERR(AGENTRT_ENOTINIT);

    taskflow_execution_t *exec = NULL;
    int rc = taskflow_engine_get_execution(loop->taskflow_engine, execution_id, &exec);
    if (rc != 0 || !exec) {
        return AGENTRT_ENOENT;
    }

    /* get_execution 返回内部指针，仅读取字段不调用 taskflow_execution_destroy */
    if (out_state) {
        const char *state_str = "unknown";
        switch (exec->state) {
            case TASKFLOW_STATE_PENDING:   state_str = "pending";   break;
            case TASKFLOW_STATE_READY:     state_str = "ready";     break;
            case TASKFLOW_STATE_RUNNING:   state_str = "running";   break;
            case TASKFLOW_STATE_WAITING:   state_str = "waiting";   break;
            case TASKFLOW_STATE_COMPLETED: state_str = "completed"; break;
            case TASKFLOW_STATE_FAILED:    state_str = "failed";    break;
            case TASKFLOW_STATE_CANCELED:  state_str = "canceled";  break;
            case TASKFLOW_STATE_SKIPPED:   state_str = "skipped";   break;
            case TASKFLOW_STATE_RETRYING:  state_str = "retrying";  break;
        }
        *out_state = AGENTRT_STRDUP(state_str);
    }

    if (out_progress) {
        *out_progress = exec->progress;
    }

    return AGENTRT_SUCCESS;
}

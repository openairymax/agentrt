/**
 * @file loop.c
 * @brief 三层核心运行时主循环实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "loop.h"

#include "agentos.h"
#include "agentos_dirent.h"
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
    do { agentos_error_push_ex((c), __FILE__, __LINE__, __func__, "%s", agentos_error_str(c)); return (c); } while(0)


/**
 * @brief 核心循环结构体
 */
struct agentos_core_loop {
    agentos_cognition_engine_t *cognition;
    agentos_execution_engine_t *execution;
    agentos_memory_engine_t *memory;
    agentos_loop_config_t manager;
    atomic_int running;
    atomic_int stop_requested;
    atomic_int task_pending;
    agentos_mutex_t *lock;
    agentos_cond_t *cond;

    /* P1.19.4: Arena 分配器用于请求处理路径中的短生命周期分配 */
    agentos_arena_t *arena;

    /* C-L02: llm_d → CoreLoopThree — IPC adapter for LLM requests */
    llm_svc_adapter_t *llm_adapter;

    /* C-L04: tool_d → CoreLoopThree — IPC adapter for tool execution */
    tool_svc_adapter_t *tool_adapter;

    /* C-L12: CoreLoopThree → MemoryRovol — 内存提供商桥接 */
    memoryrovol_bridge_t *memory_bridge;

    /* C-L01: Manager → CoreLoopThree — 配置管理适配器 */
    agentos_manager_adapter_t *manager_adapter;

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
};

/* 辅助函数声明 - 用于重构降低圈复杂度 */
static agentos_error_t validate_loop_parameters(const agentos_loop_config_t *manager,
                                                agentos_core_loop_t **out_loop);
static agentos_core_loop_t *allocate_loop_memory(void);
static agentos_error_t initialize_loop_resources(agentos_core_loop_t *loop,
                                                 const agentos_loop_config_t *manager);
static agentos_error_t create_loop_engines(agentos_core_loop_t *loop);
static void cleanup_loop_resources(agentos_core_loop_t *loop);
static void free_loop_memory(agentos_core_loop_t *loop);

/* 提取的辅助函数 - 降低 agentos_loop_submit 圈复杂度 */
static char *build_enhanced_input(const char *input, size_t input_len,
                                  agentos_memory_record_t **records, size_t record_count,
                                  size_t max_memories);
static void free_memories(agentos_memory_record_t **records, size_t record_count);
static void loop_checkpoint_auto_hook(const char *task_id, const char *state_json, void *user_data);
static void add_completed_node(agentos_core_loop_t *loop, const char *node_id);
static void clear_completed_nodes(agentos_core_loop_t *loop);
static agentos_error_t save_incremental_checkpoint(agentos_core_loop_t *loop, const char *task_id,
                                                   const char *session_id, const char *node_id);
static void add_tool_call_history(agentos_core_loop_t *loop, const char *tool_name, const char *tool_input);
static void clear_tool_call_history(agentos_core_loop_t *loop);
static char *build_rich_checkpoint_state(agentos_core_loop_t *loop, const char *task_id,
                                         const char *extra_json);

/* 默认配置 */
static void init_default_config(agentos_loop_config_t *manager)
{
    __builtin_memset(manager, 0, sizeof(agentos_loop_config_t));
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
 * @return 错误码，成功返回 AGENTOS_SUCCESS
 */
static agentos_error_t validate_loop_parameters(const agentos_loop_config_t *manager,
                                                agentos_core_loop_t **out_loop)
{
    CHECK_NULL(out_loop);

    if (manager) {
        if (manager->loop_config_cognition_threads > 1024 ||
            manager->loop_config_execution_threads > 1024 ||
            manager->loop_config_memory_threads > 1024) {
            AGENTOS_ERROR(AGENTOS_EINVAL, "failed to validate loop config: thread count exceeds max 1024");
        }

        if (manager->loop_config_max_queued_tasks == 0 ||
            manager->loop_config_max_queued_tasks > 100000) {
            AGENTOS_ERROR(AGENTOS_EINVAL, "failed to validate loop config: max_queued_tasks out of range");
        }

        if (manager->loop_config_stats_interval_ms > 3600000) {
            AGENTOS_ERROR(AGENTOS_EINVAL, "failed to validate loop config: stats_interval_ms exceeds 1 hour");
        }
    }

    return AGENTOS_SUCCESS;
}

static agentos_core_loop_t *allocate_loop_memory(void)
{
    agentos_core_loop_t *loop =
        (agentos_core_loop_t *)AGENTOS_CALLOC(1, sizeof(agentos_core_loop_t));
    if (loop) {
        __builtin_memset(loop, 0, sizeof(agentos_core_loop_t));
    }
    return loop;
}

/**
 * @brief 初始化循环资源（互斥锁和条件变量）
 * @param loop 循环结构体指针
 * @param manager 配置参数指针（可为 NULL）
 * @return 错误码，成功返回 AGENTOS_SUCCESS
 */
static agentos_error_t initialize_loop_resources(agentos_core_loop_t *loop,
                                                 const agentos_loop_config_t *manager)
{
    if (manager) {
        __builtin_memcpy(&loop->manager, manager, sizeof(agentos_loop_config_t));
    } else {
        init_default_config(&loop->manager);
    }

    loop->lock = agentos_mutex_create();
    if (!loop->lock)
        ATM_RET_ERR(AGENTOS_ENOMEM);

    loop->cond = agentos_cond_create();
    if (!loop->cond) {
        agentos_mutex_free(loop->lock);
        ATM_RET_ERR(AGENTOS_ENOMEM);
    }

    return AGENTOS_SUCCESS;
}

/**
 * @brief 创建循环引擎（认知、执行、记忆）
 * @param loop 循环结构体指针
 * @return 错误码，成功返回 AGENTOS_SUCCESS
 */
static agentos_error_t create_loop_engines(agentos_core_loop_t *loop)
{
    agentos_error_t err;

    err = agentos_cognition_create_ex_take(NULL, loop->manager.loop_config_plan_strategy,
                                      loop->manager.loop_config_coord_strategy,
                                      loop->manager.loop_config_disp_strategy, &loop->cognition);

    if (err != AGENTOS_SUCCESS)
        return err;

    err = agentos_execution_create(loop->manager.loop_config_execution_threads > 0
                                       ? loop->manager.loop_config_execution_threads
                                       : 8,
                                   &loop->execution);

    if (err != AGENTOS_SUCCESS)
        return err;

    err = agentos_memory_create(NULL, &loop->memory);
    if (err != AGENTOS_SUCCESS)
        return err;

    agentos_cognition_set_memory(loop->cognition, loop->memory);

    /* C-L02: Create and wire LLM IPC adapter if configured */
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
            agentos_cognition_set_llm_adapter(loop->cognition, loop->llm_adapter);
            AGENTOS_LOG_INFO("C-L02: LLM IPC adapter wired to cognition engine");
        } else {
            AGENTOS_LOG_WARN("C-L02: Failed to create LLM IPC adapter, "
                             "cognition will use direct LLM service if available");
        }
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
            agentos_cognition_set_tool_adapter(loop->cognition, loop->tool_adapter);
            AGENTOS_LOG_INFO("C-L04: Tool IPC adapter wired to cognition engine"
                             " (approval=%s)",
                             tool_cfg.enable_approval ? "on" : "off");
        } else {
            AGENTOS_LOG_WARN("C-L04: Failed to create tool IPC adapter, "
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
            agentos_memory_provider_t *provider =
                memoryrovol_bridge_get_provider(loop->memory_bridge);
            if (provider) {
                agentos_cognition_set_memory_provider(loop->cognition, provider);
                AGENTOS_LOG_INFO("C-L12: MemoryRovol bridge wired to cognition engine "
                                 "(provider=%s v%s, L1=%d L2=%d L3=%d L4=%d)",
                                 provider->name ? provider->name : "?",
                                 provider->version ? provider->version : "?",
                                 provider->capabilities.l1_raw,
                                 provider->capabilities.l2_feature,
                                 provider->capabilities.l3_structure,
                                 provider->capabilities.l4_pattern);
            } else {
                AGENTOS_LOG_WARN("C-L12: Bridge created but provider unavailable");
            }
        } else {
            AGENTOS_LOG_WARN("C-L12: Failed to create MemoryRovol bridge, "
                             "cognition will use builtin memory provider if available");
            loop->memory_bridge = NULL;
        }
    }

    return AGENTOS_SUCCESS;
}

/**
 * @brief 释放循环结构体内存（支持内存池和传统分配）
 * @param loop 循环结构体指针
 */
static void free_loop_memory(agentos_core_loop_t *loop)
{
    if (loop == NULL) {
        return;
    }

    AGENTOS_FREE(loop);
}

/**
 * @brief 清理循环资源（反向释放所有资源）
 * @param loop 循环结构体指针
 */
static void cleanup_loop_resources(agentos_core_loop_t *loop)
{
    if (!loop)
        return;

    if (loop->memory) {
        agentos_memory_destroy(loop->memory);
        loop->memory = NULL;
    }

    if (loop->execution) {
        agentos_execution_destroy(loop->execution);
        loop->execution = NULL;
    }

    if (loop->cognition) {
        agentos_cognition_destroy(loop->cognition);
        loop->cognition = NULL;
    }

    /* C-L02: Clean up LLM IPC adapter */
    if (loop->llm_adapter) {
        llm_svc_adapter_destroy(loop->llm_adapter);
        loop->llm_adapter = NULL;
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
        agentos_cond_free(loop->cond);
        loop->cond = NULL;
    }

    if (loop->lock) {
        agentos_mutex_free(loop->lock);
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
static void free_memories(agentos_memory_record_t **records, size_t record_count)
{
    if (!records)
        return;
    AGENTOS_FREE(records);
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
                                  agentos_memory_record_t **records, size_t record_count,
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
    char *enhanced_input = (char *)AGENTOS_ARENA_ALLOC(total_len);
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

AGENTOS_API agentos_error_t agentos_loop_create(const agentos_loop_config_t *manager,
                                                agentos_core_loop_t **out_loop)
{
    agentos_error_t err;
    agentos_core_loop_t *loop = NULL;

    AGENTOS_LOG_INFO("CoreLoopThree: agentos_loop_create START");

    err = validate_loop_parameters(manager, out_loop);
    if (err != AGENTOS_SUCCESS)
        return err;

    loop = allocate_loop_memory();
    if (!loop)
        ATM_RET_ERR(AGENTOS_ENOMEM);

    err = initialize_loop_resources(loop, manager);
    if (err != AGENTOS_SUCCESS) {
        cleanup_loop_resources(loop);
        return err;
    }

    err = create_loop_engines(loop);
    if (err != AGENTOS_SUCCESS) {
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
        AGENTOS_LOG_WARN("CoreLoopThree: Arena creation failed, falling back to malloc");
    } else {
        AGENTOS_LOG_INFO("CoreLoopThree: Arena created (chunk_size=%zu) for request-level allocations",
                         (size_t)ARENA_DEFAULT_CHUNK_SIZE);
    }

    if (loop->manager.loop_config_checkpoint_enabled) {
        const char *cp_path = loop->manager.loop_config_checkpoint_path;
        if (cp_path[0] == '\0') {
            cp_path = "./data/checkpoints";
        }
        agentos_error_t cp_err = agentos_checkpoint_init(cp_path);
        if (cp_err == AGENTOS_SUCCESS) {
            loop->checkpoint_initialized = 1;
            agentos_checkpoint_set_auto_hook(loop_checkpoint_auto_hook, loop,
                                             loop->manager.loop_config_checkpoint_interval_ms);
            AGENTOS_LOG_INFO("Checkpoint subsystem initialized: %s", cp_path);
        } else {
            AGENTOS_LOG_WARN("Checkpoint init failed (err=%d), persistence disabled", cp_err);
        }
    }

    *out_loop = loop;
    return AGENTOS_SUCCESS;
}

AGENTOS_API void agentos_loop_destroy(agentos_core_loop_t *loop)
{
    if (!loop)
        return;

    if (loop->running) {
        agentos_loop_stop(loop);
    }

    if (loop->checkpoint_initialized) {
        agentos_checkpoint_cleanup(86400, 100);
        agentos_checkpoint_shutdown();
        loop->checkpoint_initialized = 0;
    }

    clear_completed_nodes(loop);

    if (loop->persistent_original_input) {
        AGENTOS_FREE(loop->persistent_original_input);
        loop->persistent_original_input = NULL;
    }

    /* P1.6.2: 清理工具调用历史 */
    if (loop->tool_call_history) {
        for (size_t i = 0; i < loop->tool_call_history_count; i++) {
            AGENTOS_FREE(loop->tool_call_history[i]);
        }
        AGENTOS_FREE(loop->tool_call_history);
        loop->tool_call_history = NULL;
        loop->tool_call_history_count = 0;
        loop->tool_call_history_capacity = 0;
    }

    /* P1.19.4: 销毁 Arena 分配器 */
    if (loop->arena) {
        arena_stats_t astats;
        arena_get_stats(loop->arena, &astats);
        AGENTOS_LOG_INFO("CoreLoopThree: Arena destroy (allocs=%" PRIu64 ", chunks=%zu, total=%zu, reset=%" PRIu64 ")",
                         astats.alloc_count, astats.chunk_count, astats.total_chunk_bytes, astats.reset_count);
        arena_destroy(loop->arena);
        loop->arena = NULL;
    }

    if (loop->memory) {
        agentos_memory_destroy(loop->memory);
        loop->memory = NULL;
    }
    if (loop->execution) {
        agentos_execution_destroy(loop->execution);
        loop->execution = NULL;
    }
    if (loop->cognition) {
        agentos_cognition_destroy(loop->cognition);
        loop->cognition = NULL;
    }
    if (loop->cond) {
        agentos_cond_free(loop->cond);
        loop->cond = NULL;
    }
    if (loop->lock) {
        agentos_mutex_free(loop->lock);
        loop->lock = NULL;
    }

    free_loop_memory(loop);
}

AGENTOS_API agentos_error_t agentos_loop_run(agentos_core_loop_t *loop)
{
    CHECK_NULL(loop);

    agentos_mutex_lock(loop->lock);
    loop->running = 1;
    loop->stop_requested = 0;
    agentos_mutex_unlock(loop->lock);

    AGENTOS_LOG_INFO("CoreLoopThree: agentos_loop_run STARTED");

    uint64_t last_auto_checkpoint_ms = 0;
    uint32_t checkpoint_interval = loop->manager.loop_config_checkpoint_interval_ms;
    if (checkpoint_interval == 0)
        checkpoint_interval = 30000;

    uint64_t last_stats_log_ms = 0;
    uint64_t last_bridge_stats_ms = 0; /* C-L12: bridge stats timer */
    uint64_t last_manager_stats_ms = 0; /* C-L01: manager adapter stats timer */
    uint64_t last_orch_stats_ms = 0;    /* C-L06: orchestrator adapter stats timer */

    while (1) {
        agentos_mutex_lock(loop->lock);
        while (!loop->stop_requested && !loop->task_pending) {
            agentos_cond_timedwait(loop->cond, loop->lock, 50);
        }
        if (loop->stop_requested) {
            loop->running = 0;
            loop->task_pending = 0;
            agentos_cond_broadcast(loop->cond);
            agentos_mutex_unlock(loop->lock);
            AGENTOS_LOG_INFO("CoreLoopThree: agentos_loop_run STOPPED (total_turns=%" PRIu64 ")",
                             loop->loop_turn_count);
            break;
        }
        if (loop->task_pending) {
            loop->task_pending = 0;
            AGENTOS_LOG_DEBUG("CoreLoopThree: task pending flag consumed (turn=%" PRIu64 ")",
                              loop->loop_turn_count);
        }
        agentos_mutex_unlock(loop->lock);

        /* P1.6.1: 时间基检查点触发 */
        if (loop->checkpoint_initialized && loop->current_task_id[0] != '\0' &&
            checkpoint_interval > 0) {
            uint64_t now_ms = agentos_time_ms();
            if (last_auto_checkpoint_ms == 0 ||
                (now_ms - last_auto_checkpoint_ms) >= checkpoint_interval) {
                agentos_error_t cp_err = agentos_checkpoint_trigger_auto(loop->current_task_id);
                if (cp_err == AGENTOS_SUCCESS) {
                    last_auto_checkpoint_ms = now_ms;
                    AGENTOS_LOG_DEBUG("CoreLoopThree: time-based auto-checkpoint triggered "
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
                agentos_error_t cp_err = agentos_checkpoint_trigger_auto(loop->current_task_id);
                if (cp_err == AGENTOS_SUCCESS) {
                    loop->loop_checkpoint_last_turn = loop->loop_turn_count;
                    AGENTOS_LOG_INFO("CoreLoopThree: turn-based checkpoint triggered "
                                     "(interval=%u turns, total_turns=%" PRIu64 ")",
                                     interval_turns, loop->loop_turn_count);
                }
            }
        }

        /* P1.6: 定期输出 checkpoint 统计信息 */
        if (loop->checkpoint_initialized && loop->manager.loop_config_stats_interval_ms > 0) {
            uint64_t now_ms = agentos_time_ms();
            if (last_stats_log_ms == 0 ||
                (now_ms - last_stats_log_ms) >= loop->manager.loop_config_stats_interval_ms) {
                agentos_checkpoint_stats_t stats;
                if (agentos_checkpoint_get_stats(&stats) == AGENTOS_SUCCESS) {
                    AGENTOS_LOG_INFO("CoreLoopThree: checkpoint stats "
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
            uint64_t now_ms = agentos_time_ms();
            if (last_bridge_stats_ms == 0 ||
                (now_ms - last_bridge_stats_ms) >= loop->manager.loop_config_stats_interval_ms) {
                memoryrovol_bridge_dump_stats(loop->memory_bridge);
                last_bridge_stats_ms = now_ms;
            }
        }

        /* C-L01: 定期输出 Manager 适配器统计 */
        if (loop->manager_adapter && loop->manager.loop_config_stats_interval_ms > 0) {
            uint64_t now_ms = agentos_time_ms();
            if (last_manager_stats_ms == 0 ||
                (now_ms - last_manager_stats_ms) >= loop->manager.loop_config_stats_interval_ms) {
                manager_adapter_dump_stats(loop->manager_adapter);
                last_manager_stats_ms = now_ms;
            }
        }

        /* C-L06: 定期输出 Orchestrator 适配器统计 */
        if (loop->orch_adapter && loop->manager.loop_config_stats_interval_ms > 0) {
            uint64_t now_ms = agentos_time_ms();
            if (last_orch_stats_ms == 0 ||
                (now_ms - last_orch_stats_ms) >= loop->manager.loop_config_stats_interval_ms) {
                orch_adapter_dump_stats(loop->orch_adapter);
                last_orch_stats_ms = now_ms;
            }
        }
    }

    return AGENTOS_SUCCESS;
}

AGENTOS_API void agentos_loop_stop(agentos_core_loop_t *loop)
{
    if (!loop)
        return;

    agentos_mutex_lock(loop->lock);
    loop->stop_requested = 1;
    while (loop->running) {
        agentos_cond_wait(loop->cond, loop->lock);
    }
    agentos_mutex_unlock(loop->lock);
}

AGENTOS_API agentos_error_t agentos_loop_submit(agentos_core_loop_t *loop, const char *input,
                                                size_t input_len, char **out_task_id)
{
    if (!loop || !input || !out_task_id)
        AGENTOS_ERROR(AGENTOS_EINVAL, "failed to submit loop task: null loop, input, or out_task_id");
    if (!loop->cognition || !loop->execution || !loop->memory)
        ATM_RET_ERR(AGENTOS_ENOTINIT);

    AGENTOS_LOG_INFO("CoreLoopThree: agentos_loop_submit START (input_len=%zu)", input_len);

    /* P1.6.1: 递增轮次计数器 */
    agentos_mutex_lock(loop->lock);
    loop->loop_turn_count++;
    agentos_mutex_unlock(loop->lock);

    /* P1.19.4: 设置当前线程的 Arena 用于请求级短生命周期分配 */
    agentos_arena_t *prev_arena = agentos_arena_get_current();
    if (loop->arena) {
        agentos_arena_set_current(loop->arena);
    }

    /* 步骤 1: 从记忆中检索相关上下文 */
    agentos_memory_query_t query = {0};
    query.memory_query_text = (char *)input;
    query.memory_query_text_len = input_len;
    query.memory_query_limit = 5;
    query.memory_query_include_raw = 1;
    agentos_memory_result_ext_t *result = NULL;
    agentos_error_t err = agentos_memory_query(loop->memory, &query, &result);

    size_t memory_count = 0;
    agentos_memory_record_t **memories = NULL;
    if (err == AGENTOS_SUCCESS && result && result->memory_result_count > 0) {
        memory_count = result->memory_result_count;
        /* P1.19.4: 使用 Arena 分配短生命周期 memories 数组 */
        memories = (agentos_memory_record_t **)AGENTOS_ARENA_ALLOC(memory_count *
                                                              sizeof(agentos_memory_record_t *));
        if (memories) {
            for (size_t i = 0; i < memory_count; i++) {
                memories[i] = result->memory_result_items[i]->memory_result_item_record;
            }
            AGENTOS_LOG_DEBUG("CoreLoopThree: memory query returned %zu records", memory_count);
        } else {
            memory_count = 0;
        }
    }

    /* 步骤 2: 构建增强输入（如果有相关记忆） */
    char *enhanced_input = NULL;
    if (err == AGENTOS_SUCCESS && memory_count > 0) {
        enhanced_input = build_enhanced_input(input, input_len, memories, memory_count,
                                              loop->manager.loop_config_memory_query_limit);
    }

    /* 释放记忆结果（无论是否构建了增强输入） */
    if (result)
        agentos_memory_result_free(result);
    /* memories 数组由 Arena 管理，无需单独释放 */

    /* 步骤 3: 认知层处理（带上下文增强） */
    const char *process_input = enhanced_input ? enhanced_input : input;
    size_t process_len = enhanced_input ? strlen(enhanced_input) : input_len;

    agentos_task_plan_t *plan = NULL;
    err = agentos_cognition_process(loop->cognition, process_input, process_len, &plan);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_LOG_WARN("CoreLoopThree: cognition process FAILED (err=%d)", err);
        goto submit_cleanup;
    }

    if (!plan || plan->task_plan_node_count == 0) {
        agentos_task_plan_free(plan);
        AGENTOS_LOG_WARN("CoreLoopThree: empty or null task plan");
        AGENTOS_ERROR(AGENTOS_EINVAL, "failed to submit loop task: empty or null task plan");
        /* 不会到达这里，AGENTOS_ERROR 会 return */
    }

    AGENTOS_LOG_INFO("CoreLoopThree: plan created (nodes=%zu)", plan->task_plan_node_count);

    /* 步骤 4: 执行层按计划节点提交任务 */
    agentos_error_t first_err = AGENTOS_SUCCESS;
    char *saved_task_id = NULL;
    for (size_t i = 0; i < plan->task_plan_node_count; i++) {
        agentos_task_node_t *node = plan->task_plan_nodes[i];
        if (!node)
            continue;

        agentos_task_t task;
        __builtin_memset(&task, 0, sizeof(task));
        task.task_input = node->task_node_input ? node->task_node_input : (void *)process_input;
        task.task_timeout_ms = node->task_node_timeout_ms > 0
                                   ? node->task_node_timeout_ms
                                   : loop->manager.loop_config_task_timeout_ms;

        char *node_task_id = NULL;
        err = agentos_execution_submit(loop->execution, &task, &node_task_id);
        if (err != AGENTOS_SUCCESS && first_err == AGENTOS_SUCCESS) {
            first_err = err;
        }
        if (i == 0 && node_task_id && !saved_task_id) {
            saved_task_id = AGENTOS_STRDUP(node_task_id);
        }
        if (node_task_id)
            AGENTOS_FREE(node_task_id);
    }

    if (first_err == AGENTOS_SUCCESS && out_task_id) {
        *out_task_id = saved_task_id ? saved_task_id : AGENTOS_STRDUP("task-unknown");
    }

    if (first_err == AGENTOS_SUCCESS) {
        agentos_mutex_lock(loop->lock);
        loop->task_pending = 1;
        agentos_cond_signal(loop->cond);
        agentos_mutex_unlock(loop->lock);
        AGENTOS_LOG_INFO("CoreLoopThree: agentos_loop_submit OK (task_id=%s, nodes=%zu)",
                         saved_task_id ? saved_task_id : "task-unknown",
                         plan->task_plan_node_count);
    } else {
        AGENTOS_LOG_WARN("CoreLoopThree: agentos_loop_submit FAILED (err=%d)", first_err);
    }

    /* 清理临时资源 */
    agentos_task_plan_free(plan);

submit_cleanup:
    /* P1.19.4: 重置 Arena，释放所有请求级短生命周期内存 */
    if (loop->arena) {
        arena_reset(loop->arena);
    }
    /* 恢复之前的 Arena */
    agentos_arena_set_current(prev_arena);

    return first_err;
}

AGENTOS_API agentos_error_t agentos_loop_wait(agentos_core_loop_t *loop, const char *task_id,
                                              uint32_t timeout_ms, char **out_result,
                                              size_t *out_result_len)
{
    if (!loop || !task_id || !out_result || !out_result_len)
        ATM_RET_ERR(AGENTOS_EINVAL);
    if (!loop->execution || !loop->memory)
        ATM_RET_ERR(AGENTOS_ENOTINIT);

    AGENTOS_LOG_DEBUG("CoreLoopThree: agentos_loop_wait (task_id=%s, timeout=%u)", task_id, timeout_ms);

    /* 等待执行完成 */
    agentos_task_t *result_task = NULL;
    agentos_error_t err =
        agentos_execution_wait(loop->execution, task_id, timeout_ms, &result_task);

    if (err == AGENTOS_SUCCESS && result_task) {
        if (result_task->task_output) {
            size_t len = 0;
            const char *output = (const char *)result_task->task_output;
            while (output[len] != '\0')
                len++;
            *out_result = AGENTOS_STRDUP(output);
            *out_result_len = len;
        } else {
            *out_result = AGENTOS_STRDUP("");
            *out_result_len = 0;
        }

        if (!*out_result) {
            agentos_task_free(result_task);
            ATM_RET_ERR(AGENTOS_ENOMEM);
        }

        if (*out_result_len > 0) {
            agentos_memory_record_t record = {0};
            record.memory_record_data = *out_result;
            record.memory_record_data_len = *out_result_len;
            record.memory_record_type = AGENTOS_MEMTYPE_TEXT;
            record.memory_record_importance = loop->manager.loop_config_memory_importance;
            char *new_record_id = NULL;
            agentos_error_t store_err = agentos_memory_write(loop->memory, &record, &new_record_id);
            if (new_record_id) {
                AGENTOS_FREE(new_record_id);
                new_record_id = NULL;
            }
            if (store_err != AGENTOS_SUCCESS) {
                AGENTOS_LOG_WARN("Failed to store execution result to memory: %d", store_err);
            } else {
                AGENTOS_LOG_INFO("Successfully stored execution result to memory");
            }
        }

        agentos_task_free(result_task);

        if (!*out_result)
            ATM_RET_ERR(AGENTOS_ENOMEM);
    }

    return err;
}

AGENTOS_API void agentos_loop_get_engines(agentos_core_loop_t *loop,
                                          agentos_cognition_engine_t **out_cognition,
                                          agentos_execution_engine_t **out_execution,
                                          agentos_memory_engine_t **out_memory)
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
    return agentos_time_ms();
}

static void generate_task_id(char *buf, size_t buf_size)
{
    snprintf(buf, buf_size, "task-%016lx", (unsigned long)(agentos_time_ns() & 0xFFFFFFFF));
}

static void loop_checkpoint_auto_hook(const char *task_id, const char *state_json, void *user_data)
{
    agentos_core_loop_t *loop = (agentos_core_loop_t *)user_data;
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
            AGENTOS_FREE(rich_state);
        } else {
            /* 回退到简单状态 */
            snprintf(state, sizeof(state),
                     "{\"state\":\"auto\",\"task_id\":\"%s\",\"session_id\":\"%s\","
                     "\"turn_count\":%" PRIu64 "}",
                     task_id, loop->current_session_id, loop->loop_turn_count);
        }
    }

    loop->checkpoint_seq++;
    agentos_task_checkpoint_t *checkpoint = NULL;
    agentos_error_t err = agentos_checkpoint_create(
        task_id, loop->current_session_id, loop->checkpoint_seq, state, loop->completed_node_ids,
        loop->completed_node_count, NULL, 0, &checkpoint);
    if (err == AGENTOS_SUCCESS && checkpoint) {
        err = agentos_checkpoint_save(checkpoint);
        if (err == AGENTOS_SUCCESS) {
            AGENTOS_LOG_INFO("Auto-checkpoint saved for task %s (seq=%lu, completed=%zu, "
                             "turns=%" PRIu64 ", tools=%zu)",
                             task_id, (unsigned long)loop->checkpoint_seq,
                             loop->completed_node_count, loop->loop_turn_count,
                             loop->tool_call_history_count);
        } else {
            AGENTOS_LOG_WARN("Auto-checkpoint save FAILED for task %s (err=%d)", task_id, err);
        }
        agentos_checkpoint_destroy(checkpoint);
    }
    loop->last_checkpoint_time_ms = get_time_ms();
}

static agentos_error_t save_plan_checkpoint(agentos_core_loop_t *loop,
                                            const agentos_task_plan_t *plan, const char *task_id,
                                            const char *session_id, const char *original_input)
{
    if (!loop->checkpoint_initialized)
        ATM_RET_ERR(AGENTOS_ENOTINIT);
    if (!plan || !task_id)
        AGENTOS_ERROR(AGENTOS_EINVAL, "failed to save plan checkpoint: null plan or task_id");

    size_t completed_count = 0;
    size_t pending_count = plan->task_plan_node_count;
    char **completed_nodes = NULL;
    char **pending_nodes = NULL;

    if (pending_count > 0) {
        /* P1.19.4: 使用 Arena 分配短生命周期 pending_nodes 数组 */
        pending_nodes = (char **)AGENTOS_ARENA_ALLOC(pending_count * sizeof(char *));
        if (!pending_nodes)
            ATM_RET_ERR(AGENTOS_ENOMEM);
        for (size_t i = 0; i < pending_count; i++) {
            if (plan->task_plan_nodes[i] && plan->task_plan_nodes[i]->task_node_id) {
                /* STRDUP 用于 checkpoint 持久化，不能使用 Arena（reset 后会失效） */
                pending_nodes[i] = AGENTOS_STRDUP(plan->task_plan_nodes[i]->task_node_id);
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
                AGENTOS_FREE(pending_nodes[i]);
            AGENTOS_FREE(pending_nodes);
        }
        ATM_RET_ERR(AGENTOS_EOVERFLOW);
    }

    loop->checkpoint_seq++;
    agentos_task_checkpoint_t *checkpoint = NULL;
    agentos_error_t err = agentos_checkpoint_create(
        task_id, session_id ? session_id : "default", loop->checkpoint_seq, state_json,
        completed_nodes, completed_count, pending_nodes, pending_count, &checkpoint);

    if (pending_nodes) {
        for (size_t i = 0; i < pending_count; i++)
            AGENTOS_FREE(pending_nodes[i]);
        AGENTOS_FREE(pending_nodes);
        pending_nodes = NULL;
    }

    if (err != AGENTOS_SUCCESS || !checkpoint) {
        AGENTOS_LOG_WARN("Failed to create checkpoint for task %s: %d", task_id, err);
        return err;
    }

    err = agentos_checkpoint_save(checkpoint);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_LOG_WARN("Failed to save checkpoint for task %s: %d", task_id, err);
    } else {
        AGENTOS_LOG_INFO("Checkpoint saved for task %s (seq=%lu)", task_id,
                         (unsigned long)loop->checkpoint_seq);
    }

    agentos_checkpoint_destroy(checkpoint);
    loop->last_checkpoint_time_ms = get_time_ms();
    return err;
}

AGENTOS_API agentos_error_t agentos_loop_submit_persistent(agentos_core_loop_t *loop,
                                                           const char *input, size_t input_len,
                                                           const char *session_id,
                                                           char **out_task_id)
{
    if (!loop || !input || !out_task_id)
        ATM_RET_ERR(AGENTOS_EINVAL);
    if (!loop->cognition || !loop->execution || !loop->memory)
        ATM_RET_ERR(AGENTOS_ENOTINIT);

    AGENTOS_LOG_INFO("CoreLoopThree: agentos_loop_submit_persistent START (input_len=%zu, session=%s)",
                     input_len, session_id ? session_id : "auto");

    /* P1.6.1: 递增轮次计数器 */
    agentos_mutex_lock(loop->lock);
    loop->loop_turn_count++;
    agentos_mutex_unlock(loop->lock);

    char task_id_buf[128];
    generate_task_id(task_id_buf, sizeof(task_id_buf));

    agentos_mutex_lock(loop->lock);
    snprintf(loop->current_task_id, sizeof(loop->current_task_id), "%s", task_id_buf);
    if (session_id) {
        snprintf(loop->current_session_id, sizeof(loop->current_session_id), "%s", session_id);
    } else {
        snprintf(loop->current_session_id, sizeof(loop->current_session_id), "sess-%016lx",
                 (unsigned long)(agentos_time_ns() & 0xFFFFFFFF));
    }
    loop->checkpoint_seq = 0;
    clear_completed_nodes(loop);
    clear_tool_call_history(loop);
    agentos_mutex_unlock(loop->lock);

    /* P1.19.4: 设置当前线程的 Arena 用于请求级短生命周期分配 */
    agentos_arena_t *prev_arena = agentos_arena_get_current();
    if (loop->arena) {
        agentos_arena_set_current(loop->arena);
    }

    agentos_memory_query_t query = {0};
    query.memory_query_text = (char *)input;
    query.memory_query_text_len = input_len;
    query.memory_query_limit = 5;
    query.memory_query_include_raw = 1;
    agentos_memory_result_ext_t *result = NULL;
    agentos_error_t err = agentos_memory_query(loop->memory, &query, &result);

    size_t memory_count = 0;
    agentos_memory_record_t **memories = NULL;
    if (err == AGENTOS_SUCCESS && result && result->memory_result_count > 0) {
        memory_count = result->memory_result_count;
        /* P1.19.4: 使用 Arena 分配短生命周期 memories 数组 */
        memories = (agentos_memory_record_t **)AGENTOS_ARENA_ALLOC(memory_count *
                                                              sizeof(agentos_memory_record_t *));
        if (memories) {
            for (size_t i = 0; i < memory_count; i++) {
                memories[i] = result->memory_result_items[i]->memory_result_item_record;
            }
        } else {
            memory_count = 0;
        }
    }

    char *enhanced_input = NULL;
    if (err == AGENTOS_SUCCESS && memory_count > 0) {
        enhanced_input = build_enhanced_input(input, input_len, memories, memory_count,
                                              loop->manager.loop_config_memory_query_limit);
    }

    if (result)
        agentos_memory_result_free(result);
    /* memories 数组由 Arena 管理，无需单独释放 */

    const char *process_input = enhanced_input ? enhanced_input : input;
    size_t process_len = enhanced_input ? strlen(enhanced_input) : input_len;

    agentos_task_plan_t *plan = NULL;
    err = agentos_cognition_process(loop->cognition, process_input, process_len, &plan);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_LOG_WARN("CoreLoopThree: persistent cognition process FAILED (err=%d)", err);
        goto persistent_cleanup;
    }

    if (!plan || plan->task_plan_node_count == 0) {
        agentos_task_plan_free(plan);
        AGENTOS_LOG_WARN("CoreLoopThree: persistent empty or null task plan");
        goto persistent_cleanup;
    }

    AGENTOS_LOG_INFO("CoreLoopThree: persistent plan created (nodes=%zu)", plan->task_plan_node_count);

    if (loop->checkpoint_initialized) {
        save_plan_checkpoint(loop, plan, task_id_buf, loop->current_session_id, process_input);

        if (loop->persistent_original_input) {
            AGENTOS_FREE(loop->persistent_original_input);
        }
        loop->persistent_original_input = AGENTOS_STRDUP(process_input);
    }

    agentos_error_t first_err = AGENTOS_SUCCESS;
    for (size_t i = 0; i < plan->task_plan_node_count; i++) {
        agentos_task_node_t *node = plan->task_plan_nodes[i];
        if (!node)
            continue;

        agentos_task_t task;
        __builtin_memset(&task, 0, sizeof(task));
        task.task_input = node->task_node_input ? node->task_node_input : (void *)process_input;
        task.task_timeout_ms = node->task_node_timeout_ms > 0
                                   ? node->task_node_timeout_ms
                                   : loop->manager.loop_config_task_timeout_ms;

        char *node_task_id = NULL;
        err = agentos_execution_submit(loop->execution, &task, &node_task_id);
        if (err != AGENTOS_SUCCESS && first_err == AGENTOS_SUCCESS) {
            first_err = err;
        }
        if (node_task_id)
            AGENTOS_FREE(node_task_id);

        if (err == AGENTOS_SUCCESS && loop->checkpoint_initialized && node->task_node_id) {
            add_completed_node(loop, node->task_node_id);
            /* P1.6.2: 记录工具调用历史 */
            add_tool_call_history(loop, node->task_node_id,
                                  node->task_node_input ? (const char *)node->task_node_input : NULL);
            save_incremental_checkpoint(loop, task_id_buf, loop->current_session_id,
                                        node->task_node_id);
        }
    }

    if (first_err == AGENTOS_SUCCESS) {
        *out_task_id = AGENTOS_STRDUP(task_id_buf);
        if (!*out_task_id)
            first_err = AGENTOS_ENOMEM;

        agentos_mutex_lock(loop->lock);
        loop->task_pending = 1;
        agentos_cond_signal(loop->cond);
        agentos_mutex_unlock(loop->lock);
        AGENTOS_LOG_INFO("CoreLoopThree: agentos_loop_submit_persistent OK (task_id=%s, nodes=%zu)",
                         task_id_buf, plan->task_plan_node_count);
    } else {
        AGENTOS_LOG_WARN("CoreLoopThree: agentos_loop_submit_persistent FAILED (err=%d)", first_err);
    }

    agentos_task_plan_free(plan);

persistent_cleanup:
    /* P1.19.4: 重置 Arena，释放所有请求级短生命周期内存 */
    if (loop->arena) {
        arena_reset(loop->arena);
    }
    /* 恢复之前的 Arena */
    agentos_arena_set_current(prev_arena);

    return first_err;
}

AGENTOS_API agentos_error_t agentos_loop_restore_task(agentos_core_loop_t *loop,
                                                      const char *task_id,
                                                      char **out_restored_task_id)
{
    if (!loop || !task_id || !out_restored_task_id)
        ATM_RET_ERR(AGENTOS_EINVAL);
    if (!loop->checkpoint_initialized)
        ATM_RET_ERR(AGENTOS_ENOTINIT);
    if (!loop->cognition || !loop->execution)
        ATM_RET_ERR(AGENTOS_ENOTINIT);

    AGENTOS_LOG_INFO("CoreLoopThree: agentos_loop_restore_task (task_id=%s)", task_id);

    agentos_task_checkpoint_t **checkpoints = NULL;
    size_t cp_count = 0;
    agentos_error_t err = agentos_checkpoint_list(task_id, &checkpoints, &cp_count);
    if (err != AGENTOS_SUCCESS || cp_count == 0) {
        if (checkpoints) {
            for (size_t i = 0; i < cp_count; i++) {
                agentos_checkpoint_destroy(checkpoints[i]);
            }
            AGENTOS_FREE(checkpoints);
        }
        ATM_RET_ERR(AGENTOS_ENOENT);
    }

    agentos_task_checkpoint_t *latest = NULL;
    uint64_t latest_seq = 0;
    for (size_t i = 0; i < cp_count; i++) {
        if (checkpoints[i] && checkpoints[i]->sequence_num > latest_seq) {
            latest_seq = checkpoints[i]->sequence_num;
            latest = checkpoints[i];
        }
    }

    if (!latest || !latest->state_json) {
        for (size_t i = 0; i < cp_count; i++) {
            agentos_checkpoint_destroy(checkpoints[i]);
        }
        AGENTOS_FREE(checkpoints);
        ATM_RET_ERR(AGENTOS_ENOENT);
    }

    bool is_valid = false;
    agentos_checkpoint_verify(latest, &is_valid);
    if (!is_valid) {
        AGENTOS_LOG_WARN("Checkpoint for task %s seq %lu failed verification", task_id,
                         (unsigned long)latest_seq);
        for (size_t i = 0; i < cp_count; i++) {
            agentos_checkpoint_destroy(checkpoints[i]);
        }
        AGENTOS_FREE(checkpoints);
        ATM_RET_ERR(AGENTOS_EIO);
    }

    char restored_id[128];
    snprintf(restored_id, sizeof(restored_id), "task-%s-restored-%016lx", task_id,
             (unsigned long)(agentos_time_ns() & 0xFFFFFFFF));

    agentos_mutex_lock(loop->lock);
    snprintf(loop->current_task_id, sizeof(loop->current_task_id), "%s", restored_id);
    snprintf(loop->current_session_id, sizeof(loop->current_session_id), "%s", latest->session_id);
    loop->checkpoint_seq = latest->sequence_num;
    clear_completed_nodes(loop);
    for (size_t i = 0; i < latest->completed_count; i++) {
        if (latest->completed_nodes && latest->completed_nodes[i])
            add_completed_node(loop, latest->completed_nodes[i]);
    }
    agentos_mutex_unlock(loop->lock);

    char *recovered_input = NULL;
    if (loop->persistent_original_input) {
        AGENTOS_FREE(loop->persistent_original_input);
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
                recovered_input = (char *)AGENTOS_MALLOC(input_len + 1);
                if (recovered_input) {
                    __builtin_memcpy(recovered_input, input_pos, input_len);
                    recovered_input[input_len] = '\0';
                    loop->persistent_original_input = AGENTOS_STRDUP(recovered_input);
                }
            }
        }
    }

    for (size_t i = 0; i < latest->pending_count; i++) {
        if (!latest->pending_nodes || !latest->pending_nodes[i])
            continue;

        agentos_task_t task;
        __builtin_memset(&task, 0, sizeof(task));
        task.task_input =
            recovered_input ? (void *)recovered_input : (void *)latest->pending_nodes[i];
        task.task_timeout_ms = loop->manager.loop_config_task_timeout_ms;

        char *node_task_id = NULL;
        err = agentos_execution_submit(loop->execution, &task, &node_task_id);
        if (err != AGENTOS_SUCCESS) {
            AGENTOS_LOG_WARN("Failed to resubmit pending node %s: %d", latest->pending_nodes[i],
                             err);
        }
        if (node_task_id)
            AGENTOS_FREE(node_task_id);
    }

    if (recovered_input) {
        AGENTOS_FREE(recovered_input);
        recovered_input = NULL;
    }

    *out_restored_task_id = AGENTOS_STRDUP(restored_id);

    agentos_mutex_lock(loop->lock);
    loop->task_pending = 1;
    agentos_cond_signal(loop->cond);
    agentos_mutex_unlock(loop->lock);

    AGENTOS_LOG_INFO("Restored task %s from checkpoint seq %lu (%zu pending nodes)", task_id,
                     (unsigned long)latest_seq, latest->pending_count);

    for (size_t i = 0; i < cp_count; i++) {
        agentos_checkpoint_destroy(checkpoints[i]);
    }
    AGENTOS_FREE(checkpoints);

    return AGENTOS_SUCCESS;
}

AGENTOS_API agentos_error_t agentos_loop_list_checkpoints(agentos_core_loop_t *loop,
                                                          char ***out_task_ids, size_t *out_count)
{
    if (!loop || !out_task_ids || !out_count)
        ATM_RET_ERR(AGENTOS_EINVAL);
    if (!loop->checkpoint_initialized)
        ATM_RET_ERR(AGENTOS_ENOTINIT);

    *out_count = 0;
    *out_task_ids = NULL;

    const char *cp_dir = loop->manager.loop_config_checkpoint_path;
    if (cp_dir[0] == '\0')
        cp_dir = "./data/checkpoints";

    DIR *dir = opendir(cp_dir);
    if (!dir)
        return AGENTOS_SUCCESS;

    size_t capacity = 16;
    char **ids = (char **)AGENTOS_CALLOC(capacity, sizeof(char *));
    if (!ids) {
        closedir(dir);
        ATM_RET_ERR(AGENTOS_ENOMEM);
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
            char **new_ids = (char **)AGENTOS_REALLOC(ids, capacity * sizeof(char *));
            if (!new_ids) {
                for (size_t j = 0; j < count; j++)
                    AGENTOS_FREE(ids[j]);
                AGENTOS_FREE(ids);
                closedir(dir);
                ATM_RET_ERR(AGENTOS_ENOMEM);
            }
            ids = new_ids;
            __builtin_memset(ids + count, 0, (capacity - count) * sizeof(char *));
        }

        ids[count] = AGENTOS_STRDUP(tid);
        if (ids[count])
            count++;
    }
    closedir(dir);

    *out_task_ids = ids;
    *out_count = count;
    return AGENTOS_SUCCESS;
}

static void add_completed_node(agentos_core_loop_t *loop, const char *node_id)
{
    if (!loop || !node_id)
        return;

    if (loop->completed_node_count >= loop->completed_node_capacity) {
        size_t new_cap =
            loop->completed_node_capacity == 0 ? 16 : loop->completed_node_capacity * 2;
        char **new_arr =
            (char **)AGENTOS_REALLOC(loop->completed_node_ids, new_cap * sizeof(char *));
        if (!new_arr)
            return;
        loop->completed_node_ids = new_arr;
        __builtin_memset(loop->completed_node_ids + loop->completed_node_capacity, 0,
               (new_cap - loop->completed_node_capacity) * sizeof(char *));
        loop->completed_node_capacity = new_cap;
    }

    loop->completed_node_ids[loop->completed_node_count] = AGENTOS_STRDUP(node_id);
    if (loop->completed_node_ids[loop->completed_node_count])
        loop->completed_node_count++;
}

static void clear_completed_nodes(agentos_core_loop_t *loop)
{
    if (!loop)
        return;

    if (loop->completed_node_ids) {
        for (size_t i = 0; i < loop->completed_node_count; i++) {
            AGENTOS_FREE(loop->completed_node_ids[i]);
        }
        AGENTOS_FREE(loop->completed_node_ids);
        loop->completed_node_ids = NULL;
    }
    loop->completed_node_count = 0;
    loop->completed_node_capacity = 0;
}

static agentos_error_t save_incremental_checkpoint(agentos_core_loop_t *loop, const char *task_id,
                                                   const char *session_id, const char *node_id)
{
    if (!loop || !loop->checkpoint_initialized || !task_id)
        ATM_RET_ERR(AGENTOS_EINVAL);

    char state_json[8192];
    int json_len = snprintf(state_json, sizeof(state_json),
                            "{\"type\":\"incremental\",\"task_id\":\"%s\","
                            "\"session_id\":\"%s\",\"completed_node\":\"%s\","
                            "\"total_completed\":%zu}",
                            task_id, session_id ? session_id : "", node_id ? node_id : "",
                            loop->completed_node_count);

    if (json_len <= 0 || (size_t)json_len >= sizeof(state_json))
        ATM_RET_ERR(AGENTOS_EOVERFLOW);

    loop->checkpoint_seq++;
    agentos_task_checkpoint_t *checkpoint = NULL;
    agentos_error_t err = agentos_checkpoint_create(
        task_id, session_id ? session_id : "default", loop->checkpoint_seq, state_json,
        loop->completed_node_ids, loop->completed_node_count, NULL, 0, &checkpoint);

    if (err == AGENTOS_SUCCESS && checkpoint) {
        err = agentos_checkpoint_save(checkpoint);
        if (err == AGENTOS_SUCCESS) {
            AGENTOS_LOG_INFO(
                "Incremental checkpoint saved for task %s node %s (seq=%lu, completed=%zu)",
                task_id, node_id ? node_id : "", (unsigned long)loop->checkpoint_seq,
                loop->completed_node_count);
        }
        agentos_checkpoint_destroy(checkpoint);
    }

    loop->last_checkpoint_time_ms = get_time_ms();
    return err;
}

/* ==================== P1.6.2: 工具调用历史管理 ==================== */

static void add_tool_call_history(agentos_core_loop_t *loop, const char *tool_name, const char *tool_input)
{
    if (!loop || !tool_name)
        return;

    if (loop->tool_call_history_count >= loop->tool_call_history_capacity) {
        size_t new_cap =
            loop->tool_call_history_capacity == 0 ? 8 : loop->tool_call_history_capacity * 2;
        char **new_arr =
            (char **)AGENTOS_REALLOC(loop->tool_call_history, new_cap * sizeof(char *));
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

    loop->tool_call_history[loop->tool_call_history_count] = AGENTOS_STRDUP(entry);
    if (loop->tool_call_history[loop->tool_call_history_count])
        loop->tool_call_history_count++;

    AGENTOS_LOG_DEBUG("CoreLoopThree: tool call recorded: %s (total=%zu)",
                      tool_name, loop->tool_call_history_count);
}

static void clear_tool_call_history(agentos_core_loop_t *loop)
{
    if (!loop || !loop->tool_call_history)
        return;

    for (size_t i = 0; i < loop->tool_call_history_count; i++) {
        AGENTOS_FREE(loop->tool_call_history[i]);
    }
    AGENTOS_FREE(loop->tool_call_history);
    loop->tool_call_history = NULL;
    loop->tool_call_history_count = 0;
    loop->tool_call_history_capacity = 0;
}

/* ==================== P1.6.2: 丰富的 checkpoint 状态构建 ==================== */

static char *build_rich_checkpoint_state(agentos_core_loop_t *loop, const char *task_id,
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
    char *state = (char *)AGENTOS_MALLOC(state_size);
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
        AGENTOS_FREE(state);
        return NULL;
    }

    return state;
}

/* ==================== P1.6.3: 快照 API 实现 ==================== */

AGENTOS_API agentos_error_t agentos_loop_create_snapshot(agentos_core_loop_t *loop,
                                                         const char *task_id,
                                                         const char *snapshot_path)
{
    if (!loop || !task_id || !snapshot_path)
        ATM_RET_ERR(AGENTOS_EINVAL);
    if (!loop->checkpoint_initialized)
        ATM_RET_ERR(AGENTOS_ENOTINIT);

    AGENTOS_LOG_INFO("CoreLoopThree: creating snapshot for task %s (path=%s)",
                     task_id, snapshot_path);

    agentos_error_t err = agentos_snapshot_create(task_id, snapshot_path);
    if (err == AGENTOS_SUCCESS) {
        AGENTOS_LOG_INFO("CoreLoopThree: snapshot created OK (task=%s, path=%s, "
                         "turns=%" PRIu64 ", completed=%zu)",
                         task_id, snapshot_path, loop->loop_turn_count,
                         loop->completed_node_count);
    } else {
        AGENTOS_LOG_WARN("CoreLoopThree: snapshot creation FAILED (task=%s, err=%d)",
                         task_id, err);
    }

    return err;
}

AGENTOS_API agentos_error_t agentos_loop_restore_snapshot(agentos_core_loop_t *loop,
                                                          const char *snapshot_path,
                                                          char **out_restored_task_id)
{
    if (!loop || !snapshot_path || !out_restored_task_id)
        ATM_RET_ERR(AGENTOS_EINVAL);
    if (!loop->checkpoint_initialized)
        ATM_RET_ERR(AGENTOS_ENOTINIT);

    AGENTOS_LOG_INFO("CoreLoopThree: restoring from snapshot (path=%s)", snapshot_path);

    *out_restored_task_id = NULL;

    /* 从快照文件恢复 task_id */
    char *task_id = NULL;
    agentos_error_t err = agentos_snapshot_restore(snapshot_path, &task_id);
    if (err != AGENTOS_SUCCESS || !task_id) {
        AGENTOS_LOG_WARN("CoreLoopThree: snapshot restore FAILED (path=%s, err=%d)",
                         snapshot_path, err);
        if (task_id)
            AGENTOS_FREE(task_id);
        return err == AGENTOS_SUCCESS ? AGENTOS_EIO : err;
    }

    AGENTOS_LOG_INFO("CoreLoopThree: snapshot task_id recovered: %s", task_id);

    /* 通过 restore_task 恢复完整状态 */
    err = agentos_loop_restore_task(loop, task_id, out_restored_task_id);
    AGENTOS_FREE(task_id);
    task_id = NULL;

    if (err == AGENTOS_SUCCESS) {
        AGENTOS_LOG_INFO("CoreLoopThree: snapshot restored OK (new_task=%s)",
                         *out_restored_task_id ? *out_restored_task_id : "unknown");
    } else {
        AGENTOS_LOG_WARN("CoreLoopThree: snapshot restore via restore_task FAILED (err=%d)", err);
    }

    return err;
}

AGENTOS_API agentos_error_t agentos_loop_get_checkpoint_stats(agentos_core_loop_t *loop,
                                                              agentos_checkpoint_stats_t *out_stats)
{
    if (!loop || !out_stats)
        ATM_RET_ERR(AGENTOS_EINVAL);
    if (!loop->checkpoint_initialized)
        ATM_RET_ERR(AGENTOS_ENOTINIT);

    return agentos_checkpoint_get_stats(out_stats);
}

/* ================================================================
 * C-L01 + C-L06: 外部适配器注入
 * ================================================================ */

AGENTOS_API void agentos_loop_set_manager_adapter(agentos_core_loop_t *loop,
                                                   agentos_manager_adapter_t *adapter)
{
    if (!loop) return;
    loop->manager_adapter = adapter;
    AGENTOS_LOG_INFO("C-L01: Manager adapter %s to CoreLoopThree",
                     adapter ? "attached" : "detached");
}

AGENTOS_API void agentos_loop_set_orch_adapter(agentos_core_loop_t *loop,
                                                orch_adapter_t *adapter)
{
    if (!loop) return;
    loop->orch_adapter = adapter;
    AGENTOS_LOG_INFO("C-L06: Orchestrator adapter %s to CoreLoopThree",
                     adapter ? "attached" : "detached");
}

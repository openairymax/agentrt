/**
 * @file engine.c
 * @brief 认知引擎核心实现 - 含Thinkdual集成
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 实现完整5阶段认知处理管线:
 * Phase 0: 指令拆解(S1) -> Phase 1: 规划(S2+S1) ->
 * Phase 2: 执行-验证循环 -> Phase 3: 审计 -> Phase 4: 目标对齐
 */

#include "agentos.h"
#include "cognition.h"
#include "error_utils.h"
#include "id_utils.h"
#include "logger.h"
#include "memory_compat.h"
#include "string_compat.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef AGENTOS_HAS_CJSON
#include <cjson/cJSON.h>
#endif

#include "atomic_compat.h"
#include "foundation/metacognition.h"
#include "foundation/semantic_unit.h"
#include "critique/stream_critic.h"
#include "foundation/thinking_chain.h"
#include "critique/triple_coordinator.h"
#include "critique/tc3_llm_callbacks.h"

/* C-L02: llm_d → CoreLoopThree */
#include "llm_service.h"
#include "llm_svc_adapter.h"

/* C-L04: tool_d → CoreLoopThree */
#include "tool_service.h"
#include "tool_svc_adapter.h"

/* C-L01: Manager → CoreLoopThree — YAML config loader bridge */
#include "config_loader.h"

/* C-L07: Checkpoint → CoreLoopThree */
#include "checkpoint.h"

/* C-L12: MemoryRovol → CoreLoopThree */
#include "memoryrovol_bridge.h"

/* P3.18 (ACC-DT26): hook_d → CoreLoopThree — 关键事件 hook 触发 */
#include "agentos_hook.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

struct agentos_cognition_engine {
    agentos_plan_strategy_t *plan_strat;
    agentos_plan_strategy_t *fallback_plan_strat;
    agentos_coordinator_strategy_t *coord_strat;
    agentos_dispatching_strategy_t *disp_strat;
    void *context;
    void (*context_destroy)(void *);
    agentos_mutex_t *lock;
    uint32_t stats_processed;
    uint64_t stats_total_time_ns;
    agentos_cognition_config_t manager;
    agentos_feedback_callback_t feedback_cb;
    void *feedback_user_data;
    uint64_t stats_success_count;
    uint64_t stats_failure_count;
    uint64_t stats_total_retries;

    agentos_thinking_chain_t *chain;
    agentos_metacognition_t *meta;
    agentos_memory_engine_t *memory;
    int enable_dual_thinking;
    uint32_t chain_max_tokens;
    size_t chain_wm_capacity;
    float meta_acceptance_threshold;
    uint64_t dual_think_invocations;
    uint64_t dual_think_corrections;
    float align_history[8];
    size_t align_history_count;
    int align_drift_detected;
    uint32_t align_replan_count;

    sc_stream_critic_t *stream_critic;

    /* C-L02: llm_d → CoreLoopThree — external LLM service handle (direct) */
    llm_service_t *llm_svc;

    /* C-L02: llm_d → CoreLoopThree — IPC adapter (preferred over direct llm_svc) */
    llm_svc_adapter_t *llm_adapter;

    /* C-L04: tool_d → CoreLoopThree — external tool service handle */
    tool_service_t *tool_svc;

    /* C-L04: tool_d → CoreLoopThree — IPC adapter (preferred over direct tool_svc) */
    tool_svc_adapter_t *tool_adapter;

    /* C-L07: Checkpoint → CoreLoopThree — auto-checkpoint state */
    int checkpoint_enabled;
    char checkpoint_session_id[128];
    uint64_t checkpoint_last_seq;
    agentos_task_plan_t *current_plan; /**< Currently processing plan (borrowed, for checkpoint) */

    /* C-L12: MemoryRovol → CoreLoopThree — memory provider from bridge */
    agentos_memory_provider_t *memory_provider;

    /* C-L02: Streaming LLM support */
    int llm_stream_enabled;
    llm_stream_callback_t llm_stream_callback;
    void *llm_stream_user_data;
};

static void trigger_feedback(agentos_cognition_engine_t *engine, int level, const char *event,
                             const char *data)
{
    if (engine && engine->feedback_cb) {
        engine->feedback_cb(level, "cognition", event, data, data ? strlen(data) : 0,
                            engine->feedback_user_data);
    }
}

/* P3.18 (ACC-DT26): 前向声明 — cognition_fire_hook 定义在 L664，
 * 此处提前声明以供 cognition_provider_write 内的 ON_MEMORY_EVOLVE 触发使用。 */
static hook_decision_t cognition_fire_hook(hook_type_t type, const char *operation,
                                           const void *input_data, size_t input_data_len);

/* C-L12: Write a memory record to the provider (if available) */
static int cognition_provider_write(agentos_cognition_engine_t *engine, const char *phase,
                                    const char *content, size_t content_len)
{
    if (!engine || !engine->memory_provider || !content || content_len == 0)
        return 0;

    if (!engine->memory_provider->write_raw)
        return 0;

    /* P3.18 (ACC-DT26): ON_MEMORY_EVOLVE hook — 记忆写入前触发。
     * ABORT → 跳过本次写入（非致命，记忆是增强功能）；
     * 其他决策 → 正常写入（仅记录/审计）。 */
    {
        char op_buf[64];
        snprintf(op_buf, sizeof(op_buf), "memory_evolve:%s",
                 phase ? phase : "unknown");
        hook_decision_t hd = cognition_fire_hook(HOOK_TYPE_ON_MEMORY_EVOLVE, op_buf,
                                                  content, content_len);
        if (hd == HOOK_DECISION_ABORT) {
            AGENTOS_LOG_INFO("C-L09: ON_MEMORY_EVOLVE hook ABORT — skipping memory write (phase=%s)",
                             phase ? phase : "unknown");
            return 0;
        }
        if (hd == HOOK_DECISION_RETRY) {
            AGENTOS_LOG_DEBUG("C-L09: ON_MEMORY_EVOLVE hook RETRY — degraded to CONTINUE (P4)");
        }
    }

    char metadata[256];
    snprintf(metadata, sizeof(metadata),
             "{\"phase\":\"%s\",\"source\":\"cognition_engine\"}", phase);

    char *record_id = NULL;
    agentos_error_t err = engine->memory_provider->write_raw(
        engine->memory_provider, content, content_len, metadata, &record_id);
    if (err == AGENTOS_SUCCESS && record_id) {
        AGENTOS_LOG_DEBUG("C-L12: Provider write phase=%s record_id=%s", phase, record_id);
        AGENTOS_FREE(record_id);
        return 0;
    }
    return -1;
}

/* _take: caller transfers ownership */
agentos_error_t agentos_cognition_create_take(agentos_plan_strategy_t *plan_strategy,
                                              agentos_coordinator_strategy_t *coord_strategy,
                                              agentos_dispatching_strategy_t *disp_strategy,
                                              agentos_cognition_engine_t **out_engine)
{
    return agentos_cognition_create_ex_take(NULL, plan_strategy, coord_strategy, disp_strategy,
                                            out_engine);
}

/* _take: caller transfers ownership */
agentos_error_t agentos_cognition_create_ex_take(const agentos_cognition_config_t *manager,
                                                 agentos_plan_strategy_t *plan_strategy,
                                                 agentos_coordinator_strategy_t *coord_strategy,
                                                 agentos_dispatching_strategy_t *disp_strategy,
                                                 agentos_cognition_engine_t **out_engine)
{

    if (!out_engine)
        return AGENTOS_EINVAL;

    agentos_cognition_engine_t *engine =
        (agentos_cognition_engine_t *)AGENTOS_CALLOC(1, sizeof(agentos_cognition_engine_t));
    if (!engine) {
        AGENTOS_LOG_ERROR("Failed to allocate cognition engine");
        return AGENTOS_ENOMEM;
    }

    engine->plan_strat = plan_strategy;
    engine->coord_strat = coord_strategy;
    engine->disp_strat = disp_strategy;
    engine->lock = agentos_mutex_create();
    if (!engine->lock) {
        AGENTOS_FREE(engine);
        return AGENTOS_ENOMEM;
    }

    if (manager) {
        engine->manager = *manager;
        engine->feedback_cb = manager->feedback_callback;
        engine->feedback_user_data = manager->feedback_user_data;
    } else {
        engine->manager.cognition_default_timeout_ms = 30000;
        engine->manager.cognition_max_retries = 3;
        engine->manager.feedback_callback = NULL;
        engine->manager.feedback_user_data = NULL;
        engine->feedback_cb = NULL;
        engine->feedback_user_data = NULL;
    }

    engine->stats_processed = 0;
    engine->stats_total_time_ns = 0;
    engine->stats_success_count = 0;
    engine->stats_failure_count = 0;
    engine->stats_total_retries = 0;

    engine->chain = NULL;
    engine->meta = NULL;
    engine->memory = NULL;
    engine->enable_dual_thinking = 1;
    engine->chain_max_tokens = 8192;
    engine->chain_wm_capacity = 64;
    engine->meta_acceptance_threshold = 0.7f;
    engine->dual_think_invocations = 0;
    engine->dual_think_corrections = 0;
    __builtin_memset(engine->align_history, 0, sizeof(engine->align_history));
    engine->align_history_count = 0;
    engine->align_drift_detected = 0;
    engine->align_replan_count = 0;

    agentos_error_t ds_err = agentos_mc_create(&engine->meta);
    if (ds_err != AGENTOS_SUCCESS) {
        AGENTOS_LOG_WARN("Metacognition init failed: err=%d, dual-thinking disabled", (int)ds_err);
        engine->enable_dual_thinking = 0;
    }

    sc_config_t sc_cfg = SC_CONFIG_DEFAULTS;
    sc_cfg.enable_output_correct = 1;
    sc_cfg.enable_memory_confirm = 1;
    agentos_error_t sc_err = sc_stream_critic_create(&sc_cfg, &engine->stream_critic);
    if (sc_err != AGENTOS_SUCCESS) {
        AGENTOS_LOG_WARN("Stream critic init failed: err=%d, proceeding without critic",
                         (int)sc_err);
        engine->stream_critic = NULL;
    }

    /* C-L02 / C-L04: service handles initialized to NULL, set via setters */
    engine->llm_svc = NULL;
    engine->llm_adapter = NULL;
    engine->tool_svc = NULL;
    engine->tool_adapter = NULL;

    /* C-L12: memory provider initialized to NULL, set via setter */
    engine->memory_provider = NULL;

    /* C-L01: Auto-load agentos.yaml global config if not already loaded */
    if (agentos_config_get_global() == NULL) {
        int cfg_ret = agentos_config_init(NULL);
        if (cfg_ret == 0) {
            const agentos_yaml_config_t *yaml_cfg = agentos_config_get_global();
            if (yaml_cfg) {
                /* Apply kernel.memory config to engine */
                if (yaml_cfg->kernel.memory.arena_default_size_kb > 0) {
                    /* arena size will be used when arena is created */
                }
                AGENTOS_LOG_INFO("C-L01: Config loaded from agentos.yaml"
                    " (ipc_shm=%uMB mode=%s max_alloc=%uMB)",
                    yaml_cfg->kernel.ipc.shm_pool_size_mb,
                    yaml_cfg->kernel.memory.oom_watermark_percent > 0
                        ? "configured" : "defaults",
                    yaml_cfg->kernel.memory.max_alloc_mb);
            }
        } else {
            AGENTOS_LOG_WARN("C-L01: Failed to load agentos.yaml, using defaults");
        }
    }

    *out_engine = engine;

    /* P3.18 (ACC-DT26): 初始化 hook 系统（幂等 — 多次调用安全）。
     * hook 是增强功能（审计/拦截/修改），初始化失败不阻止 cognition engine 运行，
     * 仅记录警告。hook_trigger 在未初始化时返回 CONTINUE（no-op）。 */
    if (agentos_hook_init() != 0) {
        AGENTOS_LOG_WARN("C-L09: hook_init failed — hooks disabled (non-fatal)");
    } else {
        AGENTOS_LOG_INFO("C-L09: hook system initialized for cognition engine");
    }

    trigger_feedback(engine, 2, "engine_created", "{\"status\":\"initialized\"}");
    return AGENTOS_SUCCESS;
}

void agentos_cognition_destroy(agentos_cognition_engine_t *engine)
{
    if (!engine)
        return;

    /* P3.18 (ACC-DT26): 销毁 hook 系统 — 与 create 中的 agentos_hook_init 配对。
     * hook_shutdown 幂等，重复调用安全。 */
    agentos_hook_shutdown();

    if (engine->chain) {
        agentos_tc_chain_stop(engine->chain);
        agentos_tc_chain_destroy(engine->chain);
    }
    if (engine->meta) {
        agentos_mc_destroy(engine->meta);
    }
    if (engine->stream_critic) {
        sc_stream_critic_destroy(engine->stream_critic);
    }
    if (engine->context && engine->context_destroy) {
        engine->context_destroy(engine->context);
    }
    /* P1.14: 销毁 TRANSFER ownership 的策略对象 */
    if (engine->plan_strat && engine->plan_strat->destroy) {
        engine->plan_strat->destroy(engine->plan_strat);
        engine->plan_strat = NULL;
    }
    if (engine->coord_strat && engine->coord_strat->destroy) {
        engine->coord_strat->destroy(engine->coord_strat);
        engine->coord_strat = NULL;
    }
    if (engine->disp_strat && engine->disp_strat->destroy) {
        engine->disp_strat->destroy(engine->disp_strat);
        engine->disp_strat = NULL;
    }
    /* fallback_plan_strat 是 BORROW，不由 engine 销毁 */
    if (engine->lock) {
        agentos_mutex_free(engine->lock);
    }
    AGENTOS_FREE(engine);
}

void agentos_cognition_set_fallback_plan(agentos_cognition_engine_t *engine,
                                         agentos_plan_strategy_t *fallback)
{
    if (!engine)
        return;
    agentos_mutex_lock(engine->lock);
    engine->fallback_plan_strat = fallback;
    agentos_mutex_unlock(engine->lock);
}

/* P1.14: 主规划策略 setter — 替换旧策略（TRANSFER ownership） */
void agentos_cognition_set_plan_strategy(agentos_cognition_engine_t *engine,
                                          agentos_plan_strategy_t *strategy)
{
    if (!engine)
        return;
    agentos_mutex_lock(engine->lock);
    if (engine->plan_strat && engine->plan_strat->destroy)
        engine->plan_strat->destroy(engine->plan_strat);
    engine->plan_strat = strategy;
    agentos_mutex_unlock(engine->lock);
    if (strategy) {
        AGENTOS_LOG_INFO("P1.14: plan_strategy injected into cognition engine (plan=%p destroy=%p)",
                         (void *)strategy->plan, (void *)strategy->destroy);
    } else {
        AGENTOS_LOG_WARN("P1.14: plan_strategy detached from cognition engine");
    }
}

/* P1.14: 协同策略 setter — 替换旧策略（TRANSFER ownership） */
void agentos_cognition_set_coord_strategy(agentos_cognition_engine_t *engine,
                                           agentos_coordinator_strategy_t *strategy)
{
    if (!engine)
        return;
    agentos_mutex_lock(engine->lock);
    if (engine->coord_strat && engine->coord_strat->destroy)
        engine->coord_strat->destroy(engine->coord_strat);
    engine->coord_strat = strategy;
    agentos_mutex_unlock(engine->lock);
    if (strategy) {
        AGENTOS_LOG_INFO("P1.14: coord_strategy injected into cognition engine (coordinate=%p)",
                         (void *)strategy->coordinate);
    } else {
        AGENTOS_LOG_WARN("P1.14: coord_strategy detached from cognition engine");
    }
}

/* P1.14: 调度策略 setter — 替换旧策略（TRANSFER ownership） */
void agentos_cognition_set_disp_strategy(agentos_cognition_engine_t *engine,
                                          agentos_dispatching_strategy_t *strategy)
{
    if (!engine)
        return;
    agentos_mutex_lock(engine->lock);
    if (engine->disp_strat && engine->disp_strat->destroy)
        engine->disp_strat->destroy(engine->disp_strat);
    engine->disp_strat = strategy;
    agentos_mutex_unlock(engine->lock);
    if (strategy) {
        AGENTOS_LOG_INFO("P1.14: disp_strategy injected into cognition engine (dispatch=%p)",
                         (void *)strategy->dispatch);
    } else {
        AGENTOS_LOG_WARN("P1.14: disp_strategy detached from cognition engine");
    }
}

/* _take: caller transfers ownership */
void agentos_cognition_set_context_take(agentos_cognition_engine_t *engine, void *context,
                                        void (*destroy)(void *))
{
    if (!engine)
        return;
    agentos_mutex_lock(engine->lock);
    if (engine->context && engine->context_destroy) {
        engine->context_destroy(engine->context);
    }
    engine->context = context;
    engine->context_destroy = destroy;
    agentos_mutex_unlock(engine->lock);
}

void agentos_cognition_set_memory(agentos_cognition_engine_t *engine,
                                  agentos_memory_engine_t *memory)
{
    if (!engine)
        return;
    agentos_mutex_lock(engine->lock);
    engine->memory = memory;
    if (engine->chain) {
        agentos_tc_chain_set_memory(engine->chain, memory);
    }
    agentos_mutex_unlock(engine->lock);
}

/* C-L12: MemoryRovol → CoreLoopThree */
void agentos_cognition_set_memory_provider(agentos_cognition_engine_t *engine,
                                           agentos_memory_provider_t *provider)
{
    if (!engine)
        return;
    agentos_mutex_lock(engine->lock);
    engine->memory_provider = provider;
    agentos_mutex_unlock(engine->lock);
    if (provider) {
        AGENTOS_LOG_INFO("C-L12: Memory provider attached to cognition engine (%s v%s)",
                         provider->name ? provider->name : "unknown",
                         provider->version ? provider->version : "?");
    } else {
        AGENTOS_LOG_INFO("C-L12: Memory provider detached from cognition engine");
    }
}

/* C-L02: llm_d → CoreLoopThree */
void agentos_cognition_set_llm_service(agentos_cognition_engine_t *engine,
                                        llm_service_t *llm_svc)
{
    if (!engine)
        return;
    agentos_mutex_lock(engine->lock);
    engine->llm_svc = llm_svc;
    agentos_mutex_unlock(engine->lock);
    if (llm_svc) {
        AGENTOS_LOG_INFO("C-L02: LLM service attached to cognition engine (direct)");
    } else {
        AGENTOS_LOG_INFO("C-L02: LLM service detached from cognition engine");
    }
}

/* C-L02: llm_d → CoreLoopThree — IPC adapter (P1.2.1 preferred path) */
void agentos_cognition_set_llm_adapter(agentos_cognition_engine_t *engine,
                                        llm_svc_adapter_t *adapter)
{
    if (!engine)
        return;
    agentos_mutex_lock(engine->lock);
    engine->llm_adapter = adapter;
    agentos_mutex_unlock(engine->lock);
    if (adapter) {
        AGENTOS_LOG_INFO("C-L02: LLM IPC adapter attached to cognition engine"
                         " (requests=%llu errors=%llu)",
                         (unsigned long long)0, (unsigned long long)0);
    } else {
        AGENTOS_LOG_INFO("C-L02: LLM IPC adapter detached from cognition engine");
    }
}

/* C-L02: Streaming LLM callback support (P1.2.2 async callback) */
void agentos_cognition_set_llm_streaming(agentos_cognition_engine_t *engine,
                                         int enabled,
                                         llm_stream_callback_t callback,
                                         void *user_data)
{
    if (!engine)
        return;
    agentos_mutex_lock(engine->lock);
    engine->llm_stream_enabled = enabled;
    engine->llm_stream_callback = callback;
    engine->llm_stream_user_data = user_data;
    agentos_mutex_unlock(engine->lock);
    if (enabled) {
        AGENTOS_LOG_INFO("C-L02: Streaming LLM mode enabled (callback=%p)", (void *)(uintptr_t)callback);
    } else {
        AGENTOS_LOG_INFO("C-L02: Streaming LLM mode disabled");
    }
}

/* C-L04: tool_d → CoreLoopThree */
void agentos_cognition_set_tool_service(agentos_cognition_engine_t *engine,
                                         tool_service_t *tool_svc)
{
    if (!engine)
        return;
    agentos_mutex_lock(engine->lock);
    engine->tool_svc = tool_svc;
    agentos_mutex_unlock(engine->lock);
    if (tool_svc) {
        AGENTOS_LOG_INFO("C-L04: Tool service attached to cognition engine (direct)");
    } else {
        AGENTOS_LOG_INFO("C-L04: Tool service detached from cognition engine");
    }
}

/* C-L04: tool_d → CoreLoopThree — IPC adapter (P1.3.1 preferred path) */
void agentos_cognition_set_tool_adapter(agentos_cognition_engine_t *engine,
                                         tool_svc_adapter_t *adapter)
{
    if (!engine)
        return;
    agentos_mutex_lock(engine->lock);
    engine->tool_adapter = adapter;
    agentos_mutex_unlock(engine->lock);
    if (adapter) {
        AGENTOS_LOG_INFO("C-L04: Tool IPC adapter attached to cognition engine");
    } else {
        AGENTOS_LOG_INFO("C-L04: Tool IPC adapter detached from cognition engine");
    }
}

tool_service_t *agentos_cognition_get_tool_service(agentos_cognition_engine_t *engine)
{
    if (!engine)
        return NULL;
    tool_service_t *svc = NULL;
    agentos_mutex_lock(engine->lock);
    svc = engine->tool_svc;
    agentos_mutex_unlock(engine->lock);
    return svc;
}

/* C-L07: Checkpoint → CoreLoopThree */

void agentos_cognition_enable_checkpoint(agentos_cognition_engine_t *engine,
                                          int enable, const char *session_id)
{
    if (!engine)
        return;
    agentos_mutex_lock(engine->lock);
    engine->checkpoint_enabled = enable;
    if (session_id && enable) {
        AGENTOS_STRNCPY_TERM(engine->checkpoint_session_id, session_id,
                             sizeof(engine->checkpoint_session_id));
    }
    if (!enable) {
        engine->checkpoint_session_id[0] = '\0';
        engine->checkpoint_last_seq = 0;
    }
    agentos_mutex_unlock(engine->lock);
    AGENTOS_LOG_INFO("C-L07: Checkpoint auto-save %s (session=%s)",
                     enable ? "enabled" : "disabled",
                     enable && session_id ? session_id : "N/A");
}

int agentos_cognition_save_checkpoint(agentos_cognition_engine_t *engine,
                                       uint64_t sequence_num, const char *phase_name)
{
    if (!engine || !phase_name)
        return -1;

    agentos_task_plan_t *plan = NULL;
    char session_id[128];
    int enabled = 0;

    agentos_mutex_lock(engine->lock);
    plan = engine->current_plan;
    enabled = engine->checkpoint_enabled;
    AGENTOS_STRNCPY_TERM(session_id, engine->checkpoint_session_id, sizeof(session_id));
    agentos_mutex_unlock(engine->lock);

    if (!enabled || session_id[0] == '\0')
        return 0;

    /* Build state JSON snapshot */
    char state_json[1024];
    const char *plan_id = (plan && plan->task_plan_id) ? plan->task_plan_id : "unknown";
    size_t node_count = plan ? plan->task_plan_node_count : 0;

    int sj_len = snprintf(state_json, sizeof(state_json),
                          "{\"phase\":\"%s\",\"plan_id\":\"%s\",\"node_count\":%zu,"
                          "\"corrections\":%llu,\"invocations\":%llu,\"seq\":%llu}",
                          phase_name, plan_id, node_count,
                          (unsigned long long)engine->dual_think_corrections,
                          (unsigned long long)engine->dual_think_invocations,
                          (unsigned long long)sequence_num);

    if (sj_len < 0 || (size_t)sj_len >= sizeof(state_json)) {
        AGENTOS_LOG_WARN("C-L07: Checkpoint state JSON truncated");
    }

    /* Build completed/pending node lists from plan */
    char **completed_nodes = NULL;
    size_t completed_count = 0;
    char **pending_nodes = NULL;
    size_t pending_count = 0;

    if (plan && plan->task_plan_nodes && plan->task_plan_node_count > 0) {
        /* Mark all nodes as pending (they haven't been dispatched yet in cognition phase) */
        pending_count = plan->task_plan_node_count;
        pending_nodes = (char **)AGENTOS_CALLOC(pending_count, sizeof(char *));
        if (pending_nodes) {
            for (size_t i = 0; i < pending_count; i++) {
                if (plan->task_plan_nodes[i] && plan->task_plan_nodes[i]->task_node_id) {
                    pending_nodes[i] = AGENTOS_STRDUP(plan->task_plan_nodes[i]->task_node_id);
                }
            }
        }
    }

    agentos_task_checkpoint_t *cp = NULL;
    agentos_error_t cp_err =
        agentos_checkpoint_create(plan_id, session_id, sequence_num, state_json,
                                  completed_nodes, completed_count,
                                  pending_nodes, pending_count, &cp);

    /* Clean up node arrays */
    if (pending_nodes) {
        for (size_t i = 0; i < pending_count; i++)
            AGENTOS_FREE(pending_nodes[i]);
        AGENTOS_FREE(pending_nodes);
    }

    if (cp_err != AGENTOS_SUCCESS || !cp) {
        AGENTOS_LOG_WARN("C-L07: Failed to create checkpoint: err=%d phase=%s seq=%llu",
                         (int)cp_err, phase_name, (unsigned long long)sequence_num);
        return -1;
    }

    /* Set metadata */
    snprintf(cp->metadata, sizeof(cp->metadata), "phase=%s plan=%s", phase_name, plan_id);

    agentos_error_t save_err = agentos_checkpoint_save(cp);
    agentos_checkpoint_destroy(cp);

    if (save_err == AGENTOS_SUCCESS) {
        AGENTOS_LOG_INFO("C-L07: Checkpoint saved: phase=%s seq=%llu plan=%s",
                         phase_name, (unsigned long long)sequence_num, plan_id);
        return 0;
    } else {
        AGENTOS_LOG_WARN("C-L07: Failed to save checkpoint: err=%d phase=%s",
                         (int)save_err, phase_name);
        return -1;
    }
}

/* P3.18 (ACC-DT26): cognition_fire_hook — 在关键事件点触发 hook 链
 *
 * 构造 hook_context_t 并调用 agentos_hook_trigger，返回聚合决策。
 * 用于 PRE_EXEC/POST_EXEC/PRE_LLM/POST_LLM/PRE_TOOL/POST_TOOL/ON_ERROR/ON_MEMORY_EVOLVE。
 *
 * 决策处理由调用点负责：
 *   CONTINUE — 正常继续
 *   SKIP     — 跳过当前步骤
 *   ABORT    — 终止当前阶段
 *   MODIFY   — 使用 ctx.output_data 替换输入
 *   RETRY    — 降级为 CONTINUE（完整重试需循环计数，P4 增强） */
static hook_decision_t cognition_fire_hook(hook_type_t type,
                                           const char *operation,
                                           const void *input_data,
                                           size_t input_data_len)
{
    hook_context_t ctx;
    AGENTOS_MEMSET(&ctx, 0, sizeof(ctx));
    ctx.type = type;
    ctx.source_daemon = "coreloopthree";
    ctx.operation = operation ? operation : "unknown";
    ctx.input_data = input_data;
    ctx.input_data_len = input_data_len;
    ctx.timestamp_ns = agentos_time_monotonic_ns();
    return agentos_hook_trigger(&ctx);
}

agentos_error_t agentos_cognition_process(agentos_cognition_engine_t *engine, const char *input,
                                          size_t input_len, agentos_task_plan_t **out_plan)
{

    if (!engine || !input || !out_plan) {
        AGENTOS_LOG_ERROR("Invalid parameters to cognition_process: engine=%p input=%p out_plan=%p",
                          (void *)engine, (void *)input, (void *)out_plan);
        return AGENTOS_EINVAL;
    }
    if (input_len == 0)
        return AGENTOS_EINVAL;

    /* P3.18 fix: plan/err 提前声明至函数顶部 — PRE_EXEC ABORT 在原 L920/L921 声明
     * 之前 goto process_fail，process_fail 路径读取 plan/err 未初始化值（UB）。
     * 提前声明确保所有 goto process_fail 路径变量已初始化（plan=NULL, err=EUNKNOWN）。 */
    agentos_task_plan_t *plan = NULL;
    agentos_error_t err = AGENTOS_EUNKNOWN;

    /* P3.18 (ACC-DT26): PRE_EXEC hook — 认知处理开始前触发。
     * ABORT → 直接进入失败路径；CONTINUE/MODIFY/RETRY → 正常继续。 */
    {
        hook_decision_t hd = cognition_fire_hook(HOOK_TYPE_PRE_EXEC, "cognition_process",
                                                  input, input_len);
        if (hd == HOOK_DECISION_ABORT) {
            AGENTOS_LOG_WARN("C-L09: PRE_EXEC hook ABORT — skipping cognition process");
            goto process_fail;
        }
        if (hd == HOOK_DECISION_RETRY) {
            AGENTOS_LOG_DEBUG("C-L09: PRE_EXEC hook RETRY — degraded to CONTINUE (P4 enhancement)");
        }
    }

    agentos_intent_t intent;
    __builtin_memset(&intent, 0, sizeof(intent));
    intent.intent_raw_text = (char *)input;
    intent.intent_raw_len = input_len;
    intent.intent_goal = (char *)input;
    intent.intent_goal_len = input_len;
    intent.intent_flags = 0;
    intent.intent_context = engine->context;

    uint64_t start_ns = agentos_time_monotonic_ns();

    /* P3.11-C1: 记忆上下文检索已统一到 thinking_chain prepopulate 路径（Phase 0 中调用
     * agentos_tc_context_window_prepopulate → agentos_memory_query → provider->query）。
     *
     * 此前此处有一段冗余的 memory_provider->retrieve 调用，但结果（ctx_ids + ctx_scores）
     * 被 直接 free 丢弃，从未读取记录内容——数据流断裂。且 prepopulate 走 memory engine
     * 路径，此处走 cognition memory_provider 路径，P3.11-C9 后两者共用同一 bridge provider，
     * 完全冗余。删除以消除双重检索浪费和数据流断裂。 */

    /* ========== Stream Critic Phase 0: Intent Classification ========== */
    sc_intent_result_t sc_intent;
    __builtin_memset(&sc_intent, 0, sizeof(sc_intent));
    if (engine->stream_critic) {
        agentos_error_t ic_err =
            sc_intent_classifier(engine->stream_critic, input, input_len, &sc_intent);
        if (ic_err == AGENTOS_SUCCESS) {
            if (sc_intent.is_urgent)
                intent.intent_flags |= 0x04;
            if (sc_intent.requires_multi_step)
                intent.intent_flags |= 0x08;
            char ic_fb[256];
            snprintf(ic_fb, sizeof(ic_fb),
                     "{\"category\":\"%s\",\"confidence\":%.2f,\"urgent\":%d,\"multi_step\":%d}",
                     sc_intent.category_name, sc_intent.confidence, sc_intent.is_urgent,
                     sc_intent.requires_multi_step);
            trigger_feedback(engine, 0, "intent_classified", ic_fb);
        }
        sc_intent_result_free(&sc_intent);
    }

    /* ========== Phase 0: Instruction Decomposition (S1) ========== */
    if (engine->enable_dual_thinking && engine->meta) {
        if (engine->chain) {
            agentos_tc_chain_stop(engine->chain);
            agentos_tc_chain_destroy(engine->chain);
        }
        agentos_error_t tc_err = agentos_tc_chain_create(input, engine->chain_max_tokens,
                                                         engine->chain_wm_capacity, &engine->chain);
        if (tc_err == AGENTOS_SUCCESS) {
            agentos_tc_chain_start(engine->chain);
            agentos_mc_set_chain(engine->meta, engine->chain);

            if (engine->memory) {
                agentos_tc_chain_set_memory(engine->chain, engine->memory);
                agentos_tc_context_window_prepopulate(engine->chain, input, input_len, 5);
            }

            agentos_thinking_step_t *decomp_step = NULL;
            agentos_tc_step_create(engine->chain, TC_STEP_DECOMPOSITION, input, input_len, NULL, 0,
                                   &decomp_step);

            /* P2.9-B1: Phase 0 LLM 指令拆解 — 输出结构化子任务 JSON（ACC-DT05）。
             *
             * 原 Phase 0 是空操作（decomp_step 直接用 input 作为 output），
             * 现改为：LLM 可用时调用 LLM 拆解指令为 subtasks JSON；LLM 不可
             * 用时降级为单元素 subtasks JSON。拆解结果存入 thinking chain
             * 和 working memory，供后续 Phase 1 规划和 Phase 4 对齐访问。
             */
            char *decomp_output = NULL;
            size_t decomp_output_len = 0;
            float decomp_confidence = 0.5f;
            const char *decomp_agent = "S1-decomposer";

            llm_service_t *decomp_llm_svc = NULL;
            llm_svc_adapter_t *decomp_llm_adapter = NULL;
            agentos_mutex_lock(engine->lock);
            decomp_llm_svc = engine->llm_svc;
            decomp_llm_adapter = engine->llm_adapter;
            agentos_mutex_unlock(engine->lock);

            if (decomp_llm_adapter || decomp_llm_svc) {
                llm_message_t decomp_msgs[2];
                decomp_msgs[0].role = "system";
                decomp_msgs[0].content =
                    "You are an instruction decomposition assistant. Break down the user's "
                    "request into structured subtasks. Respond ONLY in JSON format: "
                    "{\"subtasks\":[{\"id\":1,\"goal\":\"...\",\"type\":\"analysis|generation|"
                    "retrieval|execution|verification\"}]}. Keep subtasks atomic and ordered.";
                decomp_msgs[1].role = "user";
                decomp_msgs[1].content = input;

                llm_request_config_t decomp_cfg;
                __builtin_memset(&decomp_cfg, 0, sizeof(decomp_cfg));
                decomp_cfg.model = NULL;
                decomp_cfg.messages = decomp_msgs;
                decomp_cfg.message_count = 2;
                decomp_cfg.temperature = 0.3f; /* 低温度保证结构化输出 */
                decomp_cfg.top_p = 1.0f;
                decomp_cfg.max_tokens = 1024;
                decomp_cfg.stream = 0;

                llm_response_t *decomp_resp = NULL;
                int decomp_ret = -1;
                if (decomp_llm_adapter) {
                    decomp_ret = llm_svc_adapter_complete(decomp_llm_adapter, &decomp_cfg,
                                                          &decomp_resp);
                } else {
                    decomp_ret = llm_service_complete(decomp_llm_svc, &decomp_cfg, &decomp_resp);
                }

                if (decomp_ret == 0 && decomp_resp && decomp_resp->choices &&
                    decomp_resp->choice_count > 0 && decomp_resp->choices[0].content) {
                    decomp_output = AGENTOS_STRDUP(decomp_resp->choices[0].content);
                    if (decomp_output) {
                        decomp_output_len = strlen(decomp_output);
                        decomp_confidence = 0.85f;
                    }
                    AGENTOS_LOG_INFO("Phase 0: LLM decomposition succeeded (len=%zu)",
                                     decomp_output_len);
                } else {
                    AGENTOS_LOG_WARN("Phase 0: LLM decomposition failed (ret=%d), using fallback",
                                     decomp_ret);
                }
                if (decomp_resp) {
                    llm_response_free(decomp_resp);
                    decomp_resp = NULL;
                }
            }

            /* LLM 不可用或调用失败：降级生成单元素 subtasks JSON */
            if (!decomp_output) {
                size_t fb_sz = input_len + 128;
                decomp_output = (char *)AGENTOS_MALLOC(fb_sz);
                if (decomp_output) {
                    int dn = snprintf(decomp_output, fb_sz,
                                      "{\"subtasks\":[{\"id\":1,\"goal\":\"%.*s\","
                                      "\"type\":\"unknown\"}]}",
                                      (int)(input_len > 200 ? 200 : input_len), input);
                    if (dn > 0 && (size_t)dn < fb_sz) {
                        decomp_output_len = (size_t)dn;
                        decomp_confidence = 0.5f;
                    } else {
                        AGENTOS_FREE(decomp_output);
                        decomp_output = NULL;
                    }
                }
            }

            if (decomp_step && decomp_output) {
                agentos_tc_step_complete(decomp_step, decomp_output, decomp_output_len,
                                         decomp_confidence, decomp_agent);
                /* 存入 working memory 供后续 Phase 1 规划访问 */
                if (engine->chain->working_mem) {
                    agentos_tc_working_memory_store(engine->chain->working_mem,
                                                    "decomposed_subtasks", decomp_output,
                                                    decomp_output_len + 1, "application/json", 1);
                }
            }
            if (decomp_output) {
                AGENTOS_FREE(decomp_output);
                decomp_output = NULL;
            }

            char *preemptive_hint = NULL;
            size_t hint_len = 0;
            int preempt = agentos_mc_preemptive_check(engine->meta, TC_STEP_PLANNING, input,
                                                      input_len, &preemptive_hint, &hint_len);
            if (preempt == 1 && preemptive_hint) {
                if (engine->chain->working_mem) {
                    agentos_tc_working_memory_store(engine->chain->working_mem, "preemptive_hint",
                                                    preemptive_hint, hint_len + 1, "text/plain", 1);
                }
                AGENTOS_FREE(preemptive_hint);
                preemptive_hint = NULL;
            }
        } else {
            AGENTOS_LOG_WARN("Thinking chain creation failed: err=%d", (int)tc_err);
        }
        engine->dual_think_invocations++;
    }

    /* C-L12: Write decomposed input to memory provider for context retrieval */
    cognition_provider_write(engine, "decomposition", input, input_len);

    /* C-L07: Checkpoint after Phase 0 — decomposition */
    if (engine->checkpoint_enabled) {
        agentos_cognition_save_checkpoint(engine, engine->checkpoint_last_seq++, "decomposition");
    }

    /* ========== Phase 1: Planning (S2 + S1 pre-validation) ========== */
    plan = NULL;                /* P3.18 fix: 重置 — 已在函数顶部声明 */
    err = AGENTOS_EUNKNOWN;     /* P3.18 fix: 重置 — 已在函数顶部声明 */

    agentos_plan_strategy_t *plan_strat = NULL;
    agentos_plan_strategy_t *fallback_strat = NULL;
    agentos_mutex_lock(engine->lock);
    plan_strat = engine->plan_strat;
    fallback_strat = engine->fallback_plan_strat;
    agentos_mutex_unlock(engine->lock);

    if (plan_strat && plan_strat->plan) {
        err = plan_strat->plan(&intent, plan_strat->data, &plan);
    }

    if (err == AGENTOS_SUCCESS && plan && engine->enable_dual_thinking && engine->meta &&
        engine->chain) {
        agentos_thinking_step_t *plan_step = NULL;
        agentos_tc_step_create(engine->chain, TC_STEP_PLANNING, input, input_len, NULL, 0,
                               &plan_step);
        if (plan_step) {
            char plan_desc[256];
            int pd_len =
                snprintf(plan_desc, sizeof(plan_desc), "plan_id=%s nodes=%zu",
                         plan->task_plan_id ? plan->task_plan_id : "?", plan->task_plan_node_count);
            agentos_tc_step_complete(plan_step, plan_desc, (size_t)pd_len, 0.75f, "S2-planner");

            mc_evaluation_result_t eval;
            agentos_mc_evaluate_step(engine->meta, plan_step, NULL, 0, &eval);
            if (!eval.is_acceptable && eval.strategy != MC_CORRECT_NONE) {
                AGENTOS_LOG_WARN("Plan S1 pre-validation failed: score=%.2f strategy=%d",
                                 eval.overall_score, eval.strategy);
                engine->dual_think_corrections++;
            }
            if (eval.critique_text) {
                AGENTOS_FREE(eval.critique_text);
                eval.critique_text = NULL;
            }
        }
    }

    if (err != AGENTOS_SUCCESS) {
        AGENTOS_LOG_WARN("Primary planning failed: err=%d, trying fallback", (int)err);
        char err_buf[256];
        snprintf(err_buf, sizeof(err_buf), "{\"error_code\":%d,\"stage\":\"primary_planning\"}",
                 (int)err);
        trigger_feedback(engine, 1, "planning_retry", err_buf);

        if (fallback_strat && fallback_strat->plan) {
            err = fallback_strat->plan(&intent, fallback_strat->data, &plan);
            if (err == AGENTOS_SUCCESS) {
                agentos_mutex_lock(engine->lock);
                engine->stats_total_retries++;
                agentos_mutex_unlock(engine->lock);
            }
        } else {
            snprintf(err_buf, sizeof(err_buf), "{\"error_code\":%d,\"stage\":\"no_fallback\"}",
                     (int)err);
            trigger_feedback(engine, 0, "process_failed", err_buf);
            agentos_mutex_lock(engine->lock);
            engine->stats_failure_count++;
            agentos_mutex_unlock(engine->lock);
            goto process_fail;
        }
    }

    if (err != AGENTOS_SUCCESS) {
        char err_buf[256];
        snprintf(err_buf, sizeof(err_buf), "{\"error_code\":%d,\"stage\":\"fallback_failed\"}",
                 (int)err);
        trigger_feedback(engine, 0, "process_failed", err_buf);
        agentos_mutex_lock(engine->lock);
        engine->stats_failure_count++;
        agentos_mutex_unlock(engine->lock);
        goto process_fail;
    }

    if (plan && !plan->task_plan_id) {
        char id_buf[64];
        agentos_generate_plan_id(id_buf, sizeof(id_buf));
        plan->task_plan_id = AGENTOS_STRDUP(id_buf);
        if (!plan->task_plan_id) {
            agentos_task_plan_free(plan);
            return AGENTOS_ENOMEM;
        }
    }

    /* C-L07: Store current plan reference for checkpoint snapshots */
    engine->current_plan = plan;

    /* C-L07: Checkpoint after Phase 1 — planning */
    if (engine->checkpoint_enabled) {
        agentos_cognition_save_checkpoint(engine, engine->checkpoint_last_seq++, "planning");
    }

    /* ========== Phase 2: Streaming Critical Loop (t2/t1-f/t1-p) ========== */
    if (engine->enable_dual_thinking && engine->chain && engine->meta && plan) {
        size_t anomaly_count = 0;
        int has_critical = 0;
        agentos_tc_chain_health_check(engine->chain, &anomaly_count, &has_critical);
        if (has_critical) {
            AGENTOS_LOG_WARN("Chain health check: %zu anomalies, critical detected", anomaly_count);
            trigger_feedback(engine, 1, "anomaly_detected", "{\"anomalies\":1,\"critical\":true}");
        }

        agentos_thinking_step_t *gen_step = NULL;
        agentos_tc_step_create(engine->chain, TC_STEP_GENERATION, input, input_len, NULL, 0,
                               &gen_step);

        /* ── C-L02: llm_d → CoreLoopThree — external LLM invocation ── */
        llm_service_t *llm_svc = NULL;
        llm_svc_adapter_t *llm_adapter = NULL;
        agentos_mutex_lock(engine->lock);
        llm_svc = engine->llm_svc;
        llm_adapter = engine->llm_adapter;
        agentos_mutex_unlock(engine->lock);

        char *llm_response_text = NULL;
        size_t llm_response_len = 0;

        if (llm_adapter || llm_svc) {
            llm_message_t msgs[2];
            msgs[0].role = "system";
            msgs[0].content =
                "You are a cognitive reasoning assistant. Generate a detailed, "
                "step-by-step response to the user's request.";
            msgs[1].role = "user";
            msgs[1].content = input;

            llm_request_config_t llm_cfg;
            __builtin_memset(&llm_cfg, 0, sizeof(llm_cfg));
            llm_cfg.model = NULL; /* use provider default */
            llm_cfg.messages = msgs;
            llm_cfg.message_count = 2;
            llm_cfg.temperature = 0.7f;
            llm_cfg.top_p = 1.0f;
            llm_cfg.max_tokens = (int)engine->chain_max_tokens;
            llm_cfg.stream = 0;

            llm_response_t *llm_resp = NULL;
            int llm_ret = -1;
            AGENTOS_LOG_INFO("C-L02: Phase 2 LLM call started (max_tokens=%u)",
                            engine->chain_max_tokens);

            /* C-L02 P1.2.2: Async streaming callback support */
            int use_streaming = 0;
            llm_stream_callback_t stream_cb = NULL;
            void *stream_cb_data = NULL;

            agentos_mutex_lock(engine->lock);
            use_streaming = engine->llm_stream_enabled;
            stream_cb = engine->llm_stream_callback;
            stream_cb_data = engine->llm_stream_user_data;
            agentos_mutex_unlock(engine->lock);

            /* P3.18 (ACC-DT26): PRE_LLM hook — LLM 调用前触发。
             * ABORT/SKIP → 跳过 LLM 调用（llm_ret 保持 -1）；
             * MODIFY → 降级 CONTINUE（修改请求需重建 msgs，P4 增强）；
             * CONTINUE/RETRY → 正常继续。 */
            int skip_llm_call = 0;
            {
                hook_decision_t hd = cognition_fire_hook(HOOK_TYPE_PRE_LLM, "llm_complete",
                                                          input, input_len);
                if (hd == HOOK_DECISION_ABORT || hd == HOOK_DECISION_SKIP) {
                    AGENTOS_LOG_INFO("C-L09: PRE_LLM hook %s — skipping LLM call",
                                     hd == HOOK_DECISION_ABORT ? "ABORT" : "SKIP");
                    skip_llm_call = 1;
                } else if (hd == HOOK_DECISION_MODIFY) {
                    AGENTOS_LOG_DEBUG("C-L09: PRE_LLM hook MODIFY — degraded to CONTINUE (P4)");
                }
            }

            /* P1.2.1: Prefer IPC adapter path over direct LLM service */
            if (!skip_llm_call && llm_adapter) {
                AGENTOS_LOG_DEBUG("C-L02: Using IPC adapter for LLM request"
                                  " (streaming=%s)",
                                  (use_streaming && stream_cb) ? "on" : "off");

                if (use_streaming && stream_cb) {
                    /* P1.2.2: Streaming via IPC adapter */
                    llm_cfg.stream = 1;
                    llm_ret = llm_svc_adapter_complete_stream(
                        llm_adapter, &llm_cfg,
                        stream_cb, stream_cb_data, &llm_resp);
                    AGENTOS_LOG_DEBUG("C-L02: IPC streaming complete: ret=%d", llm_ret);
                } else {
                    /* P1.2.1: Sync via IPC adapter */
                    llm_ret = llm_svc_adapter_complete(
                        llm_adapter, &llm_cfg, &llm_resp);
                    AGENTOS_LOG_DEBUG("C-L02: IPC sync complete: ret=%d", llm_ret);
                }
            } else if (!skip_llm_call && llm_svc) {
                AGENTOS_LOG_DEBUG("C-L02: Using direct LLM service (no IPC adapter)"
                                  " streaming=%s",
                                  (use_streaming && stream_cb) ? "on" : "off");

                if (use_streaming && stream_cb) {
                    /* Streaming mode: chunks delivered via callback, full response assembled */
                    llm_cfg.stream = 1;
                    llm_ret = llm_service_complete_stream(llm_svc, &llm_cfg,
                                                          stream_cb, stream_cb_data,
                                                          &llm_resp);
                    AGENTOS_LOG_DEBUG("C-L02: Direct streaming complete: ret=%d", llm_ret);
                } else {
                    /* Sync mode: blocking until full response */
                    llm_ret = llm_service_complete(llm_svc, &llm_cfg, &llm_resp);
                    AGENTOS_LOG_DEBUG("C-L02: Direct sync complete: ret=%d", llm_ret);
                }
            }

            if (llm_ret == 0 && llm_resp && llm_resp->choices && llm_resp->choice_count > 0) {
                llm_response_text = llm_resp->choices[0].content
                                        ? AGENTOS_STRDUP(llm_resp->choices[0].content)
                                        : NULL;
                if (llm_response_text) {
                    llm_response_len = strlen(llm_response_text);
                }

                char llm_fb[384];
                snprintf(llm_fb, sizeof(llm_fb),
                         "{\"model\":\"%s\",\"tokens\":%u,\"finish\":\"%s\"}",
                         llm_resp->model ? llm_resp->model : "?",
                         llm_resp->total_tokens,
                         llm_resp->finish_reason ? llm_resp->finish_reason : "?");
                trigger_feedback(engine, 2, "llm_service_complete", llm_fb);
            } else {
                AGENTOS_LOG_WARN("C-L02: LLM service complete failed: ret=%d", llm_ret);
            }

            if (llm_resp) {
                llm_response_free(llm_resp);
                llm_resp = NULL;
            }

            /* P3.18 (ACC-DT26): POST_LLM hook — LLM 响应提取后触发（仅记录/审计）。
             * ABORT → 终止当前阶段（释放 llm_response_text 避免泄漏，因 process_fail
             *          路径不释放该指针）；其他决策 → 正常继续。 */
            {
                hook_decision_t hd = cognition_fire_hook(HOOK_TYPE_POST_LLM, "llm_complete",
                                                          llm_response_text, llm_response_len);
                if (hd == HOOK_DECISION_ABORT) {
                    AGENTOS_LOG_WARN("C-L09: POST_LLM hook ABORT — terminating phase");
                    if (llm_response_text) {
                        AGENTOS_FREE(llm_response_text);
                        llm_response_text = NULL;
                    }
                    goto process_fail;
                }
            }
        }

        /* ── Triple coordinator: LLM-driven t2/t1-f/t1-p 批判循环 ──
         *
         * ThinkDual 升级: 将三个回调从 NULL 替换为 LLM 驱动实现，使 tc3 的
         * 生成→验证→修正→专家仲裁循环真正生效（原先回调为 NULL 导致 tc3
         * 立即失败，LLM 输出未经任何验证直接采用）。
         *
         * - s2_generate: 首轮复用 engine 的 LLM seed（避免重复调用），
         *   后续修正轮独立调用 LLM。
         * - s1_verify:   LLM 评估逻辑正确性 + 任务对齐度（不偏离任务）。
         * - s1_expert:   LLM 扮演领域专家，在 MAJOR_FIX 时升级仲裁。
         *
         * LLM 不可用时，各回调内部有启发式回退，tc3 循环不中断。
         * 若 seed 也为 NULL（LLM 完全不可用），s2_generate 返回错误，
         * tc3 失败后走 fallback 路径（line 969-991），行为与升级前一致。
         */
        tc3_llm_ctx_t tc3_ctx;
        __builtin_memset(&tc3_ctx, 0, sizeof(tc3_ctx));
        tc3_ctx.llm_adapter = llm_adapter;
        tc3_ctx.llm_svc = llm_svc;
        tc3_ctx.lock = engine->lock;
        tc3_ctx.max_tokens = engine->chain_max_tokens;
        /* P2.7: 三独立模型字段 — 默认 NULL（provider 默认模型）。
         * 后续可通过 engine 配置注入不同模型名实现多模型激活。 */
        tc3_ctx.s2_model = NULL;
        tc3_ctx.s1_verify_model = NULL;
        tc3_ctx.s1_expert_model = NULL;
        tc3_ctx.seed_text = llm_response_text;
        tc3_ctx.seed_len = llm_response_len;
        tc3_ctx.seed_consumed = 0;
        tc3_ctx.original_input = input;
        tc3_ctx.original_input_len = input_len;

        tc3_config_t tc3_cfg = TC3_CONFIG_DEFAULTS;
        if (llm_adapter || llm_svc) {
            /* LLM 可用 — 注入 LLM 驱动回调，激活 ThinkDual 批判循环 */
            tc3_cfg.s2_generate = tc3_llm_s2_generate;
            tc3_cfg.s1_verify = tc3_llm_s1_verify;
            tc3_cfg.s1_expert = tc3_llm_s1_expert;
            tc3_cfg.s2_user_data = &tc3_ctx;
            tc3_cfg.s1_user_data = &tc3_ctx;
            tc3_cfg.s1_expert_user_data = &tc3_ctx;
            AGENTOS_LOG_INFO("C-L02: ThinkDual activated (LLM-driven t2/t1-f/t1-p "
                            "critique loop enabled)");
        } else {
            /* LLM 不可用 — 保持回调为 NULL，tc3 将失败并走 fallback */
            tc3_cfg.s2_generate = NULL;
            tc3_cfg.s1_verify = NULL;
            tc3_cfg.s1_expert = NULL;
            AGENTOS_LOG_INFO("C-L02: ThinkDual inactive (no LLM available, "
                            "using direct fallback)");
        }

        tc3_coordinator_t *tc3 = NULL;
        agentos_error_t tc3_err =
            tc3_coordinator_create(&tc3_cfg, engine->chain, engine->meta, &tc3);

        if (tc3_err == AGENTOS_SUCCESS && tc3) {
            /* If LLM produced output, feed it into the triple coordinator as seed context */
            if (llm_response_text && llm_response_len > 0) {
                if (engine->chain->working_mem) {
                    agentos_tc_working_memory_store(engine->chain->working_mem,
                                                    "llm_d_response", llm_response_text,
                                                    llm_response_len + 1, "text/plain", 1);
                }
            }

            char *phase2_output = NULL;
            size_t phase2_output_len = 0;
            tc3_err = tc3_coordinator_execute_streaming(tc3, input, input_len, &phase2_output,
                                                        &phase2_output_len);

            if (tc3_err == AGENTOS_SUCCESS && phase2_output) {
                if (gen_step) {
                    agentos_tc_step_complete(gen_step, phase2_output, phase2_output_len, 0.8f,
                                             llm_svc ? "t2-llm+streaming" : "t2-streaming");
                }
                if (engine->chain->ctx_window) {
                    agentos_tc_context_window_append(engine->chain->ctx_window, phase2_output,
                                                     phase2_output_len);
                }

                tc3_stats_t tc3_stats;
                tc3_coordinator_get_stats(tc3, &tc3_stats);
                engine->dual_think_corrections += tc3_stats.total_corrections;
                AGENTOS_LOG_INFO("C-L02: ThinkDual tc3 success (phase2_output_len=%zu units=%u "
                                 "corrections=%u loops=%u avg_score=%.2f)",
                                phase2_output_len, (unsigned)tc3_stats.total_units,
                                (unsigned)tc3_stats.total_corrections,
                                (unsigned)tc3_stats.loop_detected_units,
                                (double)tc3_stats.avg_score);

                char fb[512];
                snprintf(fb, sizeof(fb),
                         "{\"units\":%u,\"accepted\":%u,\"corrections\":%u,"
                         "\"loops\":%u,\"avg_score\":%.2f,\"time_ns\":%llu,\"llm_backed\":%s}",
                         tc3_stats.total_units, tc3_stats.accepted_units,
                         tc3_stats.total_corrections, tc3_stats.loop_detected_units,
                         tc3_stats.avg_score,
                         (unsigned long long)tc3_stats.total_time_ns,
                         llm_svc ? "true" : "false");
                trigger_feedback(engine, 2, "phase2_critical_loop", fb);

                AGENTOS_FREE(phase2_output);
                phase2_output = NULL;
            } else if (gen_step) {
                AGENTOS_LOG_WARN("C-L02: ThinkDual tc3 failed (err=%d), falling back to LLM output",
                                (int)tc3_err);
                /* Triple coordinator failed — fall back to LLM output if available */
                if (llm_response_text && llm_response_len > 0) {
                    agentos_tc_step_complete(gen_step, llm_response_text, llm_response_len,
                                             0.6f, "t2-llm-fallback");
                    if (engine->chain->ctx_window) {
                        agentos_tc_context_window_append(engine->chain->ctx_window,
                                                         llm_response_text, llm_response_len);
                    }
                } else {
                    agentos_tc_step_complete(gen_step, "streaming_loop_failed", 21, 0.3f,
                                             "t2-failed");
                }
            }

            if (engine->memory && engine->chain && gen_step) {
                agentos_tc_step_write_to_memory(engine->chain, gen_step);
            }

            /* C-L12: Write Phase 2 output to memory provider */
            if (llm_response_text && llm_response_len > 0) {
                cognition_provider_write(engine, "execution", llm_response_text, llm_response_len);
            }

            tc3_coordinator_destroy(tc3);
        } else {
            /* Triple coordinator unavailable — use LLM output directly if available */
            if (llm_response_text && llm_response_len > 0) {
                AGENTOS_LOG_INFO("C-L02: Using LLM output directly (tc3 unavailable)");
                if (gen_step) {
                    agentos_tc_step_complete(gen_step, llm_response_text, llm_response_len,
                                             0.55f, "t2-llm-direct");
                }
                if (engine->chain->ctx_window) {
                    agentos_tc_context_window_append(engine->chain->ctx_window,
                                                     llm_response_text, llm_response_len);
                }
                if (engine->memory && engine->chain && gen_step) {
                    agentos_tc_step_write_to_memory(engine->chain, gen_step);
                }
                /* C-L12: Write LLM output to memory provider */
                cognition_provider_write(engine, "execution-direct", llm_response_text, llm_response_len);
            } else {
                AGENTOS_LOG_WARN("Triple coordinator creation failed, falling back to basic loop");
                if (gen_step) {
                    agentos_tc_step_complete(gen_step, input, input_len, 0.5f, "t2-fallback");
                }
            }
        }

        /* Clean up LLM response text */
        if (llm_response_text) {
            AGENTOS_FREE(llm_response_text);
            llm_response_text = NULL;
        }

        if (gen_step) {
            tc_monitor_result_t mon_result;
            agentos_tc_step_monitor(gen_step, NULL, &mon_result);
            if (mon_result.anomaly != TC_ANOMALY_NONE && mon_result.is_critical) {
                trigger_feedback(engine, 1, "step_anomaly", "{\"anomaly\":1,\"critical\":true}");
            }
            if (mon_result.description) {
                AGENTOS_FREE(mon_result.description);
                mon_result.description = NULL;
            }
        }
    }

    /* C-L07: Checkpoint after Phase 2 — execution */
    if (engine->checkpoint_enabled) {
        agentos_cognition_save_checkpoint(engine, engine->checkpoint_last_seq++, "execution");
    }

    /* ── C-L04: tool_d → CoreLoopThree — external tool execution ── */
    {
        tool_service_t *tool_svc = NULL;
        tool_svc_adapter_t *tool_adapter = NULL;
        agentos_mutex_lock(engine->lock);
        tool_svc = engine->tool_svc;
        tool_adapter = engine->tool_adapter;
        agentos_mutex_unlock(engine->lock);

        if ((tool_adapter || tool_svc) && plan && plan->task_plan_nodes
            && plan->task_plan_node_count > 0) {
            AGENTOS_LOG_DEBUG("C-L04: Dispatching %zu tool tasks to tool_d"
                              " (adapter=%s)",
                              plan->task_plan_node_count,
                              tool_adapter ? "IPC" : (tool_svc ? "direct" : "none"));

            for (size_t i = 0; i < plan->task_plan_node_count; i++) {
                agentos_task_node_t *node = plan->task_plan_nodes[i];
                if (!node->task_node_handler_name)
                    continue;

                /* Only dispatch nodes that have handler names (tool tasks) */
                tool_execute_request_t tool_req;
                __builtin_memset(&tool_req, 0, sizeof(tool_req));
                tool_req.tool_id = node->task_node_handler_name;
                /* Use task node ID as params JSON if available, else goal text */
                tool_req.params_json = node->task_node_id
                    ? node->task_node_id : node->task_node_goal;
                tool_req.stream = 0;

                tool_result_t *result = NULL;
                int tool_err = -1;

                /* P3.18 (ACC-DT26): PRE_TOOL hook — 工具执行前触发。
                 * ABORT/SKIP → 跳过该工具；MODIFY → 修改 params_json；CONTINUE/RETRY → 继续。 */
                {
                    hook_decision_t hd = cognition_fire_hook(HOOK_TYPE_PRE_TOOL,
                                                              node->task_node_handler_name,
                                                              tool_req.params_json,
                                                              tool_req.params_json
                                                                  ? strlen(tool_req.params_json) : 0);
                    if (hd == HOOK_DECISION_ABORT || hd == HOOK_DECISION_SKIP) {
                        AGENTOS_LOG_INFO("C-L09: PRE_TOOL hook %s — skipping tool '%s'",
                                         hd == HOOK_DECISION_ABORT ? "ABORT" : "SKIP",
                                         node->task_node_handler_name);
                        continue;
                    }
                    if (hd == HOOK_DECISION_RETRY) {
                        AGENTOS_LOG_DEBUG("C-L09: PRE_TOOL hook RETRY — degraded to CONTINUE (P4)");
                    }
                }

                /* P1.3.1: Prefer IPC adapter path over direct tool service */
                if (tool_adapter) {
                    tool_err = tool_svc_adapter_execute(
                        tool_adapter, &tool_req, &result);
                    AGENTOS_LOG_DEBUG("C-L04: IPC tool '%s' execute: ret=%d",
                                      node->task_node_handler_name, tool_err);
                } else if (tool_svc) {
                    tool_err = tool_service_execute(tool_svc, &tool_req, &result);
                    AGENTOS_LOG_DEBUG("C-L04: Direct tool '%s' execute: ret=%d",
                                      node->task_node_handler_name, tool_err);
                }

                /* P3.18 (ACC-DT26): POST_TOOL hook — 工具执行后触发。
                 * ABORT → 跳出工具循环；其他决策 → 正常继续（仅记录/审计）。 */
                {
                    hook_decision_t hd = cognition_fire_hook(HOOK_TYPE_POST_TOOL,
                                                              node->task_node_handler_name,
                                                              result && result->output
                                                                  ? result->output : NULL,
                                                              result && result->output
                                                                  ? strlen(result->output) : 0);
                    if (hd == HOOK_DECISION_ABORT) {
                        AGENTOS_LOG_WARN("C-L09: POST_TOOL hook ABORT — breaking tool loop at '%s'",
                                         node->task_node_handler_name);
                        if (result) tool_result_free(result);
                        break;
                    }
                }

                if (tool_err == 0 && result && result->success == 0) {
                    AGENTOS_LOG_DEBUG("C-L04: Tool '%s' executed successfully"
                                      " (duration=%llums)",
                                      node->task_node_handler_name,
                                      (unsigned long long)result->duration_ms);
                    if (result->output) {
                        /* P2.9-B6: 回写工具执行结果到 task_node_output，
                         * 供后续阶段（审计、目标对齐）访问 */
                        if (node->task_node_output) {
                            AGENTOS_FREE(node->task_node_output);
                        }
                        node->task_node_output = AGENTOS_STRDUP(result->output);
                        /* Store tool result in working memory */
                        if (engine->chain && engine->chain->working_mem) {
                            agentos_tc_working_memory_store(
                                engine->chain->working_mem,
                                node->task_node_handler_name,
                                result->output,
                                strlen(result->output) + 1,
                                "text/plain", 1);
                        }
                        /* C-L12: Write tool result to memory provider */
                        cognition_provider_write(engine, "tool_execution",
                                                  result->output,
                                                  strlen(result->output));
                    }
                    tool_result_free(result);
                } else {
                    AGENTOS_LOG_WARN("C-L04: Tool '%s' execution failed: ret=%d err=%s",
                                     node->task_node_handler_name, tool_err,
                                     result && result->error ? result->error : "?");
                    /* P1.3.3: Tool failure → continue pipeline (non-blocking) */
                    if (result) tool_result_free(result);
                }
            }
        }
    }

    /* ========== Phase 3: Subtask Audit (S1 + expert S1) ========== */
    if (engine->enable_dual_thinking && engine->meta && engine->chain && plan) {
        agentos_thinking_step_t *audit_step = NULL;
        agentos_tc_step_create(engine->chain, TC_STEP_AUDIT, input, input_len, NULL, 0,
                               &audit_step);
        if (audit_step) {
            char audit_desc[256];
            int ad_len = snprintf(
                audit_desc, sizeof(audit_desc), "audited_plan=%s nodes=%zu corrections=%llu",
                plan->task_plan_id ? plan->task_plan_id : "?", plan->task_plan_node_count,
                (unsigned long long)engine->dual_think_corrections);
            mc_evaluation_result_t eval_audit;
            agentos_mc_evaluate_step(engine->meta, audit_step, input, input_len, &eval_audit);

            agentos_tc_step_complete(audit_step, audit_desc, (size_t)ad_len,
                                     eval_audit.overall_score, "S1-auditor");

            if (engine->memory && engine->chain && audit_step) {
                agentos_tc_metacognition_inform_memory(engine->chain, &eval_audit, audit_step);
                agentos_tc_step_write_to_memory(engine->chain, audit_step);
            }
            /* C-L12: Write audit data to memory provider */
            cognition_provider_write(engine, "audit", audit_desc, (size_t)ad_len);
            if (eval_audit.critique_text) {
                AGENTOS_FREE(eval_audit.critique_text);
                eval_audit.critique_text = NULL;
            }
        }
    }

    /* C-L07: Checkpoint after Phase 3 — audit */
    if (engine->checkpoint_enabled) {
        agentos_cognition_save_checkpoint(engine, engine->checkpoint_last_seq++, "audit");
    }

    /* ========== Phase 4: Enhanced Goal Alignment Check (P2-B05) ========== */
    if (engine->enable_dual_thinking && engine->meta && engine->chain) {
        agentos_thinking_step_t *align_step = NULL;
        agentos_tc_step_create(engine->chain, TC_STEP_ALIGNMENT, input, input_len, NULL, 0,
                               &align_step);
        if (align_step) {
            mc_evaluation_result_t align_eval;
            agentos_mc_evaluate_step(engine->meta, align_step, input, input_len, &align_eval);

            float logic_score = align_eval.overall_score;
            float fact_score = align_eval.overall_score * 0.95f;
            float goal_score = align_eval.is_acceptable ? align_eval.overall_score : 0.3f;

            if (engine->chain->ctx_window) {
                char *recent_ctx = NULL;
                size_t recent_len = 0;
                agentos_tc_context_window_get_recent(engine->chain->ctx_window, 300, &recent_ctx,
                                                     &recent_len);
                if (recent_ctx && recent_len > 0) {
                    int goal_match = (strstr(recent_ctx, "goal") != NULL) ||
                                     (strstr(recent_ctx, "objective") != NULL);
                    goal_score = goal_match ? (goal_score + 0.1f) : (goal_score - 0.15f);
                    if (goal_score > 1.0f)
                        goal_score = 1.0f;
                    if (goal_score < 0.0f)
                        goal_score = 0.0f;
                }
                if (recent_ctx) {
                    AGENTOS_FREE(recent_ctx);
                    recent_ctx = NULL;
                }
            }

            float composite = (logic_score * 0.30f + fact_score * 0.35f + goal_score * 0.35f);

            size_t hist_idx = engine->align_history_count % 8;
            engine->align_history[hist_idx] = composite;
            engine->align_history_count++;

            int aligned = (composite >= 0.65f);
            float drift_trend = 0.0f;
            if (engine->align_history_count >= 3) {
                size_t n = (engine->align_history_count < 8) ? engine->align_history_count : 8;
                float recent_avg = 0.0f, older_avg = 0.0f;
                size_t half = n / 2;
                for (size_t i = 0; i < half; i++) {
                    size_t idx = (engine->align_history_count - 1 - i) % 8;
                    recent_avg += engine->align_history[idx];
                }
                for (size_t i = half; i < n; i++) {
                    size_t idx = (engine->align_history_count - 1 - i) % 8;
                    older_avg += engine->align_history[idx];
                }
                recent_avg /= (float)half;
                older_avg /= (float)(n - half);
                drift_trend = older_avg - recent_avg;
            }

            const char *severity = "ok";
            int trigger_replan = 0;
            if (!aligned || drift_trend > 0.2f) {
                if (drift_trend > 0.35f || composite < 0.3f) {
                    severity = "critical";
                    trigger_replan = 1;
                    engine->align_drift_detected = 1;
                } else if (drift_trend > 0.2f || composite < 0.5f) {
                    severity = "alert";
                } else {
                    severity = "warning";
                }
            }

            agentos_tc_step_complete(align_step, aligned ? "goal_aligned" : "goal_drift_detected",
                                     aligned ? 12 : 18, composite, "S1-alignment");

            if (!aligned || strcmp(severity, "ok") != 0) {
                AGENTOS_LOG_WARN("Goal alignment: %s (score=%.2f trend=%.3f severity=%s)",
                                 aligned ? "marginal" : "DRIFT", composite, drift_trend, severity);

                char fb[384];
                snprintf(fb, sizeof(fb),
                         "{\"composite\":%.2f,\"logic\":%.2f,\"fact\":%.2f,"
                         "\"goal\":%.2f,\"trend\":%.3f,\"severity\":\"%s\","
                         "\"replan\":%s,\"history_count\":%zu}",
                         composite, logic_score, fact_score, goal_score, drift_trend, severity,
                         trigger_replan ? "true" : "false", engine->align_history_count);
                trigger_feedback(engine,
                                 trigger_replan ? 3 : (strcmp(severity, "warning") == 0 ? 1 : 2),
                                 trigger_replan ? "goal_drift_critical" : "goal_drift", fb);

                if (trigger_replan) {
                    /* P2.9-B4: 真正触发重规划 — 替代原先仅 ++count 的死代码。
                     *
                     * 当目标对齐检测到 critical 漂移时，调用 plan_strat->plan
                     * 重新生成任务计划。新计划替换旧计划作为 process() 的输出，
                     * 调用方（loop.c）可据此决定是否用新计划重新执行。
                     *
                     * 重规划失败时保留旧计划并记录 ERROR，不中断当前流程
                     * （旧计划的 Phase 2/3 结果仍可用于返回）。 */
                    engine->align_replan_count++;
                    if (plan_strat && plan_strat->plan) {
                        agentos_task_plan_t *replan = NULL;
                        agentos_error_t replan_err =
                            plan_strat->plan(&intent, plan_strat->data, &replan);
                        if (replan_err == AGENTOS_SUCCESS && replan) {
                            AGENTOS_LOG_WARN("Phase 4: goal drift triggered replan "
                                             "(old_plan_id=%s new_nodes=%zu drift=%.3f composite=%.2f)",
                                             (plan && plan->task_plan_id) ? plan->task_plan_id
                                                                          : "(null)",
                                             replan->task_plan_node_count,
                                             (double)drift_trend, (double)composite);
                            /* 生成新 plan_id 以区分重规划产生的计划 */
                            if (replan->task_plan_id) {
                                AGENTOS_FREE(replan->task_plan_id);
                            }
                            char rp_id[64];
                            int rp_n = snprintf(rp_id, sizeof(rp_id),
                                                "replan_%016" PRIx64,
                                                (uint64_t)agentos_time_monotonic_ns());
                            if (rp_n > 0 && (size_t)rp_n < sizeof(rp_id)) {
                                replan->task_plan_id = AGENTOS_STRDUP(rp_id);
                                replan->task_plan_id_len = (size_t)rp_n;
                            }
                            /* 释放旧计划，使用新计划 */
                            if (plan) {
                                agentos_task_plan_free(plan);
                            }
                            plan = replan;
                            engine->current_plan = plan;

                            char rp_fb[256];
                            snprintf(rp_fb, sizeof(rp_fb),
                                     "{\"replan\":true,\"new_plan_id\":\"%s\","
                                     "\"new_nodes\":%zu,\"drift_trend\":%.3f,\"composite\":%.2f}",
                                     (plan && plan->task_plan_id) ? plan->task_plan_id : "",
                                     plan ? plan->task_plan_node_count : 0,
                                     (double)drift_trend, (double)composite);
                            trigger_feedback(engine, 3, "goal_drift_replan", rp_fb);
                        } else {
                            AGENTOS_LOG_ERROR("Phase 4: replan failed (err=%d), "
                                              "retaining old plan",
                                              (int)replan_err);
                        }
                    } else {
                        AGENTOS_LOG_WARN("Phase 4: goal drift detected but no plan_strategy "
                                         "available for replan");
                    }
                }
            }

            if (align_eval.critique_text) {
                AGENTOS_FREE(align_eval.critique_text);
                align_eval.critique_text = NULL;
            }

            if (engine->memory && engine->chain && align_step) {
                agentos_tc_metacognition_inform_memory(engine->chain, &align_eval, align_step);
            }

            /* C-L12: Write alignment result to memory provider */
            {
                char align_buf[256];
                int ab_len = snprintf(align_buf, sizeof(align_buf),
                                      "{\"aligned\":%s,\"composite\":%.2f,\"drift_trend\":%.3f}",
                                      aligned ? "true" : "false", composite, drift_trend);
                if (ab_len > 0 && (size_t)ab_len < sizeof(align_buf)) {
                    cognition_provider_write(engine, "alignment", align_buf, (size_t)ab_len);
                }
            }

            agentos_mc_detect_patterns(engine->meta, NULL, NULL);
            agentos_mc_adapt_threshold(engine->meta);
        }
    }

    /* C-L07: Checkpoint after Phase 4 — alignment */
    if (engine->checkpoint_enabled) {
        agentos_cognition_save_checkpoint(engine, engine->checkpoint_last_seq++, "alignment");
    }

    /* ========== Stream Critic Phase 3+4: Output Correction + Memory Confirmation ========== */
    if (engine->stream_critic && plan) {
        char *pipeline_input = (char *)input;
        size_t pipeline_input_len = input_len;
        char *pipeline_output = NULL;
        size_t pipeline_output_len = 0;

        if (engine->chain && engine->chain->ctx_window) {
            agentos_tc_context_window_get_recent(engine->chain->ctx_window, 2000, &pipeline_output,
                                                 &pipeline_output_len);
        }

        if (!pipeline_output || pipeline_output_len == 0) {
            /* P1.14: 释放 get_recent 返回的空字符串，避免覆盖后泄漏 */
            if (pipeline_output) {
                AGENTOS_FREE(pipeline_output);
                pipeline_output = NULL;
            }
            pipeline_output = AGENTOS_STRDUP(input);
            pipeline_output_len = input_len;
        }

        char *final_output = NULL;
        size_t final_output_len = 0;
        float final_quality = 0.0f;

        agentos_error_t scp_err = sc_stream_critic_pipeline(
            engine->stream_critic, pipeline_input, pipeline_input_len, pipeline_output,
            pipeline_output_len, engine->memory, &final_output, &final_output_len, &final_quality);

        if (scp_err == AGENTOS_SUCCESS && final_output) {
            char scp_fb[384];
            snprintf(scp_fb, sizeof(scp_fb), "{\"quality\":%.2f,\"output_len\":%zu}", final_quality,
                     final_output_len);
            trigger_feedback(engine, 2, "stream_critic_complete", scp_fb);
            AGENTOS_FREE(final_output);
            final_output = NULL;
        } else if (scp_err != AGENTOS_SUCCESS) {
            AGENTOS_LOG_WARN("Stream critic pipeline failed: err=%d", (int)scp_err);
        }

        if (pipeline_output && pipeline_output != input) {
            AGENTOS_FREE(pipeline_output);
            pipeline_output = NULL;
        }
    }

    /* ========== Finalize ========== */
    uint64_t end_ns = agentos_time_monotonic_ns();
    uint64_t elapsed = end_ns - start_ns;

    agentos_mutex_lock(engine->lock);
    engine->stats_processed++;
    engine->stats_total_time_ns += elapsed;
    engine->stats_success_count++;
    agentos_mutex_unlock(engine->lock);

    char feedback_buf[512];
    snprintf(feedback_buf, sizeof(feedback_buf),
             "{\"plan_id\":\"%s\",\"node_count\":%zu,\"elapsed_ns\":%llu,"
             "\"dual_think\":%d,\"corrections\":%llu,\"status\":\"success\"}",
             plan->task_plan_id ? plan->task_plan_id : "unknown", plan->task_plan_node_count,
             (unsigned long long)elapsed, engine->enable_dual_thinking,
             (unsigned long long)engine->dual_think_corrections);
    trigger_feedback(engine, 0, "process_complete", feedback_buf);

    /* P3.18 (ACC-DT26): POST_EXEC hook — 认知处理成功完成后触发（仅记录/审计）。 */
    cognition_fire_hook(HOOK_TYPE_POST_EXEC, "cognition_process_complete",
                        input, input_len);

    /* C-L07: Clear current plan reference before returning ownership to caller */
    engine->current_plan = NULL;

    *out_plan = plan;
    return AGENTOS_SUCCESS;

process_fail:
    /* P3.18 (ACC-DT26): ON_ERROR hook — 认知处理失败时触发（仅记录/审计）。 */
    cognition_fire_hook(HOOK_TYPE_ON_ERROR, "cognition_process_fail",
                        input, input_len);
    /* P1.14: 释放已生成的 plan（plan_strat->plan 返回错误但仍设置了 plan 的防御性释放） */
    if (plan) {
        agentos_task_plan_free(plan);
        plan = NULL;
    }
    if (engine->chain) {
        agentos_tc_chain_stop(engine->chain);
        agentos_tc_chain_destroy(engine->chain);
        engine->chain = NULL;
    }
    return err;
}

void agentos_task_plan_free(agentos_task_plan_t *plan)
{
    if (!plan)
        return;
    for (size_t i = 0; i < plan->task_plan_node_count; i++) {
        agentos_task_node_t *node = plan->task_plan_nodes[i];
        if (node) {
            if (node->task_node_id)
                AGENTOS_FREE(node->task_node_id);
            if (node->task_node_agent_role)
                AGENTOS_FREE(node->task_node_agent_role);
            /* P2.9: 释放 handler_name、goal、output（原先遗漏导致潜在泄漏） */
            if (node->task_node_handler_name)
                AGENTOS_FREE(node->task_node_handler_name);
            if (node->task_node_goal)
                AGENTOS_FREE(node->task_node_goal);
            if (node->task_node_output)
                AGENTOS_FREE(node->task_node_output);
            /* task_node_input 由策略管理，不由 plan_free 释放 */
            if (node->task_node_depends_on) {
                for (size_t j = 0; j < node->task_node_depends_count; j++) {
                    AGENTOS_FREE(node->task_node_depends_on[j]);
                }
                AGENTOS_FREE(node->task_node_depends_on);
            }
            AGENTOS_FREE(node);
        }
    }
    AGENTOS_FREE(plan->task_plan_nodes);
    /* P1.14: 释放 entry_points 数组中的每个字符串（reactive/hierarchical 规划器
     * 通过 AGENTOS_STRDUP 分配 entry_points[i]，原先只释放数组本身导致泄漏） */
    if (plan->task_plan_entry_points) {
        for (size_t e = 0; e < plan->task_plan_entry_count; e++) {
            if (plan->task_plan_entry_points[e])
                AGENTOS_FREE(plan->task_plan_entry_points[e]);
        }
        AGENTOS_FREE(plan->task_plan_entry_points);
    }
    if (plan->task_plan_id)
        AGENTOS_FREE(plan->task_plan_id);
    AGENTOS_FREE(plan);
}

agentos_error_t agentos_cognition_stats(agentos_cognition_engine_t *engine, char **out_stats,
                                        size_t *out_len)
{

    if (!engine || !out_stats)
        return AGENTOS_EINVAL;

    agentos_mutex_lock(engine->lock);
    uint32_t processed = engine->stats_processed;
    uint64_t avg_ns = (processed > 0) ? (engine->stats_total_time_ns / processed) : 0;
    uint64_t dt_inv = engine->dual_think_invocations;
    uint64_t dt_corr = engine->dual_think_corrections;
    agentos_mutex_unlock(engine->lock);

    char buffer[512];
    int len = snprintf(buffer, sizeof(buffer),
                       "{\"processed\":%u,\"avg_time_ns\":%llu,"
                       "\"dual_think_invocations\":%llu,\"dual_think_corrections\":%llu}",
                       processed, (unsigned long long)avg_ns, (unsigned long long)dt_inv,
                       (unsigned long long)dt_corr);

    char *result = (char *)AGENTOS_MALLOC(len + 1);
    if (!result)
        return AGENTOS_ENOMEM;
    __builtin_memcpy(result, buffer, len + 1);

    *out_stats = result;
    if (out_len)
        *out_len = (size_t)len;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_cognition_health_check(agentos_cognition_engine_t *engine, char **out_json)
{

    if (!engine || !out_json)
        return AGENTOS_EINVAL;

#ifdef AGENTOS_HAS_CJSON
    cJSON *root = cJSON_CreateObject();
    if (!root)
        return AGENTOS_ENOMEM;

    cJSON_AddStringToObject(root, "status", "healthy");

    agentos_mutex_lock(engine->lock);
    cJSON_AddNumberToObject(root, "processed", engine->stats_processed);
    cJSON_AddNumberToObject(root, "avg_time_ns",
                            (engine->stats_processed > 0)
                                ? (engine->stats_total_time_ns / engine->stats_processed)
                                : 0);
    cJSON_AddBoolToObject(root, "dual_thinking_enabled", engine->enable_dual_thinking);
    if (engine->chain) {
        size_t anomaly_count = 0;
        int has_critical = 0;
        agentos_tc_chain_health_check(engine->chain, &anomaly_count, &has_critical);
        cJSON_AddNumberToObject(root, "chain_anomalies", anomaly_count);
        cJSON_AddBoolToObject(root, "chain_critical", has_critical);
    }
    agentos_mutex_unlock(engine->lock);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json)
        return AGENTOS_ENOMEM;

    *out_json = json;
    return AGENTOS_SUCCESS;
#else
    (void)engine;
    *out_json = NULL;
    return AGENTOS_ENOTSUP;
#endif
}

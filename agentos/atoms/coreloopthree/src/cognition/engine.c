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

/* C-L12: Write a memory record to the provider (if available) */
static int cognition_provider_write(agentos_cognition_engine_t *engine, const char *phase,
                                    const char *content, size_t content_len)
{
    if (!engine || !engine->memory_provider || !content || content_len == 0)
        return 0;

    if (!engine->memory_provider->write_raw)
        return 0;

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
    trigger_feedback(engine, 2, "engine_created", "{\"status\":\"initialized\"}");
    return AGENTOS_SUCCESS;
}

void agentos_cognition_destroy(agentos_cognition_engine_t *engine)
{
    if (!engine)
        return;
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

    agentos_intent_t intent;
    __builtin_memset(&intent, 0, sizeof(intent));
    intent.intent_raw_text = (char *)input;
    intent.intent_raw_len = input_len;
    intent.intent_goal = (char *)input;
    intent.intent_goal_len = input_len;
    intent.intent_flags = 0;
    intent.intent_context = engine->context;

    uint64_t start_ns = agentos_time_monotonic_ns();

    /* C-L12.4: Retrieve relevant context from memory provider before processing */
    if (engine->memory_provider && engine->memory_provider->retrieve) {
        char **ctx_ids = NULL;
        float *ctx_scores = NULL;
        size_t ctx_count = 0;
        agentos_error_t qerr = engine->memory_provider->retrieve(
            engine->memory_provider, input, 5, &ctx_ids, &ctx_scores, &ctx_count);
        if (qerr == AGENTOS_SUCCESS && ctx_count > 0 && ctx_ids) {
            AGENTOS_LOG_DEBUG("C-L12: Provider retrieve returned %zu context records", ctx_count);
            /* Context records are available for the pipeline to use.
             * The thinking chain prepopulate will handle integration. */
            for (size_t i = 0; i < ctx_count && ctx_ids[i]; i++) {
                AGENTOS_FREE(ctx_ids[i]);
            }
            AGENTOS_FREE(ctx_ids);
            if (ctx_scores)
                AGENTOS_FREE(ctx_scores);
        }
    }

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
            if (decomp_step) {
                agentos_tc_step_complete(decomp_step, input, input_len, 0.8f, "S1-decomposer");
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
    agentos_task_plan_t *plan = NULL;
    agentos_error_t err = AGENTOS_EUNKNOWN;

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

            /* C-L02 P1.2.2: Async streaming callback support */
            int use_streaming = 0;
            llm_stream_callback_t stream_cb = NULL;
            void *stream_cb_data = NULL;

            agentos_mutex_lock(engine->lock);
            use_streaming = engine->llm_stream_enabled;
            stream_cb = engine->llm_stream_callback;
            stream_cb_data = engine->llm_stream_user_data;
            agentos_mutex_unlock(engine->lock);

            /* P1.2.1: Prefer IPC adapter path over direct LLM service */
            if (llm_adapter) {
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
            } else if (llm_svc) {
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
        }

        /* ── Triple coordinator: verify LLM output (or generate locally) ── */
        tc3_config_t tc3_cfg = TC3_CONFIG_DEFAULTS;
        tc3_cfg.s2_generate = NULL;
        tc3_cfg.s1_verify = NULL;
        tc3_cfg.s1_expert = NULL;

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

                char fb[512];
                snprintf(fb, sizeof(fb),
                         "{\"units\":%u,\"accepted\":%u,\"corrections\":%u,"
                         "\"avg_score\":%.2f,\"time_ns\":%llu,\"llm_backed\":%s}",
                         tc3_stats.total_units, tc3_stats.accepted_units,
                         tc3_stats.total_corrections, tc3_stats.avg_score,
                         (unsigned long long)tc3_stats.total_time_ns,
                         llm_svc ? "true" : "false");
                trigger_feedback(engine, 2, "phase2_critical_loop", fb);

                AGENTOS_FREE(phase2_output);
                phase2_output = NULL;
            } else if (gen_step) {
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
                if (tool_err == 0 && result && result->success == 0) {
                    AGENTOS_LOG_DEBUG("C-L04: Tool '%s' executed successfully"
                                      " (duration=%llums)",
                                      node->task_node_handler_name,
                                      (unsigned long long)result->duration_ms);
                    if (result->output) {
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
                    engine->align_replan_count++;
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

    /* C-L07: Clear current plan reference before returning ownership to caller */
    engine->current_plan = NULL;

    *out_plan = plan;
    return AGENTOS_SUCCESS;

process_fail:
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
    if (plan->task_plan_entry_points)
        AGENTOS_FREE(plan->task_plan_entry_points);
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

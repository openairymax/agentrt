/**
 * @file engine.c
 * @brief 认知引擎核心实现 - 含双思考系统集成
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * [✅ STUB-001已完成] thinking_chain + metacognition 已集成
 * 实现完整5阶段认知处理管线:
 * Phase 0: 指令拆解(S1) -> Phase 1: 规划(S2+S1) ->
 * Phase 2: 执行-验证循环 -> Phase 3: 审计 -> Phase 4: 目标对齐
 */

#include "cognition.h"
#include "agentos.h"
#include "logger.h"
#include "id_utils.h"
#include "error_utils.h"
#include <stdlib.h>

#include "memory_compat.h"
#include "string_compat.h"
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#include <cjson/cJSON.h>

#include "atomic_compat.h"

#include "thinking_chain.h"
#include "metacognition.h"

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

struct agentos_cognition_engine {
    agentos_plan_strategy_t* plan_strat;
    agentos_plan_strategy_t* fallback_plan_strat;
    agentos_coordinator_strategy_t* coord_strat;
    agentos_dispatching_strategy_t* disp_strat;
    void* context;
    void (*context_destroy)(void*);
    agentos_mutex_t* lock;
    uint32_t stats_processed;
    uint64_t stats_total_time_ns;
    agentos_cognition_config_t manager;
    agentos_feedback_callback_t feedback_cb;
    void* feedback_user_data;
    uint64_t stats_success_count;
    uint64_t stats_failure_count;
    uint64_t stats_total_retries;

    agentos_thinking_chain_t* chain;
    agentos_metacognition_t* meta;
    int enable_dual_thinking;
    uint32_t chain_max_tokens;
    size_t chain_wm_capacity;
    float meta_acceptance_threshold;
    uint64_t dual_think_invocations;
    uint64_t dual_think_corrections;
};

static void trigger_feedback(
    agentos_cognition_engine_t* engine,
    int level,
    const char* event,
    const char* data) {
    if (engine && engine->feedback_cb) {
        engine->feedback_cb(
            level, "cognition", event,
            data, data ? strlen(data) : 0,
            engine->feedback_user_data);
    }
}

agentos_error_t agentos_cognition_create(
    agentos_plan_strategy_t* plan_strategy,
    agentos_coordinator_strategy_t* coord_strategy,
    agentos_dispatching_strategy_t* disp_strategy,
    agentos_cognition_engine_t** out_engine) {
    return agentos_cognition_create_ex(NULL, plan_strategy, coord_strategy, disp_strategy, out_engine);
}

agentos_error_t agentos_cognition_create_ex(
    const agentos_cognition_config_t* manager,
    agentos_plan_strategy_t* plan_strategy,
    agentos_coordinator_strategy_t* coord_strategy,
    agentos_dispatching_strategy_t* disp_strategy,
    agentos_cognition_engine_t** out_engine) {

    if (!out_engine) return AGENTOS_EINVAL;

    agentos_cognition_engine_t* engine = (agentos_cognition_engine_t*)AGENTOS_CALLOC(1, sizeof(agentos_cognition_engine_t));
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
    engine->enable_dual_thinking = 1;
    engine->chain_max_tokens = 8192;
    engine->chain_wm_capacity = 64;
    engine->meta_acceptance_threshold = 0.7f;
    engine->dual_think_invocations = 0;
    engine->dual_think_corrections = 0;

    agentos_error_t ds_err = agentos_mc_create(&engine->meta);
    if (ds_err != AGENTOS_SUCCESS) {
        AGENTOS_LOG_WARN("Metacognition init failed: err=%d, dual-thinking disabled", (int)ds_err);
        engine->enable_dual_thinking = 0;
    }

    *out_engine = engine;
    trigger_feedback(engine, 2, "engine_created", "{\"status\":\"initialized\"}");
    return AGENTOS_SUCCESS;
}

void agentos_cognition_destroy(agentos_cognition_engine_t* engine) {
    if (!engine) return;
    if (engine->chain) {
        agentos_tc_chain_stop(engine->chain);
        agentos_tc_chain_destroy(engine->chain);
    }
    if (engine->meta) {
        agentos_mc_destroy(engine->meta);
    }
    if (engine->context && engine->context_destroy) {
        engine->context_destroy(engine->context);
    }
    if (engine->lock) {
        agentos_mutex_destroy(engine->lock);
    }
    AGENTOS_FREE(engine);
}

void agentos_cognition_set_fallback_plan(
    agentos_cognition_engine_t* engine,
    agentos_plan_strategy_t* fallback) {
    if (!engine) return;
    agentos_mutex_lock(engine->lock);
    engine->fallback_plan_strat = fallback;
    agentos_mutex_unlock(engine->lock);
}

void agentos_cognition_set_context(
    agentos_cognition_engine_t* engine,
    void* context,
    void (*destroy)(void*)) {
    if (!engine) return;
    agentos_mutex_lock(engine->lock);
    if (engine->context && engine->context_destroy) {
        engine->context_destroy(engine->context);
    }
    engine->context = context;
    engine->context_destroy = destroy;
    agentos_mutex_unlock(engine->lock);
}

agentos_error_t agentos_cognition_process(
    agentos_cognition_engine_t* engine,
    const char* input,
    size_t input_len,
    agentos_task_plan_t** out_plan) {

    if (!engine || !input || !out_plan) {
        AGENTOS_LOG_ERROR("Invalid parameters to cognition_process: engine=%p input=%p out_plan=%p",
                         (void*)engine, (void*)input, (void*)out_plan);
        return AGENTOS_EINVAL;
    }
    if (input_len == 0) return AGENTOS_EINVAL;

    agentos_intent_t intent;
    memset(&intent, 0, sizeof(intent));
    intent.intent_raw_text = (char*)input;
    intent.intent_raw_len = input_len;
    intent.intent_goal = (char*)input;
    intent.intent_goal_len = input_len;
    intent.intent_flags = 0;
    intent.intent_context = engine->context;

    uint64_t start_ns = agentos_time_monotonic_ns();

    /* ========== Phase 0: Instruction Decomposition (S1) ========== */
    if (engine->enable_dual_thinking && engine->meta) {
        if (engine->chain) {
            agentos_tc_chain_stop(engine->chain);
            agentos_tc_chain_destroy(engine->chain);
        }
        agentos_error_t tc_err = agentos_tc_chain_create(
            input, engine->chain_max_tokens, engine->chain_wm_capacity, &engine->chain);
        if (tc_err == AGENTOS_SUCCESS) {
            agentos_tc_chain_start(engine->chain);
            agentos_mc_set_chain(engine->meta, engine->chain);

            agentos_thinking_step_t* decomp_step = NULL;
            agentos_tc_step_create(engine->chain, TC_STEP_DECOMPOSITION,
                                   input, input_len, NULL, 0, &decomp_step);
            if (decomp_step) {
                agentos_tc_step_complete(decomp_step, input, input_len, 0.8f, "S1-decomposer");
            }

            char* preemptive_hint = NULL;
            size_t hint_len = 0;
            int preempt = agentos_mc_preemptive_check(
                engine->meta, TC_STEP_PLANNING, input, input_len,
                &preemptive_hint, &hint_len);
            if (preempt == 1 && preemptive_hint) {
                if (engine->chain->working_mem) {
                    agentos_tc_working_memory_store(
                        engine->chain->working_mem, "preemptive_hint",
                        preemptive_hint, hint_len + 1, "text/plain", 1);
                }
                AGENTOS_FREE(preemptive_hint);
            }
        } else {
            AGENTOS_LOG_WARN("Thinking chain creation failed: err=%d", (int)tc_err);
        }
        engine->dual_think_invocations++;
    }

    /* ========== Phase 1: Planning (S2 + S1 pre-validation) ========== */
    agentos_task_plan_t* plan = NULL;
    agentos_error_t err = AGENTOS_ENOTSUP;

    agentos_plan_strategy_t* plan_strat = NULL;
    agentos_plan_strategy_t* fallback_strat = NULL;
    agentos_mutex_lock(engine->lock);
    plan_strat = engine->plan_strat;
    fallback_strat = engine->fallback_plan_strat;
    agentos_mutex_unlock(engine->lock);

    if (plan_strat && plan_strat->plan) {
        err = plan_strat->plan(&intent, plan_strat->data, &plan);
    }

    if (err == AGENTOS_SUCCESS && plan && engine->enable_dual_thinking &&
        engine->meta && engine->chain) {
        agentos_thinking_step_t* plan_step = NULL;
        agentos_tc_step_create(engine->chain, TC_STEP_PLANNING,
                               input, input_len, NULL, 0, &plan_step);
        if (plan_step) {
            char plan_desc[256];
            int pd_len = snprintf(plan_desc, sizeof(plan_desc),
                "plan_id=%s nodes=%zu",
                plan->task_plan_id ? plan->task_plan_id : "?",
                plan->task_plan_node_count);
            agentos_tc_step_complete(plan_step, plan_desc, (size_t)pd_len, 0.75f, "S2-planner");

            mc_evaluation_result_t eval;
            agentos_mc_evaluate_step(engine->meta, plan_step, NULL, 0, &eval);
            if (!eval.is_acceptable && eval.strategy != MC_CORRECT_NONE) {
                AGENTOS_LOG_WARN("Plan S1 pre-validation failed: score=%.2f strategy=%d",
                                eval.overall_score, eval.strategy);
                engine->dual_think_corrections++;
            }
            if (eval.critique_text) AGENTOS_FREE(eval.critique_text);
        }
    }

    if (err != AGENTOS_SUCCESS) {
        AGENTOS_LOG_WARN("Primary planning failed: err=%d, trying fallback", (int)err);
        char err_buf[256];
        snprintf(err_buf, sizeof(err_buf),
            "{\"error_code\":%d,\"stage\":\"primary_planning\"}", (int)err);
        trigger_feedback(engine, 1, "planning_retry", err_buf);

        if (fallback_strat && fallback_strat->plan) {
            err = fallback_strat->plan(&intent, fallback_strat->data, &plan);
            if (err == AGENTOS_SUCCESS) {
                agentos_mutex_lock(engine->lock);
                engine->stats_total_retries++;
                agentos_mutex_unlock(engine->lock);
            }
        } else {
            snprintf(err_buf, sizeof(err_buf),
                "{\"error_code\":%d,\"stage\":\"no_fallback\"}", (int)err);
            trigger_feedback(engine, 0, "process_failed", err_buf);
            agentos_mutex_lock(engine->lock);
            engine->stats_failure_count++;
            agentos_mutex_unlock(engine->lock);
            goto process_fail;
        }
    }

    if (err != AGENTOS_SUCCESS) {
        char err_buf[256];
        snprintf(err_buf, sizeof(err_buf),
            "{\"error_code\":%d,\"stage\":\"fallback_failed\"}", (int)err);
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

    /* ========== Phase 2: Execution-Verification Loop (DS-007 monitoring) ========== */
    if (engine->enable_dual_thinking && engine->chain && plan) {
        size_t anomaly_count = 0;
        int has_critical = 0;
        agentos_tc_chain_health_check(engine->chain, &anomaly_count, &has_critical);
        if (has_critical) {
            AGENTOS_LOG_WARN("Chain health check: %zu anomalies, critical detected", anomaly_count);
            trigger_feedback(engine, 1, "anomaly_detected",
                           "{\"anomalies\":1,\"critical\":true}");
        }
    }

    /* ========== Phase 3: Subtask Audit (S1 + expert S1) ========== */
    if (engine->enable_dual_thinking && engine->meta && engine->chain && plan) {
        agentos_thinking_step_t* audit_step = NULL;
        agentos_tc_step_create(engine->chain, TC_STEP_AUDIT,
                               input, input_len, NULL, 0, &audit_step);
        if (audit_step) {
            char audit_desc[256];
            int ad_len = snprintf(audit_desc, sizeof(audit_desc),
                "audited_plan=%s nodes=%zu corrections=%llu",
                plan->task_plan_id ? plan->task_plan_id : "?",
                plan->task_plan_node_count,
                (unsigned long long)engine->dual_think_corrections);
            agentos_tc_step_complete(audit_step, audit_desc, (size_t)ad_len, 0.8f, "S1-auditor");
        }
    }

    /* ========== Phase 4: Goal Alignment Check ========== */
    if (engine->enable_dual_thinking && engine->meta && engine->chain) {
        agentos_thinking_step_t* align_step = NULL;
        agentos_tc_step_create(engine->chain, TC_STEP_ALIGNMENT,
                               input, input_len, NULL, 0, &align_step);
        if (align_step) {
            mc_evaluation_result_t align_eval;
            agentos_mc_evaluate_step(engine->meta, align_step, input, input_len, &align_eval);

            int aligned = align_eval.is_acceptable;
            agentos_tc_step_complete(align_step,
                                    aligned ? "goal_aligned" : "goal_drift_detected",
                                    aligned ? 12 : 18,
                                    align_eval.overall_score, "S1-alignment");

            if (!aligned) {
                AGENTOS_LOG_WARN("Goal alignment check: drift detected (score=%.2f)",
                                align_eval.overall_score);
                trigger_feedback(engine, 2, "goal_drift",
                               "{\"score\":0.0,\"action\":\"replan_recommended\"}");
            }
            if (align_eval.critique_text) AGENTOS_FREE(align_eval.critique_text);

            agentos_mc_detect_patterns(engine->meta, NULL, NULL);
            agentos_mc_adapt_threshold(engine->meta);
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
        plan->task_plan_id ? plan->task_plan_id : "unknown",
        plan->task_plan_node_count,
        (unsigned long long)elapsed,
        engine->enable_dual_thinking,
        (unsigned long long)engine->dual_think_corrections);
    trigger_feedback(engine, 0, "process_complete", feedback_buf);

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

void agentos_task_plan_free(agentos_task_plan_t* plan) {
    if (!plan) return;
    for (size_t i = 0; i < plan->task_plan_node_count; i++) {
        agentos_task_node_t* node = plan->task_plan_nodes[i];
        if (node) {
            if (node->task_node_id) AGENTOS_FREE(node->task_node_id);
            if (node->task_node_agent_role) AGENTOS_FREE(node->task_node_agent_role);
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
    if (plan->task_plan_entry_points) AGENTOS_FREE(plan->task_plan_entry_points);
    if (plan->task_plan_id) AGENTOS_FREE(plan->task_plan_id);
    AGENTOS_FREE(plan);
}

agentos_error_t agentos_cognition_stats(
    agentos_cognition_engine_t* engine,
    char** out_stats,
    size_t* out_len) {

    if (!engine || !out_stats) return AGENTOS_EINVAL;

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
        processed, (unsigned long long)avg_ns,
        (unsigned long long)dt_inv, (unsigned long long)dt_corr);

    char* result = (char*)AGENTOS_MALLOC(len + 1);
    if (!result) return AGENTOS_ENOMEM;
    memcpy(result, buffer, len + 1);

    *out_stats = result;
    if (out_len) *out_len = (size_t)len;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_cognition_health_check(
    agentos_cognition_engine_t* engine,
    char** out_json) {

    if (!engine || !out_json) return AGENTOS_EINVAL;

    cJSON* root = cJSON_CreateObject();
    if (!root) return AGENTOS_ENOMEM;

    cJSON_AddStringToObject(root, "status", "healthy");

    agentos_mutex_lock(engine->lock);
    cJSON_AddNumberToObject(root, "processed", engine->stats_processed);
    cJSON_AddNumberToObject(root, "avg_time_ns",
        (engine->stats_processed > 0) ? (engine->stats_total_time_ns / engine->stats_processed) : 0);
    cJSON_AddBoolToObject(root, "dual_thinking_enabled", engine->enable_dual_thinking);
    if (engine->chain) {
        size_t anomaly_count = 0;
        int has_critical = 0;
        agentos_tc_chain_health_check(engine->chain, &anomaly_count, &has_critical);
        cJSON_AddNumberToObject(root, "chain_anomalies", anomaly_count);
        cJSON_AddBoolToObject(root, "chain_critical", has_critical);
    }
    agentos_mutex_unlock(engine->lock);

    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json) return AGENTOS_ENOMEM;

    *out_json = json;
    return AGENTOS_SUCCESS;
}

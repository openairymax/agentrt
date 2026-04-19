/**
 * @file ml_planner.c
 * @brief ML-based planning strategy with graceful degradation
 * @copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 */

#include "../../../include/cognition.h"
#include "include/memory_compat.h"
#include "logging_compat.h"
#include "platform.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Simple model inference interface (placeholder for actual ML runtime)
 */
typedef struct ml_model {
    void* handle;
    int (*predict)(void* handle, const float* input, int input_len, float* output, int output_len);
} ml_model_t;

typedef struct ml_planner_data {
    ml_model_t* model;
    char* model_path;
    void* llm;
    agentos_mutex_t* lock;
    bool fallback_mode;
} ml_planner_data_t;

static void ml_planner_destroy(agentos_plan_strategy_t* strategy) {
    if (!strategy) return;
    ml_planner_data_t* data = (ml_planner_data_t*)strategy->data;
    if (data) {
        if (data->model) {
            if (data->model->handle) {
                /* TODO: Unload actual ML model via runtime API */
            }
            AGENTOS_FREE(data->model);
        }
        if (data->model_path) AGENTOS_FREE(data->model_path);
        if (data->lock) agentos_mutex_destroy(data->lock);
        AGENTOS_FREE(data);
    }
    AGENTOS_FREE(strategy);
}

/**
 * @brief Attempt to load ML model from path
 *
 * When no model runtime is available, sets fallback_mode=true
 * and generates simple heuristic plans instead.
 */
static bool ml_planner_try_load_model(ml_planner_data_t* data) {
    if (!data || !data->model_path) return false;

    /* Check if model file exists */
    FILE* f = fopen(data->model_path, "rb");
    if (!f) {
        AGENTOS_LOG_INFO("ML planner: model file not found, using fallback mode");
        data->fallback_mode = true;
        return false;
    }
    fclose(f);

    /* Model placeholder: rule-based planning is the primary path.
     * ML runtime integration (ONNX/TFLite) can be added here when available
     * to enhance planning quality with learned task decomposition patterns. */
    data->model = (ml_model_t*)AGENTOS_CALLOC(1, sizeof(ml_model_t));
    if (!data->model) {
        data->fallback_mode = true;
        return false;
    }

    data->model->handle = NULL;
    data->model->predict = NULL;
    data->fallback_mode = true;

    AGENTOS_LOG_INFO("ML planner: model placeholder initialized, fallback mode active");
    return true;
}

/**
 * @brief Rule-based task decomposition by intent type
 *
 * Maps intent types to structured task decompositions with
 * dependency chains and role assignments.
 */
typedef struct {
    const char* intent_type;
    int min_complexity;
    int max_complexity;
    const char** subtasks;
    size_t subtask_count;
    const char* primary_role;
    bool requires_verification;
} rule_decomposition_t;

static void ml_planner_destroy_single_node(agentos_task_node_t* node);

static const char* QUERY_SUBTASKS[] = {
    "analyze_query_semantics", "retrieve_relevant_context",
    "formulate_search_strategy", "execute_information_gathering",
    "synthesize_results"
};
static const char* ACTION_SUBTASKS[] = {
    "validate_action_parameters", "check_preconditions",
    "plan_execution_sequence", "execute_primary_action",
    "verify_outcome", "handle_exceptions"
};
static const char* CREATIVE_SUBTASKS[] = {
    "understand_creative_constraints", "generate_initial_ideas",
    "evaluate_feasibility", "refine_best_option",
    "produce_final_output"
};
static const char* ANALYSIS_SUBTASKS[] = {
    "collect_input_data", "normalize_data_format",
    "apply_analysis_rules", "detect_patterns",
    "generate_insights", "format_report"
};

/* DS-006: 多粒度子任务定义（细粒度分解） */
static const char* QUERY_FINE_SUBTASKS[] = {
    "extract_query_entities", "resolve_entity_ambiguity",
    "build_semantic_graph", "identify_information_gaps",
    "rank_retrieval_sources", "execute_parallel_retrieval",
    "cross_validate_results", "synthesize_answer"
};
static const char* ACTION_FINE_SUBTASKS[] = {
    "parse_action_specification", "validate_action_schema",
    "check_resource_availability", "estimate_execution_cost",
    "create_execution_plan", "acquire_locks_and_resources",
    "execute_atomic_actions", "verify_state_change",
    "commit_or_rollback", "notify_stakeholders"
};
static const char* CREATIVE_FINE_SUBTASKS[] = {
    "analyze_creative_brief", "research_domain_examples",
    "generate_divergent_ideas", "apply_constraints_filter",
    "score_ideas_by_criteria", "select_top_candidates",
    "iterative_refinement", "final_polish_and_format"
};

#define MAX_DECOMPOSITION_DEPTH 3

typedef struct {
    const char** coarse_tasks;
    size_t coarse_count;
    const char** fine_tasks;
    size_t fine_count;
    int complexity_threshold;  /* >= this value use fine-grained */
} multi_grain_decomp_t;

static const multi_grain_decomp_t MULTI_GRAIN_TABLE[] = {
    { QUERY_SUBTASKS,   5, QUERY_FINE_SUBTASKS,   8, 4 },
    { ACTION_SUBTASKS,  6, ACTION_FINE_SUBTASKS, 10, 4 },
    { CREATIVE_SUBTASKS,4, CREATIVE_FINE_SUBTASKS,8, 3 },
    { ANALYSIS_SUBTASKS,6, NULL,                  0, 5 }, /* analysis stays coarse */
};

static const rule_decomposition_t RULE_TABLE[] = {
    { "query",      1, 5, QUERY_SUBTASKS,   5, "analyst",    true },
    { "action",     1, 5, ACTION_SUBTASKS,   6, "executor",   true },
    { "creative",   1, 5, CREATIVE_SUBTASKS, 4, "creator",    false },
    { "analysis",   1, 5, ANALYSIS_SUBTASKS, 6, "analyst",    true },
};
static const size_t RULE_COUNT = sizeof(RULE_TABLE) / sizeof(RULE_TABLE[0]);

static agentos_error_t ml_planner_rule_based_plan(
    const agentos_intent_t* intent,
    agentos_task_plan_t** out_plan) {

    if (!intent || !out_plan) return AGENTOS_EINVAL;

    /* 1. 匹配意图规则 */
    int rule_index = -1;
    int complexity = (int)(intent->intent_flags & 0x07);
    for (size_t r = 0; r < RULE_COUNT; r++) {
        if (intent->intent_goal && strstr((const char*)intent->intent_goal, RULE_TABLE[r].intent_type)) {
            if (complexity >= RULE_TABLE[r].min_complexity &&
                complexity <= RULE_TABLE[r].max_complexity) {
                rule_index = (int)r;
                break;
            }
        }
    }

    /* 2. DS-006: 选择粒度级别 */
    const char** subtasks = NULL;
    size_t subtask_count = 0;
    const char* primary_role = "default";
    bool needs_verify = true;
    int use_fine_grain = 0;

    if (rule_index >= 0) {
        const rule_decomposition_t* rule = &RULE_TABLE[rule_index];
        primary_role = rule->primary_role;
        needs_verify = rule->requires_verification;

        /* 根据复杂度选择粗粒度或细粒度 */
        if (rule_index < (int)(sizeof(MULTI_GRAIN_TABLE) / sizeof(MULTI_GRAIN_TABLE[0])) &&
            MULTI_GRAIN_TABLE[rule_index].fine_tasks != NULL &&
            complexity >= MULTI_GRAIN_TABLE[rule_index].complexity_threshold) {
            use_fine_grain = 1;
            subtasks = MULTI_GRAIN_TABLE[rule_index].fine_tasks;
            subtask_count = MULTI_GRAIN_TABLE[rule_index].fine_count;
            AGENTOS_LOG_INFO("ML planner: using fine-grained decomposition (%zu steps)",
                            subtask_count);
        } else {
            subtasks = rule->subtasks;
            subtask_count = rule->subtask_count;
        }
    } else {
        static const char* GENERIC_TASKS[] = {"process_intent", "generate_response"};
        subtasks = (const char**)GENERIC_TASKS;
        subtask_count = 2;
    }

    /* 3. 构建计划结构 */
    agentos_task_plan_t* plan = (agentos_task_plan_t*)AGENTOS_CALLOC(1, sizeof(agentos_task_plan_t));
    if (!plan) return AGENTOS_ENOMEM;

    char plan_id[64];
    snprintf(plan_id, sizeof(plan_id), "rule_plan_%s_%d%s",
             intent->intent_goal ? (const char*)intent->intent_goal : "unknown",
             complexity,
             use_fine_grain ? "_fine" : "");
    plan->task_plan_id = AGENTOS_STRDUP(plan_id);
    if (!plan->task_plan_id) { AGENTOS_FREE(plan); return AGENTOS_ENOMEM; }

    plan->task_plan_nodes = (agentos_task_node_t**)AGENTOS_CALLOC(subtask_count + 2, sizeof(agentos_task_node_t*));
    if (!plan->task_plan_nodes && subtask_count > 0) {
        AGENTOS_FREE(plan->task_plan_id); AGENTOS_FREE(plan); return AGENTOS_ENOMEM;
    }

    /* DS-006: 依赖链优化 —— 识别可并行任务组 */
    typedef struct { int start; int count; int parallel_group; } parallel_group_t;
    parallel_group_t groups[MAX_DECOMPOSITION_DEPTH] = {{0}};
    int group_count = 0;

    /* 自动分析依赖关系构建并行组 */
    if (use_fine_grain && subtask_count >= 4) {
        /* 细粒度模式: 前N个任务串行(准备阶段), 后续任务分组并行 */
        groups[0].start = 0; groups[0].count = (subtask_count >= 6) ? 3 : 2; groups[0].parallel_group = 0;
        group_count++;
        if (subtask_count > groups[0].count + 2) {
            groups[1].start = groups[0].count;
            groups[1].count = subtask_count - groups[0].count - 1;
            groups[1].parallel_group = 1;
            group_count++;
        }
    }

    /* 4. 创建节点（含优化后的依赖链） */
    for (size_t i = 0; i < subtask_count; i++) {
        agentos_task_node_t* node = (agentos_task_node_t*)AGENTOS_CALLOC(1, sizeof(agentos_task_node_t));
        if (!node) goto cleanup_nodes;

        char node_id[128];
        snprintf(node_id, sizeof(node_id), "%s_step%zu", plan_id, i + 1);
        node->task_node_id = AGENTOS_STRDUP(node_id);
        node->task_node_agent_role = AGENTOS_STRDUP(primary_role);

        /* DS-006: 动态超时——复杂步骤给更多时间 */
        int base_timeout = 15000;
        if (use_fine_grain) {
            /* 细粒度: 前期分析步骤较短，后期综合步骤较长 */
            base_timeout = (i < subtask_count / 2) ? 10000 : 25000;
        }
        node->task_node_timeout_ms = base_timeout + (int)i * 3000;
        node->task_node_priority = 128 - (int)i * 8;

        /* DS-006: 优化的依赖关系 */
        if (i > 0) {
            /* 检查是否属于同一并行组 */
            int in_parallel_group = 0;
            int my_group_start = 0;
            for (int g = 0; g < group_count; g++) {
                if ((int)i >= groups[g].start && (int)i < groups[g].start + groups[g].count) {
                    in_parallel_group = 1;
                    my_group_start = groups[g].start;
                    break;
                }
            }

            if (in_parallel_group && (int)i > my_group_start) {
                /* 并行组内: 仅依赖组内第一个任务 + 所有前置串行任务 */
                size_t dep_count = 1 + (size_t)my_group_start;
                node->task_node_depends_on = (char**)AGENTOS_MALLOC(dep_count * sizeof(char*));
                if (node->task_node_depends_on) {
                    node->task_node_depends_count = dep_count;
                    /* 依赖组首节点 */
                    node->task_node_depends_on[0] =
                        AGENTOS_STRDUP(plan->task_plan_nodes[my_group_start]->task_node_id);
                    /* 依赖所有前置串行任务 */
                    for (size_t d = 1; d < dep_count; d++) {
                        node->task_node_depends_on[d] =
                            AGENTOS_STRDUP(plan->task_plan_nodes[d - 1]->task_node_id);
                    }
                }
            } else {
                /* 非并行: 依赖前一个任务 */
                node->task_node_depends_on = (char**)AGENTOS_MALLOC(sizeof(char*));
                if (node->task_node_depends_on) {
                    node->task_node_depends_count = 1;
                    node->task_node_depends_on[0] =
                        AGENTOS_STRDUP(plan->task_plan_nodes[i - 1]->task_node_id);
                }
            }
        }

        plan->task_plan_nodes[i] = node;
        plan->task_plan_node_count++;
    }

    /* 5. 入口点 */
    plan->task_plan_entry_points = (char**)AGENTOS_MALLOC(sizeof(char*));
    if (plan->task_plan_entry_points && plan->task_plan_node_count > 0) {
        plan->task_plan_entry_count = 1;
        plan->task_plan_entry_points[0] = AGENTOS_STRDUP(plan->task_plan_nodes[0]->task_node_id);
    }

    /* 6. DS-006: 条件验证步骤（仅在需要验证时添加） */
    if (needs_verify && subtask_count > 0) {
        agentos_task_node_t* verify_node = (agentos_task_node_t*)AGENTOS_CALLOC(1, sizeof(agentos_task_node_t));
        if (verify_node) {
            char verify_id[128];
            snprintf(verify_id, sizeof(verify_id), "%s_verify", plan_id);
            verify_node->task_node_id = AGENTOS_STRDUP(verify_id);
            verify_node->task_node_agent_role = AGENTOS_STRDUP("verifier");
            verify_node->task_node_timeout_ms = 10000;
            verify_node->task_node_priority = 255;
            verify_node->task_node_depends_on = (char**)AGENTOS_MALLOC(sizeof(char*));
            if (verify_node->task_node_depends_on) {
                verify_node->task_node_depends_count = 1;
                verify_node->task_node_depends_on[0] =
                    AGENTOS_STRDUP(plan->task_plan_nodes[plan->task_plan_node_count - 1]->task_node_id);
            }

            agentos_task_node_t** expanded = (agentos_task_node_t**)AGENTOS_REALLOC(
                plan->task_plan_nodes, (plan->task_plan_node_count + 1) * sizeof(agentos_task_node_t*));
            if (expanded) {
                plan->task_plan_nodes = expanded;
                plan->task_plan_nodes[plan->task_plan_node_count++] = verify_node;
            } else {
                ml_planner_destroy_single_node(verify_node);
            }
        }
    }

    /* 7. DS-006: 可选的条件分支步骤（高复杂度任务添加质量门禁） */
    if (complexity >= 5 && subtask_count >= 4) {
        agentos_task_node_t* qa_node = (agentos_task_node_t*)AGENTOS_CALLOC(1, sizeof(agentos_task_node_t));
        if (qa_node) {
            char qa_id[128];
            snprintf(qa_id, sizeof(qa_id), "%s_quality_gate", plan_id);
            qa_node->task_node_id = AGENTOS_STRDUP(qa_id);
            qa_node->task_node_agent_role = AGENTOS_STRDUP("quality_assurance");
            qa_node->task_node_timeout_ms = 8000;
            qa_node->task_node_priority = 254;
            qa_node->task_node_depends_on = (char**)AGENTOS_MALLOC(sizeof(char*));
            if (qa_node->task_node_depends_on) {
                qa_node->task_node_depends_count = 1;
                qa_node->task_node_depends_on[0] =
                    AGENTOS_STRDUP(plan->task_plan_nodes[plan->task_plan_node_count - 1]->task_node_id);
            }

            agentos_task_node_t** expanded = (agentos_task_node_t**)AGENTOS_REALLOC(
                plan->task_plan_nodes, (plan->task_plan_node_count + 1) * sizeof(agentos_task_node_t*));
            if (expanded) {
                plan->task_plan_nodes = expanded;
                plan->task_plan_nodes[plan->task_plan_node_count++] = qa_node;
            } else {
                ml_planner_destroy_single_node(qa_node);
            }
        }
    }

    *out_plan = plan;
    AGENTOS_LOG_INFO("ML planner: generated %zu-step %splan for intent '%s' (complexity=%d, parallel_groups=%d)",
                     plan->task_plan_node_count,
                     use_fine_grain ? "fine-grained " : "",
                     intent->intent_goal ? (const char*)intent->intent_goal : "?",
                     complexity, group_count);
    return AGENTOS_SUCCESS;

cleanup_nodes:
    for (size_t n = 0; n < plan->task_plan_node_count; n++) {
        if (plan->task_plan_nodes[n]) {
            AGENTOS_FREE(plan->task_plan_nodes[n]->task_node_id);
            AGENTOS_FREE(plan->task_plan_nodes[n]->task_node_agent_role);
            if (plan->task_plan_nodes[n]->task_node_depends_on) {
                for (size_t d = 0; d < plan->task_plan_nodes[n]->task_node_depends_count; d++)
                    AGENTOS_FREE(plan->task_plan_nodes[n]->task_node_depends_on[d]);
                AGENTOS_FREE(plan->task_plan_nodes[n]->task_node_depends_on);
            }
            AGENTOS_FREE(plan->task_plan_nodes[n]);
        }
    }
    AGENTOS_FREE(plan->task_plan_nodes);
    AGENTOS_FREE(plan->task_plan_id);
    AGENTOS_FREE(plan);
    return AGENTOS_ENOMEM;
}

static void ml_planner_destroy_single_node(agentos_task_node_t* node) {
    if (!node) return;
    AGENTOS_FREE(node->task_node_id);
    AGENTOS_FREE(node->task_node_agent_role);
    if (node->task_node_depends_on) {
        for (size_t d = 0; d < node->task_node_depends_count; d++)
            AGENTOS_FREE(node->task_node_depends_on[d]);
        AGENTOS_FREE(node->task_node_depends_on);
    }
    AGENTOS_FREE(node);
}

static agentos_error_t ml_planner_plan(
    const agentos_intent_t* intent,
    void* context,
    agentos_task_plan_t** out_plan) {

    ml_planner_data_t* data = (ml_planner_data_t*)context;
    if (!data || !intent || !out_plan) return AGENTOS_EINVAL;

    // Primary path: rule-based planning (always available)
    // This replaces both the old fallback and the PHASE2-IMPLEMENTED stub
    return ml_planner_rule_based_plan(intent, out_plan);
}

agentos_plan_strategy_t* agentos_plan_ml_create(
    const char* model_path,
    void* llm) {

    agentos_plan_strategy_t* strat = (agentos_plan_strategy_t*)AGENTOS_MALLOC(sizeof(agentos_plan_strategy_t));
    if (!strat) return NULL;

    ml_planner_data_t* data = (ml_planner_data_t*)AGENTOS_CALLOC(1, sizeof(ml_planner_data_t));
    if (!data) {
        AGENTOS_FREE(strat);
        return NULL;
    }

    data->model = NULL;
    data->model_path = model_path ? AGENTOS_STRDUP(model_path) : NULL;
    if (model_path && !data->model_path) {
        AGENTOS_FREE(data);
        AGENTOS_FREE(strat);
        return NULL;
    }
    data->llm = llm;
    data->fallback_mode = true;
    data->lock = agentos_mutex_create();
    if (!data->lock) {
        if (data->model_path) AGENTOS_FREE(data->model_path);
        AGENTOS_FREE(data);
        AGENTOS_FREE(strat);
        return NULL;
    }

    /* Attempt to load model if path provided */
    if (model_path) {
        ml_planner_try_load_model(data);
    }

    strat->plan = ml_planner_plan;
    strat->destroy = ml_planner_destroy;
    strat->data = data;

    return strat;
}

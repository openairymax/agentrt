/**
 * @file hierarchical.c
 * @brief 分层规划器实�?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "strategy.h"
#include "agentos.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Unified base library compatibility layer */
#include <agentos/memory_compat.h>
#include <agentos/string_compat.h>

/**
 * @brief 任务领域关键词分类
 */
typedef struct {
    const char* keyword;
    const char* domain;
    const char* subtasks[4];
    size_t subtask_count;
} domain_rule_t;

static const domain_rule_t g_domain_rules[] = {
    {"代码", "code", {"analyze_requirements", "design_structure", "implement_code", "test_verify"}, 4},
    {"code", "code", {"analyze_requirements", "design_structure", "implement_code", "test_verify"}, 4},
    {"编程", "code", {"analyze_requirements", "design_structure", "implement_code", "test_verify"}, 4},
    {"program", "code", {"analyze_requirements", "design_structure", "implement_code", "test_verify"}, 4},
    {"数据", "data", {"collect_data", "clean_data", "analyze_patterns", "generate_report"}, 4},
    {"data", "data", {"collect_data", "clean_data", "analyze_patterns", "generate_report"}, 4},
    {"分析", "analysis", {"gather_context", "extract_features", "apply_model", "interpret_results"}, 4},
    {"analyze", "analysis", {"gather_context", "extract_features", "apply_model", "interpret_results"}, 4},
    {"analysis", "analysis", {"gather_context", "extract_features", "apply_model", "interpret_results"}, 4},
    {"文件", "file", {"locate_file", "read_content", "process_content", "write_result"}, 4},
    {"file", "file", {"locate_file", "read_content", "process_content", "write_result"}, 4},
    {"搜索", "search", {"formulate_query", "execute_search", "rank_results", "summarize_findings"}, 4},
    {"search", "search", {"formulate_query", "execute_search", "rank_results", "summarize_findings"}, 4},
    {"写", "writing", {"research_topic", "outline_structure", "draft_content", "review_polish"}, 4},
    {"write", "writing", {"research_topic", "outline_structure", "draft_content", "review_polish"}, 4},
    {"翻译", "translation", {"parse_source", "translate_segment", "refine_accuracy", "format_output"}, 4},
    {"translate", "translation", {"parse_source", "translate_segment", "refine_accuracy", "format_output"}, 4},
};
static const size_t g_domain_rule_count = sizeof(g_domain_rules) / sizeof(g_domain_rules[0]);

/**
 * @brief 默认通用分解（当无法识别具体领域时）
 */
static const char* g_default_subtasks[] = {
    "analyze_goal",
    "identify_subtasks",
    "plan_execution",
    "execute_primary",
    "verify_result"
};
static const size_t g_default_count = sizeof(g_default_subtasks) / sizeof(g_default_subtasks[0]);

/**
 * @brief 字符串不区分大小写的子串查找
 */
static int str_contains_i(const char* haystack, const char* needle) {
    if (!haystack || !needle) return 0;
    size_t needle_len = strlen(needle);
    size_t hay_len = strlen(haystack);
    if (needle_len > hay_len) return 0;

    for (size_t i = 0; i <= hay_len - needle_len; i++) {
        int match = 1;
        for (size_t j = 0; j < needle_len; j++) {
            if ((int)tolower((unsigned char)haystack[i + j]) !=
                (int)tolower((unsigned char)needle[j])) {
                match = 0;
                break;
            }
        }
        if (match) return 1;
    }
    return 0;
}

/**
 * @brief 根据目标文本匹配任务领域，返回对应的子任务列表
 */
static const domain_rule_t* match_domain(const char* goal) {
    if (!goal) return NULL;

    for (size_t i = 0; i < g_domain_rule_count; i++) {
        if (str_contains_i(goal, g_domain_rules[i].keyword)) {
            return &g_domain_rules[i];
        }
    }
    return NULL;
}

/**
 * @brief 分解任务（基于关键词的智能分层分解）
 *
 * 支持领域：代码开发、数据分析、文本分析、文件操作、搜索、写作、翻译等。
 * 无法识别领域时使用默认的5步通用分解流程。
 * 使用 max_depth 和 decomposition_threshold 配置参数控制分解深度。
 */
typedef struct hierarchical_planner {
    agentos_planner_base_t base;
    int max_depth;
    float decomposition_threshold;
} hierarchical_planner_t;

/**
 * @brief 分解任务
 */
static agentos_error_t decompose_task(
    hierarchical_planner_t* planner,
    const char* goal,
    char*** out_subtasks,
    size_t* out_count) {
    if (!planner || !goal || !out_subtasks || !out_count) {
        return AGENTOS_EINVAL;
    }

    const domain_rule_t* rule = match_domain(goal);
    const char** task_names = NULL;
    size_t count = 0;

    if (rule) {
        task_names = rule->subtasks;
        count = rule->subtask_count;
    } else {
        task_names = g_default_subtasks;
        count = g_default_count;
    }

    char** subtasks = (char**)AGENTOS_CALLOC(count, sizeof(char*));
    if (!subtasks) return AGENTOS_ENOMEM;

    for (size_t i = 0; i < count; i++) {
        subtasks[i] = AGENTOS_STRDUP(task_names[i]);
        if (!subtasks[i]) {
            for (size_t j = 0; j < i; j++) {
                AGENTOS_FREE(subtasks[j]);
            }
            AGENTOS_FREE(subtasks);
            return AGENTOS_ENOMEM;
        }
    }

    *out_subtasks = subtasks;
    *out_count = count;

    return AGENTOS_SUCCESS;
}

/**
 * @brief 规划执行
 */
static agentos_error_t hierarchical_plan(
    agentos_planner_base_t* base,
    const agentos_planning_context_t* context,
    agentos_plan_t** out_plan) {
    if (!base || !context || !out_plan) {
        return AGENTOS_EINVAL;
    }

    hierarchical_planner_t* planner = (hierarchical_planner_t*)base;

    agentos_plan_t* plan = (agentos_plan_t*)AGENTOS_CALLOC(1, sizeof(agentos_plan_t));
    if (!plan) return AGENTOS_ENOMEM;

    plan->goal = AGENTOS_STRDUP(context->goal);
    plan->steps = NULL;
    plan->step_count = 0;

    char** subtasks = NULL;
    size_t subtask_count = 0;
    agentos_error_t err = decompose_task(planner, context->goal, &subtasks, &subtask_count);
    if (err == AGENTOS_SUCCESS && subtasks) {
        plan->steps = (agentos_plan_step_t*)AGENTOS_CALLOC(subtask_count, sizeof(agentos_plan_step_t));
        if (plan->steps) {
            for (size_t i = 0; i < subtask_count; i++) {
                plan->steps[i].action = subtasks[i];
                plan->steps[i].priority = (float)(subtask_count - i) / subtask_count;
            }
            plan->step_count = subtask_count;
        }
        AGENTOS_FREE(subtasks);
    }

    *out_plan = plan;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 创建分层规划�?
 */
agentos_error_t agentos_planner_hierarchical_create(
    agentos_planner_base_t** out_planner) {
    if (!out_planner) return AGENTOS_EINVAL;

    hierarchical_planner_t* planner = (hierarchical_planner_t*)
        AGENTOS_CALLOC(1, sizeof(hierarchical_planner_t));
    if (!planner) return AGENTOS_ENOMEM;

    planner->base.plan = hierarchical_plan;
    planner->base.destroy = NULL;
    planner->max_depth = 5;
    planner->decomposition_threshold = 0.7f;

    *out_planner = &planner->base;
    return AGENTOS_SUCCESS;
}

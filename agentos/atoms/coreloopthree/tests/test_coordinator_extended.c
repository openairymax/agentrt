/**
 * @file test_coordinator.c
 * @brief 协调器单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 测试协调器的多数投票、加权协调、双模型协调等功能
 * 遵循 ARCHITECTURAL_PRINCIPLES.md 的 E-8 可测试性原则
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../../../agentos/commons/utils/memory/include/memory_compat.h"
#include "../../../agentos/commons/utils/string/include/string_compat.h"
#include "cognition/coordinator/coordinator.h"

int test_majority_vote_basic(void) {
    printf("  测试多数投票基本功能...\n");

    agentos_majority_voter_t* voter = NULL;
    int err = agentos_majority_voter_create(&voter);
    if (err != 0 || voter == NULL) {
        printf("    创建多数投票器失败\n");
        return 1;
    }

    agentos_cognition_result_t results[] = {
        {.action = "action_a", .confidence = 0.9f},
        {.action = "action_b", .confidence = 0.8f},
        {.action = "action_a", .confidence = 0.85f}
    };
    size_t count = 3;

    const char* final_action = NULL;
    float confidence = 0.0f;
    err = agentos_majority_vote(voter, results, count, &final_action, &confidence);

    if (err != 0) {
        printf("    多数投票执行失败\n");
        agentos_majority_voter_destroy(voter);
        return 1;
    }

    if (final_action == NULL || strcmp(final_action, "action_a") != 0) {
        printf("    投票结果应为 action_a\n");
        agentos_majority_voter_destroy(voter);
        return 1;
    }

    agentos_majority_voter_destroy(voter);
    printf("    多数投票基本功能测试通过\n");
    return 0;
}

int test_majority_vote_tie(void) {
    printf("  测试平票情况...\n");

    agentos_majority_voter_t* voter = NULL;
    agentos_majority_voter_create(&voter);

    agentos_cognition_result_t results[] = {
        {.action = "action_a", .confidence = 0.9f},
        {.action = "action_b", .confidence = 0.9f}
    };
    size_t count = 2;

    const char* final_action = NULL;
    float confidence = 0.0f;
    int err = agentos_majority_vote(voter, results, count, &final_action, &confidence);

    if (err != 0) {
        printf("    平票时应采用置信度策略\n");
        agentos_majority_voter_destroy(voter);
        return 1;
    }

    agentos_majority_voter_destroy(voter);
    printf("    平票情况测试通过\n");
    return 0;
}

int test_weighted_coordinator_basic(void) {
    printf("  测试加权协调基本功能...\n");

    agentos_weighted_coordinator_t* coord = NULL;
    int err = agentos_weighted_coordinator_create(&coord);
    if (err != 0 || coord == NULL) {
        printf("    创建加权协调器失败\n");
        return 1;
    }

    agentos_coordinator_config_t config = {
        .weights = {0.5f, 0.3f, 0.2f},
        .threshold = 0.6f
    };
    agentos_weighted_coordinator_configure(coord, &config);

    agentos_cognition_result_t results[] = {
        {.action = "action_a", .confidence = 0.9f},
        {.action = "action_b", .confidence = 0.8f},
        {.action = "action_c", .confidence = 0.7f}
    };
    size_t count = 3;

    const char* final_action = NULL;
    float final_confidence = 0.0f;
    err = agentos_weighted_coordinate(coord, results, count, &final_action, &final_confidence);

    if (err != 0) {
        printf("    加权协调执行失败\n");
        agentos_weighted_coordinator_destroy(coord);
        return 1;
    }

    agentos_weighted_coordinator_destroy(coord);
    printf("    加权协调基本功能测试通过\n");
    return 0;
}

int test_weighted_coordinator_threshold(void) {
    printf("  测试加权协调阈值...\n");

    agentos_weighted_coordinator_t* coord = NULL;
    agentos_weighted_coordinator_create(&coord);

    agentos_coordinator_config_t config = {
        .weights = {1.0f},
        .threshold = 0.95f
    };
    agentos_weighted_coordinator_configure(coord, &config);

    agentos_cognition_result_t results[] = {
        {.action = "action_a", .confidence = 0.5f}
    };
    size_t count = 1;

    const char* final_action = NULL;
    float final_confidence = 0.0f;
    int err = agentos_weighted_coordinate(coord, results, count, &final_action, &final_confidence);

    if (err == 0 && final_confidence < config.threshold) {
        printf("    低于阈值的结果应被拒绝\n");
        agentos_weighted_coordinator_destroy(coord);
        return 1;
    }

    agentos_weighted_coordinator_destroy(coord);
    printf("    加权协调阈值测试通过\n");
    return 0;
}

int test_dual_model_coordinator(void) {
    printf("  测试双模型协调...\n");

    agentos_dual_model_coordinator_t* coord = NULL;
    int err = agentos_dual_model_coordinator_create(&coord);
    if (err != 0 || coord == NULL) {
        printf("    创建双模型协调器失败\n");
        return 1;
    }

    agentos_cognition_result_t fast_result = {
        .action = "fast_action",
        .confidence = 0.7f
    };

    agentos_cognition_result_t slow_result = {
        .action = "slow_action",
        .confidence = 0.9f
    };

    const char* final_action = NULL;
    float confidence = 0.0f;
    err = agentos_dual_model_coordinate(coord, &fast_result, &slow_result, &final_action, &confidence);

    if (err != 0) {
        printf("    双模型协调执行失败\n");
        agentos_dual_model_coordinator_destroy(coord);
        return 1;
    }

    agentos_dual_model_coordinator_destroy(coord);
    printf("    双模型协调测试通过\n");
    return 0;
}

int test_arbiter_selection(void) {
    printf("  测试仲裁器选择...\n");

    agentos_arbiter_t* arbiter = NULL;
    int err = agentos_arbiter_create(&arbiter);
    if (err != 0 || arbiter == NULL) {
        printf("    创建仲裁器失败\n");
        return 1;
    }

    agentos_cognition_result_t candidates[] = {
        {.action = "action_1", .confidence = 0.6f},
        {.action = "action_2", .confidence = 0.8f},
        {.action = "action_3", .confidence = 0.7f}
    };
    size_t count = 3;

    const char* selected = NULL;
    err = agentos_arbiter_select(arbiter, candidates, count, &selected);

    if (err != 0 || selected == NULL) {
        printf("    仲裁器选择失败\n");
        agentos_arbiter_destroy(arbiter);
        return 1;
    }

    agentos_arbiter_destroy(arbiter);
    printf("    仲裁器选择测试通过\n");
    return 0;
}

int test_coordinator_consensus(void) {
    printf("  测试协调器共识机制...\n");

    agentos_majority_voter_t* voter = NULL;
    agentos_majority_voter_create(&voter);

    agentos_cognition_result_t results[] = {
        {.action = "A", .confidence = 0.9f},
        {.action = "A", .confidence = 0.9f},
        {.action = "A", .confidence = 0.9f}
    };

    const char* action = NULL;
    float confidence = 0.0f;
    int err = agentos_majority_vote(voter, results, 3, &action, &confidence);

    if (err == 0 && confidence < 0.8f) {
        printf("    共识结果置信度应较高\n");
        agentos_majority_voter_destroy(voter);
        return 1;
    }

    agentos_majority_voter_destroy(voter);
    printf("    协调器共识机制测试通过\n");
    return 0;
}

int test_coordinator_fallback(void) {
    printf("  测试协调器回退机制...\n");

    agentos_arbiter_t* arbiter = NULL;
    agentos_arbiter_create(&arbiter);

    agentos_cognition_result_t candidates[1];
    candidates[0].action = "default_action";
    candidates[0].confidence = 0.5f;

    const char* selected = NULL;
    int err = agentos_arbiter_select(arbiter, candidates, 1, &selected);

    if (err == 0 && selected != NULL) {
        printf("    单一候选时应直接选择\n");
        agentos_arbiter_destroy(arbiter);
        return 1;
    }

    agentos_arbiter_destroy(arbiter);
    printf("    协调器回退机制测试通过\n");
    return 0;
}

/* 第二阶段新增：测试双模型自适应学习功能 */
int test_dual_model_adaptive_learning(void) {
    printf("  测试双模型自适应学习...\n");

    agentos_dual_model_coordinator_t* coord = NULL;
    int err = agentos_dual_model_coordinator_create(&coord);
    if (err != 0 || coord == NULL) {
        printf("    创建双模型协调器失败\n");
        return 1;
    }

    /* 启用自适应学习 */
    err = agentos_coordinator_dual_model_enable_adaptive_learning(
        (agentos_coordinator_base_t*)coord, 1, 0.1f);
    if (err != 0) {
        printf("    启用自适应学习失败\n");
        agentos_dual_model_coordinator_destroy(coord);
        return 1;
    }

    /* 设置验证模式为自适应 */
    err = agentos_coordinator_dual_model_set_validation_mode(
        (agentos_coordinator_base_t*)coord, 3); /* CROSS_VALIDATION_ADAPTIVE = 3 */
    if (err != 0) {
        printf("    设置验证模式失败\n");
        agentos_dual_model_coordinator_destroy(coord);
        return 1;
    }

    /* 模拟多次决策以收集统计数据 */
    agentos_cognition_result_t fast_result = {
        .action = "action_a",
        .confidence = 0.7f
    };

    agentos_cognition_result_t slow_result = {
        .action = "action_b", 
        .confidence = 0.9f
    };

    for (int i = 0; i < 5; i++) {
        const char* final_action = NULL;
        float confidence = 0.0f;
        err = agentos_dual_model_coordinate(coord, &fast_result, &slow_result, &final_action, &confidence);
        if (err != 0) {
            printf("    第%d次双模型协调失败\n", i);
            agentos_dual_model_coordinator_destroy(coord);
            return 1;
        }
    }

    /* 获取统计信息 */
    char* stats_json = NULL;
    err = agentos_coordinator_dual_model_get_stats(
        (agentos_coordinator_base_t*)coord, &stats_json);
    if (err != 0) {
        printf("    获取统计信息失败\n");
        agentos_dual_model_coordinator_destroy(coord);
        return 1;
    }

    if (stats_json && strlen(stats_json) > 0) {
        printf("    统计信息: %s\n", stats_json);
        AGENTOS_FREE(stats_json);
    }

    /* 重置统计 */
    err = agentos_coordinator_dual_model_reset_stats((agentos_coordinator_base_t*)coord);
    if (err != 0) {
        printf("    重置统计失败\n");
        agentos_dual_model_coordinator_destroy(coord);
        return 1;
    }

    agentos_dual_model_coordinator_destroy(coord);
    printf("    双模型自适应学习测试通过\n");
    return 0;
}

/* 第二阶段新增：测试交叉验证增强功能 */
int test_cross_validation_enhanced(void) {
    printf("  测试增强的交叉验证功能...\n");

    agentos_dual_model_coordinator_t* coord = NULL;
    int err = agentos_dual_model_coordinator_create(&coord);
    if (err != 0 || coord == NULL) {
        printf("    创建双模型协调器失败\n");
        return 1;
    }

    /* 测试基础验证模式 */
    err = agentos_coordinator_dual_model_set_validation_mode(
        (agentos_coordinator_base_t*)coord, 1); /* CROSS_VALIDATION_BASIC */
    if (err != 0) {
        printf("    设置基础验证模式失败\n");
        agentos_dual_model_coordinator_destroy(coord);
        return 1;
    }

    /* 测试高级验证模式 */
    err = agentos_coordinator_dual_model_set_validation_mode(
        (agentos_coordinator_base_t*)coord, 2); /* CROSS_VALIDATION_ADVANCED */
    if (err != 0) {
        printf("    设置高级验证模式失败\n");
        agentos_dual_model_coordinator_destroy(coord);
        return 1;
    }

    agentos_dual_model_coordinator_destroy(coord);
    printf("    交叉验证增强功能测试通过\n");
    return 0;
}

int main(void) {
    printf("开始运行 coreloopthree 协调器单元测试...\n");

    int failures = 0;

    failures |= test_majority_vote_basic();
    failures |= test_majority_vote_tie();
    failures |= test_weighted_coordinator_basic();
    failures |= test_weighted_coordinator_threshold();
    failures |= test_dual_model_coordinator();
    failures |= test_arbiter_selection();
    failures |= test_coordinator_consensus();
    failures |= test_coordinator_fallback();
    /* 第二阶段新增测试 */
    failures |= test_dual_model_adaptive_learning();
    failures |= test_cross_validation_enhanced();

    if (failures == 0) {
        printf("\n所有协调器测试通过！\n");
        return 0;
    } else {
        printf("\n%d 个协调器测试失败\n", failures);
        return 1;
    }
}

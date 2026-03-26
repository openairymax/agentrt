/**
 * @file majority.c
 * @brief 多数投票协调器实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "strategy.h"
#include "agentos.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief 多数投票协调器上下文
 */
typedef struct majority_coordinator {
    agentos_coordinator_base_t base;
    size_t min_voters;
    float threshold;
} majority_coordinator_t;

/**
 * @brief 投票记录
 */
typedef struct vote_record {
    char* result;
    int count;
} vote_record_t;

/**
 * @brief 协调执行（多数投票）
 */
static agentos_error_t majority_coordinate(
    agentos_coordinator_base_t* base,
    const agentos_coordination_context_t* context,
    const char** inputs,
    size_t input_count,
    char** out_result) {
    if (!base || !out_result) {
        return AGENTOS_EINVAL;
    }

    majority_coordinator_t* coordinator = (majority_coordinator_t*)base;

    if (!inputs || input_count < coordinator->min_voters) {
        *out_result = strdup("insufficient_voters");
        return AGENTOS_SUCCESS;
    }
    
    if (input_count == 0) {
        *out_result = strdup("no_votes");
        return AGENTOS_SUCCESS;
    }

    vote_record_t* votes = (vote_record_t*)calloc(input_count, sizeof(vote_record_t));
    if (!votes) return AGENTOS_ENOMEM;

    size_t unique_count = 0;

    for (size_t i = 0; i < input_count; i++) {
        if (!inputs[i]) continue;

        int found = 0;
        for (size_t j = 0; j < unique_count; j++) {
            if (votes[j].result && strcmp(votes[j].result, inputs[i]) == 0) {
                votes[j].count++;
                found = 1;
                break;
            }
        }

        if (!found) {
            votes[unique_count].result = strdup(inputs[i]);
            votes[unique_count].count = 1;
            unique_count++;
        }
    }

    char* winner = NULL;
    int max_count = 0;
    for (size_t i = 0; i < unique_count; i++) {
        if (votes[i].count > max_count) {
            max_count = votes[i].count;
            if (winner) free(winner);
            winner = votes[i].result ? strdup(votes[i].result) : NULL;
        }
        if (votes[i].result) free(votes[i].result);
    }
    free(votes);

    float vote_ratio = (float)max_count / input_count;
    if (vote_ratio < coordinator->threshold) {
        if (winner) free(winner);
        *out_result = strdup("no_majority");
    } else {
        *out_result = winner ? winner : strdup("no_result");
    }

    return AGENTOS_SUCCESS;
}

/**
 * @brief 销毁协调器
 */
static void majority_destroy(agentos_coordinator_base_t* base) {
    if (!base) return;
    free(base);
}

/**
 * @brief 创建多数投票协调器
 */
agentos_error_t agentos_coordinator_majority_create(
    size_t min_voters,
    float threshold,
    agentos_coordinator_base_t** out_coordinator) {
    if (!out_coordinator) return AGENTOS_EINVAL;

    majority_coordinator_t* coordinator = (majority_coordinator_t*)
        calloc(1, sizeof(majority_coordinator_t));
    if (!coordinator) return AGENTOS_ENOMEM;

    coordinator->base.coordinate = majority_coordinate;
    coordinator->base.destroy = majority_destroy;
    coordinator->min_voters = min_voters > 0 ? min_voters : 3;
    coordinator->threshold = (threshold > 0 && threshold <= 1.0f) ? threshold : 0.5f;

    *out_coordinator = &coordinator->base;
    return AGENTOS_SUCCESS;
}

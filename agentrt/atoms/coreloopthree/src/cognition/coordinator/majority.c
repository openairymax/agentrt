/**
 * @file majority.c
 * @brief 多数投票协调器实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "agentrt.h"
#include "strategy.h"

#include <stdlib.h>

/* Unified base library compatibility layer */
#include "memory_compat.h"
#include "string_compat.h"

#include <string.h>

/**
 * @brief 多数投票协调器上下文
 */
typedef struct majority_coordinator {
    agentrt_coordinator_base_t base;
    size_t min_voters;
    float threshold;
} majority_coordinator_t;

/**
 * @brief 投票记录
 */
typedef struct vote_record {
    char *result;
    int count;
} vote_record_t;

/**
 * @brief 协调执行（多数投票）
 */
static agentrt_error_t
majority_coordinate(agentrt_coordinator_base_t *base,
                    const agentrt_coordination_context_t __attribute__((unused)) * context,
                    const char **inputs, size_t input_count, char **out_result)
{
    if (!base || !out_result) {
        return AGENTRT_EINVAL;
    }

    majority_coordinator_t *coordinator = (majority_coordinator_t *)base;

    if (!inputs || input_count < coordinator->min_voters) {
        *out_result = AGENTRT_STRDUP("insufficient_voters");
        if (!*out_result)
            return AGENTRT_ENOMEM;
        return AGENTRT_SUCCESS;
    }

    if (input_count == 0) {
        *out_result = AGENTRT_STRDUP("no_votes");
        if (!*out_result)
            return AGENTRT_ENOMEM;
        return AGENTRT_SUCCESS;
    }

    vote_record_t *votes = (vote_record_t *)AGENTRT_CALLOC(input_count, sizeof(vote_record_t));
    if (!votes)
        return AGENTRT_ENOMEM;

    size_t unique_count = 0;

    for (size_t i = 0; i < input_count; i++) {
        if (!inputs[i])
            continue;

        int found = 0;
        for (size_t j = 0; j < unique_count; j++) {
            if (votes[j].result && strcmp(votes[j].result, inputs[i]) == 0) {
                votes[j].count++;
                found = 1;
                break;
            }
        }

        if (!found) {
            votes[unique_count].result = AGENTRT_STRDUP(inputs[i]);
            votes[unique_count].count = 1;
            unique_count++;
        }
    }

    // 找出得票最多的选项
    char *best_result = NULL;
    int max_votes = 0;

    for (size_t i = 0; i < unique_count; i++) {
        if (votes[i].count > max_votes) {
            if (best_result)
                AGENTRT_FREE(best_result);
            best_result = AGENTRT_STRDUP(votes[i].result);
            if (!best_result) {
                for (size_t j = 0; j < unique_count; j++)
                    AGENTRT_FREE(votes[j].result);
                AGENTRT_FREE(votes);
                return AGENTRT_ENOMEM;
            }
            max_votes = votes[i].count;
        }
    }

    // 检查是否达到阈值
    float vote_ratio = (float)max_votes / (float)input_count;
    if (vote_ratio >= coordinator->threshold) {
        *out_result = best_result;
    } else {
        *out_result = AGENTRT_STRDUP("no_majority");
        if (!*out_result) {
            for (size_t j = 0; j < unique_count; j++)
                AGENTRT_FREE(votes[j].result);
            AGENTRT_FREE(votes);
            return AGENTRT_ENOMEM;
        }
        if (best_result)
            AGENTRT_FREE(best_result);
    }

    // 清理
    for (size_t i = 0; i < unique_count; i++) {
        if (votes[i].result)
            AGENTRT_FREE(votes[i].result);
    }
    AGENTRT_FREE(votes);

    return AGENTRT_SUCCESS;
}

/**
 * @brief 销毁协调器
 */
static void majority_destroy(agentrt_coordinator_base_t *base)
{
    if (!base)
        return;
    AGENTRT_FREE(base);
}

/**
 * @brief 创建多数投票协调器
 */
agentrt_error_t agentrt_coordinator_majority_create(size_t min_voters, float threshold,
                                                    agentrt_coordinator_base_t **out_base)
{
    if (!out_base)
        return AGENTRT_EINVAL;

    majority_coordinator_t *coordinator =
        (majority_coordinator_t *)AGENTRT_CALLOC(1, sizeof(majority_coordinator_t));
    if (!coordinator)
        return AGENTRT_ENOMEM;

    coordinator->min_voters = min_voters;
    coordinator->threshold = threshold;

    coordinator->base.coordinate = majority_coordinate;
    coordinator->base.destroy = majority_destroy;

    *out_base = &coordinator->base;
    return AGENTRT_SUCCESS;
}

/**
 * @file prune.c
 * @brief 遗忘裁剪实现（联动 L2 删除）
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#include "forgetting.h"
#include "layer2_feature.h"
#include "logger.h"
#include <stdlib.h>

agentos_error_t agentos_forgetting_prune(
    agentos_forgetting_engine_t* engine,
    uint32_t* out_pruned_count) {

    if (!engine) return AGENTOS_EINVAL;

    char** all_ids = NULL;
    size_t count = 0;
    agentos_error_t err = agentos_layer1_raw_list_ids(engine->layer1, &all_ids, &count);
    if (err != AGENTOS_SUCCESS) return err;

    uint32_t pruned = 0;
    for (size_t i = 0; i < count; i++) {
        float weight = 0.0f;
        if (agentos_forgetting_get_weight(engine, all_ids[i], &weight) == AGENTOS_SUCCESS) {
            if (weight < engine->config.threshold) {
                // 先删除 L2 向量
                agentos_layer2_feature_remove(engine->layer2, all_ids[i]);
                // 再删除 L1 记录
                if (agentos_layer1_raw_delete(engine->layer1, all_ids[i]) == AGENTOS_SUCCESS) {
                    pruned++;
                } else {
                    AGENTOS_LOG_WARN("Failed to delete L1 record %s", all_ids[i]);
                }
            }
        }
    }

    agentos_free_string_array(all_ids, count);
    if (out_pruned_count) *out_pruned_count = pruned;
    return AGENTOS_SUCCESS;
}
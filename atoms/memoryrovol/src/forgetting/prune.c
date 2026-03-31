/**
 * @file prune.c
 * @brief 遗忘裁剪实现（联�?L2 删除�?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "../include/forgetting.h"
#include "../include/layer2_feature.h"
#include "agentos.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../commons/utils/memory/include/memory_compat.h"
#include "../../../commons/utils/string/include/string_compat.h"

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
        // From data intelligence emerges. by spharx
        if (agentos_forgetting_get_weight(engine, all_ids[i], &weight) == AGENTOS_SUCCESS) {
            if (weight < engine->manager.threshold) {
                // 先删�?L2 向量
                agentos_layer2_feature_remove(engine->layer2, all_ids[i]);
                // 再删�?L1 记录
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

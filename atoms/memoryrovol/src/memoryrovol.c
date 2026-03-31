/**
 * @file memoryrovol.c
 * @brief MemoryRovol 系统主接口实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * MemoryRovol 是 AgentOS 的四层演化记忆系统：
 * - L1 原始卷：存储原始记忆数据，支持异步写入
 * - L2 特征层：特征提取、向量索引、相似度检索
 * - L3 结构层：关系抽取、知识图谱构建
 * - L4 模式层：模式挖掘、规则发现
 *
 * 演化过程遵循《工程控制论》的反馈闭环原则。
 * 每次演化都会触发特征提取、关系构建、模式挖掘和遗忘裁剪。
 */

#include "memoryrovol.h"
#include "layer1_raw.h"
#include "layer2_feature.h"
#include "retrieval.h"
#include "forgetting.h"
#include "manager.h"
#include "agentos.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../commons/utils/memory/include/memory_compat.h"
#include "../../../commons/utils/string/include/string_compat.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

/* ==================== 内部常量 ==================== */

/** @brief 默认演化批次大小 */
#define EVOLVE_BATCH_SIZE 100

/** @brief 默认遗忘阈值 */
#define DEFAULT_FORGET_THRESHOLD 0.1

/** @brief 默认衰减率（艾宾浩斯曲线） */
#define DEFAULT_FORGET_LAMBDA 0.1

/* ==================== 句柄结构 ==================== */

/**
 * @brief MemoryRovol 系统句柄结构
 */
struct agentos_memoryrov_handle {
    agentos_layer1_raw_t* l1_raw;           /**< L1 原始卷 */
    agentos_layer2_feature_t* l2_feature;    /**< L2 特征层 */
    agentos_layer3_structure_t* l3_struct;  /**< L3 结构层 */
    agentos_layer4_pattern_t* l4_pattern;   /**< L4 模式层 */
    agentos_forgetting_t* forgetting;        /**< 遗忘模块 */
    agentos_retrieval_t* retrieval;         /**< 检索模块 */
    int initialized;                        /**< 初始化标志 */
};

/* ==================== 公共接口实现 ==================== */

agentos_memoryrov_handle_t* agentos_memoryrov_create(void) {
    agentos_memoryrov_handle_t* handle =
        (agentos_memoryrov_handle_t*)AGENTOS_CALLOC(1, sizeof(agentos_memoryrov_handle_t));
    if (!handle) {
        return NULL;
    }

    handle->l1_raw = agentos_layer1_raw_create();
    if (!handle->l1_raw) {
        AGENTOS_FREE(handle);
        return NULL;
    }

    handle->l2_feature = agentos_layer2_feature_create();
    if (!handle->l2_feature) {
        agentos_layer1_raw_destroy(handle->l1_raw);
        AGENTOS_FREE(handle);
        return NULL;
    }

    handle->l3_struct = agentos_layer3_structure_create();
    if (!handle->l3_struct) {
        agentos_layer2_feature_destroy(handle->l2_feature);
        agentos_layer1_raw_destroy(handle->l1_raw);
        AGENTOS_FREE(handle);
        return NULL;
    }

    handle->l4_pattern = agentos_layer4_pattern_create();
    if (!handle->l4_pattern) {
        agentos_layer3_structure_destroy(handle->l3_struct);
        agentos_layer2_feature_destroy(handle->l2_feature);
        agentos_layer1_raw_destroy(handle->l1_raw);
        AGENTOS_FREE(handle);
        return NULL;
    }

    handle->forgetting = agentos_forgetting_create();
    if (!handle->forgetting) {
        agentos_layer4_pattern_destroy(handle->l4_pattern);
        agentos_layer3_structure_destroy(handle->l3_struct);
        agentos_layer2_feature_destroy(handle->l2_feature);
        agentos_layer1_raw_destroy(handle->l1_raw);
        AGENTOS_FREE(handle);
        return NULL;
    }

    handle->retrieval = agentos_retrieval_create();
    if (!handle->retrieval) {
        agentos_forgetting_destroy(handle->forgetting);
        agentos_layer4_pattern_destroy(handle->l4_pattern);
        agentos_layer3_structure_destroy(handle->l3_struct);
        agentos_layer2_feature_destroy(handle->l2_feature);
        agentos_layer1_raw_destroy(handle->l1_raw);
        AGENTOS_FREE(handle);
        return NULL;
    }

    handle->initialized = 1;
    return handle;
}

void agentos_memoryrov_destroy(agentos_memoryrov_handle_t* handle) {
    if (!handle) {
        return;
    }

    if (handle->retrieval) {
        agentos_retrieval_destroy(handle->retrieval);
    }
    if (handle->forgetting) {
        agentos_forgetting_destroy(handle->forgetting);
    }
    if (handle->l4_pattern) {
        agentos_layer4_pattern_destroy(handle->l4_pattern);
    }
    if (handle->l3_struct) {
        agentos_layer3_structure_destroy(handle->l3_struct);
    }
    if (handle->l2_feature) {
        agentos_layer2_feature_destroy(handle->l2_feature);
    }
    if (handle->l1_raw) {
        agentos_layer1_raw_destroy(handle->l1_raw);
    }

    handle->initialized = 0;
    AGENTOS_FREE(handle);
}

agentos_error_t agentos_memoryrov_add_memory(agentos_memoryrov_handle_t* handle,
                                              const char* content,
                                              size_t content_len) {
    if (!handle || !content || !handle->initialized) {
        return AGENTOS_EINVAL;
    }

    agentos_memory_t memory;
    memory.content = content;
    memory.content_len = content_len;
    memory.timestamp = time(NULL);
    memory.importance = 0.5f;

    agentos_error_t err = agentos_layer1_raw_add(handle->l1_raw, &memory);
    if (err != AGENTOS_SUCCESS) {
        return err;
    }

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_memoryrov_evolve(agentos_memoryrov_handle_t* handle) {
    if (!handle || !handle->initialized) {
        return AGENTOS_EINVAL;
    }

    agentos_memory_t* batch = NULL;
    size_t batch_size = 0;

    agentos_error_t err = agentos_layer1_raw_get_batch(handle->l1_raw, EVOLVE_BATCH_SIZE, &batch, &batch_size);
    if (err != AGENTOS_SUCCESS) {
        return err;
    }

    for (size_t i = 0; i < batch_size; i++) {
        agentos_feature_t feature;
        err = agentos_layer2_feature_extract(handle->l2_feature, &batch[i], &feature);
        if (err == AGENTOS_SUCCESS) {
            agentos_layer3_structure_add_feature(handle->l3_struct, batch[i].id, &feature);
        }

        agentos_relation_t relation;
        err = agentos_layer3_structure_extract_relation(handle->l3_struct, &batch[i], &relation);
        if (err == AGENTOS_SUCCESS) {
            agentos_layer3_structure_add_relation(handle->l3_struct, &relation);
        }

        agentos_pattern_t pattern;
        err = agentos_layer4_pattern mine(handle->l4_pattern, &batch[i], &pattern);
        if (err == AGENTOS_SUCCESS) {
            agentos_layer4_pattern_add(handle->l4_pattern, &pattern);
        }
    }

    if (batch) {
        AGENTOS_FREE(batch);
    }

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_memoryrov_retrieve(agentos_memoryrov_handle_t* handle,
                                            const char* query,
                                            size_t max_results,
                                            agentos_memory_t** out_results,
                                            size_t* out_count) {
    if (!handle || !query || !out_results || !out_count || !handle->initialized) {
        return AGENTOS_EINVAL;
    }

    return agentos_retrieval_search(handle->retrieval, query, max_results, out_results, out_count);
}

agentos_error_t agentos_memoryrov_forget(agentos_memoryrov_handle_t* handle) {
    if (!handle || !handle->initialized) {
        return AGENTOS_EINVAL;
    }

    return agentos_forgetting_apply(handle->forgetting,
                                   DEFAULT_FORGET_THRESHOLD,
                                   DEFAULT_FORGET_LAMBDA);
}

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
#include "layer3_structure.h"
#include "layer4_pattern.h"
#include "retrieval.h"
#include "forgetting.h"
#include "agentos.h"
#include <stdlib.h>
#include <time.h>  /* time()函数 */

/* Unified base library compatibility layer */
#include "memory_compat.h"
#include "string_compat.h"
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
    agentos_knowledge_graph_t* l3_struct;   /**< L3 结构层（知识图谱） */
    agentos_rule_generator_t* l4_pattern;   /**< L4 模式层（规则生成器） */
    agentos_forgetting_engine_t* forgetting; /**< 遗忘模块 */
    int initialized;                        /**< 初始化标志 */
};

/* ==================== 公共接口实现 ==================== */

agentos_memoryrov_handle_t* agentos_memoryrov_create(void) {
    agentos_memoryrov_handle_t* handle =
        (agentos_memoryrov_handle_t*)AGENTOS_CALLOC(1, sizeof(agentos_memoryrov_handle_t));
    if (!handle) {
        return NULL;
    }

    /* 创建 L1 原始卷 */
    agentos_error_t err = agentos_layer1_raw_create_async(NULL, 1024, 4, &handle->l1_raw);
    if (err != AGENTOS_SUCCESS || !handle->l1_raw) {
        AGENTOS_FREE(handle);
        return NULL;
    }

    /* 创建 L2 特征层 */
    agentos_layer2_feature_config_t l2_config = {
        .index_path = NULL,
        .embedding_model = "default",
        .dimension = 768,
        .index_type = AGENTOS_INDEX_HNSW,
        .hnsw_m = 16,
        .ivf_nlist = 100
    };
    err = agentos_layer2_feature_create(&l2_config, &handle->l2_feature);
    if (err != AGENTOS_SUCCESS || !handle->l2_feature) {
        agentos_layer1_raw_destroy(handle->l1_raw);
        AGENTOS_FREE(handle);
        return NULL;
    }

    /* 创建 L3 结构层（知识图谱） */
    err = agentos_knowledge_graph_create(&handle->l3_struct);
    if (err != AGENTOS_SUCCESS || !handle->l3_struct) {
        agentos_layer2_feature_destroy(handle->l2_feature);
        agentos_layer1_raw_destroy(handle->l1_raw);
        AGENTOS_FREE(handle);
        return NULL;
    }

    /* 创建 L4 模式层（规则生成器） */
    err = agentos_rule_generator_create(NULL, &handle->l4_pattern);
    if (err != AGENTOS_SUCCESS || !handle->l4_pattern) {
        agentos_knowledge_graph_destroy(handle->l3_struct);
        agentos_layer2_feature_destroy(handle->l2_feature);
        agentos_layer1_raw_destroy(handle->l1_raw);
        AGENTOS_FREE(handle);
        return NULL;
    }

    /* 创建遗忘引擎 */
    agentos_forgetting_config_t forget_config = {
        .strategy = AGENTOS_FORGET_EBBINGHAUS,
        .lambda = DEFAULT_FORGET_LAMBDA,
        .threshold = DEFAULT_FORGET_THRESHOLD,
        .min_access = 1,
        .check_interval_sec = 3600,
        .archive_path = NULL
    };
    err = agentos_forgetting_create(&forget_config, handle->l1_raw, handle->l2_feature, &handle->forgetting);
    if (err != AGENTOS_SUCCESS || !handle->forgetting) {
        agentos_rule_generator_destroy(handle->l4_pattern);
        agentos_knowledge_graph_destroy(handle->l3_struct);
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

    if (handle->forgetting) {
        agentos_forgetting_destroy(handle->forgetting);
    }
    if (handle->l4_pattern) {
        agentos_rule_generator_destroy(handle->l4_pattern);
    }
    if (handle->l3_struct) {
        agentos_knowledge_graph_destroy(handle->l3_struct);
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

    /* 生成唯一 ID - 使用 UUID 生成器 */
    char id[64];
    
#ifdef AGENTOS_HAS_UUID
    agentos_uuid_error_t uuid_err = agentos_uuid_with_prefix("mem_", id, sizeof(id));
    if (uuid_err != AGENTOS_UUID_SUCCESS) {
        snprintf(id, sizeof(id), "mem_%lu_%zu", (unsigned long)time(NULL), (size_t)handle);
    }
#else
    snprintf(id, sizeof(id), "mem_%lu_%zu", (unsigned long)time(NULL), (size_t)handle);
#endif

    /* 写入 L1 原始卷 */
    agentos_error_t err = agentos_layer1_raw_write(handle->l1_raw, id, content, content_len);
    if (err != AGENTOS_SUCCESS) {
        return err;
    }

    /* 添加到 L2 特征层 */
    err = agentos_layer2_feature_add(handle->l2_feature, id, content);
    if (err != AGENTOS_SUCCESS) {
        return err;
    }

    /* 添加到 L3 结构层（作为实体） */
    err = agentos_knowledge_graph_add_entity(handle->l3_struct, id);
    if (err != AGENTOS_SUCCESS) {
        return err;
    }

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_memoryrov_evolve(agentos_memoryrov_handle_t* handle, int force) {
    if (!handle || !handle->initialized) {
        return AGENTOS_EINVAL;
    }

    char** ids = NULL;
    size_t count = 0;
    agentos_error_t err = agentos_layer1_raw_list_ids(handle->l1_raw, &ids, &count);
    if (err != AGENTOS_SUCCESS || count == 0) {
        return err;
    }

    /* ========== L3 语义关系抽取 ========== */
    for (size_t i = 0; i < count; i++) {
        void* data_i = NULL;
        size_t len_i = 0;
        err = agentos_layer1_raw_read(handle->l1_raw, ids[i], &data_i, &len_i);
        if (err != AGENTOS_SUCCESS || !data_i) continue;

        char* content_i = (char*)data_i;

        /* 时序关系: 相邻记忆 */
        if (i > 0) {
            agentos_knowledge_graph_add_relation(
                handle->l3_struct, ids[i - 1], ids[i],
                AGENTOS_RELATION_BEFORE, 1.0f);
        }

        /* 语义关系: 基于内容相似度 */
        for (size_t j = 0; j < count && j < i; j++) {
            if (i == j) continue;

            void* data_j = NULL;
            size_t len_j = 0;
            agentos_error_t err_j = agentos_layer1_raw_read(handle->l1_raw, ids[j], &data_j, &len_j);
            if (err_j != AGENTOS_SUCCESS || !data_j) continue;

            char* content_j = (char*)data_j;

            /* 关键词重叠度计算 */
            float similarity = 0.0f;
            size_t overlap = 0;
            size_t min_len = len_i < len_j ? len_i : len_j;

            if (min_len > 10) {
                /* 滑动窗口关键词匹配 */
                size_t window = (min_len > 64) ? 64 : min_len;
                for (size_t w = 0; w + window <= len_i; w += window / 2) {
                    for (size_t v = 0; v + window <= len_j; v += window / 2) {
                        if (memcmp(content_i + w, content_j + v, window) == 0) {
                            overlap++;
                            break;
                        }
                    }
                }
                size_t max_windows = (len_i / (window / 2 + 1)) + 1;
                similarity = (max_windows > 0) ? (float)overlap / (float)max_windows : 0.0f;
            }

            if (similarity > 0.3f) {
                agentos_knowledge_graph_add_relation(
                    handle->l3_struct, ids[j], ids[i],
                    AGENTOS_RELATION_SIMILAR_TO, similarity);
            }

            /* 包含关系: 长内容包含短内容 */
            if (len_i > len_j * 2 && len_j > 5) {
                if (memmem(content_i, len_i, content_j, len_j) != NULL) {
                    agentos_knowledge_graph_add_relation(
                        handle->l3_struct, ids[j], ids[i],
                        AGENTOS_RELATION_MEMBERS_OF, 0.9f);
                }
            }

            /* 因果关系: 关键词检测 */
            static const char* causal_markers[] = {
                "because", "therefore", "caused", "result", "hence",
                "so that", "leads to", "due to"
            };
            for (size_t m = 0; m < sizeof(causal_markers) / sizeof(causal_markers[0]); m++) {
                if (strstr(content_i, causal_markers[m]) != NULL) {
                    agentos_knowledge_graph_add_relation(
                        handle->l3_struct, ids[j], ids[i],
                        AGENTOS_RELATION_CAUSES, 0.7f);
                    break;
                }
            }

            AGENTOS_FREE(data_j);
        }

        AGENTOS_FREE(data_i);
    }

    /* ========== L4 规则挖掘 ========== */
    if (handle->l4_pattern && count >= 3) {
        /* 使用L2特征聚类驱动L4规则发现 */
        for (size_t i = 0; i < count; i++) {
            void* data = NULL;
            size_t len = 0;
            err = agentos_layer1_raw_read(handle->l1_raw, ids[i], &data, &len);
            if (err != AGENTOS_SUCCESS || !data) continue;

            /* 通过L2检索相似记忆，构建聚类用于规则生成 */
            char** sim_ids = NULL;
            float* sim_scores = NULL;
            size_t sim_count = 0;
            agentos_layer2_feature_search(handle->l2_feature,
                (const char*)data, 5, &sim_ids, &sim_scores, &sim_count);

            if (sim_count >= 2) {
                const char** cluster_ids = (const char**)sim_ids;
                char* rule_text = NULL;
                agentos_rule_generator_from_cluster(
                    handle->l4_pattern, cluster_ids, sim_count, &rule_text);
                if (rule_text) AGENTOS_FREE(rule_text);
            }

            if (sim_ids) AGENTOS_FREE(sim_ids);
            if (sim_scores) AGENTOS_FREE(sim_scores);
            AGENTOS_FREE(data);
        }
    }

    /* ========== 遗忘裁剪 ========== */
    if (!force) {
        uint32_t pruned = 0;
        agentos_forgetting_prune(handle->forgetting, &pruned);
    }

    if (ids) {
        agentos_free_string_array(ids, count);
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

    if (max_results == 0) {
        *out_results = NULL;
        *out_count = 0;
        return AGENTOS_EINVAL;
    }

    /* 使用 L2 特征层进行相似度检索 */
    char** result_ids = NULL;
    float* scores = NULL;
    size_t result_count = 0;
    agentos_error_t err = AGENTOS_SUCCESS;

    err = agentos_layer2_feature_search(
        handle->l2_feature,
        query,
        (uint32_t)max_results,
        &result_ids,
        &scores,
        &result_count
    );

    if (err != AGENTOS_SUCCESS || result_count == 0) {
        *out_results = NULL;
        *out_count = 0;
        /* 释放临时数组（如果已分配） */
        if (result_ids) AGENTOS_FREE(result_ids);
        if (scores) AGENTOS_FREE(scores);
        return err;
    }

    /* 分配结果数组 */
    *out_results = (agentos_memory_t*)AGENTOS_CALLOC(result_count, sizeof(agentos_memory_t));
    if (!*out_results) {
        /* 释放临时数组 */
        if (result_ids) AGENTOS_FREE(result_ids);
        if (scores) AGENTOS_FREE(scores);
        return AGENTOS_ENOMEM;
    }

    /* 填充结果 */
    for (size_t i = 0; i < result_count; i++) {
        (*out_results)[i].record_id = AGENTOS_STRDUP(result_ids[i]);
        if (!(*out_results)[i].record_id) {
            for (size_t j = 0; j < i; j++) {
                AGENTOS_FREE((*out_results)[j].record_id);
                AGENTOS_FREE((*out_results)[j].data);
            }
            AGENTOS_FREE(*out_results);
            *out_results = NULL;
            AGENTOS_FREE(result_ids);
            AGENTOS_FREE(scores);
            return AGENTOS_ENOMEM;
        }
        (*out_results)[i].score = scores[i];
        (*out_results)[i].created_at = time(NULL);
        (*out_results)[i].updated_at = time(NULL);

        /* 从 L1 读取内容 */
        void* data = NULL;
        size_t len = 0;
        agentos_error_t read_err = agentos_layer1_raw_read(handle->l1_raw, result_ids[i], &data, &len);
        if (read_err == AGENTOS_SUCCESS && data) {
            (*out_results)[i].data = data;
            (*out_results)[i].data_len = len;
            (*out_results)[i].metadata = NULL;
        } else {
            (*out_results)[i].data = NULL;
            (*out_results)[i].data_len = 0;
            (*out_results)[i].metadata = NULL;
        }
    }

    *out_count = result_count;

    /* 释放临时数组（ID 已转移给结果） */
    AGENTOS_FREE(result_ids);
    AGENTOS_FREE(scores);

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_memoryrov_forget(agentos_memoryrov_handle_t* handle) {
    if (!handle || !handle->initialized) {
        return AGENTOS_EINVAL;
    }

    uint32_t pruned_count = 0;
    return agentos_forgetting_prune(handle->forgetting, &pruned_count);
}

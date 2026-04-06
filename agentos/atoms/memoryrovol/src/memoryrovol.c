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
#include "../../../agentos/commons/utils/memory/include/memory_compat.h"
#include "../../../agentos/commons/utils/string/include/string_compat.h"
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

agentos_error_t agentos_memoryrov_evolve(agentos_memoryrov_handle_t* handle) {
    if (!handle || !handle->initialized) {
        return AGENTOS_EINVAL;
    }

    /* 获取所有 ID */
    char** ids = NULL;
    size_t count = 0;
    agentos_error_t err = agentos_layer1_raw_list_ids(handle->l1_raw, &ids, &count);
    if (err != AGENTOS_SUCCESS || count == 0) {
        return err;
    }

    /* 对每个记忆进行演化 */
    for (size_t i = 0; i < count; i++) {
        /* 读取记忆内容 */
        void* data = NULL;
        size_t len = 0;
        err = agentos_layer1_raw_read(handle->l1_raw, ids[i], &data, &len);
        if (err != AGENTOS_SUCCESS || !data) {
            continue;
        }

        /* 在 L3 中建立关系（简单实现：按顺序连接） */
        if (i > 0) {
            err = agentos_knowledge_graph_add_relation(
                handle->l3_struct,
                ids[i - 1],
                ids[i],
                AGENTOS_RELATION_BEFORE,
                1.0f
            );
            if (err != AGENTOS_SUCCESS) {
                /* 关系添加失败不影响主流程 */
            }
        }

        AGENTOS_FREE(data);
    }

    /* 释放 ID 数组 */
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

    /* 使用 L2 特征层进行相似度检索 */
    char** result_ids = NULL;
    float* scores = NULL;
    size_t result_count = 0;

    agentos_error_t err = agentos_layer2_feature_search(
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
        return err;
    }

    /* 分配结果数组 */
    *out_results = (agentos_memory_t*)AGENTOS_CALLOC(result_count, sizeof(agentos_memory_t));
    if (!*out_results) {
        return AGENTOS_ENOMEM;
    }

    /* 填充结果 */
    for (size_t i = 0; i < result_count; i++) {
        /* 使用新的结构体字段名（record_id/score/data/data_len/metadata） */
        (*out_results)[i].record_id = AGENTOS_STRDUP(result_ids[i]);
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

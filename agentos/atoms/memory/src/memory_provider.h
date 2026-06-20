/**
 * @file memory_provider.h
 * @brief AgentRT 内存提供商接口（可拔插架构）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 定义内存提供商的函数指针表和能力标记。
 * AgentRT 内置免费提供商（builtin_provider）实现此接口，
 * MemoryRovol 商业提供商同样实现此接口。
 *
 * 架构：
 *   engine.c → agentos_memory_provider_t* → builtin_provider / MemoryRovol
 */

#ifndef AGENTOS_MEMORY_PROVIDER_H
#define AGENTOS_MEMORY_PROVIDER_H

#include "error.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 提供商能力标记
 */
typedef struct agentos_memory_capabilities {
    uint8_t l1_raw;          /* L1 原始存储 */
    uint8_t l2_feature;      /* L2 特征提取/向量索引 */
    uint8_t l3_structure;    /* L3 结构绑定/知识图谱 */
    uint8_t l4_pattern;      /* L4 模式识别 */
    uint8_t forgetting;      /* 遗忘引擎 */
    uint8_t attractor;       /* 吸引子网络 */
    uint8_t persistence;     /* 持久同调 */
    uint8_t faiss;           /* FAISS 加速索引 */
    uint8_t async_ops;       /* 异步操作 */
    uint8_t llm_integration; /* LLM 集成 */
    uint8_t reserved[5];     /* 保留 */
} agentos_memory_capabilities_t;

/**
 * @brief 内存统计信息
 */
typedef struct agentos_memory_stats {
    uint64_t total_records;
    uint64_t total_bytes;
    uint64_t l1_count;
    uint64_t l2_indexed;
    uint64_t l3_relations;
    uint64_t l4_patterns;
    double index_utilization;
    char provider_name[64];
    char provider_version[32];
} agentos_memory_stats_t;

/**
 * @brief 查询结果
 */
typedef struct agentos_memory_query_result {
    char **record_ids;
    float *scores;
    size_t count;
} agentos_memory_query_result_t;

struct agentos_memory_provider;

/**
 * @brief 内存提供商函数指针表
 */
typedef struct agentos_memory_provider {
    const char *name;
    const char *version;
    agentos_memory_capabilities_t capabilities;
    void *impl;
    struct agentos_memory_provider *sync_target;

    agentos_error_t (*init)(struct agentos_memory_provider *provider, const char *config_path);
    void (*destroy)(struct agentos_memory_provider *provider);

    agentos_error_t (*write_raw)(struct agentos_memory_provider *provider, const void *data,
                                 size_t len, const char *metadata_json, char **out_record_id);

    agentos_error_t (*get_raw)(struct agentos_memory_provider *provider, const char *record_id,
                               void **out_data, size_t *out_len);

    agentos_error_t (*delete_raw)(struct agentos_memory_provider *provider, const char *record_id);

    agentos_error_t (*query)(struct agentos_memory_provider *provider, const char *query_text,
                             uint32_t limit, char ***out_record_ids, float **out_scores,
                             size_t *out_count);

    agentos_error_t (*retrieve)(struct agentos_memory_provider *provider, const char *query_text,
                                uint32_t limit, char ***out_record_ids, float **out_scores,
                                size_t *out_count);

    agentos_error_t (*evolve)(struct agentos_memory_provider *provider, int force);

    agentos_error_t (*forget)(struct agentos_memory_provider *provider);

    agentos_error_t (*stats)(struct agentos_memory_provider *provider,
                             agentos_memory_stats_t *out_stats);

    agentos_error_t (*mount)(struct agentos_memory_provider *provider, const char *record_id,
                             const char *context);

    agentos_error_t (*health_check)(struct agentos_memory_provider *provider, char **out_json);

    agentos_error_t (*add_memory)(struct agentos_memory_provider *provider, const char *content,
                                  size_t content_len);

    agentos_error_t (*sync_push)(struct agentos_memory_provider *provider, const char *record_id);

    agentos_error_t (*sync_pull)(struct agentos_memory_provider *provider, const char *filter_json,
                                 char ***out_record_ids, size_t *out_count);

    int (*has_active_sync)(struct agentos_memory_provider *provider);

} agentos_memory_provider_t;

/**
 * @brief 注册内存提供商
 */
agentos_error_t agentos_memory_provider_register(agentos_memory_provider_t *provider);

/**
 * @brief 获取当前活跃的内存提供商
 */
agentos_memory_provider_t *agentos_memory_provider_get_active(void);

/**
 * @brief 设置活跃的内存提供商
 */
agentos_error_t agentos_memory_provider_set_active(agentos_memory_provider_t *provider);

void agentos_memory_provider_unregister(void);

/**
 * @brief 初始化内置免费提供商并注册为活跃
 */
agentos_error_t agentos_builtin_memory_provider_init(const char *storage_path);

agentos_memory_provider_t *agentos_builtin_provider_create(void);

void agentos_memory_provider_free_query_results(char **record_ids, float *scores, size_t count);

void agentos_memory_query_result_free(agentos_memory_query_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_MEMORY_PROVIDER_H */

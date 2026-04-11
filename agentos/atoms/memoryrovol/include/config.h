/**
 * @file manager.h
 * @brief MemoryRovol 全局配置结构
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_MEMORYROV_CONFIG_H
#define AGENTOS_MEMORYROV_CONFIG_H

#include "../../../corekern/include/agentos.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief MemoryRovol 全局配置
 */
typedef struct agentos_memoryrov_config {
    /* 存储路径 */
    char* raw_storage_path;           /**< L1原始卷存储根路径 */
    char* index_path;                 /**< L2索引持久化路径 */
    char* relation_db_path;           /**< L3关系数据库路径 */
    char* pattern_storage_path;       /**< L4 模式存储路径 */

    /* 模型配置 */
    char* embedding_model;             /**< 嵌入模型名称 */
    char* llm_model;                   /**< 用于模式挖掘的LLM模型 */

    /* L2 特征层参数 */
    uint32_t embedding_dim;            /**< 嵌入维度 */
    uint32_t index_type;               /**< 索引类型（0=flat, 1=ivf, 2=hnsw） */
    uint32_t hnsw_m;                   /**< HNSW M参数 */
    uint32_t ivf_nlist;                /**< IVF聚类中心数 */

    /* L3 结构层参数 */
    uint32_t binder_q;                 /**< 绑定算子Q参数 */
    int use_complex;                   /**< 是否使用复数域 */

    /* L4 模式层参数 */
    uint32_t pattern_min_support;      /**< 最小支持度 */
    double pattern_confidence;         /**< 最小置信度 */
    uint32_t pattern_mine_interval;    /**< 挖掘间隔（秒） */

    /* 检索参数 */
    uint32_t retrieval_max_candidates;
    float retrieval_threshold;
    uint32_t attractor_iterations;
    float attractor_damping;

    /* 遗忘参数 */
    uint32_t forget_strategy;          /**< 遗忘策略 */
    double forget_lambda;               /**< 衰减率 */
    double forget_threshold;            /**< 裁剪阈值 */
    uint32_t forget_check_interval;     /**< 检查间隔（秒） */
} agentos_memoryrov_config_t;

/**
 * @brief 加载配置文件（JSON/YAML）
 * @param file_path 配置文件路径
 * @param out_config 输出配置结构（需调用者释放）
 * @return agentos_error_t
 */
agentos_error_t agentos_memoryrov_config_load(
    const char* file_path,
    agentos_memoryrov_config_t** out_config);

/**
 * @brief 释放配置结构
 * @param manager 配置指针
 */
void agentos_memoryrov_config_free(agentos_memoryrov_config_t* manager);

/**
 * @brief 获取默认配置
 * @param out_config 输出默认配置（需调用者释放）
 * @return agentos_error_t
 */
agentos_error_t agentos_memoryrov_config_default(
    agentos_memoryrov_config_t** out_config);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_MEMORYROV_CONFIG_H */

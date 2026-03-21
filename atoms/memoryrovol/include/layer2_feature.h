/**
 * @file layer2_feature.h
 * @brief L2 特征层接口：特征提取、向量索引、相似度检索（修复版）
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#ifndef AGENTOS_LAYER2_FEATURE_H
#define AGENTOS_LAYER2_FEATURE_H

#include "agentos.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明 */
typedef struct agentos_layer2_feature agentos_layer2_feature_t;
typedef struct agentos_bm25_index agentos_bm25_index_t;
typedef struct agentos_embedder agentos_embedder_t;

/**
 * @brief 特征向量（带引用计数）
 */
typedef struct agentos_feature_vector {
    float* data;         /**< 向量数据 */
    size_t dim;          /**< 维度 */
    int ref_count;       /**< 引用计数（内部使用） */
} agentos_feature_vector_t;

/**
 * @brief L2 特征层配置
 */
typedef struct agentos_layer2_feature_config {
    const char* index_path;          /**< 索引持久化路径 */
    const char* embedding_model;     /**< 嵌入模型名称 */
    const char* api_key;             /**< API密钥（可选） */
    const char* api_base;            /**< API基础URL（可选） */
    size_t dimension;                /**< 向量维度（0表示自动） */
    uint32_t index_type;             /**< 索引类型（0=flat,1=ivf,2=hnsw） */
    uint32_t ivf_nlist;              /**< IVF聚类中心数 */
    uint32_t hnsw_m;                 /**< HNSW M参数 */
    uint32_t cache_size;             /**< 内存中向量缓存最大数量（0表示无限） */
    const char* vector_store_path;   /**< 向量持久化存储路径（若为空则不持久化） */
    uint32_t rebuild_interval_sec;   /**< 重建索引间隔（秒，0表示不自动重建） */
} agentos_layer2_feature_config_t;

/* ==================== 主层接口 ==================== */

/**
 * @brief 创建 L2 特征层实例
 * @param config 配置（若为NULL使用默认）
 * @param out_layer 输出层句柄
 * @return agentos_error_t
 */
agentos_error_t agentos_layer2_feature_create(
    const agentos_layer2_feature_config_t* config,
    agentos_layer2_feature_t** out_layer);

/**
 * @brief 销毁 L2 层
 * @param layer 层句柄
 */
void agentos_layer2_feature_destroy(agentos_layer2_feature_t* layer);

/**
 * @brief 添加向量到索引
 * @param layer L2 层句柄
 * @param record_id 关联的记录ID
 * @param text 原始文本（用于生成向量）
 * @return agentos_error_t
 */
agentos_error_t agentos_layer2_feature_add(
    agentos_layer2_feature_t* layer,
    const char* record_id,
    const char* text);

/**
 * @brief 批量添加向量
 * @param layer L2 层句柄
 * @param record_ids 记录ID数组
 * @param texts 文本数组
 * @param count 数量
 * @return agentos_error_t
 */
agentos_error_t agentos_layer2_feature_add_batch(
    agentos_layer2_feature_t* layer,
    const char** record_ids,
    const char** texts,
    size_t count);

/**
 * @brief 移除向量（删除记录时调用）
 * @param layer L2 层句柄
 * @param record_id 记录ID
 * @return agentos_error_t
 */
agentos_error_t agentos_layer2_feature_remove(
    agentos_layer2_feature_t* layer,
    const char* record_id);

/**
 * @brief 检索相似记忆（向量检索）
 * @param layer L2 层句柄
 * @param query 查询文本
 * @param top_k 返回最多数量
 * @param out_record_ids 输出记录ID数组（需调用者释放）
 * @param out_scores 输出相似度得分数组（需调用者释放）
 * @param out_count 输出实际数量
 * @return agentos_error_t
 */
agentos_error_t agentos_layer2_feature_search(
    agentos_layer2_feature_t* layer,
    const char* query,
    uint32_t top_k,
    char*** out_record_ids,
    float** out_scores,
    size_t* out_count);

/**
 * @brief 根据记录ID获取特征向量
 * @param layer L2 层句柄
 * @param record_id 记录ID
 * @param out_vector 输出向量（需调用者释放，使用后调用 agentos_feature_vector_free）
 * @return agentos_error_t
 */
agentos_error_t agentos_layer2_feature_get_vector(
    agentos_layer2_feature_t* layer,
    const char* record_id,
    agentos_feature_vector_t** out_vector);

/**
 * @brief 释放特征向量
 * @param vec 向量指针
 */
void agentos_feature_vector_free(agentos_feature_vector_t* vec);

/**
 * @brief 重建向量索引（用于删除后同步）
 * @param layer L2层句柄
 * @return agentos_error_t
 */
agentos_error_t agentos_layer2_feature_rebuild(agentos_layer2_feature_t* layer);

/**
 * @brief 获取统计信息
 * @param layer L2层句柄
 * @param out_stats 输出JSON字符串（需调用者释放）
 * @return agentos_error_t
 */
agentos_error_t agentos_layer2_feature_stats(
    agentos_layer2_feature_t* layer,
    char** out_stats);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_LAYER2_FEATURE_H */
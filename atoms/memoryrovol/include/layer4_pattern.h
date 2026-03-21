/**
 * @file layer4_pattern.h
 * @brief L4 模式层接口：持久同调挖掘、聚类、规则生成、验证（支持自动挖掘）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_LAYER4_PATTERN_H
#define AGENTOS_LAYER4_PATTERN_H

#include "agentos.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明 */
typedef struct agentos_layer4_pattern agentos_layer4_pattern_t;
typedef struct agentos_persistence_calculator agentos_persistence_calculator_t;
typedef struct agentos_clustering_engine agentos_clustering_engine_t;
typedef struct agentos_rule_generator agentos_rule_generator_t;
typedef struct agentos_pattern_validator agentos_pattern_validator_t;

/**
 * @brief L4 模式层配置
 */
 // From data intelligence emerges. by spharx
typedef struct agentos_layer4_pattern_config {
    uint32_t min_cluster_size;               /**< 最小聚类大小 */
    double persistence_threshold;             /**< 持久性阈值 */
    uint32_t mining_interval_sec;             /**< 自动挖掘间隔（秒，0表示不自动） */
    const char* pattern_storage_path;         /**< 模式持久化路径（可选） */
} agentos_layer4_pattern_config_t;

/**
 * @brief 模式结构
 */
typedef struct agentos_pattern {
    char* pattern_id;          /**< 模式ID */
    char* description;         /**< 模式描述 */
    float confidence;          /**< 置信度（0-1） */
    uint64_t created_at;       /**< 创建时间（纳秒） */
    uint64_t last_used;        /**< 最后使用时间 */
    uint32_t usage_count;      /**< 使用次数 */
    char* rule_json;           /**< 可执行规则（JSON） */
    float* centroid;           /**< 模式中心向量（可选） */
    size_t dimension;          /**< 中心向量维度 */
} agentos_pattern_t;

/**
 * @brief 持久特征
 */
typedef struct agentos_persistence_feature {
    uint32_t dimension;        /**< 同调维数（0,1,2...） */
    double birth;              /**< 出生阈值 */
    double death;              /**< 死亡阈值 */
    double persistence;        /**< 持久性 = death - birth */
    float confidence;          /**< 置信度 */
} agentos_persistence_feature_t;

/* ==================== 持久同调挖掘器接口 ==================== */

/**
 * @brief 创建持久同调挖掘器
 * @param config 配置（若为NULL使用默认）
 * @param out_miner 输出挖掘器句柄
 * @return agentos_error_t
 */
agentos_error_t agentos_pattern_miner_create(
    const agentos_layer4_pattern_config_t* config,
    agentos_layer4_pattern_t** out_miner);

/**
 * @brief 销毁挖掘器
 * @param miner 挖掘器句柄
 */
void agentos_pattern_miner_destroy(agentos_layer4_pattern_t* miner);

/**
 * @brief 执行模式挖掘
 * @param miner 挖掘器
 * @param vectors 向量数组（L2/L3层输出）
 * @param vector_ids 向量对应的记忆ID数组
 * @param count 向量数量
 * @param out_patterns 输出发现的模式数组（需调用者释放）
 * @param out_count 输出数量
 * @return agentos_error_t
 */
agentos_error_t agentos_pattern_miner_mine(
    agentos_layer4_pattern_t* miner,
    const float* vectors,
    const char** vector_ids,
    size_t count,
    agentos_pattern_t*** out_patterns,
    size_t* out_count);

/**
 * @brief 启动自动挖掘（后台线程）
 * @param miner 挖掘器
 * @return agentos_error_t
 */
agentos_error_t agentos_pattern_miner_start_auto(agentos_layer4_pattern_t* miner);

/**
 * @brief 停止自动挖掘
 * @param miner 挖掘器
 */
void agentos_pattern_miner_stop_auto(agentos_layer4_pattern_t* miner);

/* ==================== 持久性计算器接口 ==================== */

/**
 * @brief 创建持久性计算器
 * @param config 配置（若为NULL使用默认）
 * @param out_calc 输出计算器句柄
 * @return agentos_error_t
 */
agentos_error_t agentos_persistence_calculator_create(
    const void* config,
    agentos_persistence_calculator_t** out_calc);

/**
 * @brief 销毁计算器
 * @param calc 计算器句柄
 */
void agentos_persistence_calculator_destroy(agentos_persistence_calculator_t* calc);

/**
 * @brief 计算持久性特征
 * @param calc 计算器
 * @param distance_matrix 距离矩阵（上三角或完整矩阵）
 * @param n 点数
 * @param out_features 输出持久特征数组（需调用者释放）
 * @param out_count 输出数量
 * @return agentos_error_t
 */
agentos_error_t agentos_persistence_calculator_compute(
    agentos_persistence_calculator_t* calc,
    const float* distance_matrix,
    size_t n,
    agentos_persistence_feature_t*** out_features,
    size_t* out_count);

/**
 * @brief 释放持久特征数组
 * @param features 特征数组
 * @param count 数量
 */
void agentos_persistence_features_free(
    agentos_persistence_feature_t** features,
    size_t count);

/**
 * @brief 计算噪声阈值（基于随机拓扑理论）
 * @param calc 计算器
 * @param distances 距离数组（用于估计噪声分布）
 * @param count 距离数量
 * @return 阈值
 */
double agentos_persistence_noise_threshold(
    agentos_persistence_calculator_t* calc,
    const float* distances,
    size_t count);

/* ==================== 聚类引擎接口 ==================== */

/**
 * @brief 创建聚类引擎
 * @param method 聚类方法（"hdbscan", "dbscan", "kmeans"等）
 * @param config 配置（JSON字符串）
 * @param out_engine 输出引擎句柄
 * @return agentos_error_t
 */
agentos_error_t agentos_clustering_engine_create(
    const char* method,
    const char* config,
    agentos_clustering_engine_t** out_engine);

/**
 * @brief 销毁聚类引擎
 * @param engine 引擎句柄
 */
void agentos_clustering_engine_destroy(agentos_clustering_engine_t* engine);

/**
 * @brief 执行聚类
 * @param engine 聚类引擎
 * @param vectors 向量数组
 * @param count 向量数量
 * @param out_labels 输出标签数组（-1表示噪声）
 * @param out_centroids 输出聚类中心（可选，需释放）
 * @param out_num_clusters 输出聚类数量
 * @return agentos_error_t
 */
agentos_error_t agentos_clustering_engine_cluster(
    agentos_clustering_engine_t* engine,
    const float* vectors,
    size_t count,
    int** out_labels,
    float** out_centroids,
    int* out_num_clusters);

/* ==================== 规则生成器接口 ==================== */

/**
 * @brief 创建规则生成器
 * @param llm_service LLM服务句柄（用于生成规则描述）
 * @param out_gen 输出生成器句柄
 * @return agentos_error_t
 */
agentos_error_t agentos_rule_generator_create(
    void* llm_service,
    agentos_rule_generator_t** out_gen);

/**
 * @brief 销毁规则生成器
 * @param gen 生成器句柄
 */
void agentos_rule_generator_destroy(agentos_rule_generator_t* gen);

/**
 * @brief 从聚类生成规则
 * @param gen 规则生成器
 * @param cluster_vectors 聚类向量
 * @param cluster_ids 聚类的记忆ID
 * @param count 聚类大小
 * @param out_rule 输出规则JSON字符串（需调用者释放）
 * @return agentos_error_t
 */
agentos_error_t agentos_rule_generator_generate(
    agentos_rule_generator_t* gen,
    const float* cluster_vectors,
    const char** cluster_ids,
    size_t count,
    char** out_rule);

/* ==================== 模式验证器接口 ==================== */

/**
 * @brief 创建模式验证器
 * @param config 配置
 * @param out_validator 输出验证器句柄
 * @return agentos_error_t
 */
agentos_error_t agentos_pattern_validator_create(
    const void* config,
    agentos_pattern_validator_t** out_validator);

/**
 * @brief 销毁验证器
 * @param validator 验证器句柄
 */
void agentos_pattern_validator_destroy(agentos_pattern_validator_t* validator);

/**
 * @brief 验证模式的有效性
 * @param validator 验证器
 * @param pattern 模式
 * @param test_vectors 测试向量（可NULL）
 * @param test_count 测试数量
 * @param out_valid 输出是否有效
 * @param out_confidence 输出置信度
 * @return agentos_error_t
 */
agentos_error_t agentos_pattern_validator_validate(
    agentos_pattern_validator_t* validator,
    const agentos_pattern_t* pattern,
    const float* test_vectors,
    size_t test_count,
    int* out_valid,
    float* out_confidence);

/* ==================== 模式对象管理 ==================== */

/**
 * @brief 创建模式对象
 * @param description 描述
 * @param rule_json 规则JSON
 * @param confidence 置信度
 * @param centroid 中心向量（可选）
 * @param dimension 中心维度
 * @return 新模式，失败返回NULL
 */
agentos_pattern_t* agentos_pattern_create(
    const char* description,
    const char* rule_json,
    float confidence,
    const float* centroid,
    size_t dimension);

/**
 * @brief 复制模式
 * @param src 源模式
 * @return 新副本，失败返回NULL
 */
agentos_pattern_t* agentos_pattern_copy(const agentos_pattern_t* src);

/**
 * @brief 释放模式
 * @param pattern 模式指针
 */
void agentos_pattern_free(agentos_pattern_t* pattern);

/**
 * @brief 释放模式数组
 * @param patterns 模式数组
 * @param count 数量
 */
void agentos_patterns_free(agentos_pattern_t** patterns, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_LAYER4_PATTERN_H */
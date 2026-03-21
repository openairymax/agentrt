/**
 * @file retrieval.h
 * @brief 检索机制接口：吸引子网络、缓存、能量函数、挂载、重排序
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#ifndef AGENTOS_RETRIEVAL_H
#define AGENTOS_RETRIEVAL_H

#include "agentos.h"
#include "layer1_raw.h"
#include "layer2_feature.h"
#include "forgetting.h"
#include "llm_client.h"
#include "token_counter.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 前向声明 ==================== */
typedef struct agentos_attractor_network agentos_attractor_network_t;
typedef struct agentos_retrieval_cache agentos_retrieval_cache_t;
typedef struct agentos_mounter agentos_mounter_t;
typedef struct agentos_reranker agentos_reranker_t;

/**
 * @brief 检索配置
 */
typedef struct agentos_retrieval_config {
    uint32_t max_iterations;       /**< 吸引子网络最大迭代次数 */
    float tolerance;               /**< 收敛容差 */
    float beta;                    /**< 非线性参数 */
} agentos_retrieval_config_t;

/* ==================== 吸引子网络 ==================== */

/**
 * @brief 创建吸引子网络
 * @param layer2 特征层句柄
 * @param config 配置（可为NULL使用默认）
 * @param out_net 输出网络句柄
 * @return agentos_error_t
 */
agentos_error_t agentos_attractor_network_create(
    agentos_layer2_feature_t* layer2,
    const agentos_retrieval_config_t* config,
    agentos_attractor_network_t** out_net);

/**
 * @brief 销毁吸引子网络
 * @param net 网络句柄
 */
void agentos_attractor_network_destroy(agentos_attractor_network_t* net);

/**
 * @brief 使用吸引子网络检索最佳匹配
 * @param net 网络句柄
 * @param query_vector 查询向量
 * @param candidate_ids 候选记录ID数组
 * @param candidate_count 候选数量
 * @param out_best_id 输出最佳记录ID（需调用者释放）
 * @param out_confidence 输出置信度
 * @return agentos_error_t
 */
agentos_error_t agentos_attractor_network_retrieve(
    agentos_attractor_network_t* net,
    const float* query_vector,
    const char** candidate_ids,
    size_t candidate_count,
    char** out_best_id,
    float* out_confidence);

/* ==================== 检索缓存 ==================== */

/**
 * @brief 创建检索缓存
 * @param max_size 最大缓存条目数
 * @param out_cache 输出缓存句柄
 * @return agentos_error_t
 */
agentos_error_t agentos_retrieval_cache_create(
    size_t max_size,
    agentos_retrieval_cache_t** out_cache);

/**
 * @brief 销毁检索缓存
 * @param cache 缓存句柄
 */
void agentos_retrieval_cache_destroy(agentos_retrieval_cache_t* cache);

/**
 * @brief 从缓存中获取检索结果
 * @param cache 缓存句柄
 * @param query 查询文本
 * @param out_result_ids 输出结果ID数组（需调用者释放）
 * @param out_count 输出数量
 * @return AGENTOS_SUCCESS 或 AGENTOS_ENOENT
 */
agentos_error_t agentos_retrieval_cache_get(
    agentos_retrieval_cache_t* cache,
    const char* query,
    char*** out_result_ids,
    size_t* out_count);

/**
 * @brief 将检索结果存入缓存
 * @param cache 缓存句柄
 * @param query 查询文本
 * @param result_ids 结果ID数组
 * @param result_count 结果数量
 * @return agentos_error_t
 */
agentos_error_t agentos_retrieval_cache_put(
    agentos_retrieval_cache_t* cache,
    const char* query,
    const char** result_ids,
    size_t result_count);

/* ==================== 能量函数 ==================== */

/**
 * @brief Hopfield网络能量函数
 * @param state 当前状态向量
 * @param patterns 存储的模式矩阵 (n x dim)
 * @param n 模式数量
 * @param dim 向量维度
 * @return 能量值
 */
float agentos_energy_hopfield(
    const float* state,
    const float* patterns,
    size_t n,
    size_t dim);

/**
 * @brief 记忆能量函数（用于遗忘）
 * @param forgetting 遗忘引擎句柄
 * @param record_id 记录ID
 * @return 能量值（越小越稳定）
 */
float agentos_energy_memory(
    agentos_forgetting_engine_t* forgetting,
    const char* record_id);

/* ==================== 挂载器 ==================== */

/**
 * @brief 创建挂载器
 * @param layer1 原始层句柄
 * @param token_counter token计数器句柄
 * @param token_limit 最大允许token数
 * @param out_mounter 输出挂载器句柄
 * @return agentos_error_t
 */
agentos_error_t agentos_mounter_create(
    agentos_layer1_raw_t* layer1,
    agentos_token_counter_t* token_counter,
    size_t token_limit,
    agentos_mounter_t** out_mounter);

/**
 * @brief 销毁挂载器
 * @param mounter 挂载器句柄
 */
void agentos_mounter_destroy(agentos_mounter_t* mounter);

/**
 * @brief 挂载记忆到工作记忆
 * @param mounter 挂载器句柄
 * @param record_id 记录ID
 * @param context 上下文标识（可为NULL）
 * @param out_content 输出挂载内容（需调用者释放）
 * @return agentos_error_t
 */
agentos_error_t agentos_mounter_mount(
    agentos_mounter_t* mounter,
    const char* record_id,
    const char* context,
    char** out_content);

/**
 * @brief 从工作记忆中卸载记忆
 * @param mounter 挂载器句柄
 * @param record_id 记录ID
 * @return agentos_error_t
 */
agentos_error_t agentos_mounter_unmount(
    agentos_mounter_t* mounter,
    const char* record_id);

/**
 * @brief 列出当前挂载的所有记忆ID
 * @param mounter 挂载器句柄
 * @param out_ids 输出ID数组（需调用者释放）
 * @param out_count 输出数量
 * @return agentos_error_t
 */
agentos_error_t agentos_mounter_list(
    agentos_mounter_t* mounter,
    char*** out_ids,
    size_t* out_count);

/* ==================== 重排序器 ==================== */

/**
 * @brief 创建重排序器
 * @param llm LLM服务句柄（用于交叉编码）
 * @param layer1 原始层句柄（用于获取文档文本）
 * @param out_reranker 输出重排序器句柄
 * @return agentos_error_t
 */
agentos_error_t agentos_reranker_create(
    agentos_llm_service_t* llm,
    agentos_layer1_raw_t* layer1,
    agentos_reranker_t** out_reranker);

/**
 * @brief 销毁重排序器
 * @param reranker 重排序器句柄
 */
void agentos_reranker_destroy(agentos_reranker_t* reranker);

/**
 * @brief 设置是否使用LLM（降级控制）
 * @param reranker 重排序器句柄
 * @param use_llm 1使用LLM，0降级
 */
void agentos_reranker_set_use_llm(agentos_reranker_t* reranker, int use_llm);

/**
 * @brief 对检索结果进行重排序
 * @param reranker 重排序器句柄
 * @param query 查询文本
 * @param candidate_ids 候选记录ID数组
 * @param initial_scores 初始分数数组（可为NULL）
 * @param candidate_count 候选数量
 * @param out_reranked_ids 输出重排序后的ID数组（需调用者释放）
 * @param out_reranked_scores 输出重排序后的分数数组（需调用者释放）
 * @return agentos_error_t
 */
agentos_error_t agentos_reranker_rerank(
    agentos_reranker_t* reranker,
    const char* query,
    const char** candidate_ids,
    const float* initial_scores,
    size_t candidate_count,
    char*** out_reranked_ids,
    float** out_reranked_scores);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_RETRIEVAL_H */
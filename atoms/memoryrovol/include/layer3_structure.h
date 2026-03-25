/**
 * @file layer3_structure.h
 * @brief L3 结构层接口：关系编码、绑定算子、图结构、时序编码（支持持久化）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_LAYER3_STRUCTURE_H
#define AGENTOS_LAYER3_STRUCTURE_H

#include "agentos.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明 */
typedef struct agentos_layer3_structure agentos_layer3_structure_t;
typedef struct agentos_binder agentos_binder_t;
typedef struct agentos_unbinder agentos_unbinder_t;
typedef struct agentos_relation_encoder agentos_relation_encoder_t;
typedef struct agentos_sequence_encoder agentos_sequence_encoder_t;
typedef struct agentos_graph_encoder agentos_graph_encoder_t;

/**
 * @brief L3 结构层配置
 // From data intelligence emerges. by spharx
 */
typedef struct agentos_layer3_structure_config {
    const char* db_path;               /**< SQLite数据库路径（用于持久化） */
    uint32_t binder_q;                  /**< 绑定算子Q参数 */
    int use_complex;                    /**< 是否使用复数域 */
} agentos_layer3_structure_config_t;

/**
 * @brief 关系类型
 */
typedef enum {
    AGENTOS_REL_UNKNOWN = 0,
    AGENTOS_REL_SEQUENCE,      /**< 时序关系 */
    AGENTOS_REL_CAUSAL,        /**< 因果关系 */
    AGENTOS_REL_ATTRIBUTE,     /**< 属性-主体关系 */
    AGENTOS_REL_SIMILAR,       /**< 相似关系 */
    AGENTOS_REL_COMPOSITE      /**< 组合关系 */
} agentos_relation_type_t;

/**
 * @brief 关系定义
 */
typedef struct agentos_relation {
    char* from_id;                   /**< 源记忆ID */
    char* to_id;                     /**< 目标记忆ID */
    agentos_relation_type_t type;    /**< 关系类型 */
    float weight;                    /**< 权重（0-1） */
    char* metadata_json;             /**< 附加元数据（JSON） */
} agentos_relation_t;

/**
 * @brief 图节点（记忆ID）
 */
typedef struct agentos_graph_node {
    char* node_id;                   /**< 节点ID（记忆ID） */
    float* feature;                  /**< 节点特征向量（可选，仅内存） */
} agentos_graph_node_t;

/**
 * @brief 图边（关系）
 */
typedef struct agentos_graph_edge {
    char* from_id;                   /**< 源节点ID */
    char* to_id;                     /**< 目标节点ID */
    float weight;                    /**< 边权重 */
    agentos_relation_type_t type;    /**< 边类型 */
} agentos_graph_edge_t;

/* ==================== 绑定算子接口 ==================== */

/**
 * @brief 创建绑定算子
 * @param dimension 向量维度
 * @param Q 绑定参数（1 ≤ Q ≤ dimension）
 * @param use_complex 是否使用复数域
 * @return 绑定算子句柄，失败返回NULL
 */
agentos_binder_t* agentos_binder_create(size_t dimension, int Q, int use_complex);

/**
 * @brief 销毁绑定算子
 * @param binder 绑定算子句柄
 */
void agentos_binder_destroy(agentos_binder_t* binder);

/**
 * @brief 绑定多个向量
 * @param binder 绑定算子
 * @param vectors 向量数组
 * @param count 向量数量
 * @param out_vector 输出绑定后的向量（需调用者释放）
 * @return agentos_error_t
 */
agentos_error_t agentos_binder_bind(
    agentos_binder_t* binder,
    const float* vectors[],
    size_t count,
    float** out_vector);

/**
 * @brief 获取当前Q参数
 * @param binder 绑定算子
 * @return Q值
 */
int agentos_binder_get_q(agentos_binder_t* binder);

/**
 * @brief 设置Q参数（重新生成绑定矩阵）
 * @param binder 绑定算子
 * @param Q 新Q值
 */
void agentos_binder_set_q(agentos_binder_t* binder, int Q);

/* ==================== 解绑算子接口 ==================== */

/**
 * @brief 创建解绑算子
 * @param binder 关联的绑定算子
 * @return 解绑算子句柄，失败返回NULL
 */
agentos_unbinder_t* agentos_unbinder_create(agentos_binder_t* binder);

/**
 * @brief 销毁解绑算子
 * @param unbinder 解绑算子句柄
 */
void agentos_unbinder_destroy(agentos_unbinder_t* unbinder);

/**
 * @brief 从绑定向量中解绑出未知向量
 * @param unbinder 解绑算子
 * @param bound_vector 绑定后的向量
 * @param known_vectors 已知向量数组
 * @param known_count 已知向量数量
 * @param out_vectors 输出解绑后的向量数组（需调用者释放）
 * @param out_count 输出向量数量
 * @return agentos_error_t
 */
agentos_error_t agentos_unbinder_unbind(
    agentos_unbinder_t* unbinder,
    const float* bound_vector,
    const float* known_vectors[],
    size_t known_count,
    float*** out_vectors,
    size_t* out_count);

/* ==================== 关系编码器接口 ==================== */

/**
 * @brief 创建关系编码器
 * @param binder 绑定算子
 * @return 编码器句柄，失败返回NULL
 */
agentos_relation_encoder_t* agentos_relation_encoder_create(agentos_binder_t* binder);

/**
 * @brief 销毁关系编码器
 * @param enc 编码器句柄
 */
void agentos_relation_encoder_destroy(agentos_relation_encoder_t* enc);

/**
 * @brief 编码三元组 (subject, predicate, object)
 * @param enc 编码器
 * @param subject 主体向量
 * @param predicate 谓词向量
 * @param object 客体向量
 * @param out_relation 输出编码后的关系向量（需释放）
 * @return agentos_error_t
 */
agentos_error_t agentos_relation_encode_triple(
    agentos_relation_encoder_t* enc,
    const float* subject,
    const float* predicate,
    const float* object,
    float** out_relation);

/**
 * @brief 编码事件（多个关系的组合）
 * @param enc 编码器
 * @param relations 关系数组（包含ID，需从记忆获取向量）
 * @param count 数量
 * @param out_event 输出事件向量
 * @return agentos_error_t
 */
agentos_error_t agentos_relation_encode_event(
    agentos_relation_encoder_t* enc,
    const agentos_relation_t** relations,
    size_t count,
    float** out_event);

/* ==================== 时序编码器接口 ==================== */

/**
 * @brief 创建时序编码器
 * @param binder 绑定算子
 * @param position_encoding 位置编码方式（0=随机,1=正弦,2=学习）
 * @return 编码器句柄
 */
agentos_sequence_encoder_t* agentos_sequence_encoder_create(
    agentos_binder_t* binder,
    int position_encoding);

/**
 * @brief 销毁时序编码器
 * @param enc 编码器
 */
void agentos_sequence_encoder_destroy(agentos_sequence_encoder_t* enc);

/**
 * @brief 编码序列
 * @param enc 编码器
 * @param items 项目向量数组
 * @param count 项目数量
 * @param out_sequence 输出序列向量（需释放）
 * @return agentos_error_t
 */
agentos_error_t agentos_sequence_encode(
    agentos_sequence_encoder_t* enc,
    const float** items,
    size_t count,
    float** out_sequence);

/**
 * @brief 获取位置向量（用于外部使用）
 * @param enc 编码器
 * @param index 位置索引
 * @param out_vec 输出位置向量（需释放）
 * @return agentos_error_t
 */
agentos_error_t agentos_sequence_get_position(
    agentos_sequence_encoder_t* enc,
    size_t index,
    float** out_vec);

/* ==================== 图编码器接口 ==================== */

/**
 * @brief 创建图编码器
 * @param binder 绑定算子
 * @param node_dim 节点特征维度
 * @param config 配置（若为NULL使用默认）
 * @return 编码器句柄
 */
agentos_graph_encoder_t* agentos_graph_encoder_create(
    agentos_binder_t* binder,
    size_t node_dim,
    const agentos_layer3_structure_config_t* config);

/**
 * @brief 销毁图编码器
 * @param enc 编码器
 */
void agentos_graph_encoder_destroy(agentos_graph_encoder_t* enc);

/**
 * @brief 添加节点到图
 * @param enc 编码器
 * @param node_id 节点ID
 * @param feature 节点特征向量（可选，可为NULL）
 * @return agentos_error_t
 */
agentos_error_t agentos_graph_add_node(
    agentos_graph_encoder_t* enc,
    const char* node_id,
    const float* feature);

/**
 * @brief 添加边
 * @param enc 编码器
 * @param edge 边定义
 * @return agentos_error_t
 */
agentos_error_t agentos_graph_add_edge(
    agentos_graph_encoder_t* enc,
    const agentos_graph_edge_t* edge);

/**
 * @brief 编码整个图为单一向量（通过图神经网络或绑定）
 * @param enc 编码器
 * @param out_graph_vector 输出图向量
 * @return agentos_error_t
 */
agentos_error_t agentos_graph_encode(
    agentos_graph_encoder_t* enc,
    float** out_graph_vector);

/**
 * @brief 根据节点ID查询邻居
 * @param enc 编码器
 * @param node_id 节点ID
 * @param out_edges 输出边数组（需释放）
 * @param out_count 输出数量
 * @return agentos_error_t
 */
agentos_error_t agentos_graph_get_neighbors(
    agentos_graph_encoder_t* enc,
    const char* node_id,
    agentos_graph_edge_t*** out_edges,
    size_t* out_count);

/**
 * @brief 释放边数组
 * @param edges 边数组
 * @param count 数量
 */
void agentos_graph_edges_free(agentos_graph_edge_t** edges, size_t count);

/* ==================== 辅助函数 ==================== */

/**
 * @brief 释放关系数组
 * @param relations 关系数组
 * @param count 数量
 */
void agentos_relations_free(agentos_relation_t** relations, size_t count);

/**
 * @brief 复制关系
 * @param src 源关系
 * @return 新关系副本（需释放）
 */
agentos_relation_t* agentos_relation_copy(const agentos_relation_t* src);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_LAYER3_STRUCTURE_H */
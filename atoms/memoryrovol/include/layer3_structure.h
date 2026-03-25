/**
 * @file layer3_structure.h
 * @brief L3 结构层接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_LAYER3_STRUCTURE_H
#define AGENTOS_LAYER3_STRUCTURE_H

#include "agentos.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 关系类型
 */
typedef enum {
    AGENTOS_RELATION_BEFORE = 0,
    AGENTOS_RELATION_AFTER,
    AGENTOS_RELATION_CAUSES,
    AGENTOS_RELATION_ENABLED_BY,
    AGENTOS_RELATION_MEMBERS_OF,
    AGENTOS_RELATION_SIMILAR_TO
} agentos_relation_type_t;

/**
 * @brief 关系边
 */
typedef struct agentos_relation {
    char* from_id;
    char* to_id;
    agentos_relation_type_t type;
    float weight;
    struct agentos_relation* next;
} agentos_relation_t;

/**
 * @brief 知识图谱句柄
 */
typedef struct agentos_knowledge_graph agentos_knowledge_graph_t;

/**
 * @brief 创建知识图谱
 */
agentos_error_t agentos_knowledge_graph_create(
    agentos_knowledge_graph_t** out);

/**
 * @brief 销毁知识图谱
 */
void agentos_knowledge_graph_destroy(agentos_knowledge_graph_t* kg);

/**
 * @brief 添加实体
 */
agentos_error_t agentos_knowledge_graph_add_entity(
    agentos_knowledge_graph_t* kg,
    const char* entity_id);

/**
 * @brief 添加关系
 */
agentos_error_t agentos_knowledge_graph_add_relation(
    agentos_knowledge_graph_t* kg,
    const char* from_id,
    const char* to_id,
    agentos_relation_type_t type,
    float weight);

/**
 * @brief 查询关系
 */
agentos_error_t agentos_knowledge_graph_query(
    agentos_knowledge_graph_t* kg,
    const char* entity_id,
    agentos_relation_type_t relation_type,
    char*** out_related_ids,
    size_t* out_count);

/**
 * @brief 查找最短路径
 */
agentos_error_t agentos_knowledge_graph_find_path(
    agentos_knowledge_graph_t* kg,
    const char* from_id,
    const char* to_id,
    char*** out_path,
    size_t* out_path_length);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_LAYER3_STRUCTURE_H */

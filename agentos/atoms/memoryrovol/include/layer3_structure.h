/**
 * @file layer3_structure.h
 * @brief L3 结构层公共接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 公共头文件仅暴露不透明指针和 API 函数声明。
 * 内部结构定义见 layer3_structure_internal.h（仅实现文件使用）。
 */

#ifndef AGENTOS_LAYER3_STRUCTURE_H
#define AGENTOS_LAYER3_STRUCTURE_H

#include "agentos.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AGENTOS_RELATION_BEFORE = 0,
    AGENTOS_RELATION_AFTER,
    AGENTOS_RELATION_CAUSES,
    AGENTOS_RELATION_ENABLED_BY,
    AGENTOS_RELATION_MEMBERS_OF,
    AGENTOS_RELATION_SIMILAR_TO
} agentos_relation_type_t;

typedef struct agentos_relation {
    char* from_id;
    char* to_id;
    agentos_relation_type_t type;
    float weight;
    char* metadata_json;
    struct agentos_relation* next;
} agentos_relation_t;

typedef struct agentos_binder agentos_binder_t;
typedef struct agentos_unbinder agentos_unbinder_t;
typedef struct agentos_sequence_encoder agentos_sequence_encoder_t;
typedef struct agentos_relation_encoder agentos_relation_encoder_t;

agentos_error_t agentos_binder_bind(
    agentos_binder_t* binder,
    const float** vectors,
    size_t count,
    float** out_result);

typedef struct agentos_layer3_structure_config {
    uint32_t max_nodes;
    uint32_t max_edges;
    float similarity_threshold;
    const char* storage_path;
    const char* db_path;
} agentos_layer3_structure_config_t;

typedef struct agentos_knowledge_graph agentos_knowledge_graph_t;

agentos_error_t agentos_knowledge_graph_create(
    agentos_knowledge_graph_t** out);

void agentos_knowledge_graph_destroy(agentos_knowledge_graph_t* kg);

agentos_error_t agentos_knowledge_graph_add_entity(
    agentos_knowledge_graph_t* kg,
    const char* entity_id);

agentos_error_t agentos_knowledge_graph_add_relation(
    agentos_knowledge_graph_t* kg,
    const char* from_id,
    const char* to_id,
    agentos_relation_type_t type,
    float weight);

agentos_error_t agentos_knowledge_graph_query(
    agentos_knowledge_graph_t* kg,
    const char* entity_id,
    agentos_relation_type_t relation_type,
    char*** out_related_ids,
    size_t* out_count);

agentos_error_t agentos_knowledge_graph_find_path(
    agentos_knowledge_graph_t* kg,
    const char* from_id,
    const char* to_id,
    char*** out_path,
    size_t* out_path_length);

agentos_error_t agentos_knowledge_graph_stats(
    agentos_knowledge_graph_t* kg,
    size_t* out_entity_count,
    size_t* out_relation_count);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_LAYER3_STRUCTURE_H */

/**
 * @file graph.c
 * @brief L3 知识图谱实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "../include/layer3_structure.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../commons/utils/memory/include/memory_compat.h"
#include "../../../commons/utils/string/include/string_compat.h"
#include <string.h>
#include <pthread.h>
#include <limits.h>

#define INITIAL_CAPACITY 1024
#define MAX_PATH_LENGTH 100

/**
 * @brief 实体节点
 */
typedef struct entity_node {
    char* id;
    agentos_relation_t* relations;
    size_t relation_count;
    struct entity_node* next;
} entity_node_t;

/**
 * @brief BFS路径节点（用于路径查找）
 */
typedef struct {
    char* id;
    char* prev;
} path_node_t;

/**
 * @brief 知识图谱结构
 */
struct agentos_knowledge_graph {
    entity_node_t** entities;
    size_t entity_count;
    size_t capacity;
    agentos_mutex_t* mutex;
};

/**
 * @brief 创建知识图谱
 */
agentos_error_t agentos_knowledge_graph_create(
    agentos_knowledge_graph_t** out) {
    if (!out) return AGENTOS_EINVAL;

    agentos_knowledge_graph_t* kg = (agentos_knowledge_graph_t*)
        AGENTOS_CALLOC(1, sizeof(agentos_knowledge_graph_t));
    if (!kg) return AGENTOS_ENOMEM;

    kg->entities = (entity_node_t**)AGENTOS_CALLOC(INITIAL_CAPACITY, sizeof(entity_node_t*));
    if (!kg->entities) {
        AGENTOS_FREE(kg);
        return AGENTOS_ENOMEM;
    }

    kg->capacity = INITIAL_CAPACITY;
    kg->entity_count = 0;
    kg->mutex = agentos_mutex_create();
    if (!kg->mutex) {
        AGENTOS_FREE(kg->entities);
        AGENTOS_FREE(kg);
        return AGENTOS_ENOMEM;
    }

    *out = kg;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 销毁知识图�?
 */
void agentos_knowledge_graph_destroy(agentos_knowledge_graph_t* kg) {
    if (!kg) return;

    agentos_mutex_lock(kg->mutex);

    for (size_t i = 0; i < kg->entity_count; i++) {
        entity_node_t* node = kg->entities[i];
        if (node) {
            agentos_relation_t* rel = node->relations;
            while (rel) {
                agentos_relation_t* next = rel->next;
                if (rel->from_id) AGENTOS_FREE(rel->from_id);
                if (rel->to_id) AGENTOS_FREE(rel->to_id);
                AGENTOS_FREE(rel);
                rel = next;
            }
            if (node->id) AGENTOS_FREE(node->id);
            AGENTOS_FREE(node);
        }
    }
    AGENTOS_FREE(kg->entities);

    agentos_mutex_unlock(kg->mutex);
    agentos_mutex_destroy(kg->mutex);
    AGENTOS_FREE(kg);
}

/**
 * @brief 查找实体节点
 */
static entity_node_t* find_entity(agentos_knowledge_graph_t* kg, const char* entity_id) {
    for (size_t i = 0; i < kg->entity_count; i++) {
        if (kg->entities[i] && strcmp(kg->entities[i]->id, entity_id) == 0) {
            return kg->entities[i];
        }
    }
    return NULL;
}

/**
 * @brief 查找实体索引
 * @param kg 知识图谱
 * @param entity_id 实体ID
 * @return 实体索引，如果未找到返回 SIZE_MAX
 */
static size_t find_entity_index(agentos_knowledge_graph_t* kg, const char* entity_id) {
    for (size_t i = 0; i < kg->entity_count; i++) {
        if (kg->entities[i] && strcmp(kg->entities[i]->id, entity_id) == 0) {
            return i;
        }
    }
    return SIZE_MAX;
}

/**
 * @brief 添加实体
 */
agentos_error_t agentos_knowledge_graph_add_entity(
    agentos_knowledge_graph_t* kg,
    const char* entity_id) {
    if (!kg || !entity_id) return AGENTOS_EINVAL;

    pthread_mutex_lock(&kg->mutex);

    if (find_entity(kg, entity_id)) {
        pthread_mutex_unlock(&kg->mutex);
        return AGENTOS_SUCCESS;
    }

    if (kg->entity_count >= kg->capacity) {
        size_t new_cap = kg->capacity * 2;
        entity_node_t** new_entities = (entity_node_t**)AGENTOS_REALLOC(kg->entities,
            new_cap * sizeof(entity_node_t*));
        if (!new_entities) {
            pthread_mutex_unlock(&kg->mutex);
            return AGENTOS_ENOMEM;
        }
        kg->entities = new_entities;
        kg->capacity = new_cap;
    }

    entity_node_t* node = (entity_node_t*)AGENTOS_CALLOC(1, sizeof(entity_node_t));
    if (!node) {
        pthread_mutex_unlock(&kg->mutex);
        return AGENTOS_ENOMEM;
    }

    node->id = AGENTOS_STRDUP(entity_id);
    node->relations = NULL;
    node->relation_count = 0;
    node->next = NULL;

    kg->entities[kg->entity_count++] = node;

    pthread_mutex_unlock(&kg->mutex);
    return AGENTOS_SUCCESS;
}

/**
 * @brief 添加关系
 */
agentos_error_t agentos_knowledge_graph_add_relation(
    agentos_knowledge_graph_t* kg,
    const char* from_id,
    const char* to_id,
    agentos_relation_type_t type,
    float weight) {
    if (!kg || !from_id || !to_id) return AGENTOS_EINVAL;

    pthread_mutex_lock(&kg->mutex);

    agentos_error_t err = AGENTOS_SUCCESS;

    if (!find_entity(kg, from_id)) {
        err = agentos_knowledge_graph_add_entity(kg, from_id);
        if (err != AGENTOS_SUCCESS) {
            pthread_mutex_unlock(&kg->mutex);
            return err;
        }
    }

    if (!find_entity(kg, to_id)) {
        err = agentos_knowledge_graph_add_entity(kg, to_id);
        if (err != AGENTOS_SUCCESS) {
            pthread_mutex_unlock(&kg->mutex);
            return err;
        }
    }

    entity_node_t* from_node = find_entity(kg, from_id);
    if (!from_node) {
        pthread_mutex_unlock(&kg->mutex);
        return AGENTOS_ENOENT;
    }

    agentos_relation_t* rel = (agentos_relation_t*)AGENTOS_CALLOC(1, sizeof(agentos_relation_t));
    if (!rel) {
        pthread_mutex_unlock(&kg->mutex);
        return AGENTOS_ENOMEM;
    }

    rel->from_id = AGENTOS_STRDUP(from_id);
    rel->to_id = AGENTOS_STRDUP(to_id);
    rel->type = type;
    rel->weight = weight;
    rel->next = from_node->relations;
    from_node->relations = rel;
    from_node->relation_count++;

    pthread_mutex_unlock(&kg->mutex);
    return AGENTOS_SUCCESS;
}

/**
 * @brief 查询关系
 */
agentos_error_t agentos_knowledge_graph_query(
    agentos_knowledge_graph_t* kg,
    const char* entity_id,
    agentos_relation_type_t relation_type,
    char*** out_related_ids,
    size_t* out_count) {
    if (!kg || !entity_id || !out_related_ids || !out_count) return AGENTOS_EINVAL;

    pthread_mutex_lock(&kg->mutex);

    entity_node_t* node = find_entity(kg, entity_id);
    if (!node) {
        pthread_mutex_unlock(&kg->mutex);
        *out_related_ids = NULL;
        *out_count = 0;
        return AGENTOS_SUCCESS;
    }

    size_t max_related = node->relation_count;
    if (max_related == 0) {
        pthread_mutex_unlock(&kg->mutex);
        *out_related_ids = NULL;
        *out_count = 0;
        return AGENTOS_SUCCESS;
    }

    char** related_ids = (char**)AGENTOS_CALLOC(max_related, sizeof(char*));
    if (!related_ids) {
        pthread_mutex_unlock(&kg->mutex);
        return AGENTOS_ENOMEM;
    }

    size_t count = 0;
    agentos_relation_t* rel = node->relations;
    while (rel) {
        if (relation_type == 0 || rel->type == relation_type) {
            related_ids[count] = AGENTOS_STRDUP(rel->to_id);
            if (related_ids[count]) count++;
        }
        rel = rel->next;
    }

    pthread_mutex_unlock(&kg->mutex);

    *out_related_ids = related_ids;
    *out_count = count;

    return AGENTOS_SUCCESS;
}

/**
 * @brief BFS 查找最短路�?
 */
agentos_error_t agentos_knowledge_graph_find_path(
    agentos_knowledge_graph_t* kg,
    const char* from_id,
    const char* to_id,
    char*** out_path,
    size_t* out_path_length) {
    if (!kg || !from_id || !to_id || !out_path || !out_path_length) return AGENTOS_EINVAL;

    if (strcmp(from_id, to_id) == 0) {
        *out_path = (char**)AGENTOS_CALLOC(1, sizeof(char*));
        if (!*out_path) return AGENTOS_ENOMEM;
        (*out_path)[0] = AGENTOS_STRDUP(from_id);
        *out_path_length = 1;
        return AGENTOS_SUCCESS;
    }

    pthread_mutex_lock(&kg->mutex);



    path_node_t* visited = (path_node_t*)AGENTOS_CALLOC(kg->entity_count, sizeof(path_node_t));
    int* in_queue = (int*)AGENTOS_CALLOC(kg->entity_count, sizeof(int));
    char** queue = (char**)AGENTOS_CALLOC(kg->entity_count, sizeof(char*));
    size_t queue_front = 0, queue_back = 0;

    if (!visited || !in_queue || !queue) {
        pthread_mutex_unlock(&kg->mutex);
        return AGENTOS_ENOMEM;
    }

    for (size_t i = 0; i < kg->entity_count; i++) {
        in_queue[i] = 0;
        visited[i].id = NULL;
        visited[i].prev = NULL;
    }

    size_t start_idx = find_entity_index(kg, from_id);
    size_t end_idx = find_entity_index(kg, to_id);

    if (start_idx == SIZE_MAX || end_idx == SIZE_MAX) {
        pthread_mutex_unlock(&kg->mutex);
        AGENTOS_FREE(visited);
        AGENTOS_FREE(in_queue);
        AGENTOS_FREE(queue);
        return AGENTOS_ENOENT;
    }

    queue[queue_back++] = AGENTOS_STRDUP(from_id);
    in_queue[start_idx] = 1;
    visited[start_idx].id = AGENTOS_STRDUP(from_id);

    int found = 0;
    while (queue_front < queue_back && !found) {
        char* current = queue[queue_front++];
        size_t current_idx = find_entity_index(kg, current);

        if (current_idx == SIZE_MAX) {
            AGENTOS_FREE(current);
            continue;
        }

        entity_node_t* node = kg->entities[current_idx];
        agentos_relation_t* rel = node->relations;
        while (rel && !found) {
            size_t neighbor_idx = find_entity_index(kg, rel->to_id);
            if (neighbor_idx != SIZE_MAX && !in_queue[neighbor_idx]) {
                queue[queue_back++] = AGENTOS_STRDUP(rel->to_id);
                in_queue[neighbor_idx] = 1;
                visited[neighbor_idx].id = AGENTOS_STRDUP(rel->to_id);
                visited[neighbor_idx].prev = AGENTOS_STRDUP(current);
                if (strcmp(rel->to_id, to_id) == 0) {
                    found = 1;
                }
            }
            rel = rel->next;
        }
        AGENTOS_FREE(current);
    }

    char** path = NULL;
    size_t path_len = 0;

    if (found) {
        char** temp_path = (char**)AGENTOS_CALLOC(MAX_PATH_LENGTH, sizeof(char*));
        char* current = AGENTOS_STRDUP(to_id);
        size_t idx = 0;

        while (current && strcmp(current, from_id) != 0) {
            size_t current_idx = find_entity_index(kg, current);
            if (current_idx != SIZE_MAX && visited[current_idx].id && strcmp(visited[current_idx].id, current) == 0) {
                temp_path[idx++] = AGENTOS_STRDUP(current);
                if (visited[current_idx].prev) {
                    AGENTOS_FREE(current);
                    current = AGENTOS_STRDUP(visited[current_idx].prev);
                } else {
                    AGENTOS_FREE(current);
                    current = NULL;
                }
            }
            if (idx >= MAX_PATH_LENGTH) break;
        }

        if (current && strcmp(current, from_id) == 0) {
            temp_path[idx++] = AGENTOS_STRDUP(from_id);
        }

        path = (char**)AGENTOS_CALLOC(idx, sizeof(char*));
        for (size_t i = 0; i < idx; i++) {
            path[i] = temp_path[idx - 1 - i];
        }
        path_len = idx;
        AGENTOS_FREE(temp_path);
        AGENTOS_FREE(current);
    }

    for (size_t i = 0; i < kg->entity_count; i++) {
        if (visited[i].id) AGENTOS_FREE(visited[i].id);
        if (visited[i].prev) AGENTOS_FREE(visited[i].prev);
    }
    for (size_t i = 0; i < queue_back; i++) {
        if (queue[i]) AGENTOS_FREE(queue[i]);
    }
    AGENTOS_FREE(visited);
    AGENTOS_FREE(in_queue);
    AGENTOS_FREE(queue);

    pthread_mutex_unlock(&kg->mutex);

    *out_path = path;
    *out_path_length = path_len;

    return path ? AGENTOS_SUCCESS : AGENTOS_ENOENT;
}

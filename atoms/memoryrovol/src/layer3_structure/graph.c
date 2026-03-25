/**
 * @file graph.c
 * @brief L3 知识图谱实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "../include/layer3_structure.h"
#include <stdlib.h>
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
        calloc(1, sizeof(agentos_knowledge_graph_t));
    if (!kg) return AGENTOS_ENOMEM;

    kg->entities = (entity_node_t**)calloc(INITIAL_CAPACITY, sizeof(entity_node_t*));
    if (!kg->entities) {
        free(kg);
        return AGENTOS_ENOMEM;
    }

    kg->capacity = INITIAL_CAPACITY;
    kg->entity_count = 0;
    kg->mutex = agentos_mutex_create();
    if (!kg->mutex) {
        free(kg->entities);
        free(kg);
        return AGENTOS_ENOMEM;
    }

    *out = kg;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 销毁知识图谱
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
                if (rel->from_id) free(rel->from_id);
                if (rel->to_id) free(rel->to_id);
                free(rel);
                rel = next;
            }
            if (node->id) free(node->id);
            free(node);
        }
    }
    free(kg->entities);

    agentos_mutex_unlock(kg->mutex);
    agentos_mutex_destroy(kg->mutex);
    free(kg);
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
        entity_node_t** new_entities = (entity_node_t**)realloc(kg->entities,
            new_cap * sizeof(entity_node_t*));
        if (!new_entities) {
            pthread_mutex_unlock(&kg->mutex);
            return AGENTOS_ENOMEM;
        }
        kg->entities = new_entities;
        kg->capacity = new_cap;
    }

    entity_node_t* node = (entity_node_t*)calloc(1, sizeof(entity_node_t));
    if (!node) {
        pthread_mutex_unlock(&kg->mutex);
        return AGENTOS_ENOMEM;
    }

    node->id = strdup(entity_id);
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

    agentos_relation_t* rel = (agentos_relation_t*)calloc(1, sizeof(agentos_relation_t));
    if (!rel) {
        pthread_mutex_unlock(&kg->mutex);
        return AGENTOS_ENOMEM;
    }

    rel->from_id = strdup(from_id);
    rel->to_id = strdup(to_id);
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

    char** related_ids = (char**)calloc(max_related, sizeof(char*));
    if (!related_ids) {
        pthread_mutex_unlock(&kg->mutex);
        return AGENTOS_ENOMEM;
    }

    size_t count = 0;
    agentos_relation_t* rel = node->relations;
    while (rel) {
        if (relation_type == 0 || rel->type == relation_type) {
            related_ids[count] = strdup(rel->to_id);
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
 * @brief BFS 查找最短路径
 */
agentos_error_t agentos_knowledge_graph_find_path(
    agentos_knowledge_graph_t* kg,
    const char* from_id,
    const char* to_id,
    char*** out_path,
    size_t* out_path_length) {
    if (!kg || !from_id || !to_id || !out_path || !out_path_length) return AGENTOS_EINVAL;

    if (strcmp(from_id, to_id) == 0) {
        *out_path = (char**)calloc(1, sizeof(char*));
        if (!*out_path) return AGENTOS_ENOMEM;
        (*out_path)[0] = strdup(from_id);
        *out_path_length = 1;
        return AGENTOS_SUCCESS;
    }

    pthread_mutex_lock(&kg->mutex);

    typedef struct {
        char* id;
        char* prev;
    } path_node_t;

    path_node_t* visited = (path_node_t*)calloc(kg->entity_count, sizeof(path_node_t));
    int* in_queue = (int*)calloc(kg->entity_count, sizeof(int));
    char** queue = (char**)calloc(kg->entity_count, sizeof(char*));
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

    size_t start_idx = SIZE_MAX, end_idx = SIZE_MAX;
    for (size_t i = 0; i < kg->entity_count; i++) {
        if (kg->entities[i] && strcmp(kg->entities[i]->id, from_id) == 0) {
            start_idx = i;
        }
        if (kg->entities[i] && strcmp(kg->entities[i]->id, to_id) == 0) {
            end_idx = i;
        }
    }

    if (start_idx == SIZE_MAX || end_idx == SIZE_MAX) {
        pthread_mutex_unlock(&kg->mutex);
        free(visited);
        free(in_queue);
        free(queue);
        return AGENTOS_ENOENT;
    }

    queue[queue_back++] = strdup(from_id);
    in_queue[start_idx] = 1;
    visited[start_idx].id = strdup(from_id);

    int found = 0;
    while (queue_front < queue_back && !found) {
        char* current = queue[queue_front++];
        size_t current_idx = SIZE_MAX;
        for (size_t i = 0; i < kg->entity_count; i++) {
            if (kg->entities[i] && strcmp(kg->entities[i]->id, current) == 0) {
                current_idx = i;
                break;
            }
        }

        if (current_idx == SIZE_MAX) {
            free(current);
            continue;
        }

        entity_node_t* node = kg->entities[current_idx];
        agentos_relation_t* rel = node->relations;
        while (rel && !found) {
            for (size_t i = 0; i < kg->entity_count; i++) {
                if (kg->entities[i] && strcmp(kg->entities[i]->id, rel->to_id) == 0) {
                    if (!in_queue[i]) {
                        queue[queue_back++] = strdup(rel->to_id);
                        in_queue[i] = 1;
                        visited[i].id = strdup(rel->to_id);
                        visited[i].prev = strdup(current);
                        if (strcmp(rel->to_id, to_id) == 0) {
                            found = 1;
                        }
                    }
                    break;
                }
            }
            rel = rel->next;
        }
        free(current);
    }

    char** path = NULL;
    size_t path_len = 0;

    if (found) {
        char** temp_path = (char**)calloc(MAX_PATH_LENGTH, sizeof(char*));
        char* current = strdup(to_id);
        size_t idx = 0;

        while (current && strcmp(current, from_id) != 0) {
            for (size_t i = 0; i < kg->entity_count; i++) {
                if (visited[i].id && strcmp(visited[i].id, current) == 0) {
                    temp_path[idx++] = strdup(current);
                    if (visited[i].prev) {
                        free(current);
                        current = strdup(visited[i].prev);
                    } else {
                        free(current);
                        current = NULL;
                    }
                    break;
                }
            }
            if (idx >= MAX_PATH_LENGTH) break;
        }

        if (current && strcmp(current, from_id) == 0) {
            temp_path[idx++] = strdup(from_id);
        }

        path = (char**)calloc(idx, sizeof(char*));
        for (size_t i = 0; i < idx; i++) {
            path[i] = temp_path[idx - 1 - i];
        }
        path_len = idx;
        free(temp_path);
        free(current);
    }

    for (size_t i = 0; i < kg->entity_count; i++) {
        if (visited[i].id) free(visited[i].id);
        if (visited[i].prev) free(visited[i].prev);
    }
    for (size_t i = 0; i < queue_back; i++) {
        if (queue[i]) free(queue[i]);
    }
    free(visited);
    free(in_queue);
    free(queue);

    pthread_mutex_unlock(&kg->mutex);

    *out_path = path;
    *out_path_length = path_len;

    return path ? AGENTOS_SUCCESS : AGENTOS_ENOENT;
}

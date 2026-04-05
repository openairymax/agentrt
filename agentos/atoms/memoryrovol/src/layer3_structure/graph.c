/**
 * @file graph.c
 * @brief L3 知识图谱实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "../include/layer3_structure.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../agentos/commons/utils/memory/include/memory_compat.h"
#include "../../../agentos/commons/utils/string/include/string_compat.h"
#include <string.h>
#include <pthread.h>
#include <limits.h>

#define INITIAL_CAPACITY 1024
#define MAX_PATH_LENGTH 100
#define GROWTH_FACTOR 2

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
 * @brief BFS 搜索核心逻辑
 */
static agentos_error_t perform_bfs_search(agentos_knowledge_graph_t* kg,
                                          size_t start_idx,
                                          size_t end_idx,
                                          path_node_t* visited,
                                          int* in_queue,
                                          char** queue);

/**
 * @brief 从BFS结果重构路径
 */
static char** reconstruct_path_from_bfs(agentos_knowledge_graph_t* kg,
                                        size_t start_idx,
                                        size_t end_idx,
                                        path_node_t* visited,
                                        size_t* path_len);

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
        size_t new_cap = kg->capacity * GROWTH_FACTOR;
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
        *out_path = (char**)AGENTOS_CALLOC(1, sizeof(char*));
        if (!*out_path) return AGENTOS_ENOMEM;
        (*out_path)[0] = AGENTOS_STRDUP(from_id);
        *out_path_length = 1;
        return AGENTOS_SUCCESS;
    }

    pthread_mutex_lock(&kg->mutex);

    size_t start_idx = find_entity_index(kg, from_id);
    size_t end_idx = find_entity_index(kg, to_id);

    if (start_idx == SIZE_MAX || end_idx == SIZE_MAX) {
        pthread_mutex_unlock(&kg->mutex);
        return AGENTOS_ENOENT;
    }

    path_node_t* visited = (path_node_t*)AGENTOS_CALLOC(kg->entity_count, sizeof(path_node_t));
    int* in_queue = (int*)AGENTOS_CALLOC(kg->entity_count, sizeof(int));
    char** queue = (char**)AGENTOS_CALLOC(kg->entity_count, sizeof(char*));

    if (!visited || !in_queue || !queue) {
        pthread_mutex_unlock(&kg->mutex);
        AGENTOS_FREE(visited);
        AGENTOS_FREE(in_queue);
        AGENTOS_FREE(queue);
        return AGENTOS_ENOMEM;
    }

    for (size_t i = 0; i < kg->entity_count; i++) {
        in_queue[i] = 0;
        visited[i].id = NULL;
        visited[i].prev = NULL;
    }

    agentos_error_t err = perform_bfs_search(kg, start_idx, end_idx, visited, in_queue, queue);

    if (err == AGENTOS_SUCCESS) {
        *out_path = reconstruct_path_from_bfs(kg, start_idx, end_idx, visited, out_path_length);
        if (!*out_path) {
            err = AGENTOS_ENOMEM;
        }
    } else {
        *out_path = NULL;
        *out_path_length = 0;
    }

    for (size_t i = 0; i < kg->entity_count; i++) {
        if (visited[i].id) AGENTOS_FREE(visited[i].id);
        if (visited[i].prev) AGENTOS_FREE(visited[i].prev);
    }
    AGENTOS_FREE(visited);
    AGENTOS_FREE(in_queue);
    AGENTOS_FREE(queue);

    pthread_mutex_unlock(&kg->mutex);

    return err;
}

/**
 * @brief 初始化BFS搜索状态
 */
static agentos_error_t initialize_bfs_state(agentos_knowledge_graph_t* kg,
                                           size_t start_idx,
                                           char** queue,
                                           int* in_queue,
                                           path_node_t* visited) {
    char* start_id = AGENTOS_STRDUP(kg->entities[start_idx]->id);
    if (!start_id) {
        return AGENTOS_ENOMEM;
    }

    queue[0] = start_id;
    in_queue[start_idx] = 1;

    visited[start_idx].id = AGENTOS_STRDUP(kg->entities[start_idx]->id);
    if (!visited[start_idx].id) {
        AGENTOS_FREE(start_id);
        return AGENTOS_ENOMEM;
    }

    return AGENTOS_SUCCESS;
}

/**
 * @brief 探索当前节点的邻居
 */
static int explore_neighbors(agentos_knowledge_graph_t* kg,
                            size_t current_idx,
                            size_t end_idx,
                            int* in_queue,
                            path_node_t* visited,
                            char** queue,
                            size_t* queue_back,
                            const char* current) {
    entity_node_t* node = kg->entities[current_idx];
    agentos_relation_t* rel = node->relations;
    int found = 0;

    while (rel && !found) {
        size_t neighbor_idx = find_entity_index(kg, rel->to_id);
        if (neighbor_idx != SIZE_MAX && !in_queue[neighbor_idx]) {
            char* neighbor_id = AGENTOS_STRDUP(rel->to_id);
            if (!neighbor_id) {
                rel = rel->next;
                continue;
            }

            queue[(*queue_back)++] = neighbor_id;
            in_queue[neighbor_idx] = 1;

            visited[neighbor_idx].id = AGENTOS_STRDUP(rel->to_id);
            visited[neighbor_idx].prev = AGENTOS_STRDUP(current);

            if (neighbor_idx == end_idx) {
                found = 1;
            }
        }
        rel = rel->next;
    }

    return found;
}

/**
 * @brief 处理BFS队列中的当前节点
 */
static int process_current_node(agentos_knowledge_graph_t* kg,
                               size_t end_idx,
                               int* in_queue,
                               path_node_t* visited,
                               char** queue,
                               size_t* queue_front,
                               size_t* queue_back,
                               char* current) {
    size_t current_idx = find_entity_index(kg, current);

    if (current_idx == SIZE_MAX) {
        AGENTOS_FREE(current);
        return 0;
    }

    int found = explore_neighbors(kg, current_idx, end_idx, in_queue, visited,
                                 queue, queue_back, current);

    AGENTOS_FREE(current);
    return found;
}

/**
 * @brief BFS 搜索核心逻辑实现
 */
static agentos_error_t perform_bfs_search(agentos_knowledge_graph_t* kg,
                                          size_t start_idx,
                                          size_t end_idx,
                                          path_node_t* visited,
                                          int* in_queue,
                                          char** queue) {
    size_t queue_front = 0, queue_back = 0;
    int found = 0;

    agentos_error_t init_err = initialize_bfs_state(kg, start_idx, queue, in_queue, visited);
    if (init_err != AGENTOS_SUCCESS) {
        return init_err;
    }

    queue_back = 1;

    while (queue_front < queue_back && !found) {
        char* current = queue[queue_front++];
        found = process_current_node(kg, end_idx, in_queue, visited,
                                    queue, &queue_back, current);
    }

    if (found) {
        return AGENTOS_SUCCESS;
    }
    return AGENTOS_ENOENT;
}

/**
 * @brief 从终点回溯提取路径到临时数组
 */
static size_t extract_path_to_temp(agentos_knowledge_graph_t* kg,
                                  size_t start_idx,
                                  size_t end_idx,
                                  path_node_t* visited,
                                  char** temp_path) {
    char* current = AGENTOS_STRDUP(kg->entities[end_idx]->id);
    size_t idx = 0;

    if (!current) {
        return 0;
    }

    while (current && strcmp(current, kg->entities[start_idx]->id) != 0) {
        if (idx >= MAX_PATH_LENGTH) {
            AGENTOS_FREE(current);
            goto cleanup;
        }

        size_t current_idx = find_entity_index(kg, current);
        if (current_idx == SIZE_MAX || !visited[current_idx].id ||
            strcmp(visited[current_idx].id, current) != 0) {
            AGENTOS_FREE(current);
            goto cleanup;
        }

        char* node_copy = AGENTOS_STRDUP(current);
        if (!node_copy) {
            AGENTOS_FREE(current);
            goto cleanup;
        }
        temp_path[idx++] = node_copy;

        char* prev = visited[current_idx].prev;
        AGENTOS_FREE(current);

        if (prev) {
            current = AGENTOS_STRDUP(prev);
            if (!current) {
                goto cleanup;
            }
        } else {
            current = NULL;
        }
    }

    if (current && strcmp(current, kg->entities[start_idx]->id) == 0) {
        if (idx < MAX_PATH_LENGTH) {
            char* start_copy = AGENTOS_STRDUP(kg->entities[start_idx]->id);
            if (!start_copy) {
                AGENTOS_FREE(current);
                goto cleanup;
            }
            temp_path[idx++] = start_copy;
        }
        AGENTOS_FREE(current);
    } else if (current) {
        AGENTOS_FREE(current);
    }

    return idx;

cleanup:
    /* 清理已分配的字符串 */
    for (size_t i = 0; i < idx; i++) {
        if (temp_path[i]) {
            AGENTOS_FREE(temp_path[i]);
            temp_path[i] = NULL;
        }
    }
    return 0;
}

/**
 * @brief 构建反转后的最终路径
 */
static char** build_reversed_path(char** temp_path, size_t idx, size_t* path_len) {
    if (idx == 0) {
        *path_len = 0;
        return NULL;
    }

    char** path = (char**)AGENTOS_CALLOC(idx, sizeof(char*));
    if (!path) {
        *path_len = 0;
        return NULL;
    }

    for (size_t i = 0; i < idx; i++) {
        path[i] = temp_path[idx - 1 - i];
    }

    *path_len = idx;
    return path;
}



/**
 * @brief 从BFS结果重构路径实现
 */
static char** reconstruct_path_from_bfs(agentos_knowledge_graph_t* kg,
                                        size_t start_idx,
                                        size_t end_idx,
                                        path_node_t* visited,
                                        size_t* path_len) {
    char** path = NULL;
    *path_len = 0;

    char** temp_path = (char**)AGENTOS_CALLOC(MAX_PATH_LENGTH, sizeof(char*));
    if (!temp_path) {
        return NULL;
    }

    size_t idx = extract_path_to_temp(kg, start_idx, end_idx, visited, temp_path);
    if (idx == 0) {
        AGENTOS_FREE(temp_path);
        return NULL;
    }

    path = build_reversed_path(temp_path, idx, path_len);

    /* 释放temp_path数组，但不释放其中的字符串（已转移到path） */
    AGENTOS_FREE(temp_path);
    return path;
}

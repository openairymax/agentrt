// SPDX-FileCopyrightText: 2026 SPHARX.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file graph_engine.c
 * @brief Graph Engine Implementation
 * 
 * 图引擎实现，提供图的存储和基本操作。
 */

#include "graph_engine.h"
#include "taskflow.h"
#include <stdlib.h>
#include <string.h>

// ============================================================================
// 内部数据结构
// ============================================================================

// 顶点索引条目
typedef struct vertex_index_entry_s {
    vertex_id_t vertex_id;
    size_t vertex_idx;  // 在vertices数组中的索引
    struct vertex_index_entry_s* next;
} vertex_index_entry_t;

// 边索引条目
typedef struct edge_index_entry_s {
    edge_id_t edge_id;
    size_t edge_idx;    // 在edges数组中的索引
    struct edge_index_entry_s* next;
} edge_index_entry_t;

// 出边索引条目（源顶点 -> 边列表）
typedef struct out_edge_index_entry_s {
    vertex_id_t source_vertex;
    size_t edge_idx;
    struct out_edge_index_entry_s* next;
} out_edge_index_entry_t;

// 入边索引条目（目标顶点 -> 边列表）
typedef struct in_edge_index_entry_s {
    vertex_id_t target_vertex;
    size_t edge_idx;
    struct in_edge_index_entry_s* next;
} in_edge_index_entry_t;

struct graph_engine_s {
    taskflow_config_t config;
    
    // 顶点存储
    graph_vertex_t* vertices;
    size_t vertex_count;
    size_t vertex_capacity;
    
    // 边存储
    graph_edge_t* edges;
    size_t edge_count;
    size_t edge_capacity;
    
    // 索引结构
    vertex_index_entry_t** vertex_index;        // 哈希表：vertex_id -> 顶点索引
    edge_index_entry_t** edge_index;            // 哈希表：edge_id -> 边索引
    out_edge_index_entry_t** out_edge_index;    // 哈希表：source_vertex -> 出边列表
    in_edge_index_entry_t** in_edge_index;      // 哈希表：target_vertex -> 入边列表
    
    size_t index_size;                          // 哈希表大小
    
    bool initialized;
};

// ============================================================================
// 静态辅助函数
// ============================================================================

// 简单哈希函数（FNV-1a变体）
static size_t vertex_id_hash(vertex_id_t id, size_t table_size) {
    // 使用FNV-1a哈希算法
    const uint64_t FNV_offset_basis = 14695981039346656037ULL;
    const uint64_t FNV_prime = 1099511628211ULL;
    
    uint64_t hash = FNV_offset_basis;
    uint8_t* bytes = (uint8_t*)&id;
    
    for (size_t i = 0; i < sizeof(vertex_id_t); i++) {
        hash ^= bytes[i];
        hash *= FNV_prime;
    }
    
    return hash % table_size;
}

// 计算合适的哈希表大小（质数）
static size_t calculate_hash_table_size(size_t capacity) {
    // 质数表，用于减少哈希冲突
    static const size_t primes[] = {
        53, 97, 193, 389, 769, 1543, 3079, 6151,
        12289, 24593, 49157, 98317, 196613, 393241,
        786433, 1572869, 3145739, 6291469, 12582917,
        25165843, 50331653, 100663319, 201326611,
        402653189, 805306457, 1610612741
    };
    
    for (size_t i = 0; i < sizeof(primes) / sizeof(primes[0]); i++) {
        if (primes[i] > capacity * 2) {
            return primes[i];
        }
    }
    
    return capacity * 2 + 1; // 回退方案
}

// 创建哈希表
static void** create_hash_table(size_t size) {
    void** table = (void**)calloc(size, sizeof(void*));
    return table;
}

// 销毁哈希表（通用）
static void destroy_hash_table(void** table, size_t size, void (*free_entry)(void*)) {
    if (!table) return;
    
    for (size_t i = 0; i < size; i++) {
        void* entry = table[i];
        while (entry) {
            void* next = *(void**)entry; // 假设第一个字段是next指针
            free_entry(entry);
            entry = next;
        }
    }
    
    free(table);
}

// 顶点索引条目释放函数
static void free_vertex_index_entry(void* entry) {
    free(entry);
}

// 边索引条目释放函数
static void free_edge_index_entry(void* entry) {
    free(entry);
}

// 出边索引条目释放函数
static void free_out_edge_index_entry(void* entry) {
    free(entry);
}

// 入边索引条目释放函数
static void free_in_edge_index_entry(void* entry) {
    free(entry);
}

// 查找顶点索引
static size_t find_vertex_index(struct graph_engine_s* e, vertex_id_t vertex_id) {
    if (!e->vertex_index || vertex_id == 0) return SIZE_MAX;
    
    size_t hash = vertex_id_hash(vertex_id, e->index_size);
    vertex_index_entry_t* entry = e->vertex_index[hash];
    
    while (entry) {
        if (entry->vertex_id == vertex_id) {
            return entry->vertex_idx;
        }
        entry = entry->next;
    }
    
    return SIZE_MAX;
}

// 查找边索引
static size_t find_edge_index(struct graph_engine_s* e, edge_id_t edge_id) {
    if (!e->edge_index || edge_id == 0) return SIZE_MAX;
    
    size_t hash = vertex_id_hash(edge_id, e->index_size);
    edge_index_entry_t* entry = e->edge_index[hash];
    
    while (entry) {
        if (entry->edge_id == edge_id) {
            return entry->edge_idx;
        }
        entry = entry->next;
    }
    
    return SIZE_MAX;
}

// 添加顶点索引
static bool add_vertex_index(struct graph_engine_s* e, vertex_id_t vertex_id, size_t vertex_idx) {
    size_t hash = vertex_id_hash(vertex_id, e->index_size);
    
    vertex_index_entry_t* new_entry = (vertex_index_entry_t*)calloc(1, sizeof(vertex_index_entry_t));
    if (!new_entry) return false;
    
    new_entry->vertex_id = vertex_id;
    new_entry->vertex_idx = vertex_idx;
    new_entry->next = e->vertex_index[hash];
    e->vertex_index[hash] = new_entry;
    
    return true;
}

// 添加边索引
static bool add_edge_index(struct graph_engine_s* e, edge_id_t edge_id, size_t edge_idx) {
    size_t hash = vertex_id_hash(edge_id, e->index_size);
    
    edge_index_entry_t* new_entry = (edge_index_entry_t*)calloc(1, sizeof(edge_index_entry_t));
    if (!new_entry) return false;
    
    new_entry->edge_id = edge_id;
    new_entry->edge_idx = edge_idx;
    new_entry->next = e->edge_index[hash];
    e->edge_index[hash] = new_entry;
    
    return true;
}

// 添加出边索引
static bool add_out_edge_index(struct graph_engine_s* e, vertex_id_t source, size_t edge_idx) {
    size_t hash = vertex_id_hash(source, e->index_size);
    
    out_edge_index_entry_t* new_entry = (out_edge_index_entry_t*)calloc(1, sizeof(out_edge_index_entry_t));
    if (!new_entry) return false;
    
    new_entry->source_vertex = source;
    new_entry->edge_idx = edge_idx;
    new_entry->next = e->out_edge_index[hash];
    e->out_edge_index[hash] = new_entry;
    
    return true;
}

// 添加入边索引
static bool add_in_edge_index(struct graph_engine_s* e, vertex_id_t target, size_t edge_idx) {
    size_t hash = vertex_id_hash(target, e->index_size);
    
    in_edge_index_entry_t* new_entry = (in_edge_index_entry_t*)calloc(1, sizeof(in_edge_index_entry_t));
    if (!new_entry) return false;
    
    new_entry->target_vertex = target;
    new_entry->edge_idx = edge_idx;
    new_entry->next = e->in_edge_index[hash];
    e->in_edge_index[hash] = new_entry;
    
    return true;
}

// 更新顶点度数
static void update_vertex_degree(struct graph_engine_s* e, vertex_id_t vertex_id, bool is_out_edge, int delta) {
    size_t idx = find_vertex_index(e, vertex_id);
    if (idx == SIZE_MAX) return;

    if (is_out_edge) {
        if (delta > 0) e->vertices[idx].out_degree++;
        else if (e->vertices[idx].out_degree > 0) e->vertices[idx].out_degree--;
    } else {
        if (delta > 0) e->vertices[idx].in_degree++;
        else if (e->vertices[idx].in_degree > 0) e->vertices[idx].in_degree--;
    }
}

// 从哈希链中移除指定ID的索引条目（通用模式）
static void remove_index_entry(void** table, size_t hash, void* target_id,
                                size_t id_offset, size_t next_offset,
                                void (*free_fn)(void*)) {
    void** prev_ptr = &table[hash];
    void* entry = table[hash];
    while (entry) {
        void* next = *(void**)((uint8_t*)entry + next_offset);
        if (memcmp((uint8_t*)entry + id_offset, target_id, sizeof(vertex_id_t)) == 0) {
            *prev_ptr = next;
            free_fn(entry);
            return;
        }
        prev_ptr = (void**)((uint8_t*)entry + next_offset);
        entry = next;
    }
}

// 移除边索引条目并更新所有引用该边的索引
static void remove_edge_from_all_indexes(struct graph_engine_s* e, edge_id_t edge_id,
                                          vertex_id_t source, vertex_id_t target,
                                          size_t removed_idx) {
    size_t hash;

    // 从edge_index移除
    hash = vertex_id_hash(edge_id, e->index_size);
    edge_index_entry_t** ep = (edge_index_entry_t**)&e->edge_index[hash];
    edge_index_entry_t* ee = e->edge_index[hash];
    while (ee) {
        edge_index_entry_t* next = ee->next;
        if (ee->edge_id == edge_id) {
            *ep = next;
            free(ee);
            break;
        }
        ep = &ee->next;
        ee = next;
    }

    // 从out_edge_index移除
    hash = vertex_id_hash(source, e->index_size);
    out_edge_index_entry_t** op = (out_edge_index_entry_t**)&e->out_edge_index[hash];
    out_edge_index_entry_t* oe = e->out_edge_index[hash];
    while (oe) {
        out_edge_index_entry_t* next = oe->next;
        if (oe->edge_idx == removed_idx) {
            *op = next;
            free(oe);
            break;
        }
        op = &oe->next;
        oe = next;
    }

    // 从in_edge_index移除
    hash = vertex_id_hash(target, e->index_size);
    in_edge_index_entry_t** ip = (in_edge_index_entry_t**)&e->in_edge_index[hash];
    in_edge_index_entry_t* ie = e->in_edge_index[hash];
    while (ie) {
        in_edge_index_entry_t* next = ie->next;
        if (ie->edge_idx == removed_idx) {
            *ip = next;
            free(ie);
            break;
        }
        ip = &ie->next;
        ie = next;
    }
}

// 移除顶点索引条目
static void remove_vertex_index_entry(struct graph_engine_s* e, vertex_id_t vertex_id) {
    size_t hash = vertex_id_hash(vertex_id, e->index_size);
    vertex_index_entry_t** pp = (vertex_index_entry_t**)&e->vertex_index[hash];
    vertex_index_entry_t* p = e->vertex_index[hash];
    while (p) {
        vertex_index_entry_t* next = p->next;
        if (p->vertex_id == vertex_id) {
            *pp = next;
            free(p);
            return;
        }
        pp = &p->next;
        p = next;
    }
}

// 收集出边
static size_t collect_out_edges(struct graph_engine_s* e, vertex_id_t vertex_id, 
                               graph_edge_t* edges, size_t max_edges) {
    if (!e->out_edge_index || vertex_id == 0) return 0;
    
    size_t hash = vertex_id_hash(vertex_id, e->index_size);
    out_edge_index_entry_t* entry = e->out_edge_index[hash];
    size_t count = 0;
    
    while (entry && count < max_edges) {
        if (entry->source_vertex == vertex_id) {
            edges[count] = e->edges[entry->edge_idx];
            count++;
        }
        entry = entry->next;
    }
    
    return count;
}

// 收集入边
static size_t collect_in_edges(struct graph_engine_s* e, vertex_id_t vertex_id,
                              graph_edge_t* edges, size_t max_edges) {
    if (!e->in_edge_index || vertex_id == 0) return 0;
    
    size_t hash = vertex_id_hash(vertex_id, e->index_size);
    in_edge_index_entry_t* entry = e->in_edge_index[hash];
    size_t count = 0;
    
    while (entry && count < max_edges) {
        if (entry->target_vertex == vertex_id) {
            edges[count] = e->edges[entry->edge_idx];
            count++;
        }
        entry = entry->next;
    }
    
    return count;
}

// 收集邻居顶点
static size_t collect_neighbors(struct graph_engine_s* e, vertex_id_t vertex_id,
                               vertex_id_t* neighbors, size_t max_neighbors,
                               bool out_neighbors) {
    if (vertex_id == 0) return 0;
    
    size_t hash = vertex_id_hash(vertex_id, e->index_size);
    size_t count = 0;
    
    if (out_neighbors) {
        out_edge_index_entry_t* entry = e->out_edge_index ? e->out_edge_index[hash] : NULL;
        while (entry && count < max_neighbors) {
            if (entry->source_vertex == vertex_id) {
                neighbors[count] = e->edges[entry->edge_idx].target;
                count++;
            }
            entry = entry->next;
        }
    } else {
        in_edge_index_entry_t* entry = e->in_edge_index ? e->in_edge_index[hash] : NULL;
        while (entry && count < max_neighbors) {
            if (entry->target_vertex == vertex_id) {
                neighbors[count] = e->edges[entry->edge_idx].source;
                count++;
            }
            entry = entry->next;
        }
    }
    
    return count;
}

// 简单队列实现（用于BFS）
typedef struct queue_node_s {
    vertex_id_t vertex_id;
    struct queue_node_s* next;
} queue_node_t;

typedef struct {
    queue_node_t* front;
    queue_node_t* rear;
    size_t size;
} vertex_queue_t;

static vertex_queue_t* queue_create(void) {
    vertex_queue_t* q = (vertex_queue_t*)calloc(1, sizeof(vertex_queue_t));
    return q;
}

static void queue_destroy(vertex_queue_t* q) {
    if (!q) return;
    
    queue_node_t* node = q->front;
    while (node) {
        queue_node_t* next = node->next;
        free(node);
        node = next;
    }
    
    free(q);
}

static void queue_enqueue(vertex_queue_t* q, vertex_id_t vertex_id) {
    if (!q) return;
    
    queue_node_t* node = (queue_node_t*)calloc(1, sizeof(queue_node_t));
    if (!node) return;
    
    node->vertex_id = vertex_id;
    
    if (!q->rear) {
        q->front = q->rear = node;
    } else {
        q->rear->next = node;
        q->rear = node;
    }
    
    q->size++;
}

static vertex_id_t queue_dequeue(vertex_queue_t* q) {
    if (!q || !q->front) return 0;
    
    queue_node_t* node = q->front;
    vertex_id_t vertex_id = node->vertex_id;
    
    q->front = node->next;
    if (!q->front) q->rear = NULL;
    
    free(node);
    q->size--;
    
    return vertex_id;
}

static bool queue_is_empty(const vertex_queue_t* q) {
    return !q || !q->front;
}

// 简单栈实现（用于DFS）
typedef struct stack_node_s {
    vertex_id_t vertex_id;
    struct stack_node_s* next;
} stack_node_t;

typedef struct {
    stack_node_t* top;
    size_t size;
} vertex_stack_t;

static vertex_stack_t* stack_create(void) {
    vertex_stack_t* s = (vertex_stack_t*)calloc(1, sizeof(vertex_stack_t));
    return s;
}

static void stack_destroy(vertex_stack_t* s) {
    if (!s) return;
    
    stack_node_t* node = s->top;
    while (node) {
        stack_node_t* next = node->next;
        free(node);
        node = next;
    }
    
    free(s);
}

static void stack_push(vertex_stack_t* s, vertex_id_t vertex_id) {
    if (!s) return;
    
    stack_node_t* node = (stack_node_t*)calloc(1, sizeof(stack_node_t));
    if (!node) return;
    
    node->vertex_id = vertex_id;
    node->next = s->top;
    s->top = node;
    s->size++;
}

static vertex_id_t stack_pop(vertex_stack_t* s) {
    if (!s || !s->top) return 0;
    
    stack_node_t* node = s->top;
    vertex_id_t vertex_id = node->vertex_id;
    
    s->top = node->next;
    free(node);
    s->size--;
    
    return vertex_id;
}

static bool stack_is_empty(const vertex_stack_t* s) {
    return !s || !s->top;
}

// 简单哈希集合（用于跟踪已访问顶点）
typedef struct hashset_node_s {
    vertex_id_t vertex_id;
    struct hashset_node_s* next;
} hashset_node_t;

typedef struct {
    hashset_node_t** buckets;
    size_t size;
} vertex_hashset_t;

static vertex_hashset_t* hashset_create(size_t size) {
    vertex_hashset_t* set = (vertex_hashset_t*)calloc(1, sizeof(vertex_hashset_t));
    if (!set) return NULL;
    
    set->size = size;
    set->buckets = (hashset_node_t**)calloc(size, sizeof(hashset_node_t*));
    if (!set->buckets) {
        free(set);
        return NULL;
    }
    
    return set;
}

static void hashset_destroy(vertex_hashset_t* set) {
    if (!set) return;
    
    for (size_t i = 0; i < set->size; i++) {
        hashset_node_t* node = set->buckets[i];
        while (node) {
            hashset_node_t* next = node->next;
            free(node);
            node = next;
        }
    }
    
    free(set->buckets);
    free(set);
}

static bool hashset_contains(vertex_hashset_t* set, vertex_id_t vertex_id) {
    if (!set) return false;
    
    size_t hash = vertex_id_hash(vertex_id, set->size);
    hashset_node_t* node = set->buckets[hash];
    
    while (node) {
        if (node->vertex_id == vertex_id) {
            return true;
        }
        node = node->next;
    }
    
    return false;
}

static bool hashset_add(vertex_hashset_t* set, vertex_id_t vertex_id) {
    if (!set) return false;
    
    size_t hash = vertex_id_hash(vertex_id, set->size);
    
    // 检查是否已存在
    hashset_node_t* node = set->buckets[hash];
    while (node) {
        if (node->vertex_id == vertex_id) {
            return true; // 已存在
        }
        node = node->next;
    }
    
    // 添加新节点
    hashset_node_t* new_node = (hashset_node_t*)calloc(1, sizeof(hashset_node_t));
    if (!new_node) return false;
    
    new_node->vertex_id = vertex_id;
    new_node->next = set->buckets[hash];
    set->buckets[hash] = new_node;
    
    return true;
}

// ============================================================================
// 核心API实现
// ============================================================================

graph_engine_handle_t graph_engine_create(const taskflow_config_t* config)
{
    if (!config) {
        return NULL;
    }
    
    struct graph_engine_s* engine = (struct graph_engine_s*)calloc(1, sizeof(struct graph_engine_s));
    if (!engine) {
        return NULL;
    }
    
    // 复制配置
    engine->config = *config;
    
    // 初始化存储
    engine->vertex_capacity = config->max_vertices;
    engine->edge_capacity = config->max_edges;
    
    engine->vertices = (graph_vertex_t*)calloc(engine->vertex_capacity, sizeof(graph_vertex_t));
    if (!engine->vertices) {
        free(engine);
        return NULL;
    }
    
    engine->edges = (graph_edge_t*)calloc(engine->edge_capacity, sizeof(graph_edge_t));
    if (!engine->edges) {
        free(engine->vertices);
        free(engine);
        return NULL;
    }
    
    // 计算哈希表大小
    engine->index_size = calculate_hash_table_size(engine->vertex_capacity + engine->edge_capacity);
    
    // 创建哈希表
    engine->vertex_index = (vertex_index_entry_t**)create_hash_table(engine->index_size);
    engine->edge_index = (edge_index_entry_t**)create_hash_table(engine->index_size);
    engine->out_edge_index = (out_edge_index_entry_t**)create_hash_table(engine->index_size);
    engine->in_edge_index = (in_edge_index_entry_t**)create_hash_table(engine->index_size);
    
    if (!engine->vertex_index || !engine->edge_index || 
        !engine->out_edge_index || !engine->in_edge_index) {
        // 清理已分配的资源
        if (engine->vertex_index) free(engine->vertex_index);
        if (engine->edge_index) free(engine->edge_index);
        if (engine->out_edge_index) free(engine->out_edge_index);
        if (engine->in_edge_index) free(engine->in_edge_index);
        free(engine->vertices);
        free(engine->edges);
        free(engine);
        return NULL;
    }
    
    engine->vertex_count = 0;
    engine->edge_count = 0;
    engine->initialized = false;
    
    return (graph_engine_handle_t)engine;
}

void graph_engine_destroy(graph_engine_handle_t engine)
{
    if (!engine) return;
    
    struct graph_engine_s* e = (struct graph_engine_s*)engine;
    
    // 释放索引结构
    if (e->vertex_index) {
        destroy_hash_table((void**)e->vertex_index, e->index_size, free_vertex_index_entry);
    }
    
    if (e->edge_index) {
        destroy_hash_table((void**)e->edge_index, e->index_size, free_edge_index_entry);
    }
    
    if (e->out_edge_index) {
        destroy_hash_table((void**)e->out_edge_index, e->index_size, free_out_edge_index_entry);
    }
    
    if (e->in_edge_index) {
        destroy_hash_table((void**)e->in_edge_index, e->index_size, free_in_edge_index_entry);
    }
    
    // 释放顶点存储
    if (e->vertices) {
        free(e->vertices);
    }
    
    // 释放边存储
    if (e->edges) {
        free(e->edges);
    }
    
    free(e);
}

taskflow_error_t graph_engine_init(graph_engine_handle_t engine)
{
    if (!engine) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    struct graph_engine_s* e = (struct graph_engine_s*)engine;
    
    if (e->initialized) {
        return TASKFLOW_ERROR_ALREADY_INITIALIZED;
    }
    
    // 索引结构已经在create时创建，这里只是标记为已初始化
    // 如果需要，可以在这里进行额外的初始化工作
    
    e->initialized = true;
    return TASKFLOW_SUCCESS;
}

taskflow_error_t graph_engine_add_vertex(graph_engine_handle_t engine,
                                        const graph_vertex_t* vertex)
{
    if (!engine || !vertex) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    struct graph_engine_s* e = (struct graph_engine_s*)engine;
    
    if (!e->initialized) {
        return TASKFLOW_ERROR_NOT_INITIALIZED;
    }
    
    // 检查容量
    if (e->vertex_count >= e->vertex_capacity) {
        return TASKFLOW_ERROR_GRAPH_TOO_LARGE;
    }
    
    // 检查顶点ID是否已存在
    if (find_vertex_index(e, vertex->id) != SIZE_MAX) {
        return TASKFLOW_ERROR_INVALID_ARG; // 顶点已存在
    }
    
    // 添加顶点
    e->vertices[e->vertex_count] = *vertex;
    
    // 初始化度数
    e->vertices[e->vertex_count].out_degree = 0;
    e->vertices[e->vertex_count].in_degree = 0;
    
    // 添加索引
    if (!add_vertex_index(e, vertex->id, e->vertex_count)) {
        return TASKFLOW_ERROR_MEMORY;
    }
    
    e->vertex_count++;
    
    return TASKFLOW_SUCCESS;
}

taskflow_error_t graph_engine_remove_vertex(graph_engine_handle_t engine,
                                           vertex_id_t vertex_id)
{
    if (!engine || vertex_id == 0) return TASKFLOW_ERROR_INVALID_ARG;

    struct graph_engine_s* e = (struct graph_engine_s*)engine;
    if (!e->initialized) return TASKFLOW_ERROR_NOT_INITIALIZED;

    size_t idx = find_vertex_index(e, vertex_id);
    if (idx == SIZE_MAX) return TASKFLOW_ERROR_INVALID_ARG;

    // 收集并移除所有关联边（先收集ID列表避免遍历时修改）
    graph_edge_t out_edges[256], in_edges[256];
    size_t out_count = collect_out_edges(e, vertex_id, out_edges, 256);
    size_t in_count = collect_in_edges(e, vertex_id, in_edges, 256);

    for (size_t i = 0; i < out_count; i++) {
        graph_engine_remove_edge(engine, out_edges[i].id);
    }
    for (size_t i = 0; i < in_count; i++) {
        // 入边可能已被出边移除时连带删除，跳过已不存在的
        if (find_edge_index(e, in_edges[i].id) != SIZE_MAX) {
            graph_engine_remove_edge(engine, in_edges[i].id);
        }
    }

    // 从vertex_index哈希表中移除
    remove_vertex_index_entry(e, vertex_id);

    // 用最后一个元素覆盖被删除的元素（swap-and-pop）
    if (idx < e->vertex_count - 1) {
        e->vertices[idx] = e->vertices[e->vertex_count - 1];
        // 更新被移动顶点的索引
        remove_vertex_index_entry(e, e->vertices[idx].id);
        add_vertex_index(e, e->vertices[idx].id, idx);
    }
    e->vertex_count--;

    return TASKFLOW_SUCCESS;
}

taskflow_error_t graph_engine_add_edge(graph_engine_handle_t engine,
                                      const graph_edge_t* edge)
{
    if (!engine || !edge) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    struct graph_engine_s* e = (struct graph_engine_s*)engine;
    
    if (!e->initialized) {
        return TASKFLOW_ERROR_NOT_INITIALIZED;
    }
    
    // 检查容量
    if (e->edge_count >= e->edge_capacity) {
        return TASKFLOW_ERROR_GRAPH_TOO_LARGE;
    }
    
    // 检查边ID是否已存在
    if (find_edge_index(e, edge->id) != SIZE_MAX) {
        return TASKFLOW_ERROR_INVALID_ARG; // 边已存在
    }
    
    // 检查顶点是否存在
    if (find_vertex_index(e, edge->source) == SIZE_MAX ||
        find_vertex_index(e, edge->target) == SIZE_MAX) {
        return TASKFLOW_ERROR_INVALID_ARG; // 顶点不存在
    }
    
    // 添加边
    e->edges[e->edge_count] = *edge;
    size_t edge_idx = e->edge_count;
    
    // 添加索引
    if (!add_edge_index(e, edge->id, edge_idx)) {
        return TASKFLOW_ERROR_MEMORY;
    }
    
    if (!add_out_edge_index(e, edge->source, edge_idx)) {
        // 回滚：移除已添加的边索引
        size_t eh = vertex_id_hash(edge->id, e->index_size);
        edge_index_entry_t** rep = (edge_index_entry_t**)&e->edge_index[eh];
        edge_index_entry_t* re = e->edge_index[eh];
        while (re) {
            edge_index_entry_t* rnext = re->next;
            if (re->edge_id == edge->id) { *rep = rnext; free(re); break; }
            rep = &re->next; re = rnext;
        }
        return TASKFLOW_ERROR_MEMORY;
    }

    if (!add_in_edge_index(e, edge->target, edge_idx)) {
        // 回滚：移除out_edge_index和edge_index
        size_t oh = vertex_id_hash(edge->source, e->index_size);
        out_edge_index_entry_t** rop = (out_edge_index_entry_t**)&e->out_edge_index[oh];
        out_edge_index_entry_t* roe = e->out_edge_index[oh];
        while (roe) {
            out_edge_index_entry_t* rnext = roe->next;
            if (roe->edge_idx == edge_idx) { *rop = rnext; free(roe); break; }
            rop = &roe->next; roe = rnext;
        }
        size_t eh = vertex_id_hash(edge->id, e->index_size);
        edge_index_entry_t** rep = (edge_index_entry_t**)&e->edge_index[eh];
        edge_index_entry_t* re = e->edge_index[eh];
        while (re) {
            edge_index_entry_t* rnext = re->next;
            if (re->edge_id == edge->id) { *rep = rnext; free(re); break; }
            rep = &re->next; re = rnext;
        }
        return TASKFLOW_ERROR_MEMORY;
    }
    
    // 更新顶点度数
    update_vertex_degree(e, edge->source, true, 1);   // 出度+1
    update_vertex_degree(e, edge->target, false, 1);  // 入度+1
    
    e->edge_count++;
    
    return TASKFLOW_SUCCESS;
}

taskflow_error_t graph_engine_remove_edge(graph_engine_handle_t engine,
                                         edge_id_t edge_id)
{
    if (!engine || edge_id == 0) return TASKFLOW_ERROR_INVALID_ARG;

    struct graph_engine_s* e = (struct graph_engine_s*)engine;
    if (!e->initialized) return TASKFLOW_ERROR_NOT_INITIALIZED;

    size_t idx = find_edge_index(e, edge_id);
    if (idx == SIZE_MAX) return TASKFLOW_ERROR_INVALID_ARG;

    graph_edge_t* removed = &e->edges[idx];
    vertex_id_t source = removed->source;
    vertex_id_t target = removed->target;

    // 从所有索引中移除
    remove_edge_from_all_indexes(e, edge_id, source, target, idx);

    // 更新顶点度数
    update_vertex_degree(e, source, true, -1);
    update_vertex_degree(e, target, false, -1);

    // swap-and-pop: 用最后一个边覆盖被删除的边
    if (idx < e->edge_count - 1) {
        graph_edge_t* last_edge = &e->edges[e->edge_count - 1];
        // 移除被移动边的旧索引
        size_t last_hash = vertex_id_hash(last_edge->id, e->index_size);
        edge_index_entry_t** lep = (edge_index_entry_t**)&e->edge_index[last_hash];
        edge_index_entry_t* lee = e->edge_index[last_hash];
        while (lee) {
            edge_index_entry_t* lnext = lee->next;
            if (lee->edge_id == last_edge->id) {
                *lep = lnext;
                free(lee);
                break;
            }
            lep = &lee->next;
            lee = lnext;
        }

        // 更新out/in edge index中被移动边的引用
        size_t src_hash = vertex_id_hash(last_edge->source, e->index_size);
        out_edge_index_entry_t** oep = (out_edge_index_entry_t**)&e->out_edge_index[src_hash];
        out_edge_index_entry_t* oee = e->out_edge_index[src_hash];
        while (oee) {
            if (oee->edge_idx == e->edge_count - 1) {
                oee->edge_idx = idx;
                break;
            }
            oee = oee->next;
        }
        size_t tgt_hash = vertex_id_hash(last_edge->target, e->index_size);
        in_edge_index_entry_t** iep = (in_edge_index_entry_t**)&e->in_edge_index[tgt_hash];
        in_edge_index_entry_t* iee = e->in_edge_index[tgt_hash];
        while (iee) {
            if (iee->edge_idx == e->edge_count - 1) {
                iee->edge_idx = idx;
                break;
            }
            iee = iee->next;
        }

        e->edges[idx] = *last_edge;
        add_edge_index(e, e->edges[idx].id, idx);
    }
    e->edge_count--;

    return TASKFLOW_SUCCESS;
}

taskflow_error_t graph_engine_get_vertex(graph_engine_handle_t engine,
                                        vertex_id_t vertex_id,
                                        graph_vertex_t* vertex)
{
    if (!engine || vertex_id == 0 || !vertex) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    struct graph_engine_s* e = (struct graph_engine_s*)engine;
    
    if (!e->initialized) {
        return TASKFLOW_ERROR_NOT_INITIALIZED;
    }
    
    size_t idx = find_vertex_index(e, vertex_id);
    if (idx == SIZE_MAX) {
        return TASKFLOW_ERROR_INVALID_ARG; // 顶点不存在
    }
    
    *vertex = e->vertices[idx];
    return TASKFLOW_SUCCESS;
}

taskflow_error_t graph_engine_get_edge(graph_engine_handle_t engine,
                                      edge_id_t edge_id,
                                      graph_edge_t* edge)
{
    if (!engine || edge_id == 0 || !edge) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    struct graph_engine_s* e = (struct graph_engine_s*)engine;
    
    if (!e->initialized) {
        return TASKFLOW_ERROR_NOT_INITIALIZED;
    }
    
    size_t idx = find_edge_index(e, edge_id);
    if (idx == SIZE_MAX) {
        return TASKFLOW_ERROR_INVALID_ARG; // 边不存在
    }
    
    *edge = e->edges[idx];
    return TASKFLOW_SUCCESS;
}

// ============================================================================
// 其他API实现（存根）
// ============================================================================

size_t graph_engine_get_out_edges(graph_engine_handle_t engine,
                                 vertex_id_t vertex_id,
                                 graph_edge_t* edges,
                                 size_t max_edges)
{
    if (!engine || vertex_id == 0) {
        return 0;
    }
    
    struct graph_engine_s* e = (struct graph_engine_s*)engine;
    
    if (!e->initialized) {
        return 0;
    }
    
    return collect_out_edges(e, vertex_id, edges, max_edges);
}

size_t graph_engine_get_in_edges(graph_engine_handle_t engine,
                                vertex_id_t vertex_id,
                                graph_edge_t* edges,
                                size_t max_edges)
{
    if (!engine || vertex_id == 0) {
        return 0;
    }
    
    struct graph_engine_s* e = (struct graph_engine_s*)engine;
    
    if (!e->initialized) {
        return 0;
    }
    
    return collect_in_edges(e, vertex_id, edges, max_edges);
}

size_t graph_engine_get_neighbors(graph_engine_handle_t engine,
                                 vertex_id_t vertex_id,
                                 vertex_id_t* neighbors,
                                 size_t max_neighbors)
{
    if (!engine || vertex_id == 0 || !neighbors) {
        return 0;
    }
    
    struct graph_engine_s* e = (struct graph_engine_s*)engine;
    
    if (!e->initialized) {
        return 0;
    }
    
    // 收集出邻居（目标顶点）
    size_t count = collect_neighbors(e, vertex_id, neighbors, max_neighbors, true);
    return count;
}

taskflow_error_t graph_engine_bfs(graph_engine_handle_t engine,
                                 vertex_id_t start_vertex,
                                 void (*visitor)(vertex_id_t vertex_id, void* user_data),
                                 void* user_data)
{
    if (!engine || start_vertex == 0 || !visitor) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    struct graph_engine_s* e = (struct graph_engine_s*)engine;
    
    if (!e->initialized) {
        return TASKFLOW_ERROR_NOT_INITIALIZED;
    }
    
    // 检查起始顶点是否存在
    if (find_vertex_index(e, start_vertex) == SIZE_MAX) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    // 创建已访问集合
    vertex_hashset_t* visited = hashset_create(e->index_size);
    if (!visited) {
        return TASKFLOW_ERROR_MEMORY;
    }
    
    // 创建队列
    vertex_queue_t* queue = queue_create();
    if (!queue) {
        hashset_destroy(visited);
        return TASKFLOW_ERROR_MEMORY;
    }
    
    // 起始顶点入队并标记为已访问
    hashset_add(visited, start_vertex);
    queue_enqueue(queue, start_vertex);
    
    // BFS主循环
    while (!queue_is_empty(queue)) {
        vertex_id_t current = queue_dequeue(queue);
        
        // 访问当前顶点
        visitor(current, user_data);
        
        // 获取所有邻居
        const size_t MAX_NEIGHBORS_PER_BATCH = 32;
        vertex_id_t neighbors[MAX_NEIGHBORS_PER_BATCH];
        
        size_t offset = 0;
        while (true) {
            size_t count = collect_neighbors(e, current, 
                                           neighbors + offset, 
                                           MAX_NEIGHBORS_PER_BATCH - offset,
                                           true);
            if (count == 0) break;
            
            for (size_t i = 0; i < count; i++) {
                vertex_id_t neighbor = neighbors[offset + i];
                
                if (!hashset_contains(visited, neighbor)) {
                    hashset_add(visited, neighbor);
                    queue_enqueue(queue, neighbor);
                }
            }
            
            offset += count;
            if (offset >= MAX_NEIGHBORS_PER_BATCH) {
                offset = 0;
            }
        }
    }
    
    // 清理资源
    queue_destroy(queue);
    hashset_destroy(visited);
    
    return TASKFLOW_SUCCESS;
}

taskflow_error_t graph_engine_dfs(graph_engine_handle_t engine,
                                 vertex_id_t start_vertex,
                                 void (*visitor)(vertex_id_t vertex_id, void* user_data),
                                 void* user_data)
{
    if (!engine || start_vertex == 0 || !visitor) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    struct graph_engine_s* e = (struct graph_engine_s*)engine;
    
    if (!e->initialized) {
        return TASKFLOW_ERROR_NOT_INITIALIZED;
    }
    
    // 检查起始顶点是否存在
    if (find_vertex_index(e, start_vertex) == SIZE_MAX) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    // 创建已访问集合
    vertex_hashset_t* visited = hashset_create(e->index_size);
    if (!visited) {
        return TASKFLOW_ERROR_MEMORY;
    }
    
    // 创建栈
    vertex_stack_t* stack = stack_create();
    if (!stack) {
        hashset_destroy(visited);
        return TASKFLOW_ERROR_MEMORY;
    }
    
    // 起始顶点入栈并标记为已访问
    hashset_add(visited, start_vertex);
    stack_push(stack, start_vertex);
    
    // DFS主循环
    while (!stack_is_empty(stack)) {
        vertex_id_t current = stack_pop(stack);
        
        // 访问当前顶点
        visitor(current, user_data);
        
        // 获取所有邻居
        const size_t MAX_NEIGHBORS_PER_BATCH = 32;
        vertex_id_t neighbors[MAX_NEIGHBORS_PER_BATCH];
        
        size_t offset = 0;
        while (true) {
            size_t count = collect_neighbors(e, current, 
                                           neighbors + offset, 
                                           MAX_NEIGHBORS_PER_BATCH - offset,
                                           true);
            if (count == 0) break;
            
            // 逆序入栈，保证顺序遍历（可选）
            for (size_t i = count; i > 0; i--) {
                vertex_id_t neighbor = neighbors[offset + i - 1];
                
                if (!hashset_contains(visited, neighbor)) {
                    hashset_add(visited, neighbor);
                    stack_push(stack, neighbor);
                }
            }
            
            offset += count;
            if (offset >= MAX_NEIGHBORS_PER_BATCH) {
                offset = 0;
            }
        }
    }
    
    // 清理资源
    stack_destroy(stack);
    hashset_destroy(visited);
    
    return TASKFLOW_SUCCESS;
}

taskflow_error_t graph_engine_get_stats(graph_engine_handle_t engine,
                                       size_t* vertex_count,
                                       size_t* edge_count,
                                       uint32_t* max_out_degree,
                                       uint32_t* max_in_degree)
{
    if (!engine || !vertex_count || !edge_count) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    struct graph_engine_s* e = (struct graph_engine_s*)engine;
    
    *vertex_count = e->vertex_count;
    *edge_count = e->edge_count;
    
    // 计算最大度数
    if (max_out_degree || max_in_degree) {
        uint32_t max_out = 0;
        uint32_t max_in = 0;
        
        for (size_t i = 0; i < e->vertex_count; i++) {
            if (e->vertices[i].out_degree > max_out) {
                max_out = e->vertices[i].out_degree;
            }
            if (e->vertices[i].in_degree > max_in) {
                max_in = e->vertices[i].in_degree;
            }
        }
        
        if (max_out_degree) *max_out_degree = max_out;
        if (max_in_degree) *max_in_degree = max_in;
    }
    
    return TASKFLOW_SUCCESS;
}

taskflow_error_t graph_engine_load(graph_engine_handle_t engine,
                                  const graph_vertex_t* vertices,
                                  size_t vertex_count,
                                  const graph_edge_t* edges,
                                  size_t edge_count)
{
    if (!engine || (!vertices && vertex_count > 0) || (!edges && edge_count > 0)) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    struct graph_engine_s* e = (struct graph_engine_s*)engine;
    
    if (!e->initialized) {
        return TASKFLOW_ERROR_NOT_INITIALIZED;
    }
    
    // 检查容量
    if (vertex_count > e->vertex_capacity || edge_count > e->edge_capacity) {
        return TASKFLOW_ERROR_GRAPH_TOO_LARGE;
    }
    
    // 清空当前图
    taskflow_error_t result = graph_engine_clear(engine);
    if (result != TASKFLOW_SUCCESS) {
        return result;
    }
    
    // 添加顶点
    for (size_t i = 0; i < vertex_count; i++) {
        result = graph_engine_add_vertex(engine, &vertices[i]);
        if (result != TASKFLOW_SUCCESS) {
            // 回滚：清空已添加的内容
            graph_engine_clear(engine);
            return result;
        }
    }
    
    // 添加边
    for (size_t i = 0; i < edge_count; i++) {
        result = graph_engine_add_edge(engine, &edges[i]);
        if (result != TASKFLOW_SUCCESS) {
            // 回滚：清空已添加的内容
            graph_engine_clear(engine);
            return result;
        }
    }
    
    return TASKFLOW_SUCCESS;
}

taskflow_error_t graph_engine_save(graph_engine_handle_t engine,
                                  graph_vertex_t* vertices,
                                  size_t max_vertices,
                                  graph_edge_t* edges,
                                  size_t max_edges,
                                  size_t* actual_vertices,
                                  size_t* actual_edges)
{
    if (!engine || !vertices || !edges || !actual_vertices || !actual_edges) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    struct graph_engine_s* e = (struct graph_engine_s*)engine;
    
    if (!e->initialized) {
        return TASKFLOW_ERROR_NOT_INITIALIZED;
    }
    
    // 检查容量
    if (max_vertices < e->vertex_count || max_edges < e->edge_count) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    // 复制顶点
    if (e->vertex_count > 0) {
        memcpy(vertices, e->vertices, e->vertex_count * sizeof(graph_vertex_t));
    }
    
    // 复制边
    if (e->edge_count > 0) {
        memcpy(edges, e->edges, e->edge_count * sizeof(graph_edge_t));
    }
    
    *actual_vertices = e->vertex_count;
    *actual_edges = e->edge_count;
    
    return TASKFLOW_SUCCESS;
}

taskflow_error_t graph_engine_clear(graph_engine_handle_t engine)
{
    if (!engine) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    struct graph_engine_s* e = (struct graph_engine_s*)engine;
    
    // 清理索引
    if (e->vertex_index) {
        destroy_hash_table((void**)e->vertex_index, e->index_size, free_vertex_index_entry);
        e->vertex_index = (vertex_index_entry_t**)create_hash_table(e->index_size);
    }
    
    if (e->edge_index) {
        destroy_hash_table((void**)e->edge_index, e->index_size, free_edge_index_entry);
        e->edge_index = (edge_index_entry_t**)create_hash_table(e->index_size);
    }
    
    if (e->out_edge_index) {
        destroy_hash_table((void**)e->out_edge_index, e->index_size, free_out_edge_index_entry);
        e->out_edge_index = (out_edge_index_entry_t**)create_hash_table(e->index_size);
    }
    
    if (e->in_edge_index) {
        destroy_hash_table((void**)e->in_edge_index, e->index_size, free_in_edge_index_entry);
        e->in_edge_index = (in_edge_index_entry_t**)create_hash_table(e->index_size);
    }
    
    e->vertex_count = 0;
    e->edge_count = 0;
    
    return TASKFLOW_SUCCESS;
}

bool graph_engine_is_empty(graph_engine_handle_t engine)
{
    if (!engine) return true;
    
    struct graph_engine_s* e = (struct graph_engine_s*)engine;
    return (e->vertex_count == 0);
}

taskflow_error_t graph_engine_get_vertex_ids(graph_engine_handle_t engine,
                                             vertex_id_t* out_ids,
                                             size_t max_count,
                                             size_t* out_actual)
{
    if (!engine || !out_ids || max_count == 0) return TASKFLOW_ERROR_INVALID_ARG;
    struct graph_engine_s* e = (struct graph_engine_s*)engine;
    size_t actual = (e->vertex_count < max_count) ? e->vertex_count : max_count;
    for (size_t i = 0; i < actual; i++) {
        out_ids[i] = e->vertices[i].id;
    }
    if (out_actual) *out_actual = actual;
    return TASKFLOW_SUCCESS;
}
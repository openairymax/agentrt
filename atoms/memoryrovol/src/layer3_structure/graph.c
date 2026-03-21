/**
 * @file graph.c
 * @brief L3 结构层图编码器（支持 SQLite 持久化）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "layer3_structure.h"
#include "agentos.h"
#include "logger.h"
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>

typedef struct graph_node_entry {
    char* node_id;
    float* feature;   // 可选，仅内存
    struct graph_node_entry* next;
} graph_node_entry_t;

typedef struct graph_edge_entry {
    char* from_id;
    char* to_id;
    float weight;
    agentos_relation_type_t type;
    struct graph_edge_entry* next_from;
    // From data intelligence emerges. by spharx
    struct graph_edge_entry* next_to;
} graph_edge_entry_t;

struct agentos_graph_encoder {
    agentos_binder_t* binder;
    size_t node_dim;
    graph_node_entry_t* nodes;
    graph_edge_entry_t* edges;
    agentos_mutex_t* lock;
    // 持久化相关
    sqlite3* db;
    char* db_path;
};

static const char* create_node_table_sql =
    "CREATE TABLE IF NOT EXISTS graph_nodes ("
    "node_id TEXT PRIMARY KEY,"
    "feature BLOB"  // 特征向量序列化（可选）
    ");";

static const char* create_edge_table_sql =
    "CREATE TABLE IF NOT EXISTS graph_edges ("
    "from_id TEXT,"
    "to_id TEXT,"
    "weight REAL,"
    "type INTEGER,"
    "PRIMARY KEY (from_id, to_id)"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_from ON graph_edges(from_id);"
    "CREATE INDEX IF NOT EXISTS idx_to ON graph_edges(to_id);";

agentos_graph_encoder_t* agentos_graph_encoder_create(
    agentos_binder_t* binder,
    size_t node_dim,
    const agentos_layer3_structure_config_t* config) {

    if (!binder || node_dim == 0) return NULL;

    agentos_graph_encoder_t* enc = (agentos_graph_encoder_t*)calloc(1, sizeof(agentos_graph_encoder_t));
    if (!enc) {
        AGENTOS_LOG_ERROR("Failed to allocate graph encoder");
        return NULL;
    }

    enc->binder = binder;
    enc->node_dim = node_dim;
    enc->lock = agentos_mutex_create();
    if (!enc->lock) {
        free(enc);
        return NULL;
    }

    // 打开数据库
    if (config && config->db_path) {
        enc->db_path = strdup(config->db_path);
        int rc = sqlite3_open(config->db_path, &enc->db);
        if (rc != SQLITE_OK) {
            AGENTOS_LOG_ERROR("Failed to open graph database: %s", sqlite3_errmsg(enc->db));
            agentos_mutex_destroy(enc->lock);
            free(enc);
            return NULL;
        }
        // 创建表
        char* errmsg = NULL;
        rc = sqlite3_exec(enc->db, create_node_table_sql, NULL, NULL, &errmsg);
        if (rc != SQLITE_OK) {
            AGENTOS_LOG_ERROR("Failed to create node table: %s", errmsg);
            sqlite3_free(errmsg);
            sqlite3_close(enc->db);
            agentos_mutex_destroy(enc->lock);
            free(enc->db_path);
            free(enc);
            return NULL;
        }
        rc = sqlite3_exec(enc->db, create_edge_table_sql, NULL, NULL, &errmsg);
        if (rc != SQLITE_OK) {
            AGENTOS_LOG_ERROR("Failed to create edge table: %s", errmsg);
            sqlite3_free(errmsg);
            sqlite3_close(enc->db);
            agentos_mutex_destroy(enc->lock);
            free(enc->db_path);
            free(enc);
            return NULL;
        }
        // 从数据库加载已有节点和边？可以延迟加载，但这里可以先不加载，等查询时再访问数据库。
        // 为了性能，可以在启动时将所有节点ID加载到内存，但向量特征可以不用加载。
    }

    return enc;
}

void agentos_graph_encoder_destroy(agentos_graph_encoder_t* enc) {
    if (!enc) return;

    // 释放内存节点
    graph_node_entry_t* n = enc->nodes;
    while (n) {
        graph_node_entry_t* next = n->next;
        free(n->node_id);
        if (n->feature) free(n->feature);
        free(n);
        n = next;
    }

    // 释放内存边
    graph_edge_entry_t* e = enc->edges;
    while (e) {
        graph_edge_entry_t* next = e->next_from;
        free(e->from_id);
        free(e->to_id);
        free(e);
        e = next;
    }

    if (enc->db) sqlite3_close(enc->db);
    if (enc->db_path) free(enc->db_path);
    if (enc->lock) agentos_mutex_destroy(enc->lock);
    free(enc);
}

agentos_error_t agentos_graph_add_node(
    agentos_graph_encoder_t* enc,
    const char* node_id,
    const float* feature) {

    if (!enc || !node_id) return AGENTOS_EINVAL;

    graph_node_entry_t* node = (graph_node_entry_t*)malloc(sizeof(graph_node_entry_t));
    if (!node) return AGENTOS_ENOMEM;
    memset(node, 0, sizeof(graph_node_entry_t));

    node->node_id = strdup(node_id);
    if (feature) {
        node->feature = (float*)malloc(enc->node_dim * sizeof(float));
        if (!node->feature) {
            free(node->node_id);
            free(node);
            return AGENTOS_ENOMEM;
        }
        memcpy(node->feature, feature, enc->node_dim * sizeof(float));
    }

    // 插入内存链表
    agentos_mutex_lock(enc->lock);
    node->next = enc->nodes;
    enc->nodes = node;

    // 持久化到数据库（如果启用）
    if (enc->db) {
        sqlite3_stmt* stmt;
        const char* insert_sql = "INSERT OR REPLACE INTO graph_nodes (node_id, feature) VALUES (?, ?);";
        int rc = sqlite3_prepare_v2(enc->db, insert_sql, -1, &stmt, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, node_id, -1, SQLITE_STATIC);
            if (feature) {
                sqlite3_bind_blob(stmt, 2, feature, enc->node_dim * sizeof(float), SQLITE_STATIC);
            } else {
                sqlite3_bind_null(stmt, 2);
            }
            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            if (rc != SQLITE_DONE) {
                AGENTOS_LOG_WARN("Failed to insert node into database: %s", node_id);
            }
        }
    }
    agentos_mutex_unlock(enc->lock);

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_graph_add_edge(
    agentos_graph_encoder_t* enc,
    const agentos_graph_edge_t* edge) {

    if (!enc || !edge || !edge->from_id || !edge->to_id) return AGENTOS_EINVAL;

    graph_edge_entry_t* e = (graph_edge_entry_t*)malloc(sizeof(graph_edge_entry_t));
    if (!e) return AGENTOS_ENOMEM;
    memset(e, 0, sizeof(graph_edge_entry_t));

    e->from_id = strdup(edge->from_id);
    e->to_id = strdup(edge->to_id);
    e->weight = edge->weight;
    e->type = edge->type;

    agentos_mutex_lock(enc->lock);
    e->next_from = enc->edges;
    enc->edges = e;

    // 持久化到数据库
    if (enc->db) {
        sqlite3_stmt* stmt;
        const char* insert_sql = "INSERT OR REPLACE INTO graph_edges (from_id, to_id, weight, type) VALUES (?, ?, ?, ?);";
        int rc = sqlite3_prepare_v2(enc->db, insert_sql, -1, &stmt, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, edge->from_id, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, edge->to_id, -1, SQLITE_STATIC);
            sqlite3_bind_double(stmt, 3, edge->weight);
            sqlite3_bind_int(stmt, 4, (int)edge->type);
            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            if (rc != SQLITE_DONE) {
                AGENTOS_LOG_WARN("Failed to insert edge into database");
            }
        }
    }
    agentos_mutex_unlock(enc->lock);

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_graph_encode(
    agentos_graph_encoder_t* enc,
    float** out_graph_vector) {

    if (!enc || !out_graph_vector) return AGENTOS_EINVAL;

    // 统计有特征的节点
    size_t count = 0;
    graph_node_entry_t* n = enc->nodes;
    while (n) {
        if (n->feature) count++;
        n = n->next;
    }

    if (count == 0) {
        *out_graph_vector = NULL;
        return AGENTOS_ENOENT;
    }

    size_t dim = enc->node_dim;
    float* sum = (float*)calloc(dim, sizeof(float));
    if (!sum) return AGENTOS_ENOMEM;

    n = enc->nodes;
    while (n) {
        if (n->feature) {
            for (size_t i = 0; i < dim; i++) sum[i] += n->feature[i];
        }
        n = n->next;
    }

    // 平均
    for (size_t i = 0; i < dim; i++) sum[i] /= count;

    *out_graph_vector = sum;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_graph_get_neighbors(
    agentos_graph_encoder_t* enc,
    const char* node_id,
    agentos_graph_edge_t*** out_edges,
    size_t* out_count) {

    if (!enc || !node_id || !out_edges || !out_count) return AGENTOS_EINVAL;

    // 先统计内存中的边（可以结合数据库查询，但这里简化：只查内存）
    size_t cap = 8;
    size_t cnt = 0;
    agentos_graph_edge_t** edges = (agentos_graph_edge_t**)malloc(cap * sizeof(agentos_graph_edge_t*));
    if (!edges) return AGENTOS_ENOMEM;

    agentos_mutex_lock(enc->lock);
    graph_edge_entry_t* e = enc->edges;
    while (e) {
        if (strcmp(e->from_id, node_id) == 0 || strcmp(e->to_id, node_id) == 0) {
            if (cnt >= cap) {
                cap *= 2;
                agentos_graph_edge_t** new_edges = (agentos_graph_edge_t**)realloc(edges, cap * sizeof(agentos_graph_edge_t*));
                if (!new_edges) {
                    for (size_t i = 0; i < cnt; i++) free(edges[i]);
                    free(edges);
                    agentos_mutex_unlock(enc->lock);
                    return AGENTOS_ENOMEM;
                }
                edges = new_edges;
            }
            agentos_graph_edge_t* copy = (agentos_graph_edge_t*)malloc(sizeof(agentos_graph_edge_t));
            if (!copy) {
                for (size_t i = 0; i < cnt; i++) free(edges[i]);
                free(edges);
                agentos_mutex_unlock(enc->lock);
                return AGENTOS_ENOMEM;
            }
            copy->from_id = strdup(e->from_id);
            copy->to_id = strdup(e->to_id);
            copy->weight = e->weight;
            copy->type = e->type;
            edges[cnt++] = copy;
        }
        e = e->next_from;
    }
    agentos_mutex_unlock(enc->lock);

    *out_edges = edges;
    *out_count = cnt;
    return AGENTOS_SUCCESS;
}

void agentos_graph_edges_free(agentos_graph_edge_t** edges, size_t count) {
    if (!edges) return;
    for (size_t i = 0; i < count; i++) {
        if (edges[i]) {
            free(edges[i]->from_id);
            free(edges[i]->to_id);
            free(edges[i]);
        }
    }
    free(edges);
}
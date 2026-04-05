/**
 * @file bm25.c
 * @brief BM25 文本相似度算法
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "layer2_feature.h"
#include <sqlite3.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../agentos/commons/utils/memory/include/memory_compat.h"
#include "../../../agentos/commons/utils/string/include/string_compat.h"
#include <string.h>

struct agentos_bm25_index {
    sqlite3* db;
    char* db_path;
    float k1;
    float b;
    agentos_mutex_t* lock;
};

agentos_error_t agentos_bm25_create(
    const char* db_path,
    float k1,
    float b,
    agentos_bm25_index_t** out_idx) {

    if (!db_path || !out_idx) return AGENTOS_EINVAL;

    agentos_bm25_index_t* idx = AGENTOS_CALLOC(1, sizeof(agentos_bm25_index_t));
    if (!idx) return AGENTOS_ENOMEM;

    idx->db_path = AGENTOS_STRDUP(db_path);
    if (!idx->db_path) {
        AGENTOS_FREE(idx);
        return AGENTOS_ENOMEM;
    }
    idx->k1 = (k1 > 0) ? k1 : 1.2f;
    idx->b = (b > 0) ? b : 0.75f;
    idx->lock = agentos_mutex_create();
    if (!idx->lock) {
        AGENTOS_FREE(idx->db_path);
        AGENTOS_FREE(idx);
        return AGENTOS_ENOMEM;
    }

    int rc = sqlite3_open(db_path, &idx->db);
    if (rc != SQLITE_OK) {
        agentos_mutex_destroy(idx->lock);
        AGENTOS_FREE(idx->db_path);
        AGENTOS_FREE(idx);
        return AGENTOS_EIO;
    }

    const char* create_sql =
        "CREATE VIRTUAL TABLE IF NOT EXISTS documents_fts "
        "USING fts5(record_id UNINDEXED, content, tokenize='porter');";
    rc = sqlite3_exec(idx->db, create_sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_close(idx->db);
        agentos_mutex_destroy(idx->lock);
        AGENTOS_FREE(idx->db_path);
        AGENTOS_FREE(idx);
        return AGENTOS_EIO;
    }

    *out_idx = idx;
    return AGENTOS_SUCCESS;
}

void agentos_bm25_destroy(agentos_bm25_index_t* idx) {
    if (!idx) return;
    sqlite3_close(idx->db);
    AGENTOS_FREE(idx->db_path);
    agentos_mutex_destroy(idx->lock);
    AGENTOS_FREE(idx);
}

agentos_error_t agentos_bm25_add_document(
    agentos_bm25_index_t* idx,
    const char* record_id,
    const char* content) {

    if (!idx || !record_id || !content) return AGENTOS_EINVAL;

    agentos_mutex_lock(idx->lock);
    const char* sql = "INSERT OR REPLACE INTO documents_fts (record_id, content) VALUES (?, ?);";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(idx->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        agentos_mutex_unlock(idx->lock);
        return AGENTOS_EIO;
    }
    sqlite3_bind_text(stmt, 1, record_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, content, -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    agentos_mutex_unlock(idx->lock);
    return (rc == SQLITE_DONE) ? AGENTOS_SUCCESS : AGENTOS_EIO;
}

agentos_error_t agentos_bm25_remove_document(
    agentos_bm25_index_t* idx,
    const char* record_id) {

    if (!idx || !record_id) return AGENTOS_EINVAL;

    agentos_mutex_lock(idx->lock);
    const char* sql = "DELETE FROM documents_fts WHERE record_id = ?;";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(idx->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        agentos_mutex_unlock(idx->lock);
        return AGENTOS_EIO;
    }
    sqlite3_bind_text(stmt, 1, record_id, -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    agentos_mutex_unlock(idx->lock);
    return (rc == SQLITE_DONE) ? AGENTOS_SUCCESS : AGENTOS_EIO;
}

agentos_error_t agentos_bm25_search(
    agentos_bm25_index_t* idx,
    const char* query,
    uint32_t top_k,
    char*** out_ids,
    float** out_scores,
    size_t* out_count) {

    if (!idx || !query || !out_ids || !out_scores || !out_count) return AGENTOS_EINVAL;

    char sql[512];
    snprintf(sql, sizeof(sql),
        "SELECT record_id, rank FROM documents_fts WHERE documents_fts MATCH ? ORDER BY rank LIMIT %d;", top_k);

    agentos_mutex_lock(idx->lock);
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(idx->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        agentos_mutex_unlock(idx->lock);
        return AGENTOS_EIO;
    }
    sqlite3_bind_text(stmt, 1, query, -1, SQLITE_STATIC);

    size_t cap = 16;
    char** ids = AGENTOS_MALLOC(cap * sizeof(char*));
    float* scores = AGENTOS_MALLOC(cap * sizeof(float));
    if (!ids || !scores) {
        sqlite3_finalize(stmt);
        agentos_mutex_unlock(idx->lock);
        AGENTOS_FREE(ids);
        AGENTOS_FREE(scores);
        return AGENTOS_ENOMEM;
    }
    size_t count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= cap) {
            cap *= 2;
            char** new_ids = AGENTOS_REALLOC(ids, cap * sizeof(char*));
            float* new_scores = AGENTOS_REALLOC(scores, cap * sizeof(float));
            if (!new_ids || !new_scores) {
                sqlite3_finalize(stmt);
                agentos_mutex_unlock(idx->lock);
                for (size_t i = 0; i < count; i++) AGENTOS_FREE(ids[i]);
                AGENTOS_FREE(ids);
                AGENTOS_FREE(scores);
                return AGENTOS_ENOMEM;
            }
            ids = new_ids;
            scores = new_scores;
        }
        const unsigned char* id = sqlite3_column_text(stmt, 0);
        float rank = (float)sqlite3_column_double(stmt, 1);
        ids[count] = AGENTOS_STRDUP((const char*)id);
        // FTS5 rank 越小越相关，转换为越大越�?
        scores[count] = 1.0f / (1.0f + rank);
        count++;
    }
    sqlite3_finalize(stmt);
    agentos_mutex_unlock(idx->lock);

    *out_ids = ids;
    *out_scores = scores;
    *out_count = count;
    return AGENTOS_SUCCESS;
}

/**
 * @file vector_store.c
 * @brief 向量存储实现（SQLite 实现）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "vector_store.h"
#include "logger.h"
#include <sqlite3.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../commons/utils/memory/include/memory_compat.h"
#include "../../../commons/utils/string/include/string_compat.h"
#include <string.h>

struct agentos_vector_store {
    sqlite3* db;
    size_t dimension;
    agentos_mutex_t* lock;
};

agentos_error_t agentos_vector_store_create(
    const agentos_vector_store_config_t* manager,
    agentos_vector_store_t** out_store) {

    if (!manager || !manager->db_path || manager->dimension == 0 || !out_store)
        return AGENTOS_EINVAL;

    agentos_vector_store_t* store = (agentos_vector_store_t*)AGENTOS_CALLOC(1, sizeof(agentos_vector_store_t));
    if (!store) {
        AGENTOS_LOG_ERROR("Failed to allocate vector store");
        return AGENTOS_ENOMEM;
    }

    int rc = sqlite3_open(manager->db_path, &store->db);
    if (rc != SQLITE_OK) {
        AGENTOS_LOG_ERROR("Failed to open vector store database: %s", sqlite3_errmsg(store->db));
        AGENTOS_FREE(store);
        return AGENTOS_EIO;
    }

    store->dimension = manager->dimension;
    store->lock = agentos_mutex_create();
    if (!store->lock) {
        sqlite3_close(store->db);
        AGENTOS_FREE(store);
        return AGENTOS_ENOMEM;
    }

    // 创建�?
    const char* create_sql =
        "CREATE TABLE IF NOT EXISTS vectors ("
        "record_id TEXT PRIMARY KEY,"
        "vector BLOB NOT NULL"
        ");";
    char* errmsg = NULL;
    rc = sqlite3_exec(store->db, create_sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        AGENTOS_LOG_ERROR("Failed to create vector table: %s", errmsg);
        sqlite3_free(errmsg);
        agentos_mutex_destroy(store->lock);
        sqlite3_close(store->db);
        AGENTOS_FREE(store);
        return AGENTOS_EIO;
    }

    *out_store = store;
    return AGENTOS_SUCCESS;
}

void agentos_vector_store_destroy(agentos_vector_store_t* store) {
    if (!store) return;
    sqlite3_close(store->db);
    agentos_mutex_destroy(store->lock);
    AGENTOS_FREE(store);
}

agentos_error_t agentos_vector_store_put(
    agentos_vector_store_t* store,
    const char* record_id,
    const float* vector,
    size_t dim) {

    if (!store || !record_id || !vector || dim != store->dimension) {
        AGENTOS_LOG_ERROR("Invalid parameters to vector_store_put");
        return AGENTOS_EINVAL;
    }

    agentos_mutex_lock(store->lock);

    const char* insert_sql = "INSERT OR REPLACE INTO vectors (record_id, vector) VALUES (?, ?);";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(store->db, insert_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        AGENTOS_LOG_ERROR("Failed to prepare insert statement: %s", sqlite3_errmsg(store->db));
        agentos_mutex_unlock(store->lock);
        return AGENTOS_EIO;
    }

    sqlite3_bind_text(stmt, 1, record_id, -1, SQLITE_STATIC);
    sqlite3_bind_blob(stmt, 2, vector, dim * sizeof(float), SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    agentos_mutex_unlock(store->lock);
    return (rc == SQLITE_DONE) ? AGENTOS_SUCCESS : AGENTOS_EIO;
}

agentos_error_t agentos_vector_store_get(
    agentos_vector_store_t* store,
    const char* record_id,
    float** out_vector,
    size_t* out_dim) {

    if (!store || !record_id || !out_vector || !out_dim) return AGENTOS_EINVAL;

    agentos_mutex_lock(store->lock);

    const char* select_sql = "SELECT vector FROM vectors WHERE record_id = ?;";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(store->db, select_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        AGENTOS_LOG_ERROR("Failed to prepare select statement: %s", sqlite3_errmsg(store->db));
        agentos_mutex_unlock(store->lock);
        return AGENTOS_EIO;
    }

    sqlite3_bind_text(stmt, 1, record_id, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        agentos_mutex_unlock(store->lock);
        return AGENTOS_ENOENT;
    }

    const void* blob = sqlite3_column_blob(stmt, 0);
    int blob_size = sqlite3_column_bytes(stmt, 0);
    size_t dim = blob_size / sizeof(float);
    if (dim != store->dimension) {
        AGENTOS_LOG_WARN("Vector dimension mismatch in store: expected %zu, got %zu", store->dimension, dim);
    }

    float* vec = (float*)AGENTOS_MALLOC(blob_size);
    if (!vec) {
        sqlite3_finalize(stmt);
        agentos_mutex_unlock(store->lock);
        return AGENTOS_ENOMEM;
    }
    memcpy(vec, blob, blob_size);

    sqlite3_finalize(stmt);
    agentos_mutex_unlock(store->lock);

    *out_vector = vec;
    *out_dim = dim;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_vector_store_delete(
    agentos_vector_store_t* store,
    const char* record_id) {

    if (!store || !record_id) return AGENTOS_EINVAL;

    agentos_mutex_lock(store->lock);

    const char* delete_sql = "DELETE FROM vectors WHERE record_id = ?;";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(store->db, delete_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        AGENTOS_LOG_ERROR("Failed to prepare delete statement: %s", sqlite3_errmsg(store->db));
        agentos_mutex_unlock(store->lock);
        return AGENTOS_EIO;
    }

    sqlite3_bind_text(stmt, 1, record_id, -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    agentos_mutex_unlock(store->lock);
    return (rc == SQLITE_DONE) ? AGENTOS_SUCCESS : AGENTOS_EIO;
}

agentos_error_t agentos_vector_store_list_ids(
    agentos_vector_store_t* store,
    char*** out_ids,
    size_t* out_count) {

    if (!store || !out_ids || !out_count) return AGENTOS_EINVAL;

    agentos_mutex_lock(store->lock);

    const char* select_sql = "SELECT record_id FROM vectors;";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(store->db, select_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        AGENTOS_LOG_ERROR("Failed to prepare list statement: %s", sqlite3_errmsg(store->db));
        agentos_mutex_unlock(store->lock);
        return AGENTOS_EIO;
    }

    size_t capacity = 64;
    size_t count = 0;
    char** ids = (char**)AGENTOS_MALLOC(capacity * sizeof(char*));
    if (!ids) {
        sqlite3_finalize(stmt);
        agentos_mutex_unlock(store->lock);
        return AGENTOS_ENOMEM;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= capacity) {
            capacity *= 2;
            char** new_ids = (char**)AGENTOS_REALLOC(ids, capacity * sizeof(char*));
            if (!new_ids) {
                for (size_t i = 0; i < count; i++) AGENTOS_FREE(ids[i]);
                AGENTOS_FREE(ids);
                sqlite3_finalize(stmt);
                agentos_mutex_unlock(store->lock);
                return AGENTOS_ENOMEM;
            }
            ids = new_ids;
        }
        const unsigned char* id = sqlite3_column_text(stmt, 0);
        ids[count] = AGENTOS_STRDUP((const char*)id);
        if (!ids[count]) {
            for (size_t i = 0; i < count; i++) AGENTOS_FREE(ids[i]);
            AGENTOS_FREE(ids);
            sqlite3_finalize(stmt);
            agentos_mutex_unlock(store->lock);
            return AGENTOS_ENOMEM;
        }
        count++;
    }

    sqlite3_finalize(stmt);
    agentos_mutex_unlock(store->lock);

    *out_ids = ids;
    *out_count = count;
    return AGENTOS_SUCCESS;
}

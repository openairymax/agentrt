/**
 * @file metadata.c
 * @brief L1 元数据索引（SQLite 实现�?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "layer1_raw.h"
#include <stdio.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../bases/utils/memory/include/memory_compat.h"
#include "../../../bases/utils/string/include/string_compat.h"
#include <string.h>
#include <sqlite3.h>

/**
 * @brief 元数据数据库结构
 */
typedef struct agentos_raw_metadata_db {
    sqlite3* db;
    agentos_mutex_t* lock;
} agentos_raw_metadata_db_t;

/**
 * @brief 创建元数据数据库实例
 * @param db_path 数据库文件路�?
 * @param out_db 输出数据库句�?
 * @return agentos_error_t
 // From data intelligence emerges. by spharx
 */
agentos_error_t agentos_raw_metadata_db_create(
    const char* db_path,
    agentos_raw_metadata_db_t** out_db) {

    if (!db_path || !out_db) return AGENTOS_EINVAL;

    agentos_raw_metadata_db_t* db_handle = (agentos_raw_metadata_db_t*)AGENTOS_MALLOC(sizeof(agentos_raw_metadata_db_t));
    if (!db_handle) return AGENTOS_ENOMEM;
    memset(db_handle, 0, sizeof(agentos_raw_metadata_db_t));

    int rc = sqlite3_open(db_path, &db_handle->db);
    if (rc != SQLITE_OK) {
        AGENTOS_FREE(db_handle);
        return AGENTOS_EIO;
    }

    // 创建�?
    const char* create_sql =
        "CREATE TABLE IF NOT EXISTS raw_metadata ("
        "record_id TEXT PRIMARY KEY,"
        "timestamp INTEGER,"
        "data_len INTEGER,"
        "access_count INTEGER DEFAULT 0,"
        "last_access INTEGER,"
        "source TEXT,"
        "trace_id TEXT,"
        "tags TEXT"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_timestamp ON raw_metadata(timestamp);"
        "CREATE INDEX IF NOT EXISTS idx_source ON raw_metadata(source);"
        "CREATE INDEX IF NOT EXISTS idx_trace ON raw_metadata(trace_id);";

    char* errmsg = NULL;
    rc = sqlite3_exec(db_handle->db, create_sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        sqlite3_close(db_handle->db);
        AGENTOS_FREE(db_handle);
        if (errmsg) sqlite3_free(errmsg);
        return AGENTOS_EIO;
    }

    db_handle->lock = agentos_mutex_create();
    if (!db_handle->lock) {
        sqlite3_close(db_handle->db);
        AGENTOS_FREE(db_handle);
        return AGENTOS_ENOMEM;
    }

    *out_db = db_handle;
    return AGENTOS_SUCCESS;
}

void agentos_raw_metadata_db_destroy(agentos_raw_metadata_db_t* db_handle) {
    if (!db_handle) return;
    if (db_handle->lock) agentos_mutex_destroy(db_handle->lock);
    if (db_handle->db) sqlite3_close(db_handle->db);
    AGENTOS_FREE(db_handle);
}

/**
 * @brief 插入或更新元数据记录
 */
agentos_error_t agentos_raw_metadata_db_upsert(
    agentos_raw_metadata_db_t* db_handle,
    const agentos_raw_metadata_t* meta) {

    if (!db_handle || !meta || !meta->record_id) return AGENTOS_EINVAL;

    agentos_mutex_lock(db_handle->lock);

    const char* upsert_sql =
        "INSERT OR REPLACE INTO raw_metadata "
        "(record_id, timestamp, data_len, access_count, last_access, source, trace_id, tags) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(db_handle->db, upsert_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        agentos_mutex_unlock(db_handle->lock);
        return AGENTOS_EIO;
    }

    sqlite3_bind_text(stmt, 1, meta->record_id, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, (sqlite3_int64)meta->timestamp);
    sqlite3_bind_int(stmt, 3, (int)meta->data_len);
    sqlite3_bind_int(stmt, 4, (int)meta->access_count);
    sqlite3_bind_int64(stmt, 5, (sqlite3_int64)meta->last_access);
    sqlite3_bind_text(stmt, 6, meta->source, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, meta->trace_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 8, meta->tags_json, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    agentos_mutex_unlock(db_handle->lock);

    return (rc == SQLITE_DONE) ? AGENTOS_SUCCESS : AGENTOS_EIO;
}

/**
 * @brief 根据记录ID查询元数�?
 */
agentos_error_t agentos_raw_metadata_db_get(
    agentos_raw_metadata_db_t* db_handle,
    const char* record_id,
    agentos_raw_metadata_t** out_meta) {

    if (!db_handle || !record_id || !out_meta) return AGENTOS_EINVAL;

    agentos_mutex_lock(db_handle->lock);

    const char* select_sql = "SELECT timestamp, data_len, access_count, last_access, source, trace_id, tags "
                             "FROM raw_metadata WHERE record_id = ?;";

    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(db_handle->db, select_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        agentos_mutex_unlock(db_handle->lock);
        return AGENTOS_EIO;
    }

    sqlite3_bind_text(stmt, 1, record_id, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        agentos_mutex_unlock(db_handle->lock);
        return AGENTOS_ENOENT;
    }

    agentos_raw_metadata_t* meta = (agentos_raw_metadata_t*)AGENTOS_MALLOC(sizeof(agentos_raw_metadata_t));
    if (!meta) {
        sqlite3_finalize(stmt);
        agentos_mutex_unlock(db_handle->lock);
        return AGENTOS_ENOMEM;
    }
    memset(meta, 0, sizeof(agentos_raw_metadata_t));

    meta->record_id = AGENTOS_STRDUP(record_id);
    meta->timestamp = (uint64_t)sqlite3_column_int64(stmt, 0);
    meta->data_len = (uint32_t)sqlite3_column_int(stmt, 1);
    meta->access_count = (uint32_t)sqlite3_column_int(stmt, 2);
    meta->last_access = (uint64_t)sqlite3_column_int64(stmt, 3);
    const unsigned char* source = sqlite3_column_text(stmt, 4);
    if (source) meta->source = AGENTOS_STRDUP((const char*)source);
    const unsigned char* trace = sqlite3_column_text(stmt, 5);
    if (trace) meta->trace_id = AGENTOS_STRDUP((const char*)trace);
    const unsigned char* tags = sqlite3_column_text(stmt, 6);
    if (tags) meta->tags_json = AGENTOS_STRDUP((const char*)tags);

    sqlite3_finalize(stmt);
    agentos_mutex_unlock(db_handle->lock);

    *out_meta = meta;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 按条件查询元数据（示例：按时间范围）
 */
agentos_error_t agentos_raw_metadata_db_query_time(
    agentos_raw_metadata_db_t* db_handle,
    uint64_t start_time,
    uint64_t end_time,
    char*** out_record_ids,
    size_t* out_count) {

    if (!db_handle || !out_record_ids || !out_count) return AGENTOS_EINVAL;

    agentos_mutex_lock(db_handle->lock);

    const char* sql = "SELECT record_id FROM raw_metadata WHERE timestamp BETWEEN ? AND ? ORDER BY timestamp;";
    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(db_handle->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        agentos_mutex_unlock(db_handle->lock);
        return AGENTOS_EIO;
    }

    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)start_time);
    sqlite3_bind_int64(stmt, 2, (sqlite3_int64)end_time);

    size_t capacity = 16;
    size_t count = 0;
    char** ids = (char**)AGENTOS_MALLOC(capacity * sizeof(char*));
    if (!ids) {
        sqlite3_finalize(stmt);
        agentos_mutex_unlock(db_handle->lock);
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
                agentos_mutex_unlock(db_handle->lock);
                return AGENTOS_ENOMEM;
            }
            ids = new_ids;
        }
        const unsigned char* text = sqlite3_column_text(stmt, 0);
        ids[count++] = AGENTOS_STRDUP((const char*)text);
    }

    sqlite3_finalize(stmt);
    agentos_mutex_unlock(db_handle->lock);

    *out_record_ids = ids;
    *out_count = count;
    return AGENTOS_SUCCESS;
}
/**
 * @file vector_store.c
 * @brief 向量持久化存储实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 实现向量数据的持久化存储，支持：
 * - SQLite 数据库存储
 * - 向量索引管理
 * - 快速检索
 */

#include "../include/vector_store.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include <agentos/memory.h>
#include <agentos/string.h>
#include <string.h>
#include <stdio.h>

/* SQLite 集成 */
#ifdef AGENTOS_HAS_SQLITE3
#include <sqlite3.h>
#else
#include "../include/sqlite3_stub.h"
#endif /* AGENTOS_HAS_SQLITE3 */

/**
 * @brief 向量存储内部结构
 */
struct agentos_vector_store {
    sqlite3* db;                    /**< SQLite 数据库连接 */
    char db_path[512];              /**< 数据库路径 */
    size_t dimension;               /**< 向量维度 */
    int initialized;                /**< 初始化标志 */
};

/**
 * @brief 初始化数据库表结构
 */
static agentos_error_t vector_store_init_schema(agentos_vector_store_t* store) {
    const char* sql = 
        "CREATE TABLE IF NOT EXISTS vectors ("
        "  id TEXT PRIMARY KEY,"
        "  vector BLOB NOT NULL,"
        "  dimension INTEGER NOT NULL,"
        "  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "  updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_vectors_id ON vectors(id);";

    char* err_msg = NULL;
    int rc = sqlite3_exec(store->db, sql, NULL, NULL, &err_msg);
    
    if (rc != SQLITE_OK) {
        AGENTOS_LOG_ERROR("Failed to create vector store schema: %s", err_msg);
        sqlite3_free(err_msg);
        return AGENTOS_ERROR;
    }
    sqlite3_free(err_msg); /* 成功时也需要释放，sqlite3_free(NULL)是安全的 */

    return AGENTOS_SUCCESS;
}

/**
 * @brief 创建向量存储
 */
agentos_error_t agentos_vector_store_create(
    const agentos_vector_store_config_t* config,
    agentos_vector_store_t** out_store)
{
    if (!config || !config->db_path || !out_store) {
        AGENTOS_LOG_ERROR("Invalid parameters to vector_store_create");
        return AGENTOS_EINVAL;
    }

    if (config->dimension == 0 || config->dimension > 4096) {
        AGENTOS_LOG_ERROR("Invalid dimension: %zu", config->dimension);
        return AGENTOS_EINVAL;
    }

    agentos_vector_store_t* store = 
        (agentos_vector_store_t*)AGENTOS_CALLOC(1, sizeof(agentos_vector_store_t));
    if (!store) {
        return AGENTOS_ENOMEM;
    }

    /* 复制配置 */
    strncpy(store->db_path, config->db_path, sizeof(store->db_path) - 1);
    store->db_path[sizeof(store->db_path) - 1] = '\0';
    store->dimension = config->dimension;

    /* 打开 SQLite 数据库 */
    int rc = sqlite3_open(config->db_path, &store->db);
    if (rc != SQLITE_OK) {
        AGENTOS_LOG_ERROR("Failed to open SQLite database: %s", sqlite3_errmsg(store->db));
        AGENTOS_FREE(store);
        return AGENTOS_ERROR;
    }

    /* 初始化表结构 */
    agentos_error_t err = vector_store_init_schema(store);
    if (err != AGENTOS_SUCCESS) {
        sqlite3_close(store->db);
        AGENTOS_FREE(store);
        return err;
    }

    store->initialized = 1;
    *out_store = store;

    AGENTOS_LOG_INFO("Vector store created at %s", config->db_path);
    return AGENTOS_SUCCESS;
}

/**
 * @brief 销毁向量存储
 */
void agentos_vector_store_destroy(agentos_vector_store_t* store) {
    if (!store) return;

    if (store->db) {
        sqlite3_close(store->db);
    }

    AGENTOS_FREE(store);
}

/**
 * @brief 存储向量
 */
agentos_error_t agentos_vector_store_put(
    agentos_vector_store_t* store,
    const char* record_id,
    const float* vector,
    size_t dim)
{
    if (!store || !record_id || !vector) {
        return AGENTOS_EINVAL;
    }

    if (!store->initialized) {
        return AGENTOS_ENOTINIT;
    }

    if (dim != store->dimension) {
        AGENTOS_LOG_ERROR("Dimension mismatch: expected %zu, got %zu", 
                         store->dimension, dim);
        return AGENTOS_EINVAL;
    }

    /* 准备 SQL 语句 */
    const char* sql = 
        "INSERT OR REPLACE INTO vectors (id, vector, dimension, updated_at) "
        "VALUES (?, ?, ?, CURRENT_TIMESTAMP)";

    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        AGENTOS_LOG_ERROR("Failed to prepare statement: %s", sqlite3_errmsg(store->db));
        sqlite3_finalize(stmt); /* 即使stmt为NULL也是安全的 */
        return AGENTOS_ERROR;
    }

    /* 绑定参数 */
    sqlite3_bind_text(stmt, 1, record_id, -1, SQLITE_STATIC);
    sqlite3_bind_blob(stmt, 2, vector, (int)(dim * sizeof(float)), SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, (int)dim);

    /* 执行 */
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        AGENTOS_LOG_ERROR("Failed to insert vector: %s", sqlite3_errmsg(store->db));
        sqlite3_finalize(stmt);
        return AGENTOS_ERROR;
    }

    sqlite3_finalize(stmt);
    return AGENTOS_SUCCESS;
}

/**
 * @brief 获取向量
 */
agentos_error_t agentos_vector_store_get(
    agentos_vector_store_t* store,
    const char* record_id,
    float** out_vector,
    size_t* out_dim)
{
    if (!store || !record_id || !out_vector || !out_dim) {
        return AGENTOS_EINVAL;
    }

    if (!store->initialized) {
        return AGENTOS_ENOTINIT;
    }

    /* 准备 SQL 语句 */
    const char* sql = "SELECT vector, dimension FROM vectors WHERE id = ?";

    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        AGENTOS_LOG_ERROR("Failed to prepare statement: %s", sqlite3_errmsg(store->db));
        sqlite3_finalize(stmt); /* 即使stmt为NULL也是安全的 */
        return AGENTOS_ERROR;
    }

    /* 绑定参数 */
    sqlite3_bind_text(stmt, 1, record_id, -1, SQLITE_STATIC);

    /* 执行查询 */
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        /* 获取向量数据 */
        const void* blob = sqlite3_column_blob(stmt, 0);
        int dim = sqlite3_column_int(stmt, 1);

        if (blob && dim > 0) {
            *out_vector = (float*)AGENTOS_MALLOC(dim * sizeof(float));
            if (!*out_vector) {
                sqlite3_finalize(stmt);
                return AGENTOS_ENOMEM;
            }

            memcpy(*out_vector, blob, dim * sizeof(float));
            *out_dim = (size_t)dim;

            sqlite3_finalize(stmt);
            return AGENTOS_SUCCESS;
        }
    }

    sqlite3_finalize(stmt);
    return AGENTOS_ENOENT; /* 未找到 */
}

/**
 * @brief 删除向量
 */
agentos_error_t agentos_vector_store_delete(
    agentos_vector_store_t* store,
    const char* record_id)
{
    if (!store || !record_id) {
        return AGENTOS_EINVAL;
    }

    if (!store->initialized) {
        return AGENTOS_ENOTINIT;
    }

    /* 准备 SQL 语句 */
    const char* sql = "DELETE FROM vectors WHERE id = ?";

    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        AGENTOS_LOG_ERROR("Failed to prepare statement: %s", sqlite3_errmsg(store->db));
        sqlite3_finalize(stmt); /* 即使stmt为NULL也是安全的 */
        return AGENTOS_ERROR;
    }

    /* 绑定参数 */
    sqlite3_bind_text(stmt, 1, record_id, -1, SQLITE_STATIC);

    /* 执行删除 */
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        return AGENTOS_SUCCESS;
    }

    return AGENTOS_ERROR;
}

/**
 * @brief 检查向量是否存在
 */
agentos_error_t agentos_vector_store_exists(
    agentos_vector_store_t* store,
    const char* record_id,
    int* out_exists)
{
    if (!store || !record_id || !out_exists) {
        return AGENTOS_EINVAL;
    }

    if (!store->initialized) {
        return AGENTOS_ENOTINIT;
    }

    /* 准备 SQL 语句 */
    const char* sql = "SELECT 1 FROM vectors WHERE id = ? LIMIT 1";

    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        AGENTOS_LOG_ERROR("Failed to prepare statement: %s", sqlite3_errmsg(store->db));
        sqlite3_finalize(stmt); /* 即使stmt为NULL也是安全的 */
        return AGENTOS_ERROR;
    }

    /* 绑定参数 */
    sqlite3_bind_text(stmt, 1, record_id, -1, SQLITE_STATIC);

    /* 执行查询 */
    rc = sqlite3_step(stmt);
    *out_exists = (rc == SQLITE_ROW) ? 1 : 0;

    sqlite3_finalize(stmt);
    return AGENTOS_SUCCESS;
}

/**
 * @brief 获取所有向量 ID
 */
agentos_error_t agentos_vector_store_list_ids(
    agentos_vector_store_t* store,
    char*** out_ids,
    size_t* out_count)
{
    if (!store || !out_ids || !out_count) {
        return AGENTOS_EINVAL;
    }

    if (!store->initialized) {
        return AGENTOS_ENOTINIT;
    }

    /* 准备 SQL 语句 */
    const char* sql = "SELECT id FROM vectors ORDER BY created_at";

    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        AGENTOS_LOG_ERROR("Failed to prepare statement: %s", sqlite3_errmsg(store->db));
        sqlite3_finalize(stmt); /* 即使stmt为NULL也是安全的 */
        return AGENTOS_ERROR;
    }

    /* 计算结果数量 */
    size_t count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        count++;
    }

    /* 分配数组 */
    char** ids = (char**)AGENTOS_MALLOC(count * sizeof(char*));
    if (!ids) {
        sqlite3_finalize(stmt);
        return AGENTOS_ENOMEM;
    }

    /* 重置语句并重新执行 */
    sqlite3_reset(stmt);
    size_t i = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* id = (const char*)sqlite3_column_text(stmt, 0);
        ids[i++] = AGENTOS_STRDUP(id);
    }

    sqlite3_finalize(stmt);

    *out_ids = ids;
    *out_count = count;

    return AGENTOS_SUCCESS;
}

/**
 * @brief 清空所有向量
 */
agentos_error_t agentos_vector_store_clear(agentos_vector_store_t* store) {
    if (!store) {
        return AGENTOS_EINVAL;
    }

    if (!store->initialized) {
        return AGENTOS_ENOTINIT;
    }

    const char* sql = "DELETE FROM vectors";
    char* err_msg = NULL;

    int rc = sqlite3_exec(store->db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        AGENTOS_LOG_ERROR("Failed to clear vectors: %s", err_msg);
        sqlite3_free(err_msg);
        return AGENTOS_ERROR;
    }
    sqlite3_free(err_msg); /* 成功时也需要释放，sqlite3_free(NULL)是安全的 */

    return AGENTOS_SUCCESS;
}

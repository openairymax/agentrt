/**
 * @file heapstore_registry.c
 * @brief AgentOS 数据分区注册表实现
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * "From data intelligence emerges."
 */

#include "heapstore_registry.h"
#include "private.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

#define heapstore_SQLITE_IMPLEMENTATION

#ifndef heapstore_SQLITE_IMPLEMENTATION

typedef struct {
    heapstore_registry_type_t type;
    void* data;
    size_t size;
    void* next;
} registry_node_t;

typedef struct {
    registry_node_t* agents;
    registry_node_t* skills;
    registry_node_t* sessions;
    size_t agent_count;
    size_t skill_count;
    size_t session_count;
} registry_db_t;

static registry_db_t s_registry = {0};

heapstore_error_t heapstore_registry_init(void) {
    memset(&s_registry, 0, sizeof(s_registry));
    return heapstore_SUCCESS;
}

void heapstore_registry_shutdown(void) {
    registry_node_t* node = s_registry.agents;
    while (node) {
        registry_node_t* next = node->next;
        free(node->data);
        free(node);
        node = next;
    }
    node = s_registry.skills;
    while (node) {
        registry_node_t* next = node->next;
        free(node->data);
        free(node);
        node = next;
    }
    node = s_registry.sessions;
    while (node) {
        registry_node_t* next = node->next;
        free(node->data);
        free(node);
        node = next;
    }
    memset(&s_registry, 0, sizeof(s_registry));
}

#else

#include <sqlite3.h>

typedef struct {
    sqlite3* db;
    char db_path[512];
    pthread_mutex_t lock;
    int initialized;
} registry_db_t;

/**
 * @brief 注册表迭代器内部结构
 */
struct heapstore_registry_iter {
    sqlite3_stmt* stmt;           /* SQLite 预编译语句 */
    int current_type;             /* 当前遍历的类型 (agents/skills/sessions) */
    int has_more;                 /* 是否还有更多记录 */
};

static registry_db_t s_registry = {0};

static heapstore_error_t init_database(sqlite3* db) {
    const char* sql =
        "CREATE TABLE IF NOT EXISTS agents ("
        "    id TEXT PRIMARY KEY,"
        "    name TEXT NOT NULL,"
        "    type TEXT,"
        "    version TEXT,"
        "    status TEXT,"
        "    config_path TEXT,"
        "    created_at INTEGER,"
        "    updated_at INTEGER"
        ");"
        "CREATE TABLE IF NOT EXISTS agent_capabilities ("
        "    agent_id TEXT,"
        "    capability TEXT,"
        "    FOREIGN KEY (agent_id) REFERENCES agents(id)"
        ");"
        "CREATE TABLE IF NOT EXISTS skills ("
        "    id TEXT PRIMARY KEY,"
        "    name TEXT NOT NULL,"
        "    version TEXT,"
        "    library_path TEXT,"
        "    manifest_path TEXT,"
        "    installed_at INTEGER"
        ");"
        "CREATE TABLE IF NOT EXISTS sessions ("
        "    id TEXT PRIMARY KEY,"
        "    user_id TEXT,"
        "    created_at INTEGER,"
        "    last_active_at INTEGER,"
        "    ttl_seconds INTEGER,"
        "    status TEXT"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_agent_type ON agents(type);"
        "CREATE INDEX IF NOT EXISTS idx_skill_status ON skills(name);"
        "CREATE INDEX IF NOT EXISTS idx_session_user ON sessions(user_id);";

    char* err_msg = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        if (err_msg) {
            sqlite3_free(err_msg);
        }
        return heapstore_ERR_DB_INIT_FAILED;
    }
    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_registry_init(void) {
    if (s_registry.initialized) {
        return heapstore_SUCCESS;
    }

    char root_path[256] = "heapstore";
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s/registry", root_path);

#ifdef _WIN32
    mkdir(full_path);
#else
    mkdir(full_path, 0755);
#endif

    snprintf(s_registry.db_path, sizeof(s_registry.db_path), "%s/registry.db", full_path);

    int rc = sqlite3_open(s_registry.db_path, &s_registry.db);
    if (rc != SQLITE_OK) {
        return heapstore_ERR_DB_INIT_FAILED;
    }

    sqlite3_exec(s_registry.db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
    sqlite3_exec(s_registry.db, "PRAGMA synchronous=NORMAL;", NULL, NULL, NULL);
    sqlite3_exec(s_registry.db, "PRAGMA cache_size=10000;", NULL, NULL, NULL);
    sqlite3_exec(s_registry.db, "PRAGMA temp_store=MEMORY;", NULL, NULL, NULL);

    heapstore_error_t err = init_database(s_registry.db);
    if (err != heapstore_SUCCESS) {
        sqlite3_close(s_registry.db);
        memset(&s_registry, 0, sizeof(s_registry));
        return err;
    }

    pthread_mutex_init(&s_registry.lock, NULL);
    s_registry.initialized = true;

    return heapstore_SUCCESS;
}

void heapstore_registry_shutdown(void) {
    if (!s_registry.initialized) {
        return;
    }

    pthread_mutex_lock(&s_registry.lock);

    if (s_registry.db) {
        sqlite3_close(s_registry.db);
        s_registry.db = NULL;
    }

    s_registry.initialized = false;
    pthread_mutex_unlock(&s_registry.lock);
    pthread_mutex_destroy(&s_registry.lock);
}

static heapstore_error_t execute_sql_with_lock(const char* sql,
                                               heapstore_error_t (*bind_func)(sqlite3_stmt*, void*),
                                               void* bind_data) {
    if (!s_registry.initialized || !s_registry.db) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&s_registry.lock);

    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(s_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&s_registry.lock);
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    if (bind_func) {
        heapstore_error_t err = bind_func(stmt, bind_data);
        if (err != heapstore_SUCCESS) {
            sqlite3_finalize(stmt);
            pthread_mutex_unlock(&s_registry.lock);
            return err;
        }
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&s_registry.lock);

    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    return heapstore_SUCCESS;
}

static heapstore_error_t bind_agent_record(sqlite3_stmt* stmt, void* data) {
    const heapstore_agent_record_t* record = (const heapstore_agent_record_t*)data;

    sqlite3_bind_text(stmt, 1, record->id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, record->name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, record->type, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, record->version, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, record->status, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, record->config_path, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 7, record->created_at);
    sqlite3_bind_int64(stmt, 8, record->updated_at);

    return heapstore_SUCCESS;
}

static heapstore_error_t __attribute__((unused)) bind_agent_id(sqlite3_stmt* stmt, void* data) {
    const char* agent_id = (const char*)data;
    sqlite3_bind_text(stmt, 1, agent_id, -1, SQLITE_STATIC);
    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_registry_add_agent(const heapstore_agent_record_t* record) {
    if (!record || !record->id[0]) {
        return heapstore_ERR_INVALID_PARAM;
    }

    const char* sql =
        "INSERT INTO agents "
        "(id, name, type, version, status, config_path, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?);";

    return execute_sql_with_lock(sql, bind_agent_record, (void*)record);
}

heapstore_error_t heapstore_registry_get_agent(const char* id, heapstore_agent_record_t* record) {
    if (!id || !record) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (!s_registry.initialized || !s_registry.db) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&s_registry.lock);

    const char* sql = "SELECT id, name, type, version, status, config_path, created_at, updated_at FROM agents WHERE id = ?;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(s_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&s_registry.lock);
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    sqlite3_bind_text(stmt, 1, id, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const char* text;
        text = (const char*)sqlite3_column_text(stmt, 0);
        if (text) strncpy(record->id, text, sizeof(record->id) - 1);
        else record->id[0] = '\0';

        text = (const char*)sqlite3_column_text(stmt, 1);
        if (text) strncpy(record->name, text, sizeof(record->name) - 1);
        else record->name[0] = '\0';

        text = (const char*)sqlite3_column_text(stmt, 2);
        if (text) strncpy(record->type, text, sizeof(record->type) - 1);
        else record->type[0] = '\0';

        text = (const char*)sqlite3_column_text(stmt, 3);
        if (text) strncpy(record->version, text, sizeof(record->version) - 1);
        else record->version[0] = '\0';

        text = (const char*)sqlite3_column_text(stmt, 4);
        if (text) strncpy(record->status, text, sizeof(record->status) - 1);
        else record->status[0] = '\0';

        text = (const char*)sqlite3_column_text(stmt, 5);
        if (text) strncpy(record->config_path, text, sizeof(record->config_path) - 1);
        else record->config_path[0] = '\0';

        record->created_at = sqlite3_column_int64(stmt, 6);
        record->updated_at = sqlite3_column_int64(stmt, 7);

        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&s_registry.lock);
        return heapstore_SUCCESS;
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&s_registry.lock);
    return heapstore_ERR_NOT_FOUND;
}

heapstore_error_t heapstore_registry_update_agent(const heapstore_agent_record_t* record) {
    if (!record || !record->id[0]) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (!s_registry.initialized || !s_registry.db) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&s_registry.lock);

    const char* sql =
        "UPDATE agents SET "
        "name = ?, type = ?, version = ?, status = ?, config_path = ?, updated_at = ? "
        "WHERE id = ?;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(s_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&s_registry.lock);
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    sqlite3_bind_text(stmt, 1, record->name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, record->type, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, record->version, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, record->status, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, record->config_path, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 6, record->updated_at);
    sqlite3_bind_text(stmt, 7, record->id, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&s_registry.lock);

    if (rc != SQLITE_DONE) {
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_registry_delete_agent(const char* id) {
    if (!id) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (!s_registry.initialized || !s_registry.db) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&s_registry.lock);

    const char* sql = "DELETE FROM agents WHERE id = ?;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(s_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&s_registry.lock);
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    sqlite3_bind_text(stmt, 1, id, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&s_registry.lock);

    if (rc != SQLITE_DONE) {
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_registry_query_agents(const char* filter_type, const char* filter_status, heapstore_registry_iter_t** iter) {
    (void)filter_type;
    (void)filter_status;
    (void)iter;
    return heapstore_ERR_NOT_INITIALIZED;
}

heapstore_error_t heapstore_registry_add_skill(const heapstore_skill_record_t* record) {
    if (!record || !record->id[0]) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (!s_registry.initialized || !s_registry.db) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&s_registry.lock);

    const char* sql =
        "INSERT INTO skills "
        "(id, name, version, library_path, manifest_path, installed_at) "
        "VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(s_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&s_registry.lock);
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    sqlite3_bind_text(stmt, 1, record->id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, record->name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, record->version, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, record->library_path, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, record->manifest_path, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 6, record->installed_at);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&s_registry.lock);

    if (rc != SQLITE_DONE) {
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_registry_get_skill(const char* id, heapstore_skill_record_t* record) {
    if (!id || !record) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (!s_registry.initialized || !s_registry.db) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&s_registry.lock);

    const char* sql = "SELECT id, name, version, library_path, manifest_path, installed_at FROM skills WHERE id = ?;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(s_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&s_registry.lock);
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    sqlite3_bind_text(stmt, 1, id, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const char* text;
        text = (const char*)sqlite3_column_text(stmt, 0);
        if (text) strncpy(record->id, text, sizeof(record->id) - 1);
        text = (const char*)sqlite3_column_text(stmt, 1);
        if (text) strncpy(record->name, text, sizeof(record->name) - 1);
        text = (const char*)sqlite3_column_text(stmt, 2);
        if (text) strncpy(record->version, text, sizeof(record->version) - 1);
        text = (const char*)sqlite3_column_text(stmt, 3);
        if (text) strncpy(record->library_path, text, sizeof(record->library_path) - 1);
        text = (const char*)sqlite3_column_text(stmt, 4);
        if (text) strncpy(record->manifest_path, text, sizeof(record->manifest_path) - 1);
        record->installed_at = sqlite3_column_int64(stmt, 5);
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&s_registry.lock);
        return heapstore_SUCCESS;
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&s_registry.lock);
    return heapstore_ERR_NOT_FOUND;
}

heapstore_error_t heapstore_registry_delete_skill(const char* id) {
    if (!id) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (!s_registry.initialized || !s_registry.db) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&s_registry.lock);

    const char* sql = "DELETE FROM skills WHERE id = ?;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(s_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&s_registry.lock);
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    sqlite3_bind_text(stmt, 1, id, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&s_registry.lock);

    if (rc != SQLITE_DONE) {
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_registry_add_session(const heapstore_session_record_t* record) {
    if (!record || !record->id[0]) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (!s_registry.initialized || !s_registry.db) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&s_registry.lock);

    const char* sql =
        "INSERT INTO sessions "
        "(id, user_id, created_at, last_active_at, ttl_seconds, status) "
        "VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(s_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&s_registry.lock);
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    sqlite3_bind_text(stmt, 1, record->id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, record->user_id, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, record->created_at);
    sqlite3_bind_int64(stmt, 4, record->last_active_at);
    sqlite3_bind_int(stmt, 5, record->ttl_seconds);
    sqlite3_bind_text(stmt, 6, record->status, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&s_registry.lock);

    if (rc != SQLITE_DONE) {
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_registry_get_session(const char* id, heapstore_session_record_t* record) {
    if (!id || !record) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (!s_registry.initialized || !s_registry.db) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&s_registry.lock);

    const char* sql = "SELECT id, user_id, created_at, last_active_at, ttl_seconds, status FROM sessions WHERE id = ?;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(s_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&s_registry.lock);
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    sqlite3_bind_text(stmt, 1, id, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const char* text;
        text = (const char*)sqlite3_column_text(stmt, 0);
        if (text) strncpy(record->id, text, sizeof(record->id) - 1);
        text = (const char*)sqlite3_column_text(stmt, 1);
        if (text) strncpy(record->user_id, text, sizeof(record->user_id) - 1);
        record->created_at = sqlite3_column_int64(stmt, 2);
        record->last_active_at = sqlite3_column_int64(stmt, 3);
        record->ttl_seconds = sqlite3_column_int(stmt, 4);
        text = (const char*)sqlite3_column_text(stmt, 5);
        if (text) strncpy(record->status, text, sizeof(record->status) - 1);
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&s_registry.lock);
        return heapstore_SUCCESS;
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&s_registry.lock);
    return heapstore_ERR_NOT_FOUND;
}

heapstore_error_t heapstore_registry_update_session(const heapstore_session_record_t* record) {
    if (!record || !record->id[0]) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (!s_registry.initialized || !s_registry.db) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&s_registry.lock);

    const char* sql = "UPDATE sessions SET user_id = ?, last_active_at = ?, ttl_seconds = ?, status = ? WHERE id = ?;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(s_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&s_registry.lock);
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    sqlite3_bind_text(stmt, 1, record->user_id, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, record->last_active_at);
    sqlite3_bind_int(stmt, 3, record->ttl_seconds);
    sqlite3_bind_text(stmt, 4, record->status, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, record->id, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&s_registry.lock);

    if (rc != SQLITE_DONE) {
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_registry_delete_session(const char* id) {
    if (!id) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (!s_registry.initialized || !s_registry.db) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&s_registry.lock);

    const char* sql = "DELETE FROM sessions WHERE id = ?;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(s_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&s_registry.lock);
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    sqlite3_bind_text(stmt, 1, id, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&s_registry.lock);

    if (rc != SQLITE_DONE) {
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_registry_query_skills(heapstore_registry_iter_t** iter) {
    if (!iter) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (!s_registry.initialized || !s_registry.db) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&s_registry.lock);

    const char* sql = "SELECT id, name, version, library_path, manifest_path, installed_at FROM skills ORDER BY installed_at DESC;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(s_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&s_registry.lock);
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    heapstore_registry_iter_t* new_iter = (heapstore_registry_iter_t*)malloc(sizeof(heapstore_registry_iter_t));
    if (!new_iter) {
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&s_registry.lock);
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    new_iter->stmt = stmt;
    new_iter->current_type = 1; /* skills */
    new_iter->has_more = 1;

    *iter = new_iter;
    pthread_mutex_unlock(&s_registry.lock);

    return heapstore_SUCCESS;
}

/**
 * @brief 查询会话记录
 *
 * @param filter_status [in] 按状态过滤（NULL 表示不过滤）
 * @param iter [out] 输出迭代器
 * @return heapstore_error_t 错误码
 */
heapstore_error_t heapstore_registry_query_sessions(const char* filter_status, heapstore_registry_iter_t** iter) {
    if (!iter) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (!s_registry.initialized || !s_registry.db) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&s_registry.lock);

    const char* sql;
    sqlite3_stmt* stmt;

    if (filter_status && filter_status[0]) {
        sql = "SELECT id, user_id, created_at, last_active_at, ttl_seconds, status FROM sessions WHERE status = ? ORDER BY last_active_at DESC;";
    } else {
        sql = "SELECT id, user_id, created_at, last_active_at, ttl_seconds, status FROM sessions ORDER BY last_active_at DESC;";
    }

    int rc = sqlite3_prepare_v2(s_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&s_registry.lock);
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    if (filter_status && filter_status[0]) {
        sqlite3_bind_text(stmt, 1, filter_status, -1, SQLITE_STATIC);
    }

    heapstore_registry_iter_t* new_iter = (heapstore_registry_iter_t*)malloc(sizeof(heapstore_registry_iter_t));
    if (!new_iter) {
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&s_registry.lock);
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    new_iter->stmt = stmt;
    new_iter->current_type = 2; /* sessions */
    new_iter->has_more = 1;

    *iter = new_iter;
    pthread_mutex_unlock(&s_registry.lock);

    return heapstore_SUCCESS;
}

/**
 * @brief 遍历下一条记录
 *
 * 根据迭代器的 current_type 自动判断返回哪种类型的记录
 */
heapstore_error_t heapstore_registry_iter_next(heapstore_registry_iter_t* iter, void* record) {
    if (!iter || !record) {
        return heapstore_ERR_INVALID_PARAM;
    }
    if (!iter->stmt || !iter->has_more) {
        return heapstore_ERR_NOT_FOUND;
    }

    int rc = sqlite3_step(iter->stmt);
    if (rc != SQLITE_ROW) {
        iter->has_more = 0;
        return heapstore_ERR_NOT_FOUND;
    }

    const char* text;

    switch (iter->current_type) {
        case 0: { /* agents */
            heapstore_agent_record_t* agent_rec = (heapstore_agent_record_t*)record;
            memset(agent_rec, 0, sizeof(*agent_rec));

            text = (const char*)sqlite3_column_text(iter->stmt, 0);
            if (text) strncpy(agent_rec->id, text, sizeof(agent_rec->id) - 1);
            text = (const char*)sqlite3_column_text(iter->stmt, 1);
            if (text) strncpy(agent_rec->name, text, sizeof(agent_rec->name) - 1);
            text = (const char*)sqlite3_column_text(iter->stmt, 2);
            if (text) strncpy(agent_rec->type, text, sizeof(agent_rec->type) - 1);
            text = (const char*)sqlite3_column_text(iter->stmt, 3);
            if (text) strncpy(agent_rec->version, text, sizeof(agent_rec->version) - 1);
            text = (const char*)sqlite3_column_text(iter->stmt, 4);
            if (text) strncpy(agent_rec->status, text, sizeof(agent_rec->status) - 1);
            text = (const char*)sqlite3_column_text(iter->stmt, 5);
            if (text) strncpy(agent_rec->config_path, text, sizeof(agent_rec->config_path) - 1);
            agent_rec->created_at = sqlite3_column_int64(iter->stmt, 6);
            agent_rec->updated_at = sqlite3_column_int64(iter->stmt, 7);
            break;
        }
        case 1: { /* skills */
            heapstore_skill_record_t* skill_rec = (heapstore_skill_record_t*)record;
            memset(skill_rec, 0, sizeof(*skill_rec));

            text = (const char*)sqlite3_column_text(iter->stmt, 0);
            if (text) strncpy(skill_rec->id, text, sizeof(skill_rec->id) - 1);
            text = (const char*)sqlite3_column_text(iter->stmt, 1);
            if (text) strncpy(skill_rec->name, text, sizeof(skill_rec->name) - 1);
            text = (const char*)sqlite3_column_text(iter->stmt, 2);
            if (text) strncpy(skill_rec->version, text, sizeof(skill_rec->version) - 1);
            text = (const char*)sqlite3_column_text(iter->stmt, 3);
            if (text) strncpy(skill_rec->library_path, text, sizeof(skill_rec->library_path) - 1);
            text = (const char*)sqlite3_column_text(iter->stmt, 4);
            if (text) strncpy(skill_rec->manifest_path, text, sizeof(skill_rec->manifest_path) - 1);
            skill_rec->installed_at = sqlite3_column_int64(iter->stmt, 5);
            break;
        }
        case 2: { /* sessions */
            heapstore_session_record_t* session_rec = (heapstore_session_record_t*)record;
            memset(session_rec, 0, sizeof(*session_rec));

            text = (const char*)sqlite3_column_text(iter->stmt, 0);
            if (text) strncpy(session_rec->id, text, sizeof(session_rec->id) - 1);
            text = (const char*)sqlite3_column_text(iter->stmt, 1);
            if (text) strncpy(session_rec->user_id, text, sizeof(session_rec->user_id) - 1);
            session_rec->created_at = sqlite3_column_int64(iter->stmt, 2);
            session_rec->last_active_at = sqlite3_column_int64(iter->stmt, 3);
            session_rec->ttl_seconds = sqlite3_column_int(iter->stmt, 4);
            text = (const char*)sqlite3_column_text(iter->stmt, 5);
            if (text) strncpy(session_rec->status, text, sizeof(session_rec->status) - 1);
            break;
        }
        default:
            return heapstore_ERR_INVALID_PARAM;
    }

    return heapstore_SUCCESS;
}

void heapstore_registry_iter_destroy(heapstore_registry_iter_t* iter) {
    if (!iter) {
        return;
    }

    if (iter->stmt) {
        sqlite3_finalize(iter->stmt);
        iter->stmt = NULL;
    }

    free(iter);
}

heapstore_error_t heapstore_registry_vacuum(void) {
    if (!s_registry.initialized || !s_registry.db) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&s_registry.lock);
    sqlite3_exec(s_registry.db, "VACUUM;", NULL, NULL, NULL);
    pthread_mutex_unlock(&s_registry.lock);

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_registry_batch_insert_agents(const heapstore_agent_record_t* records, size_t count) {
    if (!records || count == 0) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (!s_registry.initialized || !s_registry.db) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    const char* sql =
        "INSERT INTO agents "
        "(id, name, type, version, status, config_path, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?);";

    pthread_mutex_lock(&s_registry.lock);

    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(s_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&s_registry.lock);
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    rc = sqlite3_exec(s_registry.db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&s_registry.lock);
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    heapstore_error_t result = heapstore_SUCCESS;
    for (size_t i = 0; i < count; i++) {
        const heapstore_agent_record_t* record = &records[i];

        sqlite3_bind_text(stmt, 1, record->id, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, record->name, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, record->type, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, record->version, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, record->status, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 6, record->config_path, -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 7, record->created_at);
        sqlite3_bind_int64(stmt, 8, record->updated_at);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            result = heapstore_ERR_DB_QUERY_FAILED;
            sqlite3_exec(s_registry.db, "ROLLBACK;", NULL, NULL, NULL);
            break;
        }

        sqlite3_reset(stmt);
    }

    if (result == heapstore_SUCCESS) {
        rc = sqlite3_exec(s_registry.db, "COMMIT;", NULL, NULL, NULL);
        if (rc != SQLITE_OK) {
            result = heapstore_ERR_DB_QUERY_FAILED;
        }
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&s_registry.lock);

    return result;
}

heapstore_error_t heapstore_registry_batch_insert_sessions(const heapstore_session_record_t* records, size_t count) {
    if (!records || count == 0) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (!s_registry.initialized || !s_registry.db) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    const char* sql =
        "INSERT INTO sessions "
        "(id, user_id, created_at, last_active_at, ttl_seconds, status) "
        "VALUES (?, ?, ?, ?, ?, ?);";

    pthread_mutex_lock(&s_registry.lock);

    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(s_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&s_registry.lock);
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    rc = sqlite3_exec(s_registry.db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&s_registry.lock);
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    heapstore_error_t result = heapstore_SUCCESS;
    for (size_t i = 0; i < count; i++) {
        const heapstore_session_record_t* record = &records[i];

        sqlite3_bind_text(stmt, 1, record->id, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, record->user_id, -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 3, record->created_at);
        sqlite3_bind_int64(stmt, 4, record->last_active_at);
        sqlite3_bind_int(stmt, 5, record->ttl_seconds);
        sqlite3_bind_text(stmt, 6, record->status, -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            result = heapstore_ERR_DB_QUERY_FAILED;
            sqlite3_exec(s_registry.db, "ROLLBACK;", NULL, NULL, NULL);
            break;
        }

        sqlite3_reset(stmt);
    }

    if (result == heapstore_SUCCESS) {
        rc = sqlite3_exec(s_registry.db, "COMMIT;", NULL, NULL, NULL);
        if (rc != SQLITE_OK) {
            result = heapstore_ERR_DB_QUERY_FAILED;
        }
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&s_registry.lock);

    return result;
}

heapstore_error_t heapstore_registry_batch_insert_skills(const heapstore_skill_record_t* records, size_t count) {
    if (!records || count == 0) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (!s_registry.initialized || !s_registry.db) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    const char* sql =
        "INSERT INTO skills "
        "(id, name, version, library_path, manifest_path, installed_at) "
        "VALUES (?, ?, ?, ?, ?, ?);";

    pthread_mutex_lock(&s_registry.lock);

    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(s_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&s_registry.lock);
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    rc = sqlite3_exec(s_registry.db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&s_registry.lock);
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    heapstore_error_t result = heapstore_SUCCESS;
    for (size_t i = 0; i < count; i++) {
        const heapstore_skill_record_t* record = &records[i];

        sqlite3_bind_text(stmt, 1, record->id, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, record->name, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, record->version, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, record->library_path, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, record->manifest_path, -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 6, record->installed_at);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            result = heapstore_ERR_DB_QUERY_FAILED;
            sqlite3_exec(s_registry.db, "ROLLBACK;", NULL, NULL, NULL);
            break;
        }

        sqlite3_reset(stmt);
    }

    if (result == heapstore_SUCCESS) {
        rc = sqlite3_exec(s_registry.db, "COMMIT;", NULL, NULL, NULL);
        if (rc != SQLITE_OK) {
            result = heapstore_ERR_DB_QUERY_FAILED;
        }
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&s_registry.lock);

    return result;
}

bool heapstore_registry_is_healthy(void) {
    return s_registry.initialized && s_registry.db != NULL;
}

#endif

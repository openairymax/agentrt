/**
 * @file partdata_registry.c
 * @brief AgentOS 数据分区注册表实现
 *
 * Copyright (c) 2026 SPHARX. All Rights Reserved.
 * "From data intelligence emerges."
 */

#include "partdata_registry.h"
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

#define PARTDATA_SQLITE_IMPLEMENTATION

#ifndef PARTDATA_SQLITE_IMPLEMENTATION

typedef struct {
    partdata_registry_type_t type;
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

static registry_db_t g_registry = {0};

partdata_error_t partdata_registry_init(void) {
    memset(&g_registry, 0, sizeof(g_registry));
    return PARTDATA_SUCCESS;
}

void partdata_registry_shutdown(void) {
    registry_node_t* node = g_registry.agents;
    while (node) {
        registry_node_t* next = node->next;
        free(node->data);
        free(node);
        node = next;
    }
    node = g_registry.skills;
    while (node) {
        registry_node_t* next = node->next;
        free(node->data);
        free(node);
        node = next;
    }
    node = g_registry.sessions;
    while (node) {
        registry_node_t* next = node->next;
        free(node->data);
        free(node);
        node = next;
    }
    memset(&g_registry, 0, sizeof(g_registry));
}

#else

#include <sqlite3.h>

typedef struct {
    sqlite3* db;
    char db_path[512];
} registry_db_t;

static registry_db_t g_registry = {0};

static partdata_error_t init_database(sqlite3* db) {
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
        return PARTDATA_ERR_DB_INIT_FAILED;
    }
    return PARTDATA_SUCCESS;
}

partdata_error_t partdata_registry_init(void) {
    char registry_path[512];
    char root_path[256] = "partdata";

    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s/registry", root_path);

#ifdef _WIN32
    mkdir(full_path);
#else
    mkdir(full_path, 0755);
#endif

    snprintf(g_registry.db_path, sizeof(g_registry.db_path), "%s/registry.db", full_path);

    int rc = sqlite3_open(g_registry.db_path, &g_registry.db);
    if (rc != SQLITE_OK) {
        return PARTDATA_ERR_DB_INIT_FAILED;
    }

    sqlite3_exec(g_registry.db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
    sqlite3_exec(g_registry.db, "PRAGMA synchronous=NORMAL;", NULL, NULL, NULL);
    sqlite3_exec(g_registry.db, "PRAGMA cache_size=10000;", NULL, NULL, NULL);
    sqlite3_exec(g_registry.db, "PRAGMA temp_store=MEMORY;", NULL, NULL, NULL);

    partdata_error_t err = init_database(g_registry.db);
    if (err != PARTDATA_SUCCESS) {
        sqlite3_close(g_registry.db);
        memset(&g_registry, 0, sizeof(g_registry));
        return err;
    }

    return PARTDATA_SUCCESS;
}

void partdata_registry_shutdown(void) {
    if (g_registry.db) {
        sqlite3_close(g_registry.db);
        g_registry.db = NULL;
    }
}

partdata_error_t partdata_registry_add_agent(const partdata_agent_record_t* record) {
    if (!record || !record->id[0]) {
        return PARTDATA_ERR_INVALID_PARAM;
    }

    if (!g_registry.db) {
        return PARTDATA_ERR_NOT_INITIALIZED;
    }

    const char* sql = "INSERT INTO agents (id, name, type, version, status, config_path, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(g_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return PARTDATA_ERR_DB_QUERY_FAILED;
    }

    sqlite3_bind_text(stmt, 1, record->id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, record->name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, record->type, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, record->version, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, record->status, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, record->config_path, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 7, record->created_at);
    sqlite3_bind_int64(stmt, 8, record->updated_at);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return PARTDATA_ERR_DB_QUERY_FAILED;
    }

    return PARTDATA_SUCCESS;
}

partdata_error_t partdata_registry_get_agent(const char* id, partdata_agent_record_t* record) {
    if (!id || !record) {
        return PARTDATA_ERR_INVALID_PARAM;
    }

    if (!g_registry.db) {
        return PARTDATA_ERR_NOT_INITIALIZED;
    }

    const char* sql = "SELECT id, name, type, version, status, config_path, created_at, updated_at FROM agents WHERE id = ?;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(g_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return PARTDATA_ERR_DB_QUERY_FAILED;
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
        if (text) strncpy(record->type, text, sizeof(record->type) - 1);
        text = (const char*)sqlite3_column_text(stmt, 3);
        if (text) strncpy(record->version, text, sizeof(record->version) - 1);
        text = (const char*)sqlite3_column_text(stmt, 4);
        if (text) strncpy(record->status, text, sizeof(record->status) - 1);
        text = (const char*)sqlite3_column_text(stmt, 5);
        if (text) strncpy(record->config_path, text, sizeof(record->config_path) - 1);
        record->created_at = sqlite3_column_int64(stmt, 6);
        record->updated_at = sqlite3_column_int64(stmt, 7);
        sqlite3_finalize(stmt);
        return PARTDATA_SUCCESS;
    }

    sqlite3_finalize(stmt);
    return PARTDATA_ERR_NOT_FOUND;
}

partdata_error_t partdata_registry_update_agent(const partdata_agent_record_t* record) {
    if (!record || !record->id[0]) {
        return PARTDATA_ERR_INVALID_PARAM;
    }

    if (!g_registry.db) {
        return PARTDATA_ERR_NOT_INITIALIZED;
    }

    const char* sql = "UPDATE agents SET name = ?, type = ?, version = ?, status = ?, config_path = ?, updated_at = ? WHERE id = ?;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(g_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return PARTDATA_ERR_DB_QUERY_FAILED;
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

    if (rc != SQLITE_DONE) {
        return PARTDATA_ERR_DB_QUERY_FAILED;
    }

    return PARTDATA_SUCCESS;
}

partdata_error_t partdata_registry_delete_agent(const char* id) {
    if (!id) {
        return PARTDATA_ERR_INVALID_PARAM;
    }

    if (!g_registry.db) {
        return PARTDATA_ERR_NOT_INITIALIZED;
    }

    const char* sql = "DELETE FROM agents WHERE id = ?;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(g_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return PARTDATA_ERR_DB_QUERY_FAILED;
    }

    sqlite3_bind_text(stmt, 1, id, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return PARTDATA_ERR_DB_QUERY_FAILED;
    }

    return PARTDATA_SUCCESS;
}

partdata_error_t partdata_registry_add_skill(const partdata_skill_record_t* record) {
    if (!record || !record->id[0]) {
        return PARTDATA_ERR_INVALID_PARAM;
    }

    if (!g_registry.db) {
        return PARTDATA_ERR_NOT_INITIALIZED;
    }

    const char* sql = "INSERT INTO skills (id, name, version, library_path, manifest_path, installed_at) VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(g_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return PARTDATA_ERR_DB_QUERY_FAILED;
    }

    sqlite3_bind_text(stmt, 1, record->id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, record->name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, record->version, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, record->library_path, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, record->manifest_path, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 6, record->installed_at);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return PARTDATA_ERR_DB_QUERY_FAILED;
    }

    return PARTDATA_SUCCESS;
}

partdata_error_t partdata_registry_get_skill(const char* id, partdata_skill_record_t* record) {
    if (!id || !record) {
        return PARTDATA_ERR_INVALID_PARAM;
    }

    if (!g_registry.db) {
        return PARTDATA_ERR_NOT_INITIALIZED;
    }

    const char* sql = "SELECT id, name, version, library_path, manifest_path, installed_at FROM skills WHERE id = ?;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(g_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return PARTDATA_ERR_DB_QUERY_FAILED;
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
        return PARTDATA_SUCCESS;
    }

    sqlite3_finalize(stmt);
    return PARTDATA_ERR_NOT_FOUND;
}

partdata_error_t partdata_registry_delete_skill(const char* id) {
    if (!id) {
        return PARTDATA_ERR_INVALID_PARAM;
    }

    if (!g_registry.db) {
        return PARTDATA_ERR_NOT_INITIALIZED;
    }

    const char* sql = "DELETE FROM skills WHERE id = ?;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(g_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return PARTDATA_ERR_DB_QUERY_FAILED;
    }

    sqlite3_bind_text(stmt, 1, id, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return PARTDATA_ERR_DB_QUERY_FAILED;
    }

    return PARTDATA_SUCCESS;
}

partdata_error_t partdata_registry_add_session(const partdata_session_record_t* record) {
    if (!record || !record->id[0]) {
        return PARTDATA_ERR_INVALID_PARAM;
    }

    if (!g_registry.db) {
        return PARTDATA_ERR_NOT_INITIALIZED;
    }

    const char* sql = "INSERT INTO sessions (id, user_id, created_at, last_active_at, ttl_seconds, status) VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(g_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return PARTDATA_ERR_DB_QUERY_FAILED;
    }

    sqlite3_bind_text(stmt, 1, record->id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, record->user_id, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, record->created_at);
    sqlite3_bind_int64(stmt, 4, record->last_active_at);
    sqlite3_bind_int(stmt, 5, record->ttl_seconds);
    sqlite3_bind_text(stmt, 6, record->status, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return PARTDATA_ERR_DB_QUERY_FAILED;
    }

    return PARTDATA_SUCCESS;
}

partdata_error_t partdata_registry_get_session(const char* id, partdata_session_record_t* record) {
    if (!id || !record) {
        return PARTDATA_ERR_INVALID_PARAM;
    }

    if (!g_registry.db) {
        return PARTDATA_ERR_NOT_INITIALIZED;
    }

    const char* sql = "SELECT id, user_id, created_at, last_active_at, ttl_seconds, status FROM sessions WHERE id = ?;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(g_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return PARTDATA_ERR_DB_QUERY_FAILED;
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
        return PARTDATA_SUCCESS;
    }

    sqlite3_finalize(stmt);
    return PARTDATA_ERR_NOT_FOUND;
}

partdata_error_t partdata_registry_update_session(const partdata_session_record_t* record) {
    if (!record || !record->id[0]) {
        return PARTDATA_ERR_INVALID_PARAM;
    }

    if (!g_registry.db) {
        return PARTDATA_ERR_NOT_INITIALIZED;
    }

    const char* sql = "UPDATE sessions SET user_id = ?, last_active_at = ?, ttl_seconds = ?, status = ? WHERE id = ?;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(g_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return PARTDATA_ERR_DB_QUERY_FAILED;
    }

    sqlite3_bind_text(stmt, 1, record->user_id, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, record->last_active_at);
    sqlite3_bind_int(stmt, 3, record->ttl_seconds);
    sqlite3_bind_text(stmt, 4, record->status, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, record->id, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return PARTDATA_ERR_DB_QUERY_FAILED;
    }

    return PARTDATA_SUCCESS;
}

partdata_error_t partdata_registry_delete_session(const char* id) {
    if (!id) {
        return PARTDATA_ERR_INVALID_PARAM;
    }

    if (!g_registry.db) {
        return PARTDATA_ERR_NOT_INITIALIZED;
    }

    const char* sql = "DELETE FROM sessions WHERE id = ?;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(g_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return PARTDATA_ERR_DB_QUERY_FAILED;
    }

    sqlite3_bind_text(stmt, 1, id, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return PARTDATA_ERR_DB_QUERY_FAILED;
    }

    return PARTDATA_SUCCESS;
}

partdata_error_t partdata_registry_query_agents(const char* filter_type, const char* filter_status, partdata_registry_iter_t** iter) {
    (void)filter_type;
    (void)filter_status;
    (void)iter;
    return PARTDATA_ERR_NOT_INITIALIZED;
}

partdata_error_t partdata_registry_query_skills(partdata_registry_iter_t** iter) {
    (void)iter;
    return PARTDATA_ERR_NOT_INITIALIZED;
}

partdata_error_t partdata_registry_iter_next(partdata_registry_iter_t* iter, void* record) {
    (void)iter;
    (void)record;
    return PARTDATA_ERR_NOT_FOUND;
}

void partdata_registry_iter_destroy(partdata_registry_iter_t* iter) {
    (void)iter;
}

partdata_error_t partdata_registry_vacuum(void) {
    if (!g_registry.db) {
        return PARTDATA_ERR_NOT_INITIALIZED;
    }

    sqlite3_exec(g_registry.db, "VACUUM;", NULL, NULL, NULL);
    return PARTDATA_SUCCESS;
}

#endif

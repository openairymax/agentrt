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

#include "../include/utils.h"
#include "platform.h"
#include "private.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "memory_compat.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifdef AGENTOS_HAS_SQLITE3
#define heapstore_SQLITE_IMPLEMENTATION
#endif

#ifndef heapstore_SQLITE_IMPLEMENTATION

typedef struct registry_node {
    void *data;
    size_t data_size;
    struct registry_node *next;
} registry_node_t;

typedef struct {
    registry_node_t *agents;
    registry_node_t *skills;
    registry_node_t *sessions;
    size_t agent_count;
    size_t skill_count;
    size_t session_count;
    int initialized;
} registry_db_t;

struct heapstore_registry_iter {
    registry_node_t *current;
    int type;
};

static registry_db_t s_registry = {0};

static registry_node_t *find_node_by_id(registry_node_t *head, const char *id, size_t id_offset)
{
    registry_node_t *node = head;
    while (node) {
        if (node->data) {
            const char *node_id = (const char *)((char *)node->data + id_offset);
            if (node_id && strcmp(node_id, id) == 0) {
                return node;
            }
        }
        node = node->next;
    }
    return NULL;
}

static void free_node_list(registry_node_t **head)
{
    registry_node_t *node = *head;
    while (node) {
        registry_node_t *next = node->next;
        AGENTOS_FREE(node->data);
        AGENTOS_FREE(node);
        node = next;
    }
    *head = NULL;
}

heapstore_error_t heapstore_registry_init(void)
{
    __builtin_memset(&s_registry, 0, sizeof(s_registry));
    s_registry.initialized = 1;
    return heapstore_SUCCESS;
}

void heapstore_registry_shutdown(void)
{
    free_node_list(&s_registry.agents);
    free_node_list(&s_registry.skills);
    free_node_list(&s_registry.sessions);
    __builtin_memset(&s_registry, 0, sizeof(s_registry));
}

heapstore_error_t heapstore_registry_add_agent(const heapstore_agent_record_t *record)
{
    if (!record || record->id[0] == '\0')
        return heapstore_ERR_INVALID_PARAM;
    if (!s_registry.initialized)
        return heapstore_ERR_NOT_INITIALIZED;
    if (find_node_by_id(s_registry.agents, record->id, offsetof(heapstore_agent_record_t, id))) {
        return heapstore_ERR_ALREADY_INITIALIZED;
    }
    registry_node_t *node = AGENTOS_CALLOC(1, sizeof(registry_node_t));
    if (!node)
        return heapstore_ERR_OUT_OF_MEMORY;
    node->data = AGENTOS_MALLOC(sizeof(heapstore_agent_record_t));
    if (!node->data) {
        AGENTOS_FREE(node);
        return heapstore_ERR_OUT_OF_MEMORY;
    }
    __builtin_memcpy(node->data, record, sizeof(heapstore_agent_record_t));
    node->data_size = sizeof(heapstore_agent_record_t);
    node->next = s_registry.agents;
    s_registry.agents = node;
    s_registry.agent_count++;
    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_registry_get_agent(const char *id, heapstore_agent_record_t *record)
{
    if (!id || !record)
        return heapstore_ERR_INVALID_PARAM;
    if (!s_registry.initialized)
        return heapstore_ERR_NOT_INITIALIZED;
    registry_node_t *node =
        find_node_by_id(s_registry.agents, id, offsetof(heapstore_agent_record_t, id));
    if (!node)
        return heapstore_ERR_NOT_FOUND;
    __builtin_memcpy(record, node->data, sizeof(heapstore_agent_record_t));
    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_registry_update_agent(const heapstore_agent_record_t *record)
{
    if (!record || record->id[0] == '\0')
        return heapstore_ERR_INVALID_PARAM;
    if (!s_registry.initialized)
        return heapstore_ERR_NOT_INITIALIZED;
    registry_node_t *node =
        find_node_by_id(s_registry.agents, record->id, offsetof(heapstore_agent_record_t, id));
    if (!node)
        return heapstore_ERR_NOT_FOUND;
    __builtin_memcpy(node->data, record, sizeof(heapstore_agent_record_t));
    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_registry_delete_agent(const char *id)
{
    if (!id)
        return heapstore_ERR_INVALID_PARAM;
    if (!s_registry.initialized)
        return heapstore_ERR_NOT_INITIALIZED;
    registry_node_t **pp = &s_registry.agents;
    while (*pp) {
        const char *node_id =
            (const char *)((char *)(*pp)->data + offsetof(heapstore_agent_record_t, id));
        if (strcmp(node_id, id) == 0) {
            registry_node_t *victim = *pp;
            *pp = victim->next;
            AGENTOS_FREE(victim->data);
            AGENTOS_FREE(victim);
            s_registry.agent_count--;
            return heapstore_SUCCESS;
        }
        pp = &(*pp)->next;
    }
    return heapstore_ERR_NOT_FOUND;
}

heapstore_error_t heapstore_registry_query_agents(const char *filter_type,
                                                  const char *filter_status,
                                                  heapstore_registry_iter_t **iter)
{
    if (!iter)
        return heapstore_ERR_INVALID_PARAM;
    if (!s_registry.initialized)
        return heapstore_ERR_NOT_INITIALIZED;
    heapstore_registry_iter_t *it = AGENTOS_CALLOC(1, sizeof(heapstore_registry_iter_t));
    if (!it)
        return heapstore_ERR_OUT_OF_MEMORY;
    it->current = s_registry.agents;
    it->type = 0;
    *iter = it;
    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_registry_add_skill(const heapstore_skill_record_t *record)
{
    if (!record || record->id[0] == '\0')
        return heapstore_ERR_INVALID_PARAM;
    if (!s_registry.initialized)
        return heapstore_ERR_NOT_INITIALIZED;
    if (find_node_by_id(s_registry.skills, record->id, offsetof(heapstore_skill_record_t, id))) {
        return heapstore_ERR_ALREADY_INITIALIZED;
    }
    registry_node_t *node = AGENTOS_CALLOC(1, sizeof(registry_node_t));
    if (!node)
        return heapstore_ERR_OUT_OF_MEMORY;
    node->data = AGENTOS_MALLOC(sizeof(heapstore_skill_record_t));
    if (!node->data) {
        AGENTOS_FREE(node);
        return heapstore_ERR_OUT_OF_MEMORY;
    }
    __builtin_memcpy(node->data, record, sizeof(heapstore_skill_record_t));
    node->data_size = sizeof(heapstore_skill_record_t);
    node->next = s_registry.skills;
    s_registry.skills = node;
    s_registry.skill_count++;
    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_registry_get_skill(const char *id, heapstore_skill_record_t *record)
{
    if (!id || !record)
        return heapstore_ERR_INVALID_PARAM;
    if (!s_registry.initialized)
        return heapstore_ERR_NOT_INITIALIZED;
    registry_node_t *node =
        find_node_by_id(s_registry.skills, id, offsetof(heapstore_skill_record_t, id));
    if (!node)
        return heapstore_ERR_NOT_FOUND;
    __builtin_memcpy(record, node->data, sizeof(heapstore_skill_record_t));
    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_registry_delete_skill(const char *id)
{
    if (!id)
        return heapstore_ERR_INVALID_PARAM;
    if (!s_registry.initialized)
        return heapstore_ERR_NOT_INITIALIZED;
    registry_node_t **pp = &s_registry.skills;
    while (*pp) {
        const char *node_id =
            (const char *)((char *)(*pp)->data + offsetof(heapstore_skill_record_t, id));
        if (strcmp(node_id, id) == 0) {
            registry_node_t *victim = *pp;
            *pp = victim->next;
            AGENTOS_FREE(victim->data);
            AGENTOS_FREE(victim);
            s_registry.skill_count--;
            return heapstore_SUCCESS;
        }
        pp = &(*pp)->next;
    }
    return heapstore_ERR_NOT_FOUND;
}

heapstore_error_t heapstore_registry_query_skills(heapstore_registry_iter_t **iter)
{
    if (!iter)
        return heapstore_ERR_INVALID_PARAM;
    if (!s_registry.initialized)
        return heapstore_ERR_NOT_INITIALIZED;
    heapstore_registry_iter_t *it = AGENTOS_CALLOC(1, sizeof(heapstore_registry_iter_t));
    if (!it)
        return heapstore_ERR_OUT_OF_MEMORY;
    it->current = s_registry.skills;
    it->type = 1;
    *iter = it;
    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_registry_add_session(const heapstore_session_record_t *record)
{
    if (!record || record->id[0] == '\0')
        return heapstore_ERR_INVALID_PARAM;
    if (!s_registry.initialized)
        return heapstore_ERR_NOT_INITIALIZED;
    if (find_node_by_id(s_registry.sessions, record->id,
                        offsetof(heapstore_session_record_t, id))) {
        return heapstore_ERR_ALREADY_INITIALIZED;
    }
    registry_node_t *node = AGENTOS_CALLOC(1, sizeof(registry_node_t));
    if (!node)
        return heapstore_ERR_OUT_OF_MEMORY;
    node->data = AGENTOS_MALLOC(sizeof(heapstore_session_record_t));
    if (!node->data) {
        AGENTOS_FREE(node);
        return heapstore_ERR_OUT_OF_MEMORY;
    }
    __builtin_memcpy(node->data, record, sizeof(heapstore_session_record_t));
    node->data_size = sizeof(heapstore_session_record_t);
    node->next = s_registry.sessions;
    s_registry.sessions = node;
    s_registry.session_count++;
    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_registry_get_session(const char *id, heapstore_session_record_t *record)
{
    if (!id || !record)
        return heapstore_ERR_INVALID_PARAM;
    if (!s_registry.initialized)
        return heapstore_ERR_NOT_INITIALIZED;
    registry_node_t *node =
        find_node_by_id(s_registry.sessions, id, offsetof(heapstore_session_record_t, id));
    if (!node)
        return heapstore_ERR_NOT_FOUND;
    __builtin_memcpy(record, node->data, sizeof(heapstore_session_record_t));
    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_registry_update_session(const heapstore_session_record_t *record)
{
    if (!record || record->id[0] == '\0')
        return heapstore_ERR_INVALID_PARAM;
    if (!s_registry.initialized)
        return heapstore_ERR_NOT_INITIALIZED;
    registry_node_t *node =
        find_node_by_id(s_registry.sessions, record->id, offsetof(heapstore_session_record_t, id));
    if (!node)
        return heapstore_ERR_NOT_FOUND;
    __builtin_memcpy(node->data, record, sizeof(heapstore_session_record_t));
    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_registry_delete_session(const char *id)
{
    if (!id)
        return heapstore_ERR_INVALID_PARAM;
    if (!s_registry.initialized)
        return heapstore_ERR_NOT_INITIALIZED;
    registry_node_t **pp = &s_registry.sessions;
    while (*pp) {
        const char *node_id =
            (const char *)((char *)(*pp)->data + offsetof(heapstore_session_record_t, id));
        if (strcmp(node_id, id) == 0) {
            registry_node_t *victim = *pp;
            *pp = victim->next;
            AGENTOS_FREE(victim->data);
            AGENTOS_FREE(victim);
            s_registry.session_count--;
            return heapstore_SUCCESS;
        }
        pp = &(*pp)->next;
    }
    return heapstore_ERR_NOT_FOUND;
}

heapstore_error_t heapstore_registry_query_sessions(const char *filter_status,
                                                    heapstore_registry_iter_t **iter)
{
    if (!iter)
        return heapstore_ERR_INVALID_PARAM;
    if (!s_registry.initialized)
        return heapstore_ERR_NOT_INITIALIZED;
    heapstore_registry_iter_t *it = AGENTOS_CALLOC(1, sizeof(heapstore_registry_iter_t));
    if (!it)
        return heapstore_ERR_OUT_OF_MEMORY;
    it->current = s_registry.sessions;
    it->type = 2;
    *iter = it;
    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_registry_iter_next(heapstore_registry_iter_t *iter, void *record)
{
    if (!iter || !record)
        return heapstore_ERR_INVALID_PARAM;
    if (!iter->current)
        return heapstore_ERR_NOT_FOUND;
    __builtin_memcpy(record, iter->current->data, iter->current->data_size);
    iter->current = iter->current->next;
    return heapstore_SUCCESS;
}

void heapstore_registry_iter_destroy(heapstore_registry_iter_t *iter)
{
    AGENTOS_FREE(iter);
}

heapstore_error_t heapstore_registry_vacuum(void)
{
    if (!s_registry.initialized)
        return heapstore_ERR_NOT_INITIALIZED;
    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_registry_batch_insert_agents(const heapstore_agent_record_t *records,
                                                         size_t count)
{
    if (!records || count == 0)
        return heapstore_ERR_INVALID_PARAM;
    if (!s_registry.initialized)
        return heapstore_ERR_NOT_INITIALIZED;
    for (size_t i = 0; i < count; i++) {
        heapstore_error_t err = heapstore_registry_add_agent(&records[i]);
        if (err != heapstore_SUCCESS)
            return err;
    }
    return heapstore_SUCCESS;
}

heapstore_error_t
heapstore_registry_batch_insert_sessions(const heapstore_session_record_t *records, size_t count)
{
    if (!records || count == 0)
        return heapstore_ERR_INVALID_PARAM;
    if (!s_registry.initialized)
        return heapstore_ERR_NOT_INITIALIZED;
    for (size_t i = 0; i < count; i++) {
        heapstore_error_t err = heapstore_registry_add_session(&records[i]);
        if (err != heapstore_SUCCESS)
            return err;
    }
    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_registry_batch_insert_skills(const heapstore_skill_record_t *records,
                                                         size_t count)
{
    if (!records || count == 0)
        return heapstore_ERR_INVALID_PARAM;
    if (!s_registry.initialized)
        return heapstore_ERR_NOT_INITIALIZED;
    for (size_t i = 0; i < count; i++) {
        heapstore_error_t err = heapstore_registry_add_skill(&records[i]);
        if (err != heapstore_SUCCESS)
            return err;
    }
    return heapstore_SUCCESS;
}

bool heapstore_registry_is_healthy(void)
{
    return s_registry.initialized != 0;
}

#else

#ifdef AGENTOS_HAS_SQLITE3
#include <sqlite3.h>

typedef struct {
    sqlite3 *db;
    char db_path[512];
    agentos_mutex_t lock;
    int initialized;
} registry_db_t;

/**
 * @brief 注册表迭代器内部结构
 */
struct heapstore_registry_iter {
    sqlite3_stmt *stmt; /* SQLite 预编译语句 */
    int current_type;   /* 当前遍历的类型 (agents/skills/sessions) */
    int has_more;       /* 是否还有更多记录 */
};

static registry_db_t s_registry = {0};

static heapstore_error_t init_database(sqlite3 *db)
{
    const char *sql = "CREATE TABLE IF NOT EXISTS agents ("
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

    char *err_msg = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        if (err_msg) {
            sqlite3_free(err_msg);
        }
        return heapstore_ERR_DB_INIT_FAILED;
    }
    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_registry_init(void)
{
    if (s_registry.initialized) {
        return heapstore_SUCCESS;
    }

    const char *configured_root = heapstore_get_root();
    char root_path[256];
    if (configured_root && configured_root[0] != '\0') {
        AGENTOS_STRNCPY_TERM(root_path, configured_root, sizeof(root_path));
    } else {
        const char *tmpdir = getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp";
        snprintf(root_path, sizeof(root_path), "%s/agentos/heapstore", tmpdir);
    }

    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s/registry", root_path);

    heapstore_ensure_directory(full_path);

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
        __builtin_memset(&s_registry, 0, sizeof(s_registry));
        return err;
    }

    agentos_mutex_init(&s_registry.lock);
    s_registry.initialized = true;

    return heapstore_SUCCESS;
}

void heapstore_registry_shutdown(void)
{
    if (!s_registry.initialized) {
        return;
    }

    agentos_mutex_lock(&s_registry.lock);

    if (s_registry.db) {
        sqlite3_close(s_registry.db);
        s_registry.db = NULL;
    }

    s_registry.initialized = false;
    agentos_mutex_unlock(&s_registry.lock);
    agentos_mutex_destroy(&s_registry.lock);
}

static heapstore_error_t
execute_sql_with_lock(const char *sql, heapstore_error_t (*bind_func)(sqlite3_stmt *, void *),
                      void *bind_data)
{
    if (!s_registry.initialized || !s_registry.db) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    agentos_mutex_lock(&s_registry.lock);

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(s_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        agentos_mutex_unlock(&s_registry.lock);
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    if (bind_func) {
        heapstore_error_t err = bind_func(stmt, bind_data);
        if (err != heapstore_SUCCESS) {
            sqlite3_finalize(stmt);
            agentos_mutex_unlock(&s_registry.lock);
            return err;
        }
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    agentos_mutex_unlock(&s_registry.lock);

    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    return heapstore_SUCCESS;
}

static heapstore_error_t bind_agent_record(sqlite3_stmt *stmt, void *data)
{
    const heapstore_agent_record_t *record = (const heapstore_agent_record_t *)data;

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

static heapstore_error_t __attribute__((unused)) bind_agent_id(sqlite3_stmt *stmt, void *data)
{
    const char *agent_id = (const char *)data;
    sqlite3_bind_text(stmt, 1, agent_id, -1, SQLITE_STATIC);
    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_registry_add_agent(const heapstore_agent_record_t *record)
{
    if (!record || !record->id[0]) {
        return heapstore_ERR_INVALID_PARAM;
    }

    const char *sql = "INSERT INTO agents "
                      "(id, name, type, version, status, config_path, created_at, updated_at) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?);";

    return execute_sql_with_lock(sql, bind_agent_record, (void *)record);
}

heapstore_error_t heapstore_registry_get_agent(const char *id, heapstore_agent_record_t *record)
{
    if (!id || !record) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (!s_registry.initialized || !s_registry.db) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    agentos_mutex_lock(&s_registry.lock);

    const char *sql = "SELECT id, name, type, version, status, config_path, created_at, updated_at "
                      "FROM agents WHERE id = ?;";
    sqlite3_stmt *stmt;

    int rc = sqlite3_prepare_v2(s_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        agentos_mutex_unlock(&s_registry.lock);
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    sqlite3_bind_text(stmt, 1, id, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const char *text;
        text = (const char *)sqlite3_column_text(stmt, 0);
        if (text) {
            AGENTOS_STRNCPY_TERM(record->id, text, sizeof(record->id));
        } else
            record->id[0] = '\0';

        text = (const char *)sqlite3_column_text(stmt, 1);
        if (text) {
            AGENTOS_STRNCPY_TERM(record->name, text, sizeof(record->name));
        } else
            record->name[0] = '\0';

        text = (const char *)sqlite3_column_text(stmt, 2);
        if (text) {
            AGENTOS_STRNCPY_TERM(record->type, text, sizeof(record->type));
        } else
            record->type[0] = '\0';

        text = (const char *)sqlite3_column_text(stmt, 3);
        if (text) {
            AGENTOS_STRNCPY_TERM(record->version, text, sizeof(record->version));
        } else
            record->version[0] = '\0';

        text = (const char *)sqlite3_column_text(stmt, 4);
        if (text) {
            AGENTOS_STRNCPY_TERM(record->status, text, sizeof(record->status));
        } else
            record->status[0] = '\0';

        text = (const char *)sqlite3_column_text(stmt, 5);
        if (text) {
            AGENTOS_STRNCPY_TERM(record->config_path, text, sizeof(record->config_path));
        } else
            record->config_path[0] = '\0';

        record->created_at = sqlite3_column_int64(stmt, 6);
        record->updated_at = sqlite3_column_int64(stmt, 7);

        sqlite3_finalize(stmt);
        agentos_mutex_unlock(&s_registry.lock);
        return heapstore_SUCCESS;
    }

    sqlite3_finalize(stmt);
    agentos_mutex_unlock(&s_registry.lock);
    return heapstore_ERR_NOT_FOUND;
}

heapstore_error_t heapstore_registry_update_agent(const heapstore_agent_record_t *record)
{
    if (!record || !record->id[0]) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (!s_registry.initialized || !s_registry.db) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    agentos_mutex_lock(&s_registry.lock);

    const char *sql =
        "UPDATE agents SET "
        "name = ?, type = ?, version = ?, status = ?, config_path = ?, updated_at = ? "
        "WHERE id = ?;";
    sqlite3_stmt *stmt;

    int rc = sqlite3_prepare_v2(s_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        agentos_mutex_unlock(&s_registry.lock);
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
    agentos_mutex_unlock(&s_registry.lock);

    if (rc != SQLITE_DONE) {
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_registry_delete_agent(const char *id)
{
    if (!id) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (!s_registry.initialized || !s_registry.db) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    agentos_mutex_lock(&s_registry.lock);

    const char *sql = "DELETE FROM agents WHERE id = ?;";
    sqlite3_stmt *stmt;

    int rc = sqlite3_prepare_v2(s_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        agentos_mutex_unlock(&s_registry.lock);
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    sqlite3_bind_text(stmt, 1, id, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    agentos_mutex_unlock(&s_registry.lock);

    if (rc != SQLITE_DONE) {
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_registry_query_agents(const char *filter_type,
                                                  const char *filter_status,
                                                  heapstore_registry_iter_t **iter)
{
    if (!iter)
        return heapstore_ERR_INVALID_PARAM;
    if (!s_registry.initialized || !s_registry.db)
        return heapstore_ERR_NOT_INITIALIZED;

    char sql[512];
    snprintf(sql, sizeof(sql),
             "SELECT id, name, type, version, status, config_path, created_at, updated_at FROM "
             "agents WHERE 1=1");
    if (filter_type) {
        size_t pos = strlen(sql);
        snprintf(sql + pos, sizeof(sql) - pos, " AND type = ?");
    }
    if (filter_status) {
        size_t pos = strlen(sql);
        snprintf(sql + pos, sizeof(sql) - pos, " AND status = ?");
    }

    agentos_mutex_lock(&s_registry.lock);

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(s_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        agentos_mutex_unlock(&s_registry.lock);
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    int param_idx = 1;
    if (filter_type) {
        sqlite3_bind_text(stmt, param_idx++, filter_type, -1, SQLITE_STATIC);
    }
    if (filter_status) {
        sqlite3_bind_text(stmt, param_idx++, filter_status, -1, SQLITE_STATIC);
    }

    heapstore_registry_iter_t *new_iter =
        (heapstore_registry_iter_t *)AGENTOS_CALLOC(1, sizeof(heapstore_registry_iter_t));
    if (!new_iter) {
        sqlite3_finalize(stmt);
        agentos_mutex_unlock(&s_registry.lock);
        return heapstore_ERR_OUT_OF_MEMORY;
    }
    new_iter->stmt = stmt;
    new_iter->current_type = 0;
    new_iter->has_more = 1;

    *iter = new_iter;
    agentos_mutex_unlock(&s_registry.lock);
    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_registry_add_skill(const heapstore_skill_record_t *record)
{
    if (!record || !record->id[0]) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (!s_registry.initialized || !s_registry.db) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    agentos_mutex_lock(&s_registry.lock);

    const char *sql = "INSERT INTO skills "
                      "(id, name, version, library_path, manifest_path, installed_at) "
                      "VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt *stmt;

    int rc = sqlite3_prepare_v2(s_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        agentos_mutex_unlock(&s_registry.lock);
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
    agentos_mutex_unlock(&s_registry.lock);

    if (rc != SQLITE_DONE) {
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_registry_get_skill(const char *id, heapstore_skill_record_t *record)
{
    if (!id || !record) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (!s_registry.initialized || !s_registry.db) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    agentos_mutex_lock(&s_registry.lock);

    const char *sql = "SELECT id, name, version, library_path, manifest_path, installed_at FROM "
                      "skills WHERE id = ?;";
    sqlite3_stmt *stmt;

    int rc = sqlite3_prepare_v2(s_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        agentos_mutex_unlock(&s_registry.lock);
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    sqlite3_bind_text(stmt, 1, id, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const char *text;
        text = (const char *)sqlite3_column_text(stmt, 0);
        if (text) {
            AGENTOS_STRNCPY_TERM(record->id, text, sizeof(record->id));
        }
        text = (const char *)sqlite3_column_text(stmt, 1);
        if (text) {
            AGENTOS_STRNCPY_TERM(record->name, text, sizeof(record->name));
        }
        text = (const char *)sqlite3_column_text(stmt, 2);
        if (text) {
            AGENTOS_STRNCPY_TERM(record->version, text, sizeof(record->version));
        }
        text = (const char *)sqlite3_column_text(stmt, 3);
        if (text) {
            AGENTOS_STRNCPY_TERM(record->library_path, text, sizeof(record->library_path));
        }
        text = (const char *)sqlite3_column_text(stmt, 4);
        if (text) {
            AGENTOS_STRNCPY_TERM(record->manifest_path, text, sizeof(record->manifest_path));
        }
        record->installed_at = sqlite3_column_int64(stmt, 5);
        sqlite3_finalize(stmt);
        agentos_mutex_unlock(&s_registry.lock);
        return heapstore_SUCCESS;
    }

    sqlite3_finalize(stmt);
    agentos_mutex_unlock(&s_registry.lock);
    return heapstore_ERR_NOT_FOUND;
}

heapstore_error_t heapstore_registry_delete_skill(const char *id)
{
    if (!id) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (!s_registry.initialized || !s_registry.db) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    agentos_mutex_lock(&s_registry.lock);

    const char *sql = "DELETE FROM skills WHERE id = ?;";
    sqlite3_stmt *stmt;

    int rc = sqlite3_prepare_v2(s_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        agentos_mutex_unlock(&s_registry.lock);
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    sqlite3_bind_text(stmt, 1, id, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    agentos_mutex_unlock(&s_registry.lock);

    if (rc != SQLITE_DONE) {
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_registry_add_session(const heapstore_session_record_t *record)
{
    if (!record || !record->id[0]) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (!s_registry.initialized || !s_registry.db) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    agentos_mutex_lock(&s_registry.lock);

    const char *sql = "INSERT INTO sessions "
                      "(id, user_id, created_at, last_active_at, ttl_seconds, status) "
                      "VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt *stmt;

    int rc = sqlite3_prepare_v2(s_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        agentos_mutex_unlock(&s_registry.lock);
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
    agentos_mutex_unlock(&s_registry.lock);

    if (rc != SQLITE_DONE) {
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_registry_get_session(const char *id, heapstore_session_record_t *record)
{
    if (!id || !record) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (!s_registry.initialized || !s_registry.db) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    agentos_mutex_lock(&s_registry.lock);

    const char *sql = "SELECT id, user_id, created_at, last_active_at, ttl_seconds, status FROM "
                      "sessions WHERE id = ?;";
    sqlite3_stmt *stmt;

    int rc = sqlite3_prepare_v2(s_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        agentos_mutex_unlock(&s_registry.lock);
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    sqlite3_bind_text(stmt, 1, id, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const char *text;
        text = (const char *)sqlite3_column_text(stmt, 0);
        if (text) {
            AGENTOS_STRNCPY_TERM(record->id, text, sizeof(record->id));
        }
        text = (const char *)sqlite3_column_text(stmt, 1);
        if (text) {
            AGENTOS_STRNCPY_TERM(record->user_id, text, sizeof(record->user_id));
        }
        record->created_at = sqlite3_column_int64(stmt, 2);
        record->last_active_at = sqlite3_column_int64(stmt, 3);
        record->ttl_seconds = sqlite3_column_int(stmt, 4);
        text = (const char *)sqlite3_column_text(stmt, 5);
        if (text) {
            AGENTOS_STRNCPY_TERM(record->status, text, sizeof(record->status));
        }
        sqlite3_finalize(stmt);
        agentos_mutex_unlock(&s_registry.lock);
        return heapstore_SUCCESS;
    }

    sqlite3_finalize(stmt);
    agentos_mutex_unlock(&s_registry.lock);
    return heapstore_ERR_NOT_FOUND;
}

heapstore_error_t heapstore_registry_update_session(const heapstore_session_record_t *record)
{
    if (!record || !record->id[0]) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (!s_registry.initialized || !s_registry.db) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    agentos_mutex_lock(&s_registry.lock);

    const char *sql = "UPDATE sessions SET user_id = ?, last_active_at = ?, ttl_seconds = ?, "
                      "status = ? WHERE id = ?;";
    sqlite3_stmt *stmt;

    int rc = sqlite3_prepare_v2(s_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        agentos_mutex_unlock(&s_registry.lock);
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    sqlite3_bind_text(stmt, 1, record->user_id, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, record->last_active_at);
    sqlite3_bind_int(stmt, 3, record->ttl_seconds);
    sqlite3_bind_text(stmt, 4, record->status, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, record->id, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    agentos_mutex_unlock(&s_registry.lock);

    if (rc != SQLITE_DONE) {
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_registry_delete_session(const char *id)
{
    if (!id) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (!s_registry.initialized || !s_registry.db) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    agentos_mutex_lock(&s_registry.lock);

    const char *sql = "DELETE FROM sessions WHERE id = ?;";
    sqlite3_stmt *stmt;

    int rc = sqlite3_prepare_v2(s_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        agentos_mutex_unlock(&s_registry.lock);
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    sqlite3_bind_text(stmt, 1, id, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    agentos_mutex_unlock(&s_registry.lock);

    if (rc != SQLITE_DONE) {
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_registry_query_skills(heapstore_registry_iter_t **iter)
{
    if (!iter) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (!s_registry.initialized || !s_registry.db) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    agentos_mutex_lock(&s_registry.lock);

    const char *sql = "SELECT id, name, version, library_path, manifest_path, installed_at FROM "
                      "skills ORDER BY installed_at DESC;";
    sqlite3_stmt *stmt;

    int rc = sqlite3_prepare_v2(s_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        agentos_mutex_unlock(&s_registry.lock);
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    heapstore_registry_iter_t *new_iter =
        (heapstore_registry_iter_t *)AGENTOS_MALLOC(sizeof(heapstore_registry_iter_t));
    if (!new_iter) {
        sqlite3_finalize(stmt);
        agentos_mutex_unlock(&s_registry.lock);
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    new_iter->stmt = stmt;
    new_iter->current_type = 1; /* skills */
    new_iter->has_more = 1;

    *iter = new_iter;
    agentos_mutex_unlock(&s_registry.lock);

    return heapstore_SUCCESS;
}

/**
 * @brief 查询会话记录
 *
 * @param filter_status [in] 按状态过滤（NULL 表示不过滤）
 * @param iter [out] 输出迭代器
 * @return heapstore_error_t 错误码
 */
heapstore_error_t heapstore_registry_query_sessions(const char *filter_status,
                                                    heapstore_registry_iter_t **iter)
{
    if (!iter) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (!s_registry.initialized || !s_registry.db) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    agentos_mutex_lock(&s_registry.lock);

    const char *sql;
    sqlite3_stmt *stmt;

    if (filter_status && filter_status[0]) {
        sql = "SELECT id, user_id, created_at, last_active_at, ttl_seconds, status FROM sessions "
              "WHERE status = ? ORDER BY last_active_at DESC;";
    } else {
        sql = "SELECT id, user_id, created_at, last_active_at, ttl_seconds, status FROM sessions "
              "ORDER BY last_active_at DESC;";
    }

    int rc = sqlite3_prepare_v2(s_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        agentos_mutex_unlock(&s_registry.lock);
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    if (filter_status && filter_status[0]) {
        sqlite3_bind_text(stmt, 1, filter_status, -1, SQLITE_STATIC);
    }

    heapstore_registry_iter_t *new_iter =
        (heapstore_registry_iter_t *)AGENTOS_MALLOC(sizeof(heapstore_registry_iter_t));
    if (!new_iter) {
        sqlite3_finalize(stmt);
        agentos_mutex_unlock(&s_registry.lock);
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    new_iter->stmt = stmt;
    new_iter->current_type = 2; /* sessions */
    new_iter->has_more = 1;

    *iter = new_iter;
    agentos_mutex_unlock(&s_registry.lock);

    return heapstore_SUCCESS;
}

/**
 * @brief 遍历下一条记录
 *
 * 根据迭代器的 current_type 自动判断返回哪种类型的记录
 */
heapstore_error_t heapstore_registry_iter_next(heapstore_registry_iter_t *iter, void *record)
{
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

    const char *text;

    switch (iter->current_type) {
    case 0: { /* agents */
        heapstore_agent_record_t *agent_rec = (heapstore_agent_record_t *)record;
        __builtin_memset(agent_rec, 0, sizeof(*agent_rec));

        text = (const char *)sqlite3_column_text(iter->stmt, 0);
        if (text)
            AGENTOS_STRNCPY_TERM(agent_rec->id, text, sizeof(agent_rec->id));
        text = (const char *)sqlite3_column_text(iter->stmt, 1);
        if (text)
            AGENTOS_STRNCPY_TERM(agent_rec->name, text, sizeof(agent_rec->name));
        text = (const char *)sqlite3_column_text(iter->stmt, 2);
        if (text)
            AGENTOS_STRNCPY_TERM(agent_rec->type, text, sizeof(agent_rec->type));
        text = (const char *)sqlite3_column_text(iter->stmt, 3);
        if (text)
            AGENTOS_STRNCPY_TERM(agent_rec->version, text, sizeof(agent_rec->version));
        text = (const char *)sqlite3_column_text(iter->stmt, 4);
        if (text)
            AGENTOS_STRNCPY_TERM(agent_rec->status, text, sizeof(agent_rec->status));
        text = (const char *)sqlite3_column_text(iter->stmt, 5);
        if (text)
            AGENTOS_STRNCPY_TERM(agent_rec->config_path, text, sizeof(agent_rec->config_path));
        agent_rec->created_at = sqlite3_column_int64(iter->stmt, 6);
        agent_rec->updated_at = sqlite3_column_int64(iter->stmt, 7);
        break;
    }
    case 1: { /* skills */
        heapstore_skill_record_t *skill_rec = (heapstore_skill_record_t *)record;
        __builtin_memset(skill_rec, 0, sizeof(*skill_rec));

        text = (const char *)sqlite3_column_text(iter->stmt, 0);
        if (text)
            AGENTOS_STRNCPY_TERM(skill_rec->id, text, sizeof(skill_rec->id));
        text = (const char *)sqlite3_column_text(iter->stmt, 1);
        if (text)
            AGENTOS_STRNCPY_TERM(skill_rec->name, text, sizeof(skill_rec->name));
        text = (const char *)sqlite3_column_text(iter->stmt, 2);
        if (text)
            AGENTOS_STRNCPY_TERM(skill_rec->version, text, sizeof(skill_rec->version));
        text = (const char *)sqlite3_column_text(iter->stmt, 3);
        if (text)
            AGENTOS_STRNCPY_TERM(skill_rec->library_path, text, sizeof(skill_rec->library_path));
        text = (const char *)sqlite3_column_text(iter->stmt, 4);
        if (text)
            AGENTOS_STRNCPY_TERM(skill_rec->manifest_path, text, sizeof(skill_rec->manifest_path));
        skill_rec->installed_at = sqlite3_column_int64(iter->stmt, 5);
        break;
    }
    case 2: { /* sessions */
        heapstore_session_record_t *session_rec = (heapstore_session_record_t *)record;
        __builtin_memset(session_rec, 0, sizeof(*session_rec));

        text = (const char *)sqlite3_column_text(iter->stmt, 0);
        if (text)
            AGENTOS_STRNCPY_TERM(session_rec->id, text, sizeof(session_rec->id));
        text = (const char *)sqlite3_column_text(iter->stmt, 1);
        if (text)
            AGENTOS_STRNCPY_TERM(session_rec->user_id, text, sizeof(session_rec->user_id));
        session_rec->created_at = sqlite3_column_int64(iter->stmt, 2);
        session_rec->last_active_at = sqlite3_column_int64(iter->stmt, 3);
        session_rec->ttl_seconds = sqlite3_column_int(iter->stmt, 4);
        text = (const char *)sqlite3_column_text(iter->stmt, 5);
        if (text)
            AGENTOS_STRNCPY_TERM(session_rec->status, text, sizeof(session_rec->status));
        break;
    }
    default:
        return heapstore_ERR_INVALID_PARAM;
    }

    return heapstore_SUCCESS;
}

void heapstore_registry_iter_destroy(heapstore_registry_iter_t *iter)
{
    if (!iter) {
        return;
    }

    if (iter->stmt) {
        sqlite3_finalize(iter->stmt);
        iter->stmt = NULL;
    }

    AGENTOS_FREE(iter);
}

heapstore_error_t heapstore_registry_vacuum(void)
{
    if (!s_registry.initialized || !s_registry.db) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    agentos_mutex_lock(&s_registry.lock);
    sqlite3_exec(s_registry.db, "VACUUM;", NULL, NULL, NULL);
    agentos_mutex_unlock(&s_registry.lock);

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_registry_batch_insert_agents(const heapstore_agent_record_t *records,
                                                         size_t count)
{
    if (!records || count == 0) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (!s_registry.initialized || !s_registry.db) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    const char *sql = "INSERT INTO agents "
                      "(id, name, type, version, status, config_path, created_at, updated_at) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?);";

    agentos_mutex_lock(&s_registry.lock);

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(s_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        agentos_mutex_unlock(&s_registry.lock);
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    rc = sqlite3_exec(s_registry.db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        agentos_mutex_unlock(&s_registry.lock);
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    heapstore_error_t result = heapstore_SUCCESS;
    for (size_t i = 0; i < count; i++) {
        const heapstore_agent_record_t *record = &records[i];

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
    agentos_mutex_unlock(&s_registry.lock);

    return result;
}

heapstore_error_t
heapstore_registry_batch_insert_sessions(const heapstore_session_record_t *records, size_t count)
{
    if (!records || count == 0) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (!s_registry.initialized || !s_registry.db) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    const char *sql = "INSERT INTO sessions "
                      "(id, user_id, created_at, last_active_at, ttl_seconds, status) "
                      "VALUES (?, ?, ?, ?, ?, ?);";

    agentos_mutex_lock(&s_registry.lock);

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(s_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        agentos_mutex_unlock(&s_registry.lock);
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    rc = sqlite3_exec(s_registry.db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        agentos_mutex_unlock(&s_registry.lock);
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    heapstore_error_t result = heapstore_SUCCESS;
    for (size_t i = 0; i < count; i++) {
        const heapstore_session_record_t *record = &records[i];

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
    agentos_mutex_unlock(&s_registry.lock);

    return result;
}

heapstore_error_t heapstore_registry_batch_insert_skills(const heapstore_skill_record_t *records,
                                                         size_t count)
{
    if (!records || count == 0) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (!s_registry.initialized || !s_registry.db) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    const char *sql = "INSERT INTO skills "
                      "(id, name, version, library_path, manifest_path, installed_at) "
                      "VALUES (?, ?, ?, ?, ?, ?);";

    agentos_mutex_lock(&s_registry.lock);

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(s_registry.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        agentos_mutex_unlock(&s_registry.lock);
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    rc = sqlite3_exec(s_registry.db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        agentos_mutex_unlock(&s_registry.lock);
        return heapstore_ERR_DB_QUERY_FAILED;
    }

    heapstore_error_t result = heapstore_SUCCESS;
    for (size_t i = 0; i < count; i++) {
        const heapstore_skill_record_t *record = &records[i];

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
    agentos_mutex_unlock(&s_registry.lock);

    return result;
}

bool heapstore_registry_is_healthy(void)
{
    return s_registry.initialized && s_registry.db != NULL;
}

#endif /* AGENTOS_HAS_SQLITE3 */
#endif

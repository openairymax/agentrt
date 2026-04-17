/**
 * @file agent_registry.c
 * @brief Agent 注册表实现（基于实际market_service.h API）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "market_service.h"
#include "platform.h"
#include "error.h"
#include "daemon_errors.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_AGENTS 1024

typedef struct {
    agent_info_t info;
} agent_entry_t;

typedef struct {
    agent_entry_t entries[MAX_AGENTS];
    size_t entry_count;
    agentos_mutex_t lock;
    int initialized;
} agent_registry_t;

static agent_registry_t g_registry = {0};

static int find_agent_index(const char* agent_id) {
    for (size_t i = 0; i < g_registry.entry_count; i++) {
        if (g_registry.entries[i].info.agent_id &&
            strcmp(g_registry.entries[i].info.agent_id, agent_id) == 0)
            return (int)i;
    }
    return -1;
}

static void free_agent_info(agent_info_t* info) {
    if (!info) return;
    free(info->agent_id);
    free(info->name);
    free(info->version);
    free(info->description);
    free(info->author);
    free(info->repository);
    free(info->dependencies);
    memset(info, 0, sizeof(agent_info_t));
}

static void free_agent_entry(agent_entry_t* entry) {
    if (!entry) return;
    free_agent_info(&entry->info);
}

static char* safe_strdup(const char* str) {
    if (!str) return NULL;
    size_t len = strlen(str);
    char* copy = (char*)malloc(len + 1);
    if (copy) memcpy(copy, str, len + 1);
    return copy;
}

int agent_registry_init(const char* db_path) {
    (void)db_path;
    if (g_registry.initialized) return AGENTOS_OK;
    agentos_mutex_init(&g_registry.lock);
    g_registry.entry_count = 0;
    g_registry.initialized = 1;
    return AGENTOS_OK;
}

void agent_registry_shutdown(void) {
    if (!g_registry.initialized) return;
    agentos_mutex_lock(&g_registry.lock);
    for (size_t i = 0; i < g_registry.entry_count; i++)
        free_agent_entry(&g_registry.entries[i]);
    g_registry.entry_count = 0;
    g_registry.initialized = 0;
    agentos_mutex_unlock(&g_registry.lock);
    agentos_mutex_destroy(&g_registry.lock);
}

int agent_registry_register(const agent_info_t* reg) {
    if (!reg || !reg->agent_id || !reg->name) return AGENTOS_ERR_INVALID_PARAM;
    if (!g_registry.initialized) return AGENTOS_ERR_STATE_ERROR;

    agentos_mutex_lock(&g_registry.lock);
    if (find_agent_index(reg->agent_id) >= 0) {
        agentos_mutex_unlock(&g_registry.lock);
        return AGENTOS_ERR_ALREADY_EXISTS;
    }
    if (g_registry.entry_count >= MAX_AGENTS) {
        agentos_mutex_unlock(&g_registry.lock);
        return AGENTOS_ERR_OVERFLOW;
    }

    agent_entry_t* entry = &g_registry.entries[g_registry.entry_count++];
    entry->info.agent_id = safe_strdup(reg->agent_id);
    entry->info.name = safe_strdup(reg->name);
    entry->info.version = safe_strdup(reg->version);
    entry->info.description = safe_strdup(reg->description);
    entry->info.type = reg->type;
    entry->info.status = AGENT_STATUS_AVAILABLE;
    entry->info.author = safe_strdup(reg->author);
    entry->info.repository = safe_strdup(reg->repository);
    entry->info.dependencies = safe_strdup(reg->dependencies);
    entry->info.rating = reg->rating;
    entry->info.download_count = reg->download_count;
    entry->info.last_updated = (uint64_t)time(NULL);

    agentos_mutex_unlock(&g_registry.lock);
    return AGENTOS_OK;
}

int agent_registry_unregister(const char* agent_id) {
    if (!agent_id) return AGENTOS_ERR_INVALID_PARAM;
    if (!g_registry.initialized) return AGENTOS_ERR_STATE_ERROR;

    agentos_mutex_lock(&g_registry.lock);
    int idx = find_agent_index(agent_id);
    if (idx < 0) { agentos_mutex_unlock(&g_registry.lock); return AGENTOS_ERR_NOT_FOUND; }

    free_agent_entry(&g_registry.entries[idx]);
    for (size_t i = (size_t)idx; i < g_registry.entry_count - 1; i++)
        g_registry.entries[i] = g_registry.entries[i + 1];
    memset(&g_registry.entries[--g_registry.entry_count], 0, sizeof(agent_entry_t));

    agentos_mutex_unlock(&g_registry.lock);
    return AGENTOS_OK;
}

int agent_registry_get(const char* agent_id, agent_info_t* out_info) {
    if (!agent_id || !out_info) return AGENTOS_ERR_INVALID_PARAM;
    if (!g_registry.initialized) return AGENTOS_ERR_STATE_ERROR;

    agentos_mutex_lock(&g_registry.lock);
    int idx = find_agent_index(agent_id);
    if (idx < 0) { agentos_mutex_unlock(&g_registry.lock); return AGENTOS_ERR_NOT_FOUND; }

    agent_entry_t* entry = &g_registry.entries[idx];
    memset(out_info, 0, sizeof(agent_info_t));
    out_info->agent_id = safe_strdup(entry->info.agent_id);
    out_info->name = safe_strdup(entry->info.name);
    out_info->version = safe_strdup(entry->info.version);
    out_info->description = safe_strdup(entry->info.description);
    out_info->type = entry->info.type;
    out_info->status = entry->info.status;
    out_info->author = safe_strdup(entry->info.author);
    out_info->repository = safe_strdup(entry->info.repository);
    out_info->dependencies = safe_strdup(entry->info.dependencies);
    out_info->rating = entry->info.rating;
    out_info->download_count = entry->info.download_count;
    out_info->last_updated = entry->info.last_updated;

    agentos_mutex_unlock(&g_registry.lock);
    return AGENTOS_OK;
}

int agent_registry_search(const search_params_t* params, agent_info_t*** results, size_t* count) {
    if (!params || !results || !count) return AGENTOS_ERR_INVALID_PARAM;
    *results = NULL;
    *count = 0;
    if (!g_registry.initialized) return AGENTOS_ERR_STATE_ERROR;

    const char* query = params->query ? params->query : "";

    agentos_mutex_lock(&g_registry.lock);
    size_t match_count = 0;
    for (size_t i = 0; i < g_registry.entry_count; i++) {
        agent_entry_t* entry = &g_registry.entries[i];
        if (!query[0] ||
            (entry->info.agent_id && strstr(entry->info.agent_id, query)) ||
            (entry->info.name && strstr(entry->info.name, query)) ||
            (entry->info.description && strstr(entry->info.description, query))) {
            match_count++;
        }
    }

    if (match_count == 0) { agentos_mutex_unlock(&g_registry.lock); return AGENTOS_OK; }

    *results = (agent_info_t**)calloc(match_count, sizeof(agent_info_t*));
    if (!*results) { agentos_mutex_unlock(&g_registry.lock); return AGENTOS_ERR_OUT_OF_MEMORY; }

    size_t result_idx = 0;
    for (size_t i = 0; i < g_registry.entry_count && result_idx < match_count; i++) {
        agent_entry_t* entry = &g_registry.entries[i];
        if (!query[0] ||
            (entry->info.agent_id && strstr(entry->info.agent_id, query)) ||
            (entry->info.name && strstr(entry->info.name, query)) ||
            (entry->info.description && strstr(entry->info.description, query))) {

            (*results)[result_idx] = (agent_info_t*)calloc(1, sizeof(agent_info_t));
            if ((*results)[result_idx]) {
                agent_registry_get(entry->info.agent_id, (*results)[result_idx]);
                result_idx++;
            }
        }
    }

    *count = result_idx;
    agentos_mutex_unlock(&g_registry.lock);
    return AGENTOS_OK;
}

int agent_registry_add_version(const char* agent_id, const char* version_str) {
    if (!agent_id || !version_str) return AGENTOS_ERR_INVALID_PARAM;
    if (!g_registry.initialized) return AGENTOS_ERR_STATE_ERROR;

    agentos_mutex_lock(&g_registry.lock);
    int idx = find_agent_index(agent_id);
    if (idx < 0) { agentos_mutex_unlock(&g_registry.lock); return AGENTOS_ERR_NOT_FOUND; }

    agent_entry_t* entry = &g_registry.entries[idx];
    free(entry->info.version);
    entry->info.version = safe_strdup(version_str);
    entry->info.last_updated = (uint64_t)time(NULL);

    agentos_mutex_unlock(&g_registry.lock);
    return AGENTOS_OK;
}

void agent_info_free(agent_info_t* info) {
    if (!info) return;
    free_agent_info(info);
}

void agent_search_results_free(agent_info_t** results, size_t count) {
    if (!results) return;
    for (size_t i = 0; i < count; i++) {
        if (results[i]) { agent_info_free(results[i]); free(results[i]); }
    }
    free(results);
}

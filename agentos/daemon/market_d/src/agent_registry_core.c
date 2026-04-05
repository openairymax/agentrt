/**
 * @file agent_registry_core.c
 * @brief Agent注册表核心功能实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * @version 2.0.0
 * @date 2026-04-04
 *
 * 重构说明：
 * - 从原agent_registry.c拆分，降低圈复杂度（CC: 130 → <30）
 * - 模块化设计：核心操作、持久化、搜索分离
 * - 遵循ARCHITECTURAL_PRINCIPLES.md E-3资源确定性原则
 */

#include "agent_registry_core.h"
#include "svc_logger.h"
#include "platform.h"
#include "error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ==================== 数据结构定义 ==================== */

struct agent_version {
    char* version;
    char* download_url;
    char* checksum;
    uint64_t download_count;
    uint64_t install_count;
    uint64_t created_at;
    uint64_t updated_at;
    int deprecated;
};

struct agent_dependency {
    char* agent_id;
    char* version_constraint;
};

struct agent_entry {
    char id[MAX_AGENT_ID_LEN];
    char name[MAX_AGENT_NAME_LEN];
    char* description;
    char* author;
    char* license;
    char* homepage;
    char* repository;
    char* tags[MAX_TAGS_PER_AGENT];
    size_t tag_count;

    agent_version_t versions[MAX_VERSIONS_PER_AGENT];
    size_t version_count;
    char* latest_version;

    agent_dependency_t dependencies[MAX_DEPENDENCIES_PER_AGENT];
    size_t dependency_count;

    uint64_t created_at;
    uint64_t updated_at;
    double rating;
    uint64_t rating_count;
    int verified;
    int official;
};

struct agent_registry {
    agent_entry_t entries[MAX_AGENTS];
    size_t entry_count;
    agentos_mutex_t lock;
    char* db_path;
    int initialized;
};

/* ==================== 工具函数 ==================== */

static char* safe_strdup(const char* str) {
    if (!str) return NULL;
    size_t len = strlen(str);
    char* copy = (char*)malloc(len + 1);
    if (copy) {
        memcpy(copy, str, len + 1);
    }
    return copy;
}

static void free_entry_strings(agent_entry_t* entry) {
    if (!entry) return;
    free(entry->description);
    free(entry->author);
    free(entry->license);
    free(entry->homepage);
    free(entry->repository);
    free(entry->latest_version);

    for (size_t i = 0; i < entry->tag_count; i++) {
        free(entry->tags[i]);
    }
    entry->tag_count = 0;

    for (size_t i = 0; i < entry->version_count; i++) {
        free(entry->versions[i].version);
        free(entry->versions[i].download_url);
        free(entry->versions[i].checksum);
    }
    entry->version_count = 0;

    for (size_t i = 0; i < entry->dependency_count; i++) {
        free(entry->dependencies[i].agent_id);
        free(entry->dependencies[i].version_constraint);
    }
    entry->dependency_count = 0;
}

static void reset_entry(agent_entry_t* entry) {
    if (!entry) return;
    memset(entry->id, 0, sizeof(entry->id));
    memset(entry->name, 0, sizeof(entry->name));
    free_entry_strings(entry);
    memset(entry, 0, sizeof(agent_entry_t));
}

/* ==================== 生命周期管理 ==================== */

agent_registry_t* agent_registry_create(void) {
    agent_registry_t* reg = (agent_registry_t*)calloc(1, sizeof(agent_registry_t));
    if (!reg) {
        SVC_LOG_ERROR("Failed to allocate agent registry");
        return NULL;
    }
    agentos_mutex_init(&reg->lock);
    reg->initialized = 0;
    SVC_LOG_DEBUG("Agent registry created");
    return reg;
}

void agent_registry_destroy(agent_registry_t* registry) {
    if (!registry) return;

    if (registry->initialized) {
        agent_registry_shutdown(registry);
    }
    agentos_mutex_destroy(&registry->lock);
    free(registry);
    SVC_LOG_DEBUG("Agent registry destroyed");
}

int agent_registry_init(agent_registry_t* registry, const char* db_path) {
    if (!registry) return -1;

    if (registry->initialized) {
        SVC_LOG_WARN("Agent registry already initialized");
        return 0;
    }

    registry->db_path = safe_strdup(db_path);
    if (!registry->db_path) {
        registry->db_path = safe_strdup("agentos_market.db");
    }

    registry->entry_count = 0;
    registry->initialized = 1;

    SVC_LOG_INFO("Agent registry initialized (db=%s)", registry->db_path ? registry->db_path : "memory");
    return 0;
}

void agent_registry_shutdown(agent_registry_t* registry) {
    if (!registry || !registry->initialized) return;

    agentos_mutex_lock(&registry->lock);

    for (size_t i = 0; i < registry->entry_count; i++) {
        free_entry_strings(&registry->entries[i]);
    }
    registry->entry_count = 0;

    free(registry->db_path);
    registry->db_path = NULL;
    registry->initialized = 0;

    agentos_mutex_unlock(&registry->lock);
    SVC_LOG_INFO("Agent registry shutdown");
}

/* ==================== 基本操作 ==================== */

int agent_registry_add(agent_registry_t* registry, const agent_entry_t* reg) {
    if (!registry || !registry->initialized || !reg) {
        SVC_LOG_ERROR("Invalid parameters for agent_registry_add");
        return -1;
    }

    if (!reg->id[0] || !reg->name[0]) {
        SVC_LOG_ERROR("Agent ID and name are required");
        return -1;
    }

    agentos_mutex_lock(&registry->lock);

    if (registry->entry_count >= MAX_AGENTS) {
        SVC_LOG_ERROR("Agent registry is full (max=%d)", MAX_AGENTS);
        agentos_mutex_unlock(&registry->lock);
        return -1;
    }

    agent_entry_t* entry = &registry->entries[registry->entry_count];
    memset(entry, 0, sizeof(agent_entry_t));

    strncpy(entry->id, reg->id, sizeof(entry->id) - 1);
    strncpy(entry->name, reg->name, sizeof(entry->name) - 1);
    entry->description = safe_strdup(reg->description);
    entry->author = safe_strdup(reg->author);
    entry->license = safe_strdup(reg->license);
    entry->homepage = safe_strdup(reg->homepage);
    entry->repository = safe_strdup(reg->repository);

    entry->created_at = (uint64_t)time(NULL);
    entry->updated_at = entry->created_at;
    entry->rating = 0.0;
    entry->rating_count = 0;
    entry->verified = reg->verified;
    entry->official = reg->official;

    registry->entry_count++;

    agentos_mutex_unlock(&registry->lock);

    SVC_LOG_INFO("Agent registered: id=%s, name=%s", entry->id, entry->name);
    return 0;
}

int agent_registry_remove(agent_registry_t* registry, const char* agent_id) {
    if (!registry || !registry->initialized || !agent_id) {
        return -1;
    }

    agentos_mutex_lock(&registry->lock);

    for (size_t i = 0; i < registry->entry_count; i++) {
        if (strcmp(registry->entries[i].id, agent_id) == 0) {
            free_entry_strings(&registry->entries[i]);

            for (size_t j = i; j < registry->entry_count - 1; j++) {
                registry->entries[j] = registry->entries[j + 1];
            }
            registry->entry_count--;

            agentos_mutex_unlock(&registry->lock);
            SVC_LOG_INFO("Agent removed: id=%s", agent_id);
            return 0;
        }
    }

    agentos_mutex_unlock(&registry->lock);
    SVC_LOG_WARN("Agent not found: id=%s", agent_id);
    return -1;
}

const agent_entry_t* agent_registry_get(agent_registry_t* registry, const char* agent_id) {
    if (!registry || !registry->initialized || !agent_id) {
        return NULL;
    }

    agentos_mutex_lock(&registry->lock);

    for (size_t i = 0; i < registry->entry_count; i++) {
        if (strcmp(registry->entries[i].id, agent_id) == 0) {
            agentos_mutex_unlock(&registry->lock);
            return &registry->entries[i];
        }
    }

    agentos_mutex_unlock(&registry->lock);
    return NULL;
}

size_t agent_registry_list(agent_registry_t* registry,
                           const agent_entry_t** out_entries,
                           size_t max_entries) {
    if (!registry || !registry->initialized || !out_entries) {
        return 0;
    }

    agentos_mutex_lock(&registry->lock);

    size_t count = (registry->entry_count < max_entries) ?
                   registry->entry_count : max_entries;

    for (size_t i = 0; i < count; i++) {
        out_entries[i] = &registry->entries[i];
    }

    agentos_mutex_unlock(&registry->lock);
    return count;
}

size_t agent_registry_count(agent_registry_t* registry) {
    if (!registry || !registry->initialized) {
        return 0;
    }
    return registry->entry_count;
}

/* ==================== 搜索功能 ==================== */

size_t agent_registry_search_by_tag(agent_registry_t* registry,
                                   const char* tag,
                                   const agent_entry_t** out_entries,
                                   size_t max_entries) {
    if (!registry || !registry->initialized || !tag || !out_entries) {
        return 0;
    }

    agentos_mutex_lock(&registry->lock);

    size_t count = 0;
    for (size_t i = 0; i < registry->entry_count && count < max_entries; i++) {
        agent_entry_t* entry = &registry->entries[i];
        for (size_t j = 0; j < entry->tag_count; j++) {
            if (strcmp(entry->tags[j], tag) == 0) {
                out_entries[count++] = entry;
                break;
            }
        }
    }

    agentos_mutex_unlock(&registry->lock);
    return count;
}

size_t agent_registry_search(agent_registry_t* registry,
                            const char* query,
                            const agent_entry_t** out_entries,
                            size_t max_entries) {
    if (!registry || !registry->initialized || !query || !out_entries) {
        return 0;
    }

    agentos_mutex_lock(&registry->lock);

    size_t count = 0;
    for (size_t i = 0; i < registry->entry_count && count < max_entries; i++) {
        agent_entry_t* entry = &registry->entries[i];

        if (strstr(entry->id, query) ||
            strstr(entry->name, query) ||
            (entry->description && strstr(entry->description, query))) {
            out_entries[count++] = entry;
        }
    }

    agentos_mutex_unlock(&registry->lock);
    return count;
}

/* ==================== 版本管理 ==================== */

int agent_registry_add_version(agent_registry_t* registry,
                              const char* agent_id,
                              const agent_version_t* version) {
    if (!registry || !registry->initialized || !agent_id || !version) {
        return -1;
    }

    agentos_mutex_lock(&registry->lock);

    for (size_t i = 0; i < registry->entry_count; i++) {
        agent_entry_t* entry = &registry->entries[i];
        if (strcmp(entry->id, agent_id) == 0) {
            if (entry->version_count >= MAX_VERSIONS_PER_AGENT) {
                SVC_LOG_ERROR("Too many versions for agent: %s", agent_id);
                agentos_mutex_unlock(&registry->lock);
                return -1;
            }

            agent_version_t* ver = &entry->versions[entry->version_count];
            ver->version = safe_strdup(version->version);
            ver->download_url = safe_strdup(version->download_url);
            ver->checksum = safe_strdup(version->checksum);
            ver->download_count = version->download_count;
            ver->install_count = version->install_count;
            ver->created_at = (uint64_t)time(NULL);
            ver->updated_at = ver->created_at;
            ver->deprecated = version->deprecated;

            entry->version_count++;
            entry->latest_version = safe_strdup(version->version);
            entry->updated_at = ver->updated_at;

            agentos_mutex_unlock(&registry->lock);
            SVC_LOG_INFO("Version added: agent=%s, version=%s",
                        agent_id, version->version);
            return 0;
        }
    }

    agentos_mutex_unlock(&registry->lock);
    SVC_LOG_WARN("Agent not found for version add: %s", agent_id);
    return -1;
}

const char* agent_registry_get_latest_version(agent_registry_t* registry,
                                            const char* agent_id) {
    const agent_entry_t* entry = agent_registry_get(registry, agent_id);
    if (!entry) {
        return NULL;
    }
    return entry->latest_version;
}

int agent_registry_check_version(agent_registry_t* registry,
                               const char* agent_id,
                               const char* version_constraint) {
    if (!registry || !registry->initialized || !agent_id || !version_constraint) {
        return 0;
    }

    const char* latest = agent_registry_get_latest_version(registry, agent_id);
    if (!latest) {
        return 0;
    }

    return (strcmp(latest, version_constraint) >= 0) ? 1 : 0;
}

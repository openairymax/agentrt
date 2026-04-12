/**
 * @file agent_registry_core.c
 * @brief Agent注册表核心功能实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "agent_registry_core.h"
#include "svc_logger.h"
#include "platform.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct agent_registry {
    agent_entry_t entries[MAX_AGENTS];
    size_t entry_count;
    agentos_mutex_t lock;
    char* db_path;
    int initialized;
};

agent_registry_t* agent_registry_core_create(void) {
    agent_registry_t* reg = (agent_registry_t*)calloc(1, sizeof(agent_registry_t));
    if (!reg) return NULL;
    agentos_mutex_init(&reg->lock);
    reg->initialized = 0;
    return reg;
}

void agent_registry_core_destroy(agent_registry_t* registry) {
    if (!registry) return;
    if (registry->initialized) agent_registry_core_shutdown(registry);
    agentos_mutex_destroy(&registry->lock);
    free(registry);
}

int agent_registry_core_init(agent_registry_t* registry, const char* db_path) {
    if (!registry || registry->initialized) return -1;
    if (db_path) registry->db_path = strdup(db_path);
    registry->entry_count = 0;
    registry->initialized = 1;
    return 0;
}

void agent_registry_core_shutdown(agent_registry_t* registry) {
    if (!registry || !registry->initialized) return;
    agentos_mutex_lock(&registry->lock);
    for (size_t i = 0; i < registry->entry_count; i++) {
        free(registry->entries[i].description);
        free(registry->entries[i].author);
    }
    registry->entry_count = 0;
    free(registry->db_path);
    registry->db_path = NULL;
    registry->initialized = 0;
    agentos_mutex_unlock(&registry->lock);
}

int agent_registry_core_add(agent_registry_t* registry, const agent_entry_t* reg) {
    if (!registry || !registry->initialized || !reg) return -1;
    if (!reg->id[0] || !reg->name[0]) return -1;
    if (registry->entry_count >= MAX_AGENTS) return -1;

    agentos_mutex_lock(&registry->lock);
    agent_entry_t* entry = &registry->entries[registry->entry_count];
    memset(entry, 0, sizeof(agent_entry_t));
    strncpy(entry->id, reg->id, sizeof(entry->id) - 1);
    strncpy(entry->name, reg->name, sizeof(entry->name) - 1);
    if (reg->description) entry->description = strdup(reg->description);
    if (reg->author) entry->author = strdup(reg->author);
    entry->created_at = (uint64_t)time(NULL);
    entry->updated_at = entry->created_at;
    entry->verified = reg->verified;
    entry->official = reg->official;
    registry->entry_count++;
    agentos_mutex_unlock(&registry->lock);
    return 0;
}

int agent_registry_core_remove(agent_registry_t* registry, const char* agent_id) {
    if (!registry || !registry->initialized || !agent_id) return -1;
    agentos_mutex_lock(&registry->lock);
    for (size_t i = 0; i < registry->entry_count; i++) {
        if (strcmp(registry->entries[i].id, agent_id) == 0) {
            free(registry->entries[i].description);
            free(registry->entries[i].author);
            for (size_t j = i; j < registry->entry_count - 1; j++) {
                registry->entries[j] = registry->entries[j + 1];
            }
            registry->entry_count--;
            agentos_mutex_unlock(&registry->lock);
            return 0;
        }
    }
    agentos_mutex_unlock(&registry->lock);
    return -1;
}

const agent_entry_t* agent_registry_core_get(agent_registry_t* registry, const char* agent_id) {
    if (!registry || !registry->initialized || !agent_id) return NULL;
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

size_t agent_registry_core_list(agent_registry_t* registry, const agent_entry_t** out_entries, size_t max_entries) {
    if (!registry || !registry->initialized || !out_entries) return 0;
    agentos_mutex_lock(&registry->lock);
    size_t count = (registry->entry_count < max_entries) ? registry->entry_count : max_entries;
    for (size_t i = 0; i < count; i++) out_entries[i] = &registry->entries[i];
    agentos_mutex_unlock(&registry->lock);
    return count;
}

size_t agent_registry_core_count(agent_registry_t* registry) {
    if (!registry || !registry->initialized) return 0;
    return registry->entry_count;
}

int agent_registry_core_add_version(agent_registry_t* registry, const char* agent_id, const agent_version_t* version) {
    if (!registry || !registry->initialized || !agent_id || !version) return -1;

    agentos_mutex_lock(&registry->lock);
    for (size_t i = 0; i < registry->entry_count; i++) {
        if (strcmp(registry->entries[i].id, agent_id) == 0) {
            if (registry->entries[i].version_count >= MAX_VERSIONS_PER_AGENT) {
                agentos_mutex_unlock(&registry->lock);
                return -1;
            }
            agent_version_t* v = &registry->entries[i].versions[registry->entries[i].version_count];
            memset(v, 0, sizeof(agent_version_t));
            strncpy(v->version, version->version, sizeof(v->version) - 1);
            strncpy(v->download_url, version->download_url, sizeof(v->download_url) - 1);
            strncpy(v->checksum, version->checksum, sizeof(v->checksum) - 1);
            v->created_at = (uint64_t)time(NULL);
            v->deprecated = version->deprecated;
            registry->entries[i].version_count++;
            strncpy(registry->entries[i].latest_version, version->version, sizeof(registry->entries[i].latest_version) - 1);
            agentos_mutex_unlock(&registry->lock);
            return 0;
        }
    }
    agentos_mutex_unlock(&registry->lock);
    return -1;
}

const char* agent_registry_core_get_latest_version(agent_registry_t* registry, const char* agent_id) {
    if (!registry || !registry->initialized || !agent_id) return NULL;
    agentos_mutex_lock(&registry->lock);
    for (size_t i = 0; i < registry->entry_count; i++) {
        if (strcmp(registry->entries[i].id, agent_id) == 0) {
            agentos_mutex_unlock(&registry->lock);
            return registry->entries[i].latest_version;
        }
    }
    agentos_mutex_unlock(&registry->lock);
    return NULL;
}

size_t agent_registry_core_search_by_tag(agent_registry_t* registry, const char* tag,
                                   const agent_entry_t** out_entries, size_t max_entries) {
    if (!registry || !registry->initialized || !tag || !out_entries) return 0;
    agentos_mutex_lock(&registry->lock);
    size_t count = 0;
    for (size_t i = 0; i < registry->entry_count && count < max_entries; i++) {
        for (size_t j = 0; j < registry->entries[i].tag_count; j++) {
            if (registry->entries[i].tags[j] && strcmp(registry->entries[i].tags[j], tag) == 0) {
                out_entries[count++] = &registry->entries[i];
                break;
            }
        }
    }
    agentos_mutex_unlock(&registry->lock);
    return count;
}

size_t agent_registry_core_search(agent_registry_t* registry, const char* query,
                            const agent_entry_t** out_entries, size_t max_entries) {
    if (!registry || !registry->initialized || !query || !out_entries) return 0;
    agentos_mutex_lock(&registry->lock);
    size_t count = 0;
    for (size_t i = 0; i < registry->entry_count && count < max_entries; i++) {
        if (strstr(registry->entries[i].id, query) ||
            strstr(registry->entries[i].name, query) ||
            (registry->entries[i].description && strstr(registry->entries[i].description, query))) {
            out_entries[count++] = &registry->entries[i];
        }
    }
    agentos_mutex_unlock(&registry->lock);
    return count;
}

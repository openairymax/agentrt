/**
 * @file agent_registry_core.h
 * @brief Agent注册表核心功能接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_AGENT_REGISTRY_CORE_H
#define AGENTOS_AGENT_REGISTRY_CORE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_AGENT_ID_LEN 128
#define MAX_AGENT_NAME_LEN 256
#define MAX_DESCRIPTION_LEN 4096
#define MAX_AGENTS 1024
#define MAX_TAGS_PER_AGENT 16
#define MAX_VERSIONS_PER_AGENT 32
#define MAX_URL_LEN 512
#define MAX_CHECKSUM_LEN 128

typedef struct agent_version {
    char version[32];
    char download_url[MAX_URL_LEN];
    char checksum[MAX_CHECKSUM_LEN];
    uint64_t created_at;
    int deprecated;
} agent_version_t;

typedef struct agent_entry {
    char id[MAX_AGENT_ID_LEN];
    char name[MAX_AGENT_NAME_LEN];
    char* description;
    char* author;
    char* tags[MAX_TAGS_PER_AGENT];
    size_t tag_count;
    agent_version_t versions[MAX_VERSIONS_PER_AGENT];
    size_t version_count;
    char latest_version[32];
    uint64_t created_at;
    uint64_t updated_at;
    int verified;
    int official;
} agent_entry_t;

typedef struct agent_registry agent_registry_t;

agent_registry_t* agent_registry_create(void);
void agent_registry_destroy(agent_registry_t* registry);
int agent_registry_init(agent_registry_t* registry, const char* db_path);
void agent_registry_shutdown(agent_registry_t* registry);

int agent_registry_add(agent_registry_t* registry, const agent_entry_t* reg);
int agent_registry_remove(agent_registry_t* registry, const char* agent_id);
const agent_entry_t* agent_registry_get(agent_registry_t* registry, const char* agent_id);
size_t agent_registry_list(agent_registry_t* registry, const agent_entry_t** out_entries, size_t max_entries);
size_t agent_registry_count(agent_registry_t* registry);

int agent_registry_add_version(agent_registry_t* registry, const char* agent_id, const agent_version_t* version);
const char* agent_registry_get_latest_version(agent_registry_t* registry, const char* agent_id);

size_t agent_registry_search_by_tag(agent_registry_t* registry, const char* tag,
                                   const agent_entry_t** out_entries, size_t max_entries);
size_t agent_registry_search(agent_registry_t* registry, const char* query,
                          const agent_entry_t** out_entries, size_t max_entries);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_AGENT_REGISTRY_CORE_H */

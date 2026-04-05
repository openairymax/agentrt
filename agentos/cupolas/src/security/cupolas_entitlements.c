/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * cupolas_entitlements.c - Entitlements Permission Declarations Implementation
 */

/**
 * @file cupolas_entitlements.c
 * @brief Entitlements Permission Declarations - Fine-grained Permission Mechanism Implementation
 * @author Spharx
 * @date 2026
 */

#include "cupolas_entitlements.h"
#include "../platform/platform.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/sha.h>
#include <openssl/err.h>

#define cupolas_MAX_PATH_LEN 4096
#define cupolas_MAX_HOST_LEN 256
#define cupolas_MAX_RULES 1024

typedef struct {
    char* key;
    char* value;
    int value_type;
} yaml_kv_t;

struct cupolas_entitlements {
    cupolas_entitlements_info_t info;
    char* raw_content;
    char* signature;
    size_t sig_len;
    uint64_t load_time;
    int is_verified;
#ifdef _WIN32
    CRITICAL_SECTION lock;
#else
    pthread_mutex_t lock;
#endif
};

static int g_initialized = 0;

static int cupolas_str_ends_with(const char* str, const char* suffix) {
    if (!str || !suffix) return 0;
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > str_len) return 0;
    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

static char* cupolas_str_trim(char* str) {
    if (!str) return NULL;
    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') str++;
    if (*str == 0) return str;
    char* end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end--;
    *(end + 1) = '\0';
    return str;
}

static void cupolas_free_string_array(char** arr, size_t count) {
    if (!arr) return;
    for (size_t i = 0; i < count; i++) {
        free(arr[i]);
    }
    free(arr);
}

static char** cupolas_parse_string_array(const char* content, size_t* count) {
    *count = 0;
    if (!content) return NULL;
    
    size_t capacity = 16;
    char** arr = (char**)malloc(capacity * sizeof(char*));
    if (!arr) return NULL;
    
    char* dup = cupolas_strdup(content);
    if (!dup) { free(arr); return NULL; }
    
    char* saveptr;
    char* token = strtok_r(dup, ",\n", &saveptr);
    while (token) {
        token = cupolas_str_trim(token);
        if (*token != '\0') {
            if (*count >= capacity) {
                capacity *= 2;
                char** new_arr = (char**)realloc(arr, capacity * sizeof(char*));
                if (!new_arr) {
                    free(dup);
                    cupolas_free_string_array(arr, *count);
                    return NULL;
                }
                arr = new_arr;
            }
            arr[*count] = cupolas_strdup(token);
            if (arr[*count]) (*count)++;
        }
        token = strtok_r(NULL, ",\n", &saveptr);
    }
    
    free(dup);
    return arr;
}

static int cupolas_parse_yaml_line(const char* line, char** key, char** value, int* indent) {
    *key = NULL;
    *value = NULL;
    *indent = 0;
    
    const char* p = line;
    while (*p == ' ') { (*indent)++; p++; }
    
    const char* colon = strchr(p, ':');
    if (!colon) return -1;
    
    size_t key_len = colon - p;
    *key = (char*)malloc(key_len + 1);
    if (!*key) return -1;
    memcpy(*key, p, key_len);
    (*key)[key_len] = '\0';
    *key = cupolas_str_trim(*key);
    
    const char* v = colon + 1;
    while (*v == ' ') v++;
    
    if (*v != '\0' && *v != '\n' && *v != '\r') {
        size_t value_len = strlen(v);
        while (value_len > 0 && (v[value_len-1] == '\n' || v[value_len-1] == '\r' || v[value_len-1] == ' ')) {
            value_len--;
        }
        *value = (char*)malloc(value_len + 1);
        if (!*value) { free(*key); *key = NULL; return -1; }
        memcpy(*value, v, value_len);
        (*value)[value_len] = '\0';
    }
    
    return 0;
}

/* ============================================================================
 * 键处理函数声明
 * ============================================================================ */

static void handle_agent_id(cupolas_entitlements_info_t* info, const char* value) {
    info->agent_id = cupolas_strdup(value);
}

static void handle_version(cupolas_entitlements_info_t* info, const char* value) {
    info->version = cupolas_strdup(value);
}

static void handle_not_before(cupolas_entitlements_info_t* info, const char* value) {
    info->not_before = strtoull(value, NULL, 10);
}

static void handle_not_after(cupolas_entitlements_info_t* info, const char* value) {
    info->not_after = strtoull(value, NULL, 10);
}

static void handle_max_cpu_percent(cupolas_entitlements_info_t* info, const char* value) {
    info->resources.max_cpu_percent = (uint32_t)strtoul(value, NULL, 10);
}

static void handle_max_memory_bytes(cupolas_entitlements_info_t* info, const char* value) {
    info->resources.max_memory_bytes = strtoull(value, NULL, 10);
}

static void handle_max_disk_bytes(cupolas_entitlements_info_t* info, const char* value) {
    info->resources.max_disk_bytes = strtoull(value, NULL, 10);
}

static void handle_max_processes(cupolas_entitlements_info_t* info, const char* value) {
    info->resources.max_processes = (uint32_t)strtoul(value, NULL, 10);
}

static void handle_max_threads(cupolas_entitlements_info_t* info, const char* value) {
    info->resources.max_threads = (uint32_t)strtoul(value, NULL, 10);
}

static void handle_max_open_files(cupolas_entitlements_info_t* info, const char* value) {
    info->resources.max_open_files = (uint32_t)strtoul(value, NULL, 10);
}

static void handle_max_network_connections(cupolas_entitlements_info_t* info, const char* value) {
    info->resources.max_network_connections = (uint32_t)strtoul(value, NULL, 10);
}

static void handle_allowed_syscalls(cupolas_entitlements_info_t* info, const char* value) {
    info->allowed_syscalls = cupolas_parse_string_array(value, &info->syscall_count);
}

static void handle_allowed_capabilities(cupolas_entitlements_info_t* info, const char* value) {
    info->allowed_capabilities = cupolas_parse_string_array(value, &info->cap_count);
}

/* ============================================================================
 * 键处理映射表
 * ============================================================================ */

typedef struct {
    const char* key;
    void (*handler)(cupolas_entitlements_info_t*, const char*);
} key_handler_map_t;

static const key_handler_map_t g_key_handlers[] = {
    {"agent_id", handle_agent_id},
    {"version", handle_version},
    {"not_before", handle_not_before},
    {"not_after", handle_not_after},
    {"max_cpu_percent", handle_max_cpu_percent},
    {"max_memory_bytes", handle_max_memory_bytes},
    {"max_disk_bytes", handle_max_disk_bytes},
    {"max_processes", handle_max_processes},
    {"max_threads", handle_max_threads},
    {"max_open_files", handle_max_open_files},
    {"max_network_connections", handle_max_network_connections},
    {"allowed_syscalls", handle_allowed_syscalls},
    {"allowed_capabilities", handle_allowed_capabilities},
};

static const size_t g_key_handlers_count = sizeof(g_key_handlers) / sizeof(g_key_handlers[0]);

/* ============================================================================
 * 主解析函数
 * ============================================================================ */

static int cupolas_parse_entitlements_content(const char* content, cupolas_entitlements_info_t* info) {
    memset(info, 0, sizeof(*info));
    
    char* dup = cupolas_strdup(content);
    if (!dup) return cupolas_ENT_PARSE_ERROR;
    
    char* saveptr;
    char* line = strtok_r(dup, "\n", &saveptr);
    
    while (line) {
        char* key = NULL;
        char* value = NULL;
        int indent = 0;
        
        if (cupolas_parse_yaml_line(line, &key, &value, &indent) == 0 && key && value) {
            for (size_t i = 0; i < g_key_handlers_count; i++) {
                if (strcmp(key, g_key_handlers[i].key) == 0) {
                    g_key_handlers[i].handler(info, value);
                    break;
                }
            }
        }
        
        free(key);
        free(value);
        line = strtok_r(NULL, "\n", &saveptr);
    }
    
    free(dup);
    return cupolas_ENT_OK;
}

int cupolas_entitlements_init(void) {
    if (g_initialized) return 0;
    
    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();
    
    g_initialized = 1;
    return 0;
}

void cupolas_entitlements_cleanup(void) {
    if (!g_initialized) return;
    
    EVP_cleanup();
    ERR_free_strings();
    
    g_initialized = 0;
}

int cupolas_entitlements_load(const char* yaml_path, cupolas_entitlements_t** entitlements) {
    if (!yaml_path || !entitlements) return cupolas_ENT_INVALID;
    
    FILE* f = fopen(yaml_path, "r");
    if (!f) return cupolas_ENT_NOT_FOUND;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* content = (char*)malloc(size + 1);
    if (!content) { fclose(f); return cupolas_ENT_PARSE_ERROR; }
    
    size_t read_size = fread(content, 1, size, f);
    fclose(f);
    content[read_size] = '\0';
    
    int result = cupolas_entitlements_load_string(content, entitlements);
    free(content);
    
    return result;
}

int cupolas_entitlements_load_json(const char* json_path, cupolas_entitlements_t** entitlements) {
    if (!json_path || !entitlements) return cupolas_ENT_INVALID;
    
    FILE* f = fopen(json_path, "r");
    if (!f) return cupolas_ENT_NOT_FOUND;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* content = (char*)malloc(size + 1);
    if (!content) { fclose(f); return cupolas_ENT_PARSE_ERROR; }
    
    size_t read_size = fread(content, 1, size, f);
    fclose(f);
    content[read_size] = '\0';
    
    int result = cupolas_entitlements_load_string(content, entitlements);
    free(content);
    
    return result;
}

int cupolas_entitlements_load_string(const char* yaml_content, cupolas_entitlements_t** entitlements) {
    if (!yaml_content || !entitlements) return cupolas_ENT_INVALID;
    
    *entitlements = (cupolas_entitlements_t*)calloc(1, sizeof(cupolas_entitlements_t));
    if (!*entitlements) return cupolas_ENT_PARSE_ERROR;
    
#ifdef _WIN32
    InitializeCriticalSection(&(*entitlements)->lock);
#else
    pthread_mutex_init(&(*entitlements)->lock, NULL);
#endif
    
    (*entitlements)->raw_content = cupolas_strdup(yaml_content);
    (*entitlements)->load_time = cupolas_get_time_ms();
    (*entitlements)->is_verified = 0;
    
    int result = cupolas_parse_entitlements_content(yaml_content, &(*entitlements)->info);
    if (result != cupolas_ENT_OK) {
        cupolas_entitlements_free(*entitlements);
        *entitlements = NULL;
        return result;
    }
    
    return cupolas_ENT_OK;
}

void cupolas_entitlements_free(cupolas_entitlements_t* entitlements) {
    if (!entitlements) return;
    
    free(entitlements->raw_content);
    free(entitlements->signature);
    
    free(entitlements->info.agent_id);
    free(entitlements->info.version);
    
    for (size_t i = 0; i < entitlements->info.fs_count; i++) {
        free(entitlements->info.fs_permissions[i].path);
        cupolas_free_string_array(entitlements->info.fs_permissions[i].permissions,
                                entitlements->info.fs_permissions[i].perm_count);
    }
    free(entitlements->info.fs_permissions);
    
    for (size_t i = 0; i < entitlements->info.net_count; i++) {
        free(entitlements->info.net_permissions[i].host);
        free(entitlements->info.net_permissions[i].protocol);
        free(entitlements->info.net_permissions[i].direction);
    }
    free(entitlements->info.net_permissions);
    
    for (size_t i = 0; i < entitlements->info.ipc_count; i++) {
        free(entitlements->info.ipc_permissions[i].target);
        cupolas_free_string_array(entitlements->info.ipc_permissions[i].permissions,
                                entitlements->info.ipc_permissions[i].perm_count);
    }
    free(entitlements->info.ipc_permissions);
    
    for (size_t i = 0; i < entitlements->info.vault_count; i++) {
        free(entitlements->info.vault_permissions[i].cred_id);
        cupolas_free_string_array(entitlements->info.vault_permissions[i].permissions,
                                entitlements->info.vault_permissions[i].perm_count);
    }
    free(entitlements->info.vault_permissions);
    
    cupolas_free_string_array(entitlements->info.allowed_syscalls, entitlements->info.syscall_count);
    cupolas_free_string_array(entitlements->info.allowed_capabilities, entitlements->info.cap_count);
    
#ifdef _WIN32
    DeleteCriticalSection(&entitlements->lock);
#else
    pthread_mutex_destroy(&entitlements->lock);
#endif
    
    free(entitlements);
}

int cupolas_entitlements_verify(cupolas_entitlements_t* entitlements, const char* public_key) {
    if (!entitlements || !public_key) return cupolas_ENT_INVALID;
    if (!entitlements->signature || entitlements->sig_len == 0) return cupolas_ENT_SIGNATURE_INVALID;
    
    BIO* bio = BIO_new_mem_buf(public_key, -1);
    if (!bio) return cupolas_ENT_SIGNATURE_INVALID;
    
    EVP_PKEY* pkey = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
    BIO_free(bio);
    
    if (!pkey) return cupolas_ENT_SIGNATURE_INVALID;
    
    EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
    if (!md_ctx) {
        EVP_PKEY_free(pkey);
        return cupolas_ENT_SIGNATURE_INVALID;
    }
    
    int result = cupolas_ENT_SIGNATURE_INVALID;
    
    if (EVP_DigestVerifyInit(md_ctx, NULL, EVP_sha256(), NULL, pkey) == 1) {
        size_t content_len = strlen(entitlements->raw_content);
        if (EVP_DigestVerify(md_ctx, (unsigned char*)entitlements->signature,
                            entitlements->sig_len,
                            (unsigned char*)entitlements->raw_content,
                            content_len) == 1) {
            result = cupolas_ENT_OK;
            entitlements->is_verified = 1;
        }
    }
    
    EVP_MD_CTX_free(md_ctx);
    EVP_PKEY_free(pkey);
    
    return result;
}

int cupolas_entitlements_sign(cupolas_entitlements_t* entitlements, const char* private_key,
                            char* signature_out, size_t* sig_len) {
    if (!entitlements || !private_key || !signature_out || !sig_len) return cupolas_ENT_INVALID;
    
    BIO* bio = BIO_new_mem_buf(private_key, -1);
    if (!bio) return cupolas_ENT_PARSE_ERROR;
    
    EVP_PKEY* pkey = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
    BIO_free(bio);
    
    if (!pkey) return cupolas_ENT_PARSE_ERROR;
    
    EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
    if (!md_ctx) {
        EVP_PKEY_free(pkey);
        return cupolas_ENT_PARSE_ERROR;
    }
    
    int result = cupolas_ENT_PARSE_ERROR;
    
    if (EVP_DigestSignInit(md_ctx, NULL, EVP_sha256(), NULL, pkey) == 1) {
        size_t content_len = strlen(entitlements->raw_content);
        size_t req_len = 0;
        
        if (EVP_DigestSign(md_ctx, NULL, &req_len,
                          (unsigned char*)entitlements->raw_content,
                          content_len) == 1) {
            if (*sig_len >= req_len) {
                if (EVP_DigestSign(md_ctx, (unsigned char*)signature_out, sig_len,
                                  (unsigned char*)entitlements->raw_content,
                                  content_len) == 1) {
                    result = cupolas_ENT_OK;
                    
                    free(entitlements->signature);
                    entitlements->signature = (char*)malloc(*sig_len);
                    if (entitlements->signature) {
                        memcpy(entitlements->signature, signature_out, *sig_len);
                        entitlements->sig_len = *sig_len;
                    }
                }
            }
        }
    }
    
    EVP_MD_CTX_free(md_ctx);
    EVP_PKEY_free(pkey);
    
    return result;
}

bool cupolas_entitlements_is_signed(cupolas_entitlements_t* entitlements) {
    if (!entitlements) return false;
    return entitlements->signature != NULL && entitlements->sig_len > 0;
}

int cupolas_entitlements_check_fs(cupolas_entitlements_t* entitlements, const char* path, const char* operation) {
    if (!entitlements || !path || !operation) return 0;
    
    for (size_t i = 0; i < entitlements->info.fs_count; i++) {
        cupolas_ent_fs_permission_t* perm = &entitlements->info.fs_permissions[i];
        
        if (cupolas_entitlements_match_path(perm->path, path)) {
            for (size_t j = 0; j < perm->perm_count; j++) {
                if (strcmp(perm->permissions[j], operation) == 0) {
                    return 1;
                }
            }
        }
    }
    
    return 0;
}

int cupolas_entitlements_check_net(cupolas_entitlements_t* entitlements, const char* host,
                                 uint16_t port, const char* protocol, const char* direction) {
    if (!entitlements || !host) return 0;
    
    for (size_t i = 0; i < entitlements->info.net_count; i++) {
        cupolas_ent_net_permission_t* perm = &entitlements->info.net_permissions[i];
        
        int host_match = cupolas_entitlements_match_host(perm->host, host);
        int port_match = (perm->port == 0 || perm->port == port);
        int proto_match = !perm->protocol || strcmp(perm->protocol, protocol) == 0;
        int dir_match = !perm->direction || strcmp(perm->direction, direction) == 0 ||
                       strcmp(perm->direction, "both") == 0;
        
        if (host_match && port_match && proto_match && dir_match) {
            return 1;
        }
    }
    
    return 0;
}

int cupolas_entitlements_check_ipc(cupolas_entitlements_t* entitlements, const char* target, const char* operation) {
    if (!entitlements || !target || !operation) return 0;
    
    for (size_t i = 0; i < entitlements->info.ipc_count; i++) {
        cupolas_ent_ipc_permission_t* perm = &entitlements->info.ipc_permissions[i];
        
        if (strcmp(perm->target, target) == 0) {
            for (size_t j = 0; j < perm->perm_count; j++) {
                if (strcmp(perm->permissions[j], operation) == 0) {
                    return 1;
                }
            }
        }
    }
    
    return 0;
}

int cupolas_entitlements_check_syscall(cupolas_entitlements_t* entitlements, const char* syscall_name) {
    if (!entitlements || !syscall_name) return 0;
    
    for (size_t i = 0; i < entitlements->info.syscall_count; i++) {
        if (strcmp(entitlements->info.allowed_syscalls[i], syscall_name) == 0) {
            return 1;
        }
    }
    
    return 0;
}

int cupolas_entitlements_check_capability(cupolas_entitlements_t* entitlements, const char* capability) {
    if (!entitlements || !capability) return 0;
    
    for (size_t i = 0; i < entitlements->info.cap_count; i++) {
        if (strcmp(entitlements->info.allowed_capabilities[i], capability) == 0) {
            return 1;
        }
    }
    
    return 0;
}

int cupolas_entitlements_check_vault(cupolas_entitlements_t* entitlements, const char* cred_id, const char* operation) {
    if (!entitlements || !cred_id || !operation) return 0;
    
    for (size_t i = 0; i < entitlements->info.vault_count; i++) {
        cupolas_ent_vault_permission_t* perm = &entitlements->info.vault_permissions[i];
        
        if (strcmp(perm->cred_id, cred_id) == 0) {
            for (size_t j = 0; j < perm->perm_count; j++) {
                if (strcmp(perm->permissions[j], operation) == 0) {
                    return 1;
                }
            }
        }
    }
    
    return 0;
}

int cupolas_entitlements_get_resource_limits(cupolas_entitlements_t* entitlements, cupolas_ent_resource_limits_t* limits) {
    if (!entitlements || !limits) return cupolas_ENT_INVALID;
    
    *limits = entitlements->info.resources;
    return cupolas_ENT_OK;
}

int cupolas_entitlements_check_resource(cupolas_entitlements_t* entitlements, const char* resource_type, uint64_t current_value) {
    if (!entitlements || !resource_type) return 0;
    
    cupolas_ent_resource_limits_t* limits = &entitlements->info.resources;
    
    if (strcmp(resource_type, "cpu") == 0) {
        return current_value <= limits->max_cpu_percent;
    } else if (strcmp(resource_type, "memory") == 0) {
        return current_value <= limits->max_memory_bytes;
    } else if (strcmp(resource_type, "disk") == 0) {
        return current_value <= limits->max_disk_bytes;
    } else if (strcmp(resource_type, "process") == 0) {
        return current_value <= limits->max_processes;
    } else if (strcmp(resource_type, "thread") == 0) {
        return current_value <= limits->max_threads;
    } else if (strcmp(resource_type, "file") == 0) {
        return current_value <= limits->max_open_files;
    } else if (strcmp(resource_type, "connection") == 0) {
        return current_value <= limits->max_network_connections;
    }
    
    return 0;
}

int cupolas_entitlements_get_info(cupolas_entitlements_t* entitlements, cupolas_entitlements_info_t* info) {
    if (!entitlements || !info) return cupolas_ENT_INVALID;
    
    *info = entitlements->info;
    return cupolas_ENT_OK;
}

void cupolas_entitlements_free_info(cupolas_entitlements_info_t* info) {
    (void)info;
}

const char* cupolas_entitlements_get_agent_id(cupolas_entitlements_t* entitlements) {
    if (!entitlements) return NULL;
    return entitlements->info.agent_id;
}

int cupolas_entitlements_check_validity(cupolas_entitlements_t* entitlements) {
    if (!entitlements) return cupolas_ENT_INVALID;
    
    uint64_t now = cupolas_get_time_ms() / 1000;
    
    if (entitlements->info.not_before > 0 && now < entitlements->info.not_before) {
        return cupolas_ENT_EXPIRED;
    }
    
    if (entitlements->info.not_after > 0 && now > entitlements->info.not_after) {
        return cupolas_ENT_EXPIRED;
    }
    
    return cupolas_ENT_OK;
}

int cupolas_entitlements_export_yaml(cupolas_entitlements_t* entitlements, char* yaml_out, size_t* len) {
    if (!entitlements || !yaml_out || !len) return cupolas_ENT_INVALID;
    
    int written = snprintf(yaml_out, *len,
        "agent_id: %s\n"
        "version: %s\n"
        "not_before: %llu\n"
        "not_after: %llu\n"
        "resources:\n"
        "  max_cpu_percent: %u\n"
        "  max_memory_bytes: %llu\n"
        "  max_disk_bytes: %llu\n"
        "  max_processes: %u\n"
        "  max_threads: %u\n"
        "  max_open_files: %u\n"
        "  max_network_connections: %u\n",
        entitlements->info.agent_id ? entitlements->info.agent_id : "",
        entitlements->info.version ? entitlements->info.version : "",
        (unsigned long long)entitlements->info.not_before,
        (unsigned long long)entitlements->info.not_after,
        entitlements->info.resources.max_cpu_percent,
        (unsigned long long)entitlements->info.resources.max_memory_bytes,
        (unsigned long long)entitlements->info.resources.max_disk_bytes,
        entitlements->info.resources.max_processes,
        entitlements->info.resources.max_threads,
        entitlements->info.resources.max_open_files,
        entitlements->info.resources.max_network_connections
    );
    
    if (written < 0 || (size_t)written >= *len) {
        *len = (size_t)written + 1;
        return cupolas_ENT_PARSE_ERROR;
    }
    
    *len = (size_t)written;
    return cupolas_ENT_OK;
}

int cupolas_entitlements_export_json(cupolas_entitlements_t* entitlements, char* json_out, size_t* len) {
    if (!entitlements || !json_out || !len) return cupolas_ENT_INVALID;
    
    int written = snprintf(json_out, *len,
        "{\n"
        "  \"agent_id\": \"%s\",\n"
        "  \"version\": \"%s\",\n"
        "  \"not_before\": %llu,\n"
        "  \"not_after\": %llu,\n"
        "  \"resources\": {\n"
        "    \"max_cpu_percent\": %u,\n"
        "    \"max_memory_bytes\": %llu,\n"
        "    \"max_disk_bytes\": %llu,\n"
        "    \"max_processes\": %u,\n"
        "    \"max_threads\": %u,\n"
        "    \"max_open_files\": %u,\n"
        "    \"max_network_connections\": %u\n"
        "  }\n"
        "}\n",
        entitlements->info.agent_id ? entitlements->info.agent_id : "",
        entitlements->info.version ? entitlements->info.version : "",
        (unsigned long long)entitlements->info.not_before,
        (unsigned long long)entitlements->info.not_after,
        entitlements->info.resources.max_cpu_percent,
        (unsigned long long)entitlements->info.resources.max_memory_bytes,
        (unsigned long long)entitlements->info.resources.max_disk_bytes,
        entitlements->info.resources.max_processes,
        entitlements->info.resources.max_threads,
        entitlements->info.resources.max_open_files,
        entitlements->info.resources.max_network_connections
    );
    
    if (written < 0 || (size_t)written >= *len) {
        *len = (size_t)written + 1;
        return cupolas_ENT_PARSE_ERROR;
    }
    
    *len = (size_t)written;
    return cupolas_ENT_OK;
}

const char* cupolas_entitlements_result_string(cupolas_ent_result_t result) {
    switch (result) {
        case cupolas_ENT_OK: return "Success";
        case cupolas_ENT_INVALID: return "Invalid parameter";
        case cupolas_ENT_SIGNATURE_INVALID: return "Invalid signature";
        case cupolas_ENT_EXPIRED: return "Expired";
        case cupolas_ENT_DENIED: return "Permission denied";
        case cupolas_ENT_NOT_FOUND: return "Not found";
        case cupolas_ENT_PARSE_ERROR: return "Parse error";
        default: return "Unknown error";
    }
}

int cupolas_entitlements_match_path(const char* pattern, const char* path) {
    if (!pattern || !path) return 0;
    
    while (*pattern && *path) {
        if (*pattern == '*') {
            pattern++;
            if (*pattern == '\0') return 1;
            
            while (*path) {
                if (cupolas_entitlements_match_path(pattern, path)) return 1;
                path++;
            }
            return 0;
        } else if (*pattern == '?') {
            pattern++;
            path++;
        } else if (*pattern == *path) {
            pattern++;
            path++;
        } else {
            return 0;
        }
    }
    
    while (*pattern == '*') pattern++;
    
    return *pattern == '\0' && *path == '\0';
}

int cupolas_entitlements_match_host(const char* pattern, const char* host) {
    if (!pattern || !host) return 0;
    
    if (strcmp(pattern, "*") == 0) return 1;
    
    if (pattern[0] == '*' && pattern[1] == '.') {
        const char* suffix = pattern + 1;
        size_t host_len = strlen(host);
        size_t suffix_len = strlen(suffix);
        
        if (host_len >= suffix_len) {
            return strcmp(host + host_len - suffix_len, suffix) == 0;
        }
        return 0;
    }
    
    return strcmp(pattern, host) == 0;
}

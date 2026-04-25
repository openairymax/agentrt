/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * cupolas_vault.c - Secure Credential Storage: iOS Keychain-like Implementation
 */

/**
 * @file cupolas_vault.c
 * @brief Secure Credential Storage - iOS Keychain-like Implementation
 * @author Spharx AgentOS Team
 * @date 2026
 */

#include "cupolas_vault.h"
#include "cupolas_error.h"
#include "utils/cupolas_utils.h"
#include "../platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef cupolas_USE_OPENSSL
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#endif

/* ============================================================================
 * 内部常量
 * ============================================================================ */

#define VAULT_MAGIC 0x564C5453  /* "VLTS" */
#define VAULT_VERSION 1
#define MAX_CREDENTIALS 1024
#define AES_KEY_SIZE 32
#define AES_IV_SIZE 16
#define SALT_SIZE 32

/* ============================================================================
 * 内部结构
 * ============================================================================ */

typedef struct {
    char* cred_id;
    cupolas_vault_cred_type_t type;
    uint8_t* encrypted_data;
    size_t encrypted_len;
    uint8_t iv[AES_IV_SIZE];
    uint8_t salt[SALT_SIZE];
    cupolas_vault_acl_t acl;
    cupolas_vault_metadata_t metadata;
} credential_entry_t;

struct cupolas_vault {
    char* vault_id;
    bool is_locked;
    uint8_t master_key[AES_KEY_SIZE];
    credential_entry_t* entries;
    size_t entry_count;
    size_t entry_capacity;
    cupolas_rwlock_t lock;
    cupolas_vault_config_t config;
};

typedef struct {
    bool initialized;
    cupolas_vault_config_t default_config;
    cupolas_rwlock_t global_lock;
} vault_global_ctx_t;

static vault_global_ctx_t g_vault_ctx = {0};

/* ============================================================================
 * 初始化/清理
 * ============================================================================ */

int cupolas_vault_init(const cupolas_vault_config_t* config) {
    if (g_vault_ctx.initialized) {
        return 0;
    }

    memset(&g_vault_ctx, 0, sizeof(g_vault_ctx));

    if (config) {
        memcpy(&g_vault_ctx.default_config, config, sizeof(cupolas_vault_config_t));
    } else {
        g_vault_ctx.default_config.enable_audit = true;
        g_vault_ctx.default_config.enable_auto_lock = true;
        g_vault_ctx.default_config.auto_lock_seconds = 300;
        g_vault_ctx.default_config.max_retry_count = 3;
    }

    cupolas_rwlock_init(&g_vault_ctx.global_lock);
    g_vault_ctx.initialized = true;

    return 0;
}

void cupolas_vault_cleanup(void) {
    if (!g_vault_ctx.initialized) {
        return;
    }

    cupolas_rwlock_destroy(&g_vault_ctx.global_lock);
    memset(&g_vault_ctx, 0, sizeof(g_vault_ctx));
}

int cupolas_vault_open(const char* vault_id, const char* password, cupolas_vault_t** vault) {
    if (!vault_id || !vault) {
        return -1;
    }

    if (!g_vault_ctx.initialized) {
        cupolas_vault_init(NULL);
    }

    cupolas_vault_t* v = (cupolas_vault_t*)calloc(1, sizeof(cupolas_vault_t));
    if (!v) {
        return -1;
    }

    v->vault_id = strdup(vault_id);
    if (!v->vault_id) {
        free(v);
        return -1;
    }
    v->is_locked = (password == NULL);
    v->entry_capacity = 64;
    v->entries = (credential_entry_t*)calloc(v->entry_capacity, sizeof(credential_entry_t));
    if (!v->entries) {
        free(v->vault_id);
        free(v);
        return -1;
    }
    v->entry_count = 0;

    cupolas_rwlock_init(&v->lock);
    memcpy(&v->config, &g_vault_ctx.default_config, sizeof(cupolas_vault_config_t));

    if (password) {
#ifdef cupolas_USE_OPENSSL
        uint8_t salt[SALT_SIZE] = {0};
        PKCS5_PBKDF2_HMAC(password, strlen(password), salt, SALT_SIZE,
                          100000, EVP_sha256(), AES_KEY_SIZE, v->master_key);
#else
        (void)password;
        memset(v->master_key, 0, AES_KEY_SIZE);
#endif
        v->is_locked = false;
    }

    *vault = v;
    return 0;
}

void cupolas_vault_close(cupolas_vault_t* vault) {
    if (!vault) {
        return;
    }

    cupolas_rwlock_wrlock(&vault->lock);

    free(vault->vault_id);

    if (vault->entries) {
        for (size_t i = 0; i < vault->entry_count; i++) {
            credential_entry_t* entry = &vault->entries[i];
            free(entry->cred_id);
            free(entry->encrypted_data);
            for (size_t j = 0; j < entry->acl.count; j++) {
                free(entry->acl.entries[j].agent_id);
            }
            free(entry->acl.entries);
            cupolas_vault_free_metadata(&entry->metadata);
        }
        free(vault->entries);
    }

    memset(vault->master_key, 0, AES_KEY_SIZE);

    cupolas_rwlock_unlock(&vault->lock);
    cupolas_rwlock_destroy(&vault->lock);

    free(vault);
}

int cupolas_vault_lock(cupolas_vault_t* vault) {
    if (!vault) {
        return -1;
    }

    cupolas_rwlock_wrlock(&vault->lock);
    memset(vault->master_key, 0, AES_KEY_SIZE);
    vault->is_locked = true;
    cupolas_rwlock_unlock(&vault->lock);

    return 0;
}

int cupolas_vault_unlock(cupolas_vault_t* vault, const char* password) {
    if (!vault || !password) {
        return -1;
    }

    cupolas_rwlock_wrlock(&vault->lock);

#ifdef cupolas_USE_OPENSSL
    uint8_t salt[SALT_SIZE] = {0};
    PKCS5_PBKDF2_HMAC(password, strlen(password), salt, SALT_SIZE,
                      100000, EVP_sha256(), AES_KEY_SIZE, vault->master_key);
#else
    (void)password;
    memset(vault->master_key, 0, AES_KEY_SIZE);
#endif

    vault->is_locked = false;
    cupolas_rwlock_unlock(&vault->lock);

    return 0;
}

bool cupolas_vault_is_locked(cupolas_vault_t* vault) {
    if (!vault) {
        return true;
    }

    cupolas_rwlock_rdlock(&vault->lock);
    bool locked = vault->is_locked;
    cupolas_rwlock_unlock(&vault->lock);

    return locked;
}

/* ============================================================================
 * 凭证操作
 * ============================================================================ */

static credential_entry_t* find_entry(cupolas_vault_t* vault, const char* cred_id) {
    for (size_t i = 0; i < vault->entry_count; i++) {
        if (strcmp(vault->entries[i].cred_id, cred_id) == 0) {
            return &vault->entries[i];
        }
    }
    return NULL;
}

int cupolas_vault_store(cupolas_vault_t* vault,
                      const char* cred_id,
                      cupolas_vault_cred_type_t type,
                      const uint8_t* data, size_t data_len,
                      const cupolas_vault_acl_t* acl) {
    if (!vault || !cred_id || !data || data_len == 0) {
        return -1;
    }

    if (vault->is_locked) {
        return -2;
    }

    cupolas_rwlock_wrlock(&vault->lock);

    if (vault->entry_count >= vault->entry_capacity) {
        size_t new_capacity = vault->entry_capacity * 2;
        credential_entry_t* new_entries = realloc(vault->entries,
                                                   new_capacity * sizeof(credential_entry_t));
        if (!new_entries) {
            cupolas_rwlock_unlock(&vault->lock);
            return -3;
        }
        vault->entries = new_entries;
        vault->entry_capacity = new_capacity;
    }

    credential_entry_t* entry = find_entry(vault, cred_id);
    if (entry) {
        free(entry->encrypted_data);
        free(entry->metadata.cred_id);
    } else {
        entry = &vault->entries[vault->entry_count++];
        memset(entry, 0, sizeof(credential_entry_t));
        entry->cred_id = strdup(cred_id);
    }

    entry->type = type;
    entry->metadata.cred_id = strdup(cred_id);
    entry->metadata.type = type;
    entry->metadata.created_at = (uint64_t)time(NULL);
    entry->metadata.updated_at = entry->metadata.created_at;

#ifdef cupolas_USE_OPENSSL
    RAND_bytes(entry->iv, AES_IV_SIZE);
    RAND_bytes(entry->salt, SALT_SIZE);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        cupolas_rwlock_unlock(&vault->lock);
        return -4;
    }

    int len;
    size_t ciphertext_len = data_len + AES_BLOCK_SIZE;
    entry->encrypted_data = (uint8_t*)malloc(ciphertext_len);
    if (!entry->encrypted_data) {
        EVP_CIPHER_CTX_free(ctx);
        cupolas_rwlock_unlock(&vault->lock);
        return -4;
    }

    EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, vault->master_key, entry->iv);
    EVP_EncryptUpdate(ctx, entry->encrypted_data, &len, data, data_len);
    EVP_EncryptFinal_ex(ctx, entry->encrypted_data + len, &len);

    entry->encrypted_len = data_len;
    EVP_CIPHER_CTX_free(ctx);
#else
    cupolas_rwlock_unlock(&vault->lock);
    return -10;
#endif

    if (acl) {
        entry->acl.count = acl->count;
        entry->acl.entries = (cupolas_vault_acl_entry_t*)malloc(
            acl->count * sizeof(cupolas_vault_acl_entry_t));
        if (!entry->acl.entries) {
            cupolas_rwlock_unlock(&vault->lock);
            return -4;
        }
        for (size_t i = 0; i < acl->count; i++) {
            entry->acl.entries[i].agent_id = strdup(acl->entries[i].agent_id);
            entry->acl.entries[i].operations = acl->entries[i].operations;
            entry->acl.entries[i].expires_at = acl->entries[i].expires_at;
        }
    }

    cupolas_rwlock_unlock(&vault->lock);
    return 0;
}

int cupolas_vault_retrieve(cupolas_vault_t* vault,
                         const char* cred_id,
                         const char* agent_id,
                         uint8_t* data_out, size_t* data_len) {
    if (!vault || !cred_id || !data_out || !data_len) {
        return -1;
    }

    if (vault->is_locked) {
        return -2;
    }

    cupolas_rwlock_rdlock(&vault->lock);

    credential_entry_t* entry = find_entry(vault, cred_id);
    if (!entry) {
        cupolas_rwlock_unlock(&vault->lock);
        return -3;
    }

    if (!cupolas_vault_check_access(vault, cred_id, agent_id, CUPOLAS_VAULT_OP_READ)) {
        cupolas_rwlock_unlock(&vault->lock);
        return -4;
    }

    if (*data_len < entry->encrypted_len) {
        *data_len = entry->encrypted_len;
        cupolas_rwlock_unlock(&vault->lock);
        return -5;
    }

#ifdef cupolas_USE_OPENSSL
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        cupolas_rwlock_unlock(&vault->lock);
        return -6;
    }

    int len;
    EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, vault->master_key, entry->iv);
    EVP_DecryptUpdate(ctx, data_out, &len, entry->encrypted_data, entry->encrypted_len);
    EVP_DecryptFinal_ex(ctx, data_out + len, &len);

    *data_len = entry->encrypted_len;
    EVP_CIPHER_CTX_free(ctx);
#else
    memcpy(data_out, entry->encrypted_data, entry->encrypted_len);
    *data_len = entry->encrypted_len;
#endif

    cupolas_rwlock_unlock(&vault->lock);
    return 0;
}

int cupolas_vault_delete(cupolas_vault_t* vault,
                       const char* cred_id,
                       const char* agent_id) {
    if (!vault || !cred_id) {
        return -1;
    }

    if (vault->is_locked) {
        return -2;
    }

    cupolas_rwlock_wrlock(&vault->lock);

    for (size_t i = 0; i < vault->entry_count; i++) {
        if (strcmp(vault->entries[i].cred_id, cred_id) == 0) {
            credential_entry_t* entry = &vault->entries[i];
            free(entry->cred_id);
            free(entry->encrypted_data);
            for (size_t j = 0; j < entry->acl.count; j++) {
                free(entry->acl.entries[j].agent_id);
            }
            free(entry->acl.entries);

            memmove(&vault->entries[i], &vault->entries[i + 1],
                    (vault->entry_count - i - 1) * sizeof(credential_entry_t));
            vault->entry_count--;

            cupolas_rwlock_unlock(&vault->lock);
            return 0;
        }
    }

    cupolas_rwlock_unlock(&vault->lock);
    return -3;
}

bool cupolas_vault_exists(cupolas_vault_t* vault, const char* cred_id) {
    if (!vault || !cred_id) {
        return false;
    }

    cupolas_rwlock_rdlock(&vault->lock);
    credential_entry_t* entry = find_entry(vault, cred_id);
    cupolas_rwlock_unlock(&vault->lock);

    return entry != NULL;
}

int cupolas_vault_update(cupolas_vault_t* vault,
                       const char* cred_id,
                       const uint8_t* data, size_t data_len,
                       const char* agent_id) {
    if (!vault || !cred_id || !data) {
        return -1;
    }

    if (!cupolas_vault_check_access(vault, cred_id, agent_id, CUPOLAS_VAULT_OP_WRITE)) {
        return -2;
    }

    cupolas_rwlock_wrlock(&vault->lock);

    credential_entry_t* entry = find_entry(vault, cred_id);
    if (!entry) {
        cupolas_rwlock_unlock(&vault->lock);
        return -3;
    }

    free(entry->encrypted_data);

#ifdef cupolas_USE_OPENSSL
    RAND_bytes(entry->iv, AES_IV_SIZE);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        cupolas_rwlock_unlock(&vault->lock);
        return -4;
    }

    int len;
    entry->encrypted_data = (uint8_t*)malloc(data_len + AES_BLOCK_SIZE);
    if (!entry->encrypted_data) {
        EVP_CIPHER_CTX_free(ctx);
        cupolas_rwlock_unlock(&vault->lock);
        return -4;
    }

    EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, vault->master_key, entry->iv);
    EVP_EncryptUpdate(ctx, entry->encrypted_data, &len, data, data_len);
    EVP_EncryptFinal_ex(ctx, entry->encrypted_data + len, &len);

    entry->encrypted_len = data_len;
    EVP_CIPHER_CTX_free(ctx);
#else
    cupolas_rwlock_unlock(&vault->lock);
    return -10;
#endif

    entry->metadata.updated_at = (uint64_t)time(NULL);

    cupolas_rwlock_unlock(&vault->lock);
    return 0;
}

/* ============================================================================
 * 访问控制
 * ============================================================================ */

bool cupolas_vault_check_access(cupolas_vault_t* vault,
                               const char* cred_id,
                               const char* agent_id,
                               cupolas_vault_operation_t operation) {
    if (!vault || !cred_id || !agent_id) {
        return false;
    }

    cupolas_rwlock_rdlock(&vault->lock);

    credential_entry_t* entry = find_entry(vault, cred_id);
    if (!entry) {
        cupolas_rwlock_unlock(&vault->lock);
        return false;
    }

    if (entry->acl.count == 0) {
        cupolas_rwlock_unlock(&vault->lock);
        return true;
    }

    for (size_t i = 0; i < entry->acl.count; i++) {
        cupolas_vault_acl_entry_t* acl = &entry->acl.entries[i];
        if (strcmp(acl->agent_id, agent_id) == 0) {
            if (acl->expires_at > 0 && (uint64_t)time(NULL) > acl->expires_at) {
                cupolas_rwlock_unlock(&vault->lock);
                return false;
            }

            if ((acl->operations & (uint32_t)operation) != 0) {
                cupolas_rwlock_unlock(&vault->lock);
                return true;
            }
        }
    }

    cupolas_rwlock_unlock(&vault->lock);
    return false;
}

int cupolas_vault_grant_access(cupolas_vault_t* vault,
                              const char* cred_id,
                              const char* agent_id,
                              uint32_t operations,
                              uint64_t expires_at) {
    if (!vault || !cred_id || !agent_id) {
        return -1;
    }

    cupolas_rwlock_wrlock(&vault->lock);

    credential_entry_t* entry = find_entry(vault, cred_id);
    if (!entry) {
        cupolas_rwlock_unlock(&vault->lock);
        return -2;
    }

    for (size_t i = 0; i < entry->acl.count; i++) {
        if (strcmp(entry->acl.entries[i].agent_id, agent_id) == 0) {
            entry->acl.entries[i].operations = operations;
            entry->acl.entries[i].expires_at = expires_at;
            cupolas_rwlock_unlock(&vault->lock);
            return 0;
        }
    }

    size_t new_count = entry->acl.count + 1;
    cupolas_vault_acl_entry_t* new_entries = realloc(entry->acl.entries,
                                                    new_count * sizeof(cupolas_vault_acl_entry_t));
    if (!new_entries) {
        cupolas_rwlock_unlock(&vault->lock);
        return -3;
    }

    entry->acl.entries = new_entries;
    entry->acl.entries[entry->acl.count].agent_id = strdup(agent_id);
    entry->acl.entries[entry->acl.count].operations = operations;
    entry->acl.entries[entry->acl.count].expires_at = expires_at;
    entry->acl.entries[entry->acl.count].access_count = 0;
    entry->acl.entries[entry->acl.count].max_access_count = 0;
    entry->acl.count = new_count;

    cupolas_rwlock_unlock(&vault->lock);
    return 0;
}

int cupolas_vault_revoke_access(cupolas_vault_t* vault,
                               const char* cred_id,
                               const char* agent_id) {
    if (!vault || !cred_id || !agent_id) {
        return -1;
    }

    cupolas_rwlock_wrlock(&vault->lock);

    credential_entry_t* entry = find_entry(vault, cred_id);
    if (!entry) {
        cupolas_rwlock_unlock(&vault->lock);
        return -2;
    }

    for (size_t i = 0; i < entry->acl.count; i++) {
        if (strcmp(entry->acl.entries[i].agent_id, agent_id) == 0) {
            free(entry->acl.entries[i].agent_id);
            memmove(&entry->acl.entries[i], &entry->acl.entries[i + 1],
                    (entry->acl.count - i - 1) * sizeof(cupolas_vault_acl_entry_t));
            entry->acl.count--;
            cupolas_rwlock_unlock(&vault->lock);
            return 0;
        }
    }

    cupolas_rwlock_unlock(&vault->lock);
    return -3;
}

/* ============================================================================
 * 元数据操作
 * ============================================================================ */

int cupolas_vault_get_metadata(cupolas_vault_t* vault,
                              const char* cred_id,
                              cupolas_vault_metadata_t* metadata) {
    if (!vault || !cred_id || !metadata) {
        return -1;
    }

    cupolas_rwlock_rdlock(&vault->lock);

    credential_entry_t* entry = find_entry(vault, cred_id);
    if (!entry) {
        cupolas_rwlock_unlock(&vault->lock);
        return -2;
    }

    metadata->cred_id = strdup(entry->metadata.cred_id);
    metadata->type = entry->metadata.type;
    metadata->created_at = entry->metadata.created_at;
    metadata->updated_at = entry->metadata.updated_at;
    metadata->expires_at = entry->metadata.expires_at;
    metadata->is_accessible = !vault->is_locked;

    cupolas_rwlock_unlock(&vault->lock);
    return 0;
}

void cupolas_vault_free_metadata(cupolas_vault_metadata_t* metadata) {
    if (!metadata) {
        return;
    }

    free(metadata->cred_id);
    free(metadata->description);
    free(metadata->service);
    free(metadata->account);
    memset(metadata, 0, sizeof(cupolas_vault_metadata_t));
}

int cupolas_vault_list(cupolas_vault_t* vault,
                     cupolas_vault_cred_type_t type,
                     cupolas_vault_metadata_t** metadata_array,
                     size_t* count) {
    if (!vault || !metadata_array || !count) {
        return -1;
    }

    cupolas_rwlock_rdlock(&vault->lock);

    size_t match_count = 0;
    for (size_t i = 0; i < vault->entry_count; i++) {
        if (type == 0 || vault->entries[i].type == type) {
            match_count++;
        }
    }

    if (match_count == 0) {
        *metadata_array = NULL;
        *count = 0;
        cupolas_rwlock_unlock(&vault->lock);
        return 0;
    }

    cupolas_vault_metadata_t* arr = calloc(match_count, sizeof(cupolas_vault_metadata_t));
    if (!arr) {
        cupolas_rwlock_unlock(&vault->lock);
        return -2;
    }

    size_t idx = 0;
    for (size_t i = 0; i < vault->entry_count; i++) {
        if (type == 0 || vault->entries[i].type == type) {
            arr[idx].cred_id = strdup(vault->entries[i].cred_id);
            arr[idx].type = vault->entries[i].type;
            arr[idx].created_at = vault->entries[i].metadata.created_at;
            arr[idx].updated_at = vault->entries[i].metadata.updated_at;
            idx++;
        }
    }

    *metadata_array = arr;
    *count = match_count;

    cupolas_rwlock_unlock(&vault->lock);
    return 0;
}

void cupolas_vault_free_list(cupolas_vault_metadata_t* metadata_array, size_t count) {
    if (!metadata_array) {
        return;
    }

    for (size_t i = 0; i < count; i++) {
        cupolas_vault_free_metadata(&metadata_array[i]);
    }
    free(metadata_array);
}

int cupolas_vault_get_acl(cupolas_vault_t* vault,
                        const char* cred_id,
                        cupolas_vault_acl_t* acl) {
    if (!vault || !cred_id || !acl) {
        return -1;
    }

    cupolas_rwlock_rdlock(&vault->lock);

    credential_entry_t* entry = find_entry(vault, cred_id);
    if (!entry) {
        cupolas_rwlock_unlock(&vault->lock);
        return -2;
    }

    acl->count = entry->acl.count;
    acl->entries = calloc(entry->acl.count, sizeof(cupolas_vault_acl_entry_t));
    if (!acl->entries && entry->acl.count > 0) {
        cupolas_rwlock_unlock(&vault->lock);
        return -4;
    }
    for (size_t i = 0; i < entry->acl.count; i++) {
        acl->entries[i].agent_id = strdup(entry->acl.entries[i].agent_id);
        acl->entries[i].operations = entry->acl.entries[i].operations;
        acl->entries[i].expires_at = entry->acl.entries[i].expires_at;
    }

    cupolas_rwlock_unlock(&vault->lock);
    return 0;
}

void cupolas_vault_free_acl(cupolas_vault_acl_t* acl) {
    if (!acl || !acl->entries) {
        return;
    }

    for (size_t i = 0; i < acl->count; i++) {
        free(acl->entries[i].agent_id);
    }
    free(acl->entries);
    acl->entries = NULL;
    acl->count = 0;
}

/* ============================================================================
 * 辅助函数
 * ============================================================================ */

const char* cupolas_vault_cred_type_string(cupolas_vault_cred_type_t type) {
    switch (type) {
        case CUPOLAS_VAULT_CRED_PASSWORD:    return "password";
        case CUPOLAS_VAULT_CRED_TOKEN:       return "token";
        case CUPOLAS_VAULT_CRED_KEY:         return "key";
        case CUPOLAS_VAULT_CRED_CERTIFICATE: return "certificate";
        case CUPOLAS_VAULT_CRED_SECRET:      return "secret";
        case CUPOLAS_VAULT_CRED_NOTE:        return "note";
        default:                           return "unknown";
    }
}

const char* cupolas_vault_operation_string(cupolas_vault_operation_t op) {
    switch (op) {
        case CUPOLAS_VAULT_OP_READ:   return "read";
        case CUPOLAS_VAULT_OP_WRITE:  return "write";
        case CUPOLAS_VAULT_OP_DELETE: return "delete";
        case CUPOLAS_VAULT_OP_EXPORT: return "export";
        default:                    return "unknown";
    }
}

int cupolas_vault_generate_password(char* password_out, size_t length) {
    if (!password_out || length < 8) {
        return -1;
    }

    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*";
    size_t charset_len = strlen(charset);

#ifdef cupolas_USE_OPENSSL
    for (size_t i = 0; i < length - 1; i++) {
        unsigned char c;
        RAND_bytes(&c, 1);
        password_out[i] = charset[c % charset_len];
    }
#else
    FILE* urandom = fopen("/dev/urandom", "rb");
    if (urandom) {
        for (size_t i = 0; i < length - 1; i++) {
            unsigned char c;
            if (fread(&c, 1, 1, urandom) != 1) {
                fclose(urandom);
                password_out[length - 1] = '\0';
                return -2;
            }
            password_out[i] = charset[c % charset_len];
        }
        fclose(urandom);
    } else {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        unsigned int seed = (unsigned int)(ts.tv_sec ^ ts.tv_nsec ^ (uintptr_t)password_out);
        srand(seed);
        for (size_t i = 0; i < length - 1; i++) {
            password_out[i] = charset[rand() % charset_len];
        }
    }
#endif

    password_out[length - 1] = '\0';
    return 0;
}

int cupolas_vault_generate_keypair(char* public_key_out, size_t* pub_len,
                                  char* private_key_out, size_t* priv_len) {
    if (!public_key_out || !pub_len || !private_key_out || !priv_len) {
        return -1;
    }

#ifdef cupolas_USE_OPENSSL
    EVP_PKEY* pkey = NULL;
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
    if (!ctx) {
        return -2;
    }

    EVP_PKEY_keygen_init(ctx);
    EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048);
    EVP_PKEY_keygen(ctx, &pkey);
    EVP_PKEY_CTX_free(ctx);

    BIO* pub_bio = BIO_new(BIO_s_mem());
    BIO* priv_bio = BIO_new(BIO_s_mem());

    PEM_write_bio_PUBKEY(pub_bio, pkey);
    PEM_write_bio_PrivateKey(priv_bio, pkey, NULL, NULL, 0, NULL, NULL);

    *pub_len = BIO_read(pub_bio, public_key_out, *pub_len);
    *priv_len = BIO_read(priv_bio, private_key_out, *priv_len);

    BIO_free(pub_bio);
    BIO_free(priv_bio);
    EVP_PKEY_free(pkey);

    return 0;
#else
    (void)public_key_out;
    (void)private_key_out;
    (void)pub_len;
    (void)priv_len;
    return -1; /* fail-closed: no crypto = no key generation */
#endif
}

int cupolas_vault_export(cupolas_vault_t* vault,
                        const char* export_path,
                        const char* password,
                        const char* agent_id) {
    if (!vault || !export_path || !password) {
        return -1;
    }

    if (vault->is_locked) {
        return -2;
    }

#ifdef cupolas_USE_OPENSSL
    FILE* f = fopen(export_path, "wb");
    if (!f) {
        return -3;
    }

    uint8_t file_salt[SALT_SIZE];
    if (RAND_bytes(file_salt, SALT_SIZE) != 1) {
        fclose(f);
        return -4;
    }

    uint8_t derived_key[AES_KEY_SIZE];
    uint8_t derived_iv[AES_IV_SIZE];

    if (PKCS5_PBKDF2_HMAC(password, strlen(password),
                           file_salt, SALT_SIZE, 10000,
                           EVP_sha256(), AES_KEY_SIZE + AES_IV_SIZE,
                           derived_key) != 1) {
        fclose(f);
        remove(export_path);
        return -5;
    }

    memcpy(derived_iv, derived_key + AES_KEY_SIZE, AES_IV_SIZE);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        fclose(f);
        remove(export_path);
        return -6;
    }

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, derived_key, derived_iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        fclose(f);
        remove(export_path);
        return -7;
    }

    fwrite(&VAULT_MAGIC, sizeof(VAULT_MAGIC), 1, f);
    fwrite(&VAULT_VERSION, sizeof(VAULT_VERSION), 1, f);
    fwrite(file_salt, SALT_SIZE, 1, f);

    time_t now = time(NULL);
    fwrite(&now, sizeof(time_t), 1, f);

    if (agent_id) {
        size_t agent_len = strlen(agent_id);
        fwrite(&agent_len, sizeof(size_t), 1, f);
        fwrite(agent_id, 1, agent_len, f);
    } else {
        size_t zero = 0;
        fwrite(&zero, sizeof(size_t), 1, f);
    }

    uint32_t cred_count = (uint32_t)vault->entry_count;
    fwrite(&cred_count, sizeof(uint32_t), 1, f);

    for (size_t i = 0; i < vault->entry_count; i++) {
        credential_entry_t* entry = &vault->entries[i];

        size_t id_len = strlen(entry->cred_id);
        fwrite(&id_len, sizeof(size_t), 1, f);
        fwrite(entry->cred_id, 1, id_len, f);

        fwrite(&entry->type, sizeof(cupolas_vault_cred_type_t), 1, f);
        fwrite(&entry->encrypted_len, sizeof(size_t), 1, f);

        int out_len = 0;
        int final_len = 0;
        uint8_t* encrypted_buf = (uint8_t*)malloc(entry->encrypted_len + AES_BLOCK_SIZE);
        if (!encrypted_buf || EVP_EncryptUpdate(ctx, encrypted_buf, &out_len,
                                               entry->encrypted_data, entry->encrypted_len) != 1) {
            free(encrypted_buf);
            EVP_CIPHER_CTX_free(ctx);
            fclose(f);
            remove(export_path);
            return -8;
        }

        if (EVP_EncryptFinal_ex(ctx, encrypted_buf + out_len, &final_len) != 1) {
            free(encrypted_buf);
            EVP_CIPHER_CTX_free(ctx);
            fclose(f);
            remove(export_path);
            return -9;
        }

        size_t total_encrypted = out_len + final_len;
        fwrite(&total_encrypted, sizeof(size_t), 1, f);
        fwrite(encrypted_buf, 1, total_encrypted, f);
        free(encrypted_buf);

        fwrite(entry->iv, AES_IV_SIZE, 1, f);
        fwrite(entry->salt, SALT_SIZE, 1, f);
        fwrite(&entry->acl, sizeof(cupolas_vault_acl_t), 1, f);
        fwrite(&entry->metadata, sizeof(cupolas_vault_metadata_t), 1, f);
    }

    EVP_CIPHER_CTX_free(ctx);
    fclose(f);

    if (vault->config.enable_audit) {
        printf("[VAULT] Export completed: %zu credentials to %s\n",
               vault->entry_count, export_path);
    }

    return 0;
#else
    (void)agent_id;
    return -10;
#endif
}

int cupolas_vault_import(cupolas_vault_t* vault,
                        const char* import_path,
                        const char* password,
                        const char* agent_id) {
    if (!vault || !import_path || !password) {
        return -1;
    }

    if (vault->is_locked) {
        return -2;
    }

#ifdef cupolas_USE_OPENSSL
    FILE* f = fopen(import_path, "rb");
    if (!f) {
        return -3;
    }

    uint32_t magic = 0;
    if (fread(&magic, sizeof(uint32_t), 1, f) != 1 || magic != VAULT_MAGIC) {
        fclose(f);
        return -4;
    }

    uint32_t version = 0;
    if (fread(&version, sizeof(uint32_t), 1, f) != 1 || version > VAULT_VERSION) {
        fclose(f);
        return -5;
    }

    uint8_t file_salt[SALT_SIZE];
    if (fread(file_salt, SALT_SIZE, 1, f) != 1) {
        fclose(f);
        return -6;
    }

    time_t export_time = 0;
    fread(&export_time, sizeof(time_t), 1, f);

    size_t agent_len = 0;
    fread(&agent_len, sizeof(size_t), 1, f);
    if (agent_len > 0) {
        char* file_agent_id = (char*)malloc(agent_len + 1);
        if (file_agent_id) {
            fread(file_agent_id, 1, agent_len, f);
            file_agent_id[agent_len] = '\0';
            free(file_agent_id);
        } else {
            fseek(f, agent_len, SEEK_CUR);
        }
    }

    uint32_t cred_count = 0;
    if (fread(&cred_count, sizeof(uint32_t), 1, f) != 1) {
        fclose(f);
        return -7;
    }

    uint8_t derived_key[AES_KEY_SIZE];
    uint8_t derived_iv[AES_IV_SIZE];

    if (PKCS5_PBKDF2_HMAC(password, strlen(password),
                           file_salt, SALT_SIZE, 10000,
                           EVP_sha256(), AES_KEY_SIZE + AES_IV_SIZE,
                           derived_key) != 1) {
        fclose(f);
        return -8;
    }

    memcpy(derived_iv, derived_key + AES_KEY_SIZE, AES_IV_SIZE);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        fclose(f);
        return -9;
    }

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, derived_key, derived_iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        fclose(f);
        return -10;
    }

    size_t imported = 0;

    for (uint32_t i = 0; i < cred_count; i++) {
        size_t id_len = 0;
        if (fread(&id_len, sizeof(size_t), 1, f) != 1) break;

        char* cred_id = (char*)malloc(id_len + 1);
        if (!cred_id || fread(cred_id, 1, id_len, f) != 1) {
            free(cred_id);
            break;
        }
        cred_id[id_len] = '\0';

        cupolas_vault_cred_type_t type;
        if (fread(&type, sizeof(cupolas_vault_cred_type_t), 1, f) != 1) {
            free(cred_id);
            break;
        }

        size_t enc_len = 0;
        if (fread(&enc_len, sizeof(size_t), 1, f) != 1) {
            free(cred_id);
            break;
        }

        uint8_t* enc_data = (uint8_t*)malloc(enc_len);
        if (!enc_data || fread(enc_data, 1, enc_len, f) != 1) {
            free(cred_id);
            free(enc_data);
            break;
        }

        int out_len = 0;
        int final_len = 0;
        uint8_t* decrypted = (uint8_t*)malloc(enc_len + AES_BLOCK_SIZE);
        if (!decrypted ||
            EVP_DecryptUpdate(ctx, decrypted, &out_len, enc_data, enc_len) != 1 ||
            EVP_DecryptFinal_ex(ctx, decrypted + out_len, &final_len) != 1) {
            free(cred_id);
            free(enc_data);
            free(decrypted);
            continue;
        }

        size_t total_decrypted = out_len + final_len;

        if (vault->entry_count >= vault->entry_capacity) {
            size_t new_cap = vault->entry_capacity * 2;
            credential_entry_t* new_entries = (credential_entry_t*)realloc(
                vault->entries, new_cap * sizeof(credential_entry_t));
            if (!new_entries) {
                free(cred_id);
                free(enc_data);
                free(decrypted);
                continue;
            }
            vault->entries = new_entries;
            vault->entry_capacity = new_cap;
        }

        credential_entry_t* entry = &vault->entries[vault->entry_count];
        entry->cred_id = cred_id;
        entry->type = type;
        entry->encrypted_data = (uint8_t*)malloc(total_decrypted);
        if (entry->encrypted_data) {
            memcpy(entry->encrypted_data, decrypted, total_decrypted);
            entry->encrypted_len = total_decrypted;
        } else {
            entry->encrypted_data = NULL;
            entry->encrypted_len = 0;
        }

        fread(entry->iv, AES_IV_SIZE, 1, f);
        fread(entry->salt, SALT_SIZE, 1, f);
        fread(&entry->acl, sizeof(cupolas_vault_acl_t), 1, f);
        fread(&entry->metadata, sizeof(cupolas_vault_metadata_t), 1, f);

        vault->entry_count++;
        imported++;

        free(enc_data);
        free(decrypted);
    }

    EVP_CIPHER_CTX_free(ctx);
    fclose(f);

    if (vault->config.enable_audit) {
        printf("[VAULT] Import completed: %zu credentials from %s\n",
               imported, import_path);
    }

    return (int)imported;
#else
    (void)password;
    (void)agent_id;

    FILE* f = fopen(import_path, "r");
    if (!f) return -3;

    char line[1024];
    size_t imported = 0;
    bool in_entry = false;
    char current_id[256] = {0};

    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';

        if (line[0] == '[' && line[strlen(line)-1] == ']') {
            if (in_entry && current_id[0]) {
                if (vault->entry_count < vault->entry_capacity) {
                    credential_entry_t* entry = &vault->entries[vault->entry_count];
                    entry->cred_id = strdup(current_id);
                entry->type = 0;
                    entry->encrypted_data = NULL;
                    entry->encrypted_len = 0;
                    memset(&entry->acl, 0, sizeof(entry->acl));
                    memset(&entry->metadata, 0, sizeof(entry->metadata));
                    vault->entry_count++;
                    imported++;
                }
            }
            strncpy(current_id, line + 1, strlen(line) - 2);
            current_id[strlen(line) - 2] = '\0';
            in_entry = true;
        }
    }

    if (in_entry && current_id[0] && vault->entry_count < vault->entry_capacity) {
        credential_entry_t* entry = &vault->entries[vault->entry_count];
        entry->cred_id = strdup(current_id);
        entry->type = 0;
        entry->encrypted_data = NULL;
        entry->encrypted_len = 0;
        memset(&entry->acl, 0, sizeof(entry->acl));
        memset(&entry->metadata, 0, sizeof(entry->metadata));
        vault->entry_count++;
        imported++;
    }

    fclose(f);
    return (int)imported;
#endif
}

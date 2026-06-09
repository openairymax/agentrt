#include "error.h"
#include "cupolas.h"
/* SPDX-License-Identifier: Apache-2.0 */
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

#include "../platform/platform.h"
#include "atomic_compat.h"
#include "cupolas_error.h"
#include "logging_compat.h"
#include "memory_compat.h"
#include "utils/cupolas_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef CUPOLAS_USE_OPENSSL
#include <openssl/aes.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#endif

/* ============================================================================
 * 内部常量
 * ============================================================================ */

#define VAULT_MAGIC 0x564C5453 /* "VLTS" */
#define VAULT_VERSION 1
#define MAX_CREDENTIALS 1024
#define AES_KEY_SIZE 32
#define AES_IV_SIZE 16
#define AES_GCM_TAG_SIZE 16
#define SALT_SIZE 32
#define MAX_ACL_ENTRIES_PER_CREDENTIAL 64

/* SEC-13: OOM 关键路径预分配常量 */
#define VAULT_OOM_PREALLOC_SLOTS 16  /**< OOM 时预分配的凭证槽位数 */
#define VAULT_OOM_PREALLOC_DATA_SIZE 4096  /**< OOM 时预分配的加密数据缓冲区大小 */

/* ============================================================================
 * 内部结构
 * ============================================================================ */

typedef struct {
    char *cred_id;
    cupolas_vault_cred_type_t type;
    uint8_t *encrypted_data;
    size_t encrypted_len;
    uint8_t iv[AES_IV_SIZE];
    uint8_t tag[AES_GCM_TAG_SIZE];
    uint8_t salt[SALT_SIZE];
    cupolas_vault_acl_t acl;
    cupolas_vault_metadata_t metadata;
} credential_entry_t;

struct cupolas_vault {
    char *vault_id;
    bool is_locked;
    uint8_t master_key[AES_KEY_SIZE];
    credential_entry_t *entries;
    size_t entry_count;
    size_t entry_capacity;
    cupolas_rwlock_t lock;
    cupolas_vault_config_t config;
};

typedef struct {
    atomic_int initialized;
    cupolas_vault_config_t default_config;
    cupolas_rwlock_t global_lock;
} vault_global_ctx_t;

static vault_global_ctx_t g_vault_ctx = {0};

/* ============================================================================
 * SEC-13: OOM 关键路径预分配池
 *
 * 启动时预分配 16 个凭证槽位和加密数据缓冲区。
 * 当系统 OOM 时，使用预分配池代替系统 malloc，
 * 确保证书存储关键路径在内存压力下仍可工作。
 * ============================================================================ */

/** @brief OOM 预分配凭证条目池 */
static credential_entry_t g_vault_oom_entries[VAULT_OOM_PREALLOC_SLOTS];

/** @brief OOM 预分配加密数据缓冲区池 */
static uint8_t g_vault_oom_data[VAULT_OOM_PREALLOC_SLOTS][VAULT_OOM_PREALLOC_DATA_SIZE];

/** @brief OOM 预分配池使用位图 */
static bool g_vault_oom_used[VAULT_OOM_PREALLOC_SLOTS];

/** @brief OOM 预分配池互斥锁 */
static cupolas_mutex_t g_vault_oom_lock;

/** @brief OOM 预分配池是否已初始化 */
static bool g_vault_oom_initialized = false;

/**
 * @brief 初始化 OOM 预分配池（SEC-13.1）
 *
 * 在 cupolas_vault_init() 中调用，启动时预分配资源。
 * 零内存分配——使用静态数组。
 */
static void vault_oom_pool_init(void)
{
    if (g_vault_oom_initialized) {
        return;
    }

    cupolas_mutex_init(&g_vault_oom_lock);

    __builtin_memset(g_vault_oom_entries, 0, sizeof(g_vault_oom_entries));
    __builtin_memset(g_vault_oom_data, 0, sizeof(g_vault_oom_data));
    __builtin_memset(g_vault_oom_used, 0, sizeof(g_vault_oom_used));

    g_vault_oom_initialized = true;
}

/**
 * @brief 从 OOM 预分配池获取一个凭证槽位（SEC-13.1）
 *
 * 当正常 AGENTOS_CALLOC 失败时调用。
 * 从预分配池中分配一个槽位，不调用系统 malloc。
 *
 * @return 凭证条目指针，池耗尽返回 NULL
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static credential_entry_t *vault_oom_pool_alloc(void)
{
    if (!g_vault_oom_initialized) {
        return NULL;
    }

    cupolas_mutex_lock(&g_vault_oom_lock);

    for (int i = 0; i < VAULT_OOM_PREALLOC_SLOTS; i++) {
        if (!g_vault_oom_used[i]) {
            g_vault_oom_used[i] = true;
            __builtin_memset(&g_vault_oom_entries[i], 0, sizeof(credential_entry_t));
            cupolas_mutex_unlock(&g_vault_oom_lock);
            return &g_vault_oom_entries[i];
        }
    }

    cupolas_mutex_unlock(&g_vault_oom_lock);
    return NULL;
}

/**
 * @brief 从 OOM 预分配池获取加密数据缓冲区（SEC-13.1）
 *
 * 返回与槽位索引对应的预分配数据缓冲区。
 *
 * @param entry 从 vault_oom_pool_alloc() 获取的条目指针
 * @return 数据缓冲区指针，无效条目返回 NULL
 */
static uint8_t *vault_oom_pool_get_data_buffer(credential_entry_t *entry)
{
    if (!entry || !g_vault_oom_initialized) {
        return NULL;
    }

    /* 计算条目在静态数组中的索引 */
    ptrdiff_t index = entry - g_vault_oom_entries;
    if (index < 0 || index >= VAULT_OOM_PREALLOC_SLOTS) {
        return NULL;
    }

    return g_vault_oom_data[index];
}

/**
 * @brief 释放 OOM 预分配池槽位（SEC-13.1）
 *
 * @param entry 要释放的条目指针
 */
static void vault_oom_pool_free(credential_entry_t *entry)
{
    if (!entry || !g_vault_oom_initialized) {
        return;
    }

    ptrdiff_t index = entry - g_vault_oom_entries;
    if (index < 0 || index >= VAULT_OOM_PREALLOC_SLOTS) {
        return;
    }

    cupolas_mutex_lock(&g_vault_oom_lock);
    g_vault_oom_used[index] = false;
    cupolas_mutex_unlock(&g_vault_oom_lock);
}
#pragma GCC diagnostic pop

/* ============================================================================
 * 初始化/清理
 * ============================================================================ */

#define VLT_INIT_UNINIT 0
#define VLT_INIT_PROGRESS 1
#define VLT_INIT_COMPLETE 2

int cupolas_vault_init(const cupolas_vault_config_t *config)
{
    if (atomic_load(&g_vault_ctx.initialized) == VLT_INIT_COMPLETE) {
        return 0;
    }

    int expected = VLT_INIT_UNINIT;
    if (atomic_compare_exchange_strong(&g_vault_ctx.initialized, &expected, VLT_INIT_PROGRESS)) {
        __builtin_memset(&g_vault_ctx, 0, sizeof(g_vault_ctx));

        if (config) {
            __builtin_memcpy(&g_vault_ctx.default_config, config, sizeof(cupolas_vault_config_t));
        } else {
            g_vault_ctx.default_config.enable_audit = true;
            g_vault_ctx.default_config.enable_auto_lock = true;
            g_vault_ctx.default_config.auto_lock_seconds = 300;
            g_vault_ctx.default_config.max_retry_count = 3;
        }

        cupolas_rwlock_init(&g_vault_ctx.global_lock);

        /* SEC-13: 初始化 OOM 预分配池 */
        vault_oom_pool_init();

        atomic_store(&g_vault_ctx.initialized, VLT_INIT_COMPLETE);
        return 0;
    }

    while (atomic_load(&g_vault_ctx.initialized) != VLT_INIT_COMPLETE) {
        sched_yield();
    }
    return 0;
}

void cupolas_vault_cleanup(void)
{
    if (atomic_load(&g_vault_ctx.initialized) != VLT_INIT_COMPLETE) {
        return;
    }

    cupolas_rwlock_destroy(&g_vault_ctx.global_lock);
    __builtin_memset(&g_vault_ctx, 0, sizeof(g_vault_ctx));
}

int cupolas_vault_open(const char *vault_id, const char *password, cupolas_vault_t **vault)
{
    if (!vault_id || !vault) {
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_UNKNOWN, "cupolas_vault_init: config validation failed");
        return AGENTOS_ERR_UNKNOWN;
    }

    if (atomic_load(&g_vault_ctx.initialized) != VLT_INIT_COMPLETE) {
        cupolas_vault_init(NULL);
    }

    cupolas_vault_t *v = (cupolas_vault_t *)AGENTOS_CALLOC(1, sizeof(cupolas_vault_t));
    if (!v) {
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_UNKNOWN, "cupolas_vault_init: storage dir create failed");
        return AGENTOS_ERR_UNKNOWN;
    }

    v->vault_id = AGENTOS_STRDUP(vault_id);
    if (!v->vault_id) {
        AGENTOS_FREE(v);
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_UNKNOWN, "cupolas_vault_init: vault file open failed");
        return AGENTOS_ERR_UNKNOWN;
    }
    v->is_locked = (password == NULL);
    v->entry_capacity = 64;
    v->entries =
        (credential_entry_t *)AGENTOS_CALLOC(v->entry_capacity, sizeof(credential_entry_t));
    if (!v->entries) {
        AGENTOS_FREE(v->vault_id);
        AGENTOS_FREE(v);
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_UNKNOWN, "cupolas_vault_init: mutex init failed");
        return AGENTOS_ERR_UNKNOWN;
    }
    v->entry_count = 0;

    cupolas_rwlock_init(&v->lock);
    __builtin_memcpy(&v->config, &g_vault_ctx.default_config, sizeof(cupolas_vault_config_t));

    if (password) {
#ifdef CUPOLAS_USE_OPENSSL
        uint8_t salt[SALT_SIZE] = {0};
        uint8_t id_hash[SHA256_DIGEST_LENGTH];
        SHA256((const unsigned char *)vault_id, strlen(vault_id), id_hash);
        __builtin_memcpy(salt, id_hash, SALT_SIZE);
        if (PKCS5_PBKDF2_HMAC(password, strlen(password), salt, SALT_SIZE, 100000, EVP_sha256(),
                              AES_KEY_SIZE, v->master_key) != 1) {
            cupolas_rwlock_destroy(&v->lock);
            AGENTOS_FREE(v->entries);
            AGENTOS_FREE(v->vault_id);
            AGENTOS_FREE(v);
            AGENTOS_ERROR_HANDLE(AGENTOS_ERR_UNKNOWN, "cupolas_vault_init: key derivation failed");
            return AGENTOS_ERR_UNKNOWN;
        }
#else
        cupolas_rwlock_destroy(&v->lock);
        AGENTOS_FREE(v->entries);
        AGENTOS_FREE(v->vault_id);
        AGENTOS_FREE(v);
        return cupolas_VAULT_ERR_CRYPTO_UNAVAILABLE;
#endif
        v->is_locked = false;
    }

    *vault = v;
    return 0;
}

void cupolas_vault_close(cupolas_vault_t *vault)
{
    if (!vault) {
        return;
    }

    cupolas_rwlock_wrlock(&vault->lock);

    AGENTOS_FREE(vault->vault_id);

    if (vault->entries) {
        for (size_t i = 0; i < vault->entry_count; i++) {
            credential_entry_t *entry = &vault->entries[i];
            AGENTOS_FREE(entry->cred_id);
            AGENTOS_FREE(entry->encrypted_data);
            for (size_t j = 0; j < entry->acl.count; j++) {
                AGENTOS_FREE(entry->acl.entries[j].agent_id);
            }
            AGENTOS_FREE(entry->acl.entries);
            cupolas_vault_free_metadata(&entry->metadata);
        }
        AGENTOS_FREE(vault->entries);
    }

    __builtin_memset(vault->master_key, 0, AES_KEY_SIZE);

    cupolas_rwlock_unlock(&vault->lock);
    cupolas_rwlock_destroy(&vault->lock);

    AGENTOS_FREE(vault);
}

int cupolas_vault_lock(cupolas_vault_t *vault)
{
    if (!vault) {
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_UNKNOWN, "cupolas_vault_store: not initialized");
        return AGENTOS_ERR_UNKNOWN;
    }

    cupolas_rwlock_wrlock(&vault->lock);
    __builtin_memset(vault->master_key, 0, AES_KEY_SIZE);
    vault->is_locked = true;
    cupolas_rwlock_unlock(&vault->lock);

    return 0;
}

int cupolas_vault_unlock(cupolas_vault_t *vault, const char *password)
{
    if (!vault || !password) {
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_UNKNOWN, "cupolas_vault_store: encryption failed");
        return AGENTOS_ERR_UNKNOWN;
    }

    cupolas_rwlock_wrlock(&vault->lock);

#ifdef CUPOLAS_USE_OPENSSL
    uint8_t salt[SALT_SIZE] = {0};
    uint8_t id_hash[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char *)vault->vault_id, strlen(vault->vault_id), id_hash);
    __builtin_memcpy(salt, id_hash, SALT_SIZE);
    if (PKCS5_PBKDF2_HMAC(password, strlen(password), salt, SALT_SIZE, 100000, EVP_sha256(),
                          AES_KEY_SIZE, vault->master_key) != 1) {
        cupolas_rwlock_unlock(&vault->lock);
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_UNKNOWN, "cupolas_vault_store: write failed");
        return AGENTOS_ERR_UNKNOWN;
    }
#else
    cupolas_rwlock_unlock(&vault->lock);
    return cupolas_VAULT_ERR_CRYPTO_UNAVAILABLE;
#endif

    vault->is_locked = false;
    cupolas_rwlock_unlock(&vault->lock);

    return 0;
}

bool cupolas_vault_is_locked(cupolas_vault_t *vault)
{
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

static credential_entry_t *find_entry(cupolas_vault_t *vault, const char *cred_id)
{
    for (size_t i = 0; i < vault->entry_count; i++) {
        if (strcmp(vault->entries[i].cred_id, cred_id) == 0) {
            return &vault->entries[i];
        }
    }
    return NULL;
}

int cupolas_vault_store(cupolas_vault_t *vault, const char *cred_id, cupolas_vault_cred_type_t type,
                        const uint8_t *data, size_t data_len, const cupolas_vault_acl_t *acl)
{
    if (!vault || !cred_id || !data || data_len == 0) {
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_UNKNOWN, "cupolas_vault_retrieve: not initialized");
        return AGENTOS_ERR_UNKNOWN;
    }

    if (vault->is_locked) {
        return cupolas_ERR_INVALID_PARAM;
    }

    cupolas_rwlock_wrlock(&vault->lock);

    if (vault->entry_count >= vault->entry_capacity) {
        size_t new_capacity = vault->entry_capacity * 2;
        credential_entry_t *new_entries =
            AGENTOS_REALLOC(vault->entries, new_capacity * sizeof(credential_entry_t));
        if (!new_entries) {
            cupolas_rwlock_unlock(&vault->lock);
            return cupolas_ERR_NULL_POINTER;
        }
        vault->entries = new_entries;
        vault->entry_capacity = new_capacity;
    }

    credential_entry_t *entry = find_entry(vault, cred_id);
    int existed = (entry != NULL);
    if (entry) {
        AGENTOS_FREE(entry->encrypted_data);
        entry->encrypted_data = NULL;
        AGENTOS_FREE(entry->metadata.cred_id);
        entry->metadata.cred_id = NULL;
    } else {
        entry = &vault->entries[vault->entry_count];
        __builtin_memset(entry, 0, sizeof(credential_entry_t));
        entry->cred_id = AGENTOS_STRDUP(cred_id);
    }

    entry->type = type;
    entry->metadata.cred_id = AGENTOS_STRDUP(cred_id);
    entry->metadata.type = type;
    entry->metadata.created_at = (uint64_t)time(NULL);
    entry->metadata.updated_at = entry->metadata.created_at;

#ifdef CUPOLAS_USE_OPENSSL
    if (RAND_bytes(entry->iv, AES_IV_SIZE) != 1 || RAND_bytes(entry->salt, SALT_SIZE) != 1) {
        if (!existed) {
            AGENTOS_FREE(entry->cred_id);
            AGENTOS_FREE(entry->metadata.cred_id);
            __builtin_memset(entry, 0, sizeof(credential_entry_t));
        }
        cupolas_rwlock_unlock(&vault->lock);
        return cupolas_ERR_OUT_OF_MEMORY;
    }

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        if (!existed) {
            AGENTOS_FREE(entry->cred_id);
            AGENTOS_FREE(entry->metadata.cred_id);
            __builtin_memset(entry, 0, sizeof(credential_entry_t));
        } else {
            entry->encrypted_data = NULL;
        }
        cupolas_rwlock_unlock(&vault->lock);
        return cupolas_ERR_OUT_OF_MEMORY;
    }

    int len = 0;
    size_t ciphertext_len = data_len + AES_BLOCK_SIZE;
    entry->encrypted_data = (uint8_t *)AGENTOS_MALLOC(ciphertext_len);
    if (!entry->encrypted_data) {
        EVP_CIPHER_CTX_free(ctx);
        if (!existed) {
            AGENTOS_FREE(entry->cred_id);
            AGENTOS_FREE(entry->metadata.cred_id);
            __builtin_memset(entry, 0, sizeof(credential_entry_t));
        } else {
            entry->encrypted_data = NULL;
        }
        cupolas_rwlock_unlock(&vault->lock);
        return cupolas_ERR_OUT_OF_MEMORY;
    }

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, vault->master_key, entry->iv) != 1 ||
        EVP_EncryptUpdate(ctx, entry->encrypted_data, &len, data, data_len) != 1 ||
        EVP_EncryptFinal_ex(ctx, entry->encrypted_data + len, &len) != 1) {
        AGENTOS_FREE(entry->encrypted_data);
        entry->encrypted_data = NULL;
        EVP_CIPHER_CTX_free(ctx);
        if (!existed) {
            AGENTOS_FREE(entry->cred_id);
            AGENTOS_FREE(entry->metadata.cred_id);
            __builtin_memset(entry, 0, sizeof(credential_entry_t));
        } else {
            entry->encrypted_data = NULL;
        }
        cupolas_rwlock_unlock(&vault->lock);
        return cupolas_ERR_OUT_OF_MEMORY;
    }

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, AES_GCM_TAG_SIZE, entry->tag) != 1) {
        AGENTOS_FREE(entry->encrypted_data);
        entry->encrypted_data = NULL;
        EVP_CIPHER_CTX_free(ctx);
        if (!existed) {
            AGENTOS_FREE(entry->cred_id);
            AGENTOS_FREE(entry->metadata.cred_id);
            __builtin_memset(entry, 0, sizeof(credential_entry_t));
        } else {
            entry->encrypted_data = NULL;
        }
        cupolas_rwlock_unlock(&vault->lock);
        return cupolas_ERR_OUT_OF_MEMORY;
    }

    entry->encrypted_len = data_len;
    EVP_CIPHER_CTX_free(ctx);
#else
    AGENTOS_FREE(entry->metadata.cred_id);
    entry->metadata.cred_id = NULL;
    if (!existed) {
        AGENTOS_FREE(entry->cred_id);
        entry->cred_id = NULL;
    } else {
        entry->encrypted_data = NULL;
    }
    cupolas_rwlock_unlock(&vault->lock);
    return cupolas_VAULT_ERR_CRYPTO_UNAVAILABLE;
#endif

    if (acl) {
        entry->acl.count = acl->count;
        entry->acl.entries = (cupolas_vault_acl_entry_t *)AGENTOS_MALLOC(
            acl->count * sizeof(cupolas_vault_acl_entry_t));
        if (!entry->acl.entries) {
            AGENTOS_FREE(entry->encrypted_data);
            entry->encrypted_data = NULL;
            entry->encrypted_len = 0;
            AGENTOS_FREE(entry->metadata.cred_id);
            entry->metadata.cred_id = NULL;
            if (!existed) {
                AGENTOS_FREE(entry->cred_id);
                entry->cred_id = NULL;
            } else {
                entry->encrypted_data = NULL;
            }
            cupolas_rwlock_unlock(&vault->lock);
            return cupolas_ERR_OUT_OF_MEMORY;
        }
        for (size_t i = 0; i < acl->count; i++) {
            entry->acl.entries[i].agent_id = AGENTOS_STRDUP(acl->entries[i].agent_id);
            if (!entry->acl.entries[i].agent_id) {
                for (size_t j = 0; j < i; j++) {
                    AGENTOS_FREE(entry->acl.entries[j].agent_id);
                    entry->acl.entries[j].agent_id = NULL;
                }
                AGENTOS_FREE(entry->acl.entries);
                entry->acl.entries = NULL;
                entry->acl.count = 0;
                AGENTOS_FREE(entry->encrypted_data);
                entry->encrypted_data = NULL;
                entry->encrypted_len = 0;
                AGENTOS_FREE(entry->metadata.cred_id);
                entry->metadata.cred_id = NULL;
                if (!existed) {
                    AGENTOS_FREE(entry->cred_id);
                    entry->cred_id = NULL;
                }
                cupolas_rwlock_unlock(&vault->lock);
                return cupolas_ERR_OUT_OF_MEMORY;
            }
            entry->acl.entries[i].operations = acl->entries[i].operations;
            entry->acl.entries[i].expires_at = acl->entries[i].expires_at;
        }
    }

    if (!existed) {
        vault->entry_count++;
    }

    cupolas_rwlock_unlock(&vault->lock);
    return 0;
}

int cupolas_vault_retrieve(cupolas_vault_t *vault, const char *cred_id, const char *agent_id,
                           uint8_t *data_out, size_t *data_len)
{
    if (!vault || !cred_id || !data_out || !data_len) {
        return AGENTOS_ERR_UNKNOWN;
    }

    if (vault->is_locked) {
        return cupolas_ERR_INVALID_PARAM;
    }

    cupolas_rwlock_rdlock(&vault->lock);

    credential_entry_t *entry = find_entry(vault, cred_id);
    if (!entry) {
        cupolas_rwlock_unlock(&vault->lock);
        return cupolas_ERR_NULL_POINTER;
    }

    if (!cupolas_vault_check_access(vault, cred_id, agent_id, CUPOLAS_VAULT_OP_READ)) {
        cupolas_rwlock_unlock(&vault->lock);
        return cupolas_ERR_OUT_OF_MEMORY;
    }

    if (*data_len < entry->encrypted_len) {
        *data_len = entry->encrypted_len;
        cupolas_rwlock_unlock(&vault->lock);
        return cupolas_ERR_BUFFER_TOO_SMALL;
    }

#ifdef CUPOLAS_USE_OPENSSL
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        cupolas_rwlock_unlock(&vault->lock);
        return cupolas_ERR_NOT_FOUND;
    }

    int len = 0;
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, vault->master_key, entry->iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        cupolas_rwlock_unlock(&vault->lock);
        return cupolas_ERR_NOT_FOUND;
    }
    if (EVP_DecryptUpdate(ctx, data_out, &len, entry->encrypted_data, entry->encrypted_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        cupolas_rwlock_unlock(&vault->lock);
        return cupolas_ERR_NOT_FOUND;
    }
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, AES_GCM_TAG_SIZE, entry->tag) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        cupolas_rwlock_unlock(&vault->lock);
        return cupolas_ERR_NOT_FOUND;
    }
    if (EVP_DecryptFinal_ex(ctx, data_out + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        cupolas_rwlock_unlock(&vault->lock);
        return cupolas_ERR_NOT_FOUND;
    }

    *data_len = entry->encrypted_len;
    EVP_CIPHER_CTX_free(ctx);
#else
    (void)data_out;
    cupolas_rwlock_unlock(&vault->lock);
    return cupolas_VAULT_ERR_CRYPTO_UNAVAILABLE;
#endif

    cupolas_rwlock_unlock(&vault->lock);
    return 0;
}

int cupolas_vault_delete(cupolas_vault_t *vault, const char *cred_id, const char *agent_id)
{
    if (!vault || !cred_id) {
        return AGENTOS_ERR_UNKNOWN;
    }

    if (vault->is_locked) {
        return cupolas_ERR_INVALID_PARAM;
    }

    cupolas_rwlock_wrlock(&vault->lock);

    for (size_t i = 0; i < vault->entry_count; i++) {
        if (strcmp(vault->entries[i].cred_id, cred_id) == 0) {
            credential_entry_t *entry = &vault->entries[i];
            AGENTOS_FREE(entry->cred_id);
            AGENTOS_FREE(entry->encrypted_data);
            for (size_t j = 0; j < entry->acl.count; j++) {
                AGENTOS_FREE(entry->acl.entries[j].agent_id);
            }
            AGENTOS_FREE(entry->acl.entries);

            __builtin_memmove(&vault->entries[i], &vault->entries[i + 1],
                    (vault->entry_count - i - 1) * sizeof(credential_entry_t));
            vault->entry_count--;

            cupolas_rwlock_unlock(&vault->lock);
            return 0;
        }
    }

    cupolas_rwlock_unlock(&vault->lock);
    return cupolas_ERR_NULL_POINTER;
}

bool cupolas_vault_exists(cupolas_vault_t *vault, const char *cred_id)
{
    if (!vault || !cred_id) {
        return false;
    }

    cupolas_rwlock_rdlock(&vault->lock);
    credential_entry_t *entry = find_entry(vault, cred_id);
    cupolas_rwlock_unlock(&vault->lock);

    return entry != NULL;
}

int cupolas_vault_update(cupolas_vault_t *vault, const char *cred_id, const uint8_t *data,
                         size_t data_len, const char *agent_id)
{
    if (!vault || !cred_id || !data) {
        return AGENTOS_ERR_UNKNOWN;
    }

    if (!cupolas_vault_check_access(vault, cred_id, agent_id, CUPOLAS_VAULT_OP_WRITE)) {
        return cupolas_ERR_INVALID_PARAM;
    }

    cupolas_rwlock_wrlock(&vault->lock);

    credential_entry_t *entry = find_entry(vault, cred_id);
    if (!entry) {
        cupolas_rwlock_unlock(&vault->lock);
        return cupolas_ERR_NULL_POINTER;
    }

    AGENTOS_FREE(entry->encrypted_data);
    entry->encrypted_data = NULL;
    entry->encrypted_len = 0;

#ifdef CUPOLAS_USE_OPENSSL
    if (RAND_bytes(entry->iv, AES_IV_SIZE) != 1) {
        cupolas_rwlock_unlock(&vault->lock);
        return cupolas_ERR_OUT_OF_MEMORY;
    }

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        cupolas_rwlock_unlock(&vault->lock);
        return cupolas_ERR_OUT_OF_MEMORY;
    }

    int len = 0;
    entry->encrypted_data = (uint8_t *)AGENTOS_MALLOC(data_len + AES_BLOCK_SIZE);
    if (!entry->encrypted_data) {
        EVP_CIPHER_CTX_free(ctx);
        cupolas_rwlock_unlock(&vault->lock);
        return cupolas_ERR_OUT_OF_MEMORY;
    }

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, vault->master_key, entry->iv) != 1 ||
        EVP_EncryptUpdate(ctx, entry->encrypted_data, &len, data, data_len) != 1 ||
        EVP_EncryptFinal_ex(ctx, entry->encrypted_data + len, &len) != 1) {
        AGENTOS_FREE(entry->encrypted_data);
        entry->encrypted_data = NULL;
        EVP_CIPHER_CTX_free(ctx);
        cupolas_rwlock_unlock(&vault->lock);
        return cupolas_ERR_OUT_OF_MEMORY;
    }

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, AES_GCM_TAG_SIZE, entry->tag) != 1) {
        AGENTOS_FREE(entry->encrypted_data);
        entry->encrypted_data = NULL;
        EVP_CIPHER_CTX_free(ctx);
        cupolas_rwlock_unlock(&vault->lock);
        return cupolas_ERR_OUT_OF_MEMORY;
    }

    entry->encrypted_len = data_len;
    EVP_CIPHER_CTX_free(ctx);
#else
    cupolas_rwlock_unlock(&vault->lock);
    return cupolas_VAULT_ERR_CRYPTO_UNAVAILABLE;
#endif

    entry->metadata.updated_at = (uint64_t)time(NULL);

    cupolas_rwlock_unlock(&vault->lock);
    return 0;
}

/* ============================================================================
 * 访问控制
 * ============================================================================ */

bool cupolas_vault_check_access(cupolas_vault_t *vault, const char *cred_id, const char *agent_id,
                                cupolas_vault_operation_t operation)
{
    if (!vault || !cred_id || !agent_id) {
        return false;
    }

    cupolas_rwlock_rdlock(&vault->lock);

    credential_entry_t *entry = find_entry(vault, cred_id);
    if (!entry) {
        cupolas_rwlock_unlock(&vault->lock);
        return false;
    }

    if (entry->acl.count == 0) {
        cupolas_rwlock_unlock(&vault->lock);
        return true;
    }

    for (size_t i = 0; i < entry->acl.count; i++) {
        cupolas_vault_acl_entry_t *acl = &entry->acl.entries[i];
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

int cupolas_vault_grant_access(cupolas_vault_t *vault, const char *cred_id, const char *agent_id,
                               uint32_t operations, uint64_t expires_at)
{
    if (!vault || !cred_id || !agent_id) {
        return AGENTOS_ERR_UNKNOWN;
    }

    cupolas_rwlock_wrlock(&vault->lock);

    credential_entry_t *entry = find_entry(vault, cred_id);
    if (!entry) {
        cupolas_rwlock_unlock(&vault->lock);
        return cupolas_ERR_INVALID_PARAM;
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
    cupolas_vault_acl_entry_t *new_entries =
        AGENTOS_REALLOC(entry->acl.entries, new_count * sizeof(cupolas_vault_acl_entry_t));
    if (!new_entries) {
        cupolas_rwlock_unlock(&vault->lock);
        return cupolas_ERR_NULL_POINTER;
    }

    entry->acl.entries = new_entries;
    entry->acl.entries[entry->acl.count].agent_id = AGENTOS_STRDUP(agent_id);
    entry->acl.entries[entry->acl.count].operations = operations;
    entry->acl.entries[entry->acl.count].expires_at = expires_at;
    entry->acl.entries[entry->acl.count].access_count = 0;
    entry->acl.entries[entry->acl.count].max_access_count = 0;
    entry->acl.count = new_count;

    cupolas_rwlock_unlock(&vault->lock);
    return 0;
}

int cupolas_vault_revoke_access(cupolas_vault_t *vault, const char *cred_id, const char *agent_id)
{
    if (!vault || !cred_id || !agent_id) {
        return AGENTOS_ERR_UNKNOWN;
    }

    cupolas_rwlock_wrlock(&vault->lock);

    credential_entry_t *entry = find_entry(vault, cred_id);
    if (!entry) {
        cupolas_rwlock_unlock(&vault->lock);
        return cupolas_ERR_INVALID_PARAM;
    }

    for (size_t i = 0; i < entry->acl.count; i++) {
        if (strcmp(entry->acl.entries[i].agent_id, agent_id) == 0) {
            AGENTOS_FREE(entry->acl.entries[i].agent_id);
            __builtin_memmove(&entry->acl.entries[i], &entry->acl.entries[i + 1],
                    (entry->acl.count - i - 1) * sizeof(cupolas_vault_acl_entry_t));
            entry->acl.count--;
            cupolas_rwlock_unlock(&vault->lock);
            return 0;
        }
    }

    cupolas_rwlock_unlock(&vault->lock);
    return cupolas_ERR_NULL_POINTER;
}

/* ============================================================================
 * 元数据操作
 * ============================================================================ */

int cupolas_vault_get_metadata(cupolas_vault_t *vault, const char *cred_id,
                               cupolas_vault_metadata_t *metadata)
{
    if (!vault || !cred_id || !metadata) {
        return AGENTOS_ERR_UNKNOWN;
    }

    cupolas_rwlock_rdlock(&vault->lock);

    credential_entry_t *entry = find_entry(vault, cred_id);
    if (!entry) {
        cupolas_rwlock_unlock(&vault->lock);
        return cupolas_ERR_INVALID_PARAM;
    }

    metadata->cred_id = AGENTOS_STRDUP(entry->metadata.cred_id);
    metadata->type = entry->metadata.type;
    metadata->created_at = entry->metadata.created_at;
    metadata->updated_at = entry->metadata.updated_at;
    metadata->expires_at = entry->metadata.expires_at;
    metadata->is_accessible = !vault->is_locked;

    cupolas_rwlock_unlock(&vault->lock);
    return 0;
}

void cupolas_vault_free_metadata(cupolas_vault_metadata_t *metadata)
{
    if (!metadata) {
        return;
    }

    AGENTOS_FREE(metadata->cred_id);
    AGENTOS_FREE(metadata->description);
    AGENTOS_FREE(metadata->service);
    AGENTOS_FREE(metadata->account);
    __builtin_memset(metadata, 0, sizeof(cupolas_vault_metadata_t));
}

int cupolas_vault_list(cupolas_vault_t *vault, cupolas_vault_cred_type_t type,
                       cupolas_vault_metadata_t **metadata_array, size_t *count)
{
    if (!vault || !metadata_array || !count) {
        return AGENTOS_ERR_UNKNOWN;
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

    cupolas_vault_metadata_t *arr = AGENTOS_CALLOC(match_count, sizeof(cupolas_vault_metadata_t));
    if (!arr) {
        cupolas_rwlock_unlock(&vault->lock);
        return cupolas_ERR_INVALID_PARAM;
    }

    size_t idx = 0;
    for (size_t i = 0; i < vault->entry_count; i++) {
        if (type == 0 || vault->entries[i].type == type) {
            arr[idx].cred_id = AGENTOS_STRDUP(vault->entries[i].cred_id);
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

void cupolas_vault_free_list(cupolas_vault_metadata_t *metadata_array, size_t count)
{
    if (!metadata_array) {
        return;
    }

    for (size_t i = 0; i < count; i++) {
        cupolas_vault_free_metadata(&metadata_array[i]);
    }
    AGENTOS_FREE(metadata_array);
}

int cupolas_vault_get_acl(cupolas_vault_t *vault, const char *cred_id, cupolas_vault_acl_t *acl)
{
    if (!vault || !cred_id || !acl) {
        return AGENTOS_ERR_UNKNOWN;
    }

    cupolas_rwlock_rdlock(&vault->lock);

    credential_entry_t *entry = find_entry(vault, cred_id);
    if (!entry) {
        cupolas_rwlock_unlock(&vault->lock);
        return cupolas_ERR_INVALID_PARAM;
    }

    acl->count = entry->acl.count;
    acl->entries = AGENTOS_CALLOC(entry->acl.count, sizeof(cupolas_vault_acl_entry_t));
    if (!acl->entries && entry->acl.count > 0) {
        cupolas_rwlock_unlock(&vault->lock);
        return cupolas_ERR_OUT_OF_MEMORY;
    }
    for (size_t i = 0; i < entry->acl.count; i++) {
        acl->entries[i].agent_id = AGENTOS_STRDUP(entry->acl.entries[i].agent_id);
        acl->entries[i].operations = entry->acl.entries[i].operations;
        acl->entries[i].expires_at = entry->acl.entries[i].expires_at;
    }

    cupolas_rwlock_unlock(&vault->lock);
    return 0;
}

void cupolas_vault_free_acl(cupolas_vault_acl_t *acl)
{
    if (!acl || !acl->entries) {
        return;
    }

    for (size_t i = 0; i < acl->count; i++) {
        AGENTOS_FREE(acl->entries[i].agent_id);
    }
    AGENTOS_FREE(acl->entries);
    acl->entries = NULL;
    acl->count = 0;
}

/* ============================================================================
 * 辅助函数
 * ============================================================================ */

const char *cupolas_vault_cred_type_string(cupolas_vault_cred_type_t type)
{
    switch (type) {
    case CUPOLAS_VAULT_CRED_PASSWORD:
        return "password";
    case CUPOLAS_VAULT_CRED_TOKEN:
        return "token";
    case CUPOLAS_VAULT_CRED_KEY:
        return "key";
    case CUPOLAS_VAULT_CRED_CERTIFICATE:
        return "certificate";
    case CUPOLAS_VAULT_CRED_SECRET:
        return "secret";
    case CUPOLAS_VAULT_CRED_NOTE:
        return "note";
    default:
        return "unknown";
    }
}

const char *cupolas_vault_operation_string(cupolas_vault_operation_t op)
{
    switch (op) {
    case CUPOLAS_VAULT_OP_READ:
        return "read";
    case CUPOLAS_VAULT_OP_WRITE:
        return "write";
    case CUPOLAS_VAULT_OP_DELETE:
        return "delete";
    case CUPOLAS_VAULT_OP_EXPORT:
        return "export";
    default:
        return "unknown";
    }
}

int cupolas_vault_generate_password(char *password_out, size_t length)
{
    if (!password_out || length < 8) {
        return AGENTOS_ERR_UNKNOWN;
    }

    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*";
    size_t charset_len = strlen(charset);

#ifdef CUPOLAS_USE_OPENSSL
    for (size_t i = 0; i < length - 1; i++) {
        unsigned char c;
        if (RAND_bytes(&c, 1) != 1) {
            password_out[length - 1] = '\0';
            return cupolas_ERR_INVALID_PARAM;
        }
        password_out[i] = charset[c % charset_len];
    }
#else
    FILE *urandom = fopen("/dev/urandom", "rb");
    if (urandom) {
        for (size_t i = 0; i < length - 1; i++) {
            unsigned char c;
            if (fread(&c, 1, 1, urandom) != 1) {
                fclose(urandom);
                password_out[length - 1] = '\0';
                return cupolas_ERR_INVALID_PARAM;
            }
            password_out[i] = charset[c % charset_len];
        }
        fclose(urandom);
    } else {
        password_out[length - 1] = '\0';
        return cupolas_VAULT_ERR_CRYPTO_UNAVAILABLE;
    }
#endif

    password_out[length - 1] = '\0';
    return 0;
}

int cupolas_vault_generate_keypair(char *public_key_out, size_t *pub_len, char *private_key_out,
                                   size_t *priv_len)
{
    if (!public_key_out || !pub_len || !private_key_out || !priv_len) {
        return AGENTOS_ERR_UNKNOWN;
    }

#ifdef CUPOLAS_USE_OPENSSL
    EVP_PKEY *pkey = NULL;
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
    if (!ctx) {
        return cupolas_ERR_INVALID_PARAM;
    }

    if (EVP_PKEY_keygen_init(ctx) <= 0 || EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) <= 0 ||
        EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return cupolas_ERR_INVALID_PARAM;
    }
    EVP_PKEY_CTX_free(ctx);

    BIO *pub_bio = BIO_new(BIO_s_mem());
    BIO *priv_bio = BIO_new(BIO_s_mem());
    if (!pub_bio || !priv_bio) {
        BIO_free(pub_bio);
        BIO_free(priv_bio);
        EVP_PKEY_free(pkey);
        return cupolas_ERR_INVALID_PARAM;
    }

    if (PEM_write_bio_PUBKEY(pub_bio, pkey) != 1 ||
        PEM_write_bio_PrivateKey(priv_bio, pkey, NULL, NULL, 0, NULL, NULL) != 1) {
        BIO_free(pub_bio);
        BIO_free(priv_bio);
        EVP_PKEY_free(pkey);
        return cupolas_ERR_INVALID_PARAM;
    }

    int pub_read = BIO_read(pub_bio, public_key_out, (int)*pub_len);
    int priv_read = BIO_read(priv_bio, private_key_out, (int)*priv_len);
    if (pub_read <= 0 || priv_read <= 0) {
        BIO_free(pub_bio);
        BIO_free(priv_bio);
        EVP_PKEY_free(pkey);
        return cupolas_ERR_INVALID_PARAM;
    }
    *pub_len = (size_t)pub_read;
    *priv_len = (size_t)priv_read;

    BIO_free(pub_bio);
    BIO_free(priv_bio);
    EVP_PKEY_free(pkey);

    return 0;

#else
    return cupolas_VAULT_ERR_CRYPTO_UNAVAILABLE;
#endif
}

int cupolas_vault_export(cupolas_vault_t *vault, const char *export_path, const char *password,
                         const char *agent_id)
{
    if (!vault || !export_path || !password) {
        return AGENTOS_ERR_UNKNOWN;
    }

    if (vault->is_locked) {
        return cupolas_ERR_INVALID_PARAM;
    }

#define VAULT_FWRITE(ptr, sz, cnt, fp)           \
    do {                                         \
        if (fwrite(ptr, sz, cnt, fp) != (cnt)) { \
            goto export_fail;                    \
        }                                        \
    } while (0)

#ifdef CUPOLAS_USE_OPENSSL
    FILE *f = fopen(export_path, "wb");
    if (!f) {
        return cupolas_ERR_NULL_POINTER;
    }

    uint8_t file_salt[SALT_SIZE] = {0};
    if (RAND_bytes(file_salt, SALT_SIZE) != 1) {
        fclose(f);
        return cupolas_ERR_OUT_OF_MEMORY;
    }

    uint8_t derived_key_iv[AES_KEY_SIZE + AES_IV_SIZE] = {0};

    if (PKCS5_PBKDF2_HMAC(password, strlen(password), file_salt, SALT_SIZE, 100000, EVP_sha256(),
                          AES_KEY_SIZE + AES_IV_SIZE, derived_key_iv) != 1) {
        fclose(f);
        remove(export_path);
        return cupolas_ERR_BUFFER_TOO_SMALL;
    }

    uint8_t *derived_key = derived_key_iv;
    uint8_t *derived_iv = derived_key_iv + AES_KEY_SIZE;

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        fclose(f);
        remove(export_path);
        return cupolas_ERR_NOT_FOUND;
    }

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, derived_key, derived_iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        fclose(f);
        remove(export_path);
        return cupolas_ERR_ALREADY_EXISTS;
    }

    uint32_t magic = VAULT_MAGIC;
    uint32_t version = VAULT_VERSION;
    VAULT_FWRITE(&magic, sizeof(magic), 1, f);
    VAULT_FWRITE(&version, sizeof(version), 1, f);
    VAULT_FWRITE(file_salt, SALT_SIZE, 1, f);

    time_t now = time(NULL);
    VAULT_FWRITE(&now, sizeof(time_t), 1, f);

    if (agent_id) {
        size_t agent_len = strlen(agent_id);
        VAULT_FWRITE(&agent_len, sizeof(size_t), 1, f);
        VAULT_FWRITE(agent_id, 1, agent_len, f);
    } else {
        size_t zero = 0;
        VAULT_FWRITE(&zero, sizeof(size_t), 1, f);
    }

    uint32_t cred_count = (uint32_t)vault->entry_count;
    VAULT_FWRITE(&cred_count, sizeof(uint32_t), 1, f);

    for (size_t i = 0; i < vault->entry_count; i++) {
        credential_entry_t *entry = &vault->entries[i];

        size_t id_len = strlen(entry->cred_id);
        VAULT_FWRITE(&id_len, sizeof(size_t), 1, f);
        VAULT_FWRITE(entry->cred_id, 1, id_len, f);

        VAULT_FWRITE(&entry->type, sizeof(cupolas_vault_cred_type_t), 1, f);
        VAULT_FWRITE(&entry->encrypted_len, sizeof(size_t), 1, f);

        int out_len = 0;
        int final_len = 0;
        uint8_t *encrypted_buf = (uint8_t *)AGENTOS_MALLOC(entry->encrypted_len + AES_BLOCK_SIZE);
        if (!encrypted_buf || EVP_EncryptUpdate(ctx, encrypted_buf, &out_len, entry->encrypted_data,
                                                entry->encrypted_len) != 1) {
            AGENTOS_FREE(encrypted_buf);
            EVP_CIPHER_CTX_free(ctx);
            fclose(f);
            remove(export_path);
            return cupolas_ERR_TIMEOUT;
        }

        if (EVP_EncryptFinal_ex(ctx, encrypted_buf + out_len, &final_len) != 1) {
            AGENTOS_FREE(encrypted_buf);
            EVP_CIPHER_CTX_free(ctx);
            fclose(f);
            remove(export_path);
            return cupolas_ERR_NOT_SUPPORTED;
        }

        size_t total_encrypted = out_len + final_len;
        VAULT_FWRITE(&total_encrypted, sizeof(size_t), 1, f);
        VAULT_FWRITE(encrypted_buf, 1, total_encrypted, f);
        AGENTOS_FREE(encrypted_buf);

        VAULT_FWRITE(entry->iv, AES_IV_SIZE, 1, f);
        VAULT_FWRITE(entry->salt, SALT_SIZE, 1, f);

        VAULT_FWRITE(&entry->acl.count, sizeof(size_t), 1, f);
        for (size_t k = 0; k < entry->acl.count; k++) {
            size_t agent_id_len =
                entry->acl.entries[k].agent_id ? strlen(entry->acl.entries[k].agent_id) : 0;
            VAULT_FWRITE(&agent_id_len, sizeof(size_t), 1, f);
            if (agent_id_len > 0) {
                VAULT_FWRITE(entry->acl.entries[k].agent_id, 1, agent_id_len, f);
            }
            VAULT_FWRITE(&entry->acl.entries[k].operations, sizeof(uint32_t), 1, f);
            VAULT_FWRITE(&entry->acl.entries[k].expires_at, sizeof(uint64_t), 1, f);
            VAULT_FWRITE(&entry->acl.entries[k].access_count, sizeof(uint32_t), 1, f);
            VAULT_FWRITE(&entry->acl.entries[k].max_access_count, sizeof(uint32_t), 1, f);
        }

        {
            const char *meta_fields[] = {entry->metadata.cred_id, entry->metadata.description,
                                         entry->metadata.service, entry->metadata.account};
            for (int m = 0; m < 4; m++) {
                size_t field_len = meta_fields[m] ? strlen(meta_fields[m]) : 0;
                VAULT_FWRITE(&field_len, sizeof(size_t), 1, f);
                if (field_len > 0) {
                    VAULT_FWRITE(meta_fields[m], 1, field_len, f);
                }
            }
            VAULT_FWRITE(&entry->metadata.type, sizeof(cupolas_vault_cred_type_t), 1, f);
            VAULT_FWRITE(&entry->metadata.created_at, sizeof(uint64_t), 1, f);
            VAULT_FWRITE(&entry->metadata.updated_at, sizeof(uint64_t), 1, f);
            VAULT_FWRITE(&entry->metadata.expires_at, sizeof(uint64_t), 1, f);
            VAULT_FWRITE(&entry->metadata.is_accessible, sizeof(bool), 1, f);
        }
    }

    EVP_CIPHER_CTX_free(ctx);
    fclose(f);

    if (vault->config.enable_audit) {
        AGENTOS_LOG_INFO("[VAULT] Export completed: %zu credentials to %s", vault->entry_count,
                         export_path);
    }

    return 0;

export_fail:
    EVP_CIPHER_CTX_free(ctx);
    fclose(f);
    remove(export_path);
    return cupolas_ERR_UNKNOWN;

#else
    (void)agent_id;
    return cupolas_VAULT_ERR_CRYPTO_UNAVAILABLE;
#endif
}

int cupolas_vault_import(cupolas_vault_t *vault, const char *import_path, const char *password,
                         const char *agent_id)
{
    if (!vault || !import_path || !password) {
        return AGENTOS_ERR_UNKNOWN;
    }

    if (vault->is_locked) {
        return cupolas_ERR_INVALID_PARAM;
    }

#ifdef CUPOLAS_USE_OPENSSL
    FILE *f = fopen(import_path, "rb");
    if (!f) {
        return cupolas_ERR_NULL_POINTER;
    }

    uint32_t magic = 0;
    if (fread(&magic, sizeof(uint32_t), 1, f) != 1 || magic != VAULT_MAGIC) {
        fclose(f);
        return cupolas_ERR_OUT_OF_MEMORY;
    }

    uint32_t version = 0;
    if (fread(&version, sizeof(uint32_t), 1, f) != 1 || version > VAULT_VERSION) {
        fclose(f);
        return cupolas_ERR_BUFFER_TOO_SMALL;
    }

    uint8_t file_salt[SALT_SIZE] = {0};
    if (fread(file_salt, SALT_SIZE, 1, f) != 1) {
        fclose(f);
        return cupolas_ERR_NOT_FOUND;
    }

    time_t export_time = 0;
    if (fread(&export_time, sizeof(time_t), 1, f) != 1) {
        fclose(f);
        return cupolas_ERR_NOT_FOUND;
    }

    size_t agent_len = 0;
    if (fread(&agent_len, sizeof(size_t), 1, f) != 1) {
        fclose(f);
        return cupolas_ERR_NOT_FOUND;
    }
    if (agent_len > 0 && agent_len < 65536) {
        char *file_agent_id = (char *)AGENTOS_MALLOC(agent_len + 1);
        if (file_agent_id) {
            if (fread(file_agent_id, 1, agent_len, f) != agent_len) {
                AGENTOS_FREE(file_agent_id);
                fclose(f);
                return cupolas_ERR_NOT_FOUND;
            }
            file_agent_id[agent_len] = '\0';
            AGENTOS_FREE(file_agent_id);
        } else {
            fseek(f, agent_len, SEEK_CUR);
        }
    } else if (agent_len >= 65536) {
        fclose(f);
        return cupolas_ERR_NOT_FOUND;
    }

    uint32_t cred_count = 0;
    if (fread(&cred_count, sizeof(uint32_t), 1, f) != 1) {
        fclose(f);
        return cupolas_ERR_ALREADY_EXISTS;
    }

    uint8_t derived_key_iv[AES_KEY_SIZE + AES_IV_SIZE] = {0};

    if (PKCS5_PBKDF2_HMAC(password, strlen(password), file_salt, SALT_SIZE, 100000, EVP_sha256(),
                          AES_KEY_SIZE + AES_IV_SIZE, derived_key_iv) != 1) {
        fclose(f);
        return cupolas_ERR_TIMEOUT;
    }

    uint8_t *derived_key = derived_key_iv;
    uint8_t *derived_iv = derived_key_iv + AES_KEY_SIZE;

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        fclose(f);
        return cupolas_ERR_NOT_SUPPORTED;
    }

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, derived_key, derived_iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        fclose(f);
        return cupolas_ERR_UNKNOWN;
    }

    size_t imported = 0;

    for (uint32_t i = 0; i < cred_count; i++) {
        size_t id_len = 0;
        if (fread(&id_len, sizeof(size_t), 1, f) != 1)
            break;

        char *cred_id = (char *)AGENTOS_MALLOC(id_len + 1);
        if (!cred_id || fread(cred_id, 1, id_len, f) != 1) {
            AGENTOS_FREE(cred_id);
            break;
        }
        cred_id[id_len] = '\0';

        cupolas_vault_cred_type_t type;
        if (fread(&type, sizeof(cupolas_vault_cred_type_t), 1, f) != 1) {
            AGENTOS_FREE(cred_id);
            break;
        }

        size_t enc_len = 0;
        if (fread(&enc_len, sizeof(size_t), 1, f) != 1) {
            AGENTOS_FREE(cred_id);
            break;
        }

        uint8_t *enc_data = (uint8_t *)AGENTOS_MALLOC(enc_len);
        if (!enc_data || fread(enc_data, 1, enc_len, f) != enc_len) {
            AGENTOS_FREE(cred_id);
            cred_id = NULL;
            AGENTOS_FREE(enc_data);
            enc_data = NULL;
            break;
        }

        int out_len = 0;
        int final_len = 0;
        uint8_t *decrypted = (uint8_t *)AGENTOS_MALLOC(enc_len + AES_BLOCK_SIZE);
        if (!decrypted || EVP_DecryptUpdate(ctx, decrypted, &out_len, enc_data, enc_len) != 1 ||
            EVP_DecryptFinal_ex(ctx, decrypted + out_len, &final_len) != 1) {
            AGENTOS_FREE(cred_id);
            cred_id = NULL;
            AGENTOS_FREE(enc_data);
            enc_data = NULL;
            AGENTOS_FREE(decrypted);
            decrypted = NULL;
            continue;
        }

        size_t total_decrypted = out_len + final_len;

        if (vault->entry_count >= vault->entry_capacity) {
            size_t new_cap = vault->entry_capacity * 2;
            credential_entry_t *new_entries = (credential_entry_t *)AGENTOS_REALLOC(
                vault->entries, new_cap * sizeof(credential_entry_t));
            if (!new_entries) {
                AGENTOS_FREE(cred_id);
                cred_id = NULL;
                AGENTOS_FREE(enc_data);
                enc_data = NULL;
                AGENTOS_FREE(decrypted);
                decrypted = NULL;
                continue;
            }
            vault->entries = new_entries;
            vault->entry_capacity = new_cap;
        }

        credential_entry_t *entry = &vault->entries[vault->entry_count];
        __builtin_memset(entry, 0, sizeof(credential_entry_t));
        entry->cred_id = cred_id;
        entry->type = type;
        entry->encrypted_data = (uint8_t *)AGENTOS_MALLOC(total_decrypted);
        if (entry->encrypted_data) {
            __builtin_memcpy(entry->encrypted_data, decrypted, total_decrypted);
            entry->encrypted_len = total_decrypted;
        } else {
            entry->encrypted_data = NULL;
            entry->encrypted_len = 0;
        }

        if (fread(entry->iv, AES_IV_SIZE, 1, f) != 1 || fread(entry->salt, SALT_SIZE, 1, f) != 1) {
            AGENTOS_FREE(entry->encrypted_data);
            entry->encrypted_data = NULL;
            entry->encrypted_len = 0;
            AGENTOS_FREE(enc_data);
            enc_data = NULL;
            AGENTOS_FREE(decrypted);
            decrypted = NULL;
            continue;
        }

        {
            size_t acl_count = 0;
            if (fread(&acl_count, sizeof(size_t), 1, f) != 1 ||
                acl_count > MAX_ACL_ENTRIES_PER_CREDENTIAL) {
                AGENTOS_FREE(entry->encrypted_data);
                entry->encrypted_data = NULL;
                entry->encrypted_len = 0;
                AGENTOS_FREE(enc_data);
                enc_data = NULL;
                AGENTOS_FREE(decrypted);
                decrypted = NULL;
                continue;
            }
            entry->acl.count = acl_count;
            if (acl_count > 0) {
                entry->acl.entries = (cupolas_vault_acl_entry_t *)AGENTOS_CALLOC(
                    acl_count, sizeof(cupolas_vault_acl_entry_t));
                if (!entry->acl.entries) {
                    continue;
                }
                for (size_t k = 0; k < acl_count; k++) {
                    size_t agent_id_len = 0;
                    if (fread(&agent_id_len, sizeof(size_t), 1, f) != 1)
                        break;
                    if (agent_id_len > 0 && agent_id_len < 65536) {
                        entry->acl.entries[k].agent_id = (char *)AGENTOS_MALLOC(agent_id_len + 1);
                        if (entry->acl.entries[k].agent_id) {
                            if (fread(entry->acl.entries[k].agent_id, 1, agent_id_len, f) !=
                                agent_id_len) {
                                AGENTOS_FREE(entry->acl.entries[k].agent_id);
                                entry->acl.entries[k].agent_id = NULL;
                            }
                            entry->acl.entries[k].agent_id[agent_id_len] = '\0';
                        }
                    }
                    {
                        size_t _fr;
                        _fr = fread(&entry->acl.entries[k].operations, sizeof(uint32_t), 1, f);
                        if (_fr < 1) { break; }
                    }
                    {
                        size_t _fr;
                        _fr = fread(&entry->acl.entries[k].expires_at, sizeof(uint64_t), 1, f);
                        if (_fr < 1) { break; }
                    }
                    {
                        size_t _fr;
                        _fr = fread(&entry->acl.entries[k].access_count, sizeof(uint32_t), 1, f);
                        if (_fr < 1) { break; }
                    }
                    {
                        size_t _fr;
                        _fr =
                            fread(&entry->acl.entries[k].max_access_count, sizeof(uint32_t), 1, f);
                        if (_fr < 1) { break; }
                    }
                }
            }
        }

        {
            __builtin_memset(&entry->metadata, 0, sizeof(entry->metadata));
            char *meta_ptrs[4] = {NULL, NULL, NULL, NULL};
            for (int m = 0; m < 4; m++) {
                size_t field_len = 0;
                if (fread(&field_len, sizeof(size_t), 1, f) != 1)
                    continue;
                if (field_len > 0 && field_len < 65536) {
                    meta_ptrs[m] = (char *)AGENTOS_MALLOC(field_len + 1);
                    if (meta_ptrs[m]) {
                        if (fread(meta_ptrs[m], 1, field_len, f) == field_len) {
                            meta_ptrs[m][field_len] = '\0';
                        } else {
                            AGENTOS_FREE(meta_ptrs[m]);
                            meta_ptrs[m] = NULL;
                        }
                    }
                }
            }
            entry->metadata.cred_id = meta_ptrs[0];
            entry->metadata.description = meta_ptrs[1];
            entry->metadata.service = meta_ptrs[2];
            entry->metadata.account = meta_ptrs[3];
            {
                size_t _fr;
                _fr = fread(&entry->metadata.type, sizeof(cupolas_vault_cred_type_t), 1, f);
                if (_fr < 1) { break; }
            }
            {
                size_t _fr;
                _fr = fread(&entry->metadata.created_at, sizeof(uint64_t), 1, f);
                if (_fr < 1) { break; }
            }
            {
                size_t _fr;
                _fr = fread(&entry->metadata.updated_at, sizeof(uint64_t), 1, f);
                if (_fr < 1) { break; }
            }
            {
                size_t _fr;
                _fr = fread(&entry->metadata.expires_at, sizeof(uint64_t), 1, f);
                if (_fr < 1) { break; }
            }
            {
                size_t _fr;
                _fr = fread(&entry->metadata.is_accessible, sizeof(bool), 1, f);
                if (_fr < 1) { break; }
            }
        }

        vault->entry_count++;
        imported++;

        AGENTOS_FREE(enc_data);
        enc_data = NULL;
        AGENTOS_FREE(decrypted);
        decrypted = NULL;
    }

    EVP_CIPHER_CTX_free(ctx);
    fclose(f);

    if (vault->config.enable_audit) {
        AGENTOS_LOG_INFO("[VAULT] Import completed: %zu credentials from %s", imported, import_path);
    }

    return (int)imported;
#else
    if (!password || !agent_id)
        return cupolas_VAULT_ERR_INVALID;

    FILE *f = fopen(import_path, "r");
    if (!f)
        return cupolas_ERR_NULL_POINTER;

    char line[1024];
    size_t imported = 0;
    bool in_entry = false;
    char current_id[256] = {0};

    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';

        if (line[0] == '[' && line[strlen(line) - 1] == ']') {
            if (in_entry && current_id[0]) {
                if (vault->entry_count < vault->entry_capacity) {
                    credential_entry_t *entry = &vault->entries[vault->entry_count];
                    entry->cred_id = AGENTOS_STRDUP(current_id);
                    entry->type = 0;
                    entry->encrypted_data = NULL;
                    entry->encrypted_len = 0;
                    __builtin_memset(&entry->acl, 0, sizeof(entry->acl));
                    __builtin_memset(&entry->metadata, 0, sizeof(entry->metadata));
                    vault->entry_count++;
                    imported++;
                }
            }
            AGENTOS_STRNCPY_TERM(current_id, line + 1, sizeof(current_id));
            in_entry = true;
        }
    }

    if (in_entry && current_id[0] && vault->entry_count < vault->entry_capacity) {
        credential_entry_t *entry = &vault->entries[vault->entry_count];
        entry->cred_id = AGENTOS_STRDUP(current_id);
        entry->type = 0;
        entry->encrypted_data = NULL;
        entry->encrypted_len = 0;
        __builtin_memset(&entry->acl, 0, sizeof(entry->acl));
        __builtin_memset(&entry->metadata, 0, sizeof(entry->metadata));
        vault->entry_count++;
        imported++;
    }

    fclose(f);
    return (int)imported;
#endif
}

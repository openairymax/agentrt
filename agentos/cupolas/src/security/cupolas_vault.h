/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * cupolas_vault.h - Secure Credential Storage: iOS Keychain-like Secure Storage
 */

#ifndef CUPOLAS_VAULT_H
#define CUPOLAS_VAULT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Credential types
 *
 * Design principles:
 * - Encrypted storage: AES-256-GCM for all credentials
 * - Access control: Fine-grained per-Agent ID authorization
 * - Audit trail: All access attempts logged
 * - Anti-tampering: Integrity checks prevent data tampering
 */
#include <cupolas_vault_cred_type.h>

/**
 * @brief 凭证轮换策略
 * 
 * 符合编码契约要求: 支持四种凭证轮换策略，用于多凭证集之间的切换。
 */
typedef enum {
    CUPOLAS_VAULT_ROTATE_ROUND_ROBIN = 1,  /**< 轮询: 按顺序轮换凭证 */
    CUPOLAS_VAULT_ROTATE_LEAST_USED  = 2,  /**< 最少使用: 轮换使用次数最少的凭证 */
    CUPOLAS_VAULT_ROTATE_RATE_LIMITED = 3, /**< 速率限制: 按速率限制轮换凭证 */
    CUPOLAS_VAULT_ROTATE_PRIORITY    = 4   /**< 优先级: 按优先级轮换凭证 */
} cupolas_vault_rotation_strategy_t;

/**
 * @brief Access operation types (bit flags)
 */
typedef enum {
    CUPOLAS_VAULT_OP_READ = 1,
    CUPOLAS_VAULT_OP_WRITE = 2,
    CUPOLAS_VAULT_OP_DELETE = 4,
    CUPOLAS_VAULT_OP_EXPORT = 8
} cupolas_vault_operation_t;

/**
 * @brief Credential metadata structure
 */
typedef struct {
    char *cred_id;                  /**< Credential identifier */
    cupolas_vault_cred_type_t type; /**< Credential type */
    char *description;              /**< Description */
    char *service;                  /**< Service name */
    char *account;                  /**< Account name */
    uint64_t created_at;            /**< Creation timestamp */
    uint64_t updated_at;            /**< Last update timestamp */
    uint64_t expires_at;            /**< Expiration timestamp */
    bool is_accessible;             /**< Is accessible */
} cupolas_vault_metadata_t;

/**
 * @brief ACL entry structure
 */
typedef struct {
    char *agent_id;            /**< Agent ID */
    uint32_t operations;       /**< Allowed operations (bitmask) */
    uint64_t expires_at;       /**< Access expiration */
    uint32_t access_count;     /**< Access count */
    uint32_t max_access_count; /**< Maximum access count */
} cupolas_vault_acl_entry_t;

/**
 * @brief Access control list
 */
typedef struct {
    cupolas_vault_acl_entry_t *entries;
    size_t count;
} cupolas_vault_acl_t;

/**
 * @brief Vault configuration
 */
typedef struct {
    const char *storage_path;    /**< Storage path */
    const char *master_key_path; /**< Master key path */
    bool enable_audit;           /**< Enable audit logging */
    bool enable_auto_lock;       /**< Enable auto-lock */
    uint32_t auto_lock_seconds;  /**< Auto-lock timeout */
    uint32_t max_retry_count;    /**< Maximum retry count */
} cupolas_vault_config_t;

/**
 * @brief Vault context (opaque)
 */
typedef struct cupolas_vault cupolas_vault_t;

/**
 * @brief Initialize Vault module
 * @param[in] config Configuration (NULL for defaults)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (initialization only)
 * @reentrant No
 * @ownership config: BORROW
 */
int cupolas_vault_init(const cupolas_vault_config_t *config);

/**
 * @brief Shutdown Vault module
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other
 * operations)
 * @reentrant No
 */
void cupolas_vault_cleanup(void);

/**
 * @brief Open a vault
 * @param[in] vault_id Vault identifier
 * @param[in] password Password (optional, may be NULL)
 * @param[out] vault Vault context output
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other
 * operations)
 * @reentrant No
 * @ownership vault_id: BORROW, password: BORROW
 * @ownership vault: OWNER (out parameter - caller must call cupolas_vault_close)
 */
int cupolas_vault_open(const char *vault_id, const char *password, cupolas_vault_t **vault);

/**
 * @brief Close a vault
 * @param[in] vault Vault context (may be NULL)
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other
 * operations)
 * @reentrant No
 * @ownership vault: TRANSFER
 */
void cupolas_vault_close(cupolas_vault_t *vault);

/**
 * @brief Lock a vault
 * @param[in] vault Vault context
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads
 * @reentrant No
 *
 * @ownership vault: BORROW
 */
int cupolas_vault_lock(cupolas_vault_t *vault);

/**
 * @brief Unlock a vault
 * @param[in] vault Vault context
 * @param[in] password Password
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other
 * operations)
 * @reentrant No
 * @ownership vault: BORROW, password: BORROW
 */
int cupolas_vault_unlock(cupolas_vault_t *vault, const char *password);

/**
 * @brief Check if vault is locked
 * @param[in] vault Vault context
 * @return true if locked, false otherwise
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 *
 * @ownership vault: BORROW
 */
bool cupolas_vault_is_locked(cupolas_vault_t *vault);

/**
 * @brief Store a credential
 * @param[in] vault Vault context
 * @param[in] cred_id Credential identifier
 * @param[in] type Credential type
 * @param[in] data Credential data
 * @param[in] data_len Data length
 * @param[in] acl Access control list (optional, may be NULL)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other
 * operations)
 * @reentrant No
 * @ownership vault: BORROW, cred_id: BORROW, data: BORROW
 * @ownership vault: BORROW, acl: BORROW
 */
int cupolas_vault_store(cupolas_vault_t *vault, const char *cred_id, cupolas_vault_cred_type_t type,
                        const uint8_t *data, size_t data_len, const cupolas_vault_acl_t *acl);

/**
 * @brief Retrieve a credential
 * @param[in] vault Vault context
 * @param[in] cred_id Credential identifier
 * @param[in] agent_id Requesting Agent ID
 * @param[out] data_out Output buffer
 * @param[in,out] data_len Buffer size / actual length
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other
 * operations)
 * @reentrant No
 * @ownership vault: BORROW, cred_id: BORROW, agent_id: BORROW
 * @ownership vault: BORROW, data_out: BORROW (caller-owned buffer, function writes to it)
 */
int cupolas_vault_retrieve(cupolas_vault_t *vault, const char *cred_id, const char *agent_id,
                           uint8_t *data_out, size_t *data_len);

/**
 * @brief Delete a credential
 * @param[in] vault Vault context
 * @param[in] cred_id Credential identifier
 * @param[in] agent_id Requesting Agent ID
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other
 * operations)
 * @reentrant No
 * @ownership vault: BORROW, cred_id: BORROW, agent_id: BORROW
 */
int cupolas_vault_delete(cupolas_vault_t *vault, const char *cred_id, const char *agent_id);

/**
 * @brief Check if credential exists
 * @param[in] vault Vault context
 * @param[in] cred_id Credential identifier
 * @return true if exists, false otherwise
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership vault: BORROW, cred_id: BORROW
 */
bool cupolas_vault_exists(cupolas_vault_t *vault, const char *cred_id);

/**
 * @brief Update a credential
 * @param[in] vault Vault context
 * @param[in] cred_id Credential identifier
 * @param[in] data New data
 * @param[in] data_len Data length
 * @param[in] agent_id Requesting Agent ID
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other
 * operations)
 * @reentrant No
 * @ownership vault: BORROW, cred_id: BORROW, data: BORROW, agent_id: BORROW
 */
int cupolas_vault_update(cupolas_vault_t *vault, const char *cred_id, const uint8_t *data,
                         size_t data_len, const char *agent_id);

/**
 * @brief Get credential metadata
 * @param[in] vault Vault context
 * @param[in] cred_id Credential identifier
 * @param[out] metadata Metadata output
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership vault: BORROW, cred_id: BORROW
 * @ownership vault: BORROW, metadata: BORROW (caller-owned buffer, function writes to it)
 */
int cupolas_vault_get_metadata(cupolas_vault_t *vault, const char *cred_id,
                               cupolas_vault_metadata_t *metadata);

/**
 * @brief Free metadata structure
 * @param[in] metadata Metadata pointer (may be NULL)
 * @note Thread-safe: Safe to call from multiple threads
 * @reentrant No
 * @ownership metadata: TRANSFER
 */
void cupolas_vault_free_metadata(cupolas_vault_metadata_t *metadata);

/**
 * @brief List all credentials
 * @param[in] vault Vault context
 * @param[in] type Credential type filter (0 for all types)
 * @param[out] metadata_array Array of metadata
 * @param[out] count Number of credentials
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership vault: BORROW, metadata_array: OWNER (out parameter - caller must call cupolas_vault_free_list)
 * @ownership vault: BORROW, acl: BORROW (caller-owned buffer, function writes to it)
 */
int cupolas_vault_list(cupolas_vault_t *vault, cupolas_vault_cred_type_t type,
                       cupolas_vault_metadata_t **metadata_array, size_t *count);

/**
 * @brief Free credential list
 * @param[in] metadata_array Metadata array
 * @param[in] count Number of entries
 * @note Thread-safe: Safe to call from multiple threads
 * @reentrant No
 * @ownership metadata_array: TRANSFER
 */
void cupolas_vault_free_list(cupolas_vault_metadata_t *metadata_array, size_t count);

/**
 * @brief Check access permission
 * @param[in] vault Vault context
 * @param[in] cred_id Credential identifier
 * @param[in] agent_id Agent ID
 * @param[in] operation Operation type
 * @return true if allowed, false if denied
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership vault: BORROW, cred_id: BORROW, agent_id: BORROW
 */
bool cupolas_vault_check_access(cupolas_vault_t *vault, const char *cred_id, const char *agent_id,
                                cupolas_vault_operation_t operation);

/**
 * @brief Grant access permission
 * @param[in] vault Vault context
 * @param[in] cred_id Credential identifier
 * @param[in] agent_id Agent ID
 * @param[in] operations Allowed operations (bitmask)
 * @param[in] expires_at Expiration timestamp (0 for no expiration)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other
 * operations)
 * @reentrant No
 * @ownership vault: BORROW, cred_id: BORROW, agent_id: BORROW
 */
int cupolas_vault_grant_access(cupolas_vault_t *vault, const char *cred_id, const char *agent_id,
                               uint32_t operations, uint64_t expires_at);

/**
 * @brief Revoke access permission
 * @param[in] vault Vault context
 * @param[in] cred_id Credential identifier
 * @param[in] agent_id Agent ID
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other
 * operations)
 * @reentrant No
 * @ownership vault: BORROW, cred_id: BORROW, agent_id: BORROW
 */
int cupolas_vault_revoke_access(cupolas_vault_t *vault, const char *cred_id, const char *agent_id);

/**
 * @brief Get ACL for credential
 * @param[in] vault Vault context
 * @param[in] cred_id Credential identifier
 * @param[out] acl ACL output
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership vault: BORROW, cred_id: BORROW
 * @ownership vault: BORROW, acl: BORROW (caller-owned buffer, function writes to it)
 */
int cupolas_vault_get_acl(cupolas_vault_t *vault, const char *cred_id, cupolas_vault_acl_t *acl);

/**
 * @brief Free ACL structure
 * @param[in] acl ACL pointer (may be NULL)
 * @note Thread-safe: Safe to call from multiple threads
 * @reentrant No
 * @ownership acl: TRANSFER
 */
void cupolas_vault_free_acl(cupolas_vault_acl_t *acl);

/**
 * @brief Export vault
 * @param[in] vault Vault context
 * @param[in] export_path Export path
 * @param[in] password Encryption password
 * @param[in] agent_id Requesting Agent ID
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other
 * operations)
 * @reentrant No
 * @ownership vault: BORROW, export_path: BORROW, password: BORROW, agent_id: BORROW
 */
int cupolas_vault_export(cupolas_vault_t *vault, const char *export_path, const char *password,
                         const char *agent_id);

/**
 * @brief Import vault
 * @param[in] vault Vault context
 * @param[in] import_path Import path
 * @param[in] password Decryption password
 * @param[in] agent_id Requesting Agent ID
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other
 * operations)
 * @reentrant No
 * @ownership vault: BORROW, import_path: BORROW, password: BORROW, agent_id: BORROW
 */
int cupolas_vault_import(cupolas_vault_t *vault, const char *import_path, const char *password,
                         const char *agent_id);

/**
 * @brief Get credential type string
 * @param[in] type Credential type
 * @return Type name string (static, do not free)
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 *
 * @ownership return: BORROW (static string, do not free)
 */
const char *cupolas_vault_cred_type_string(cupolas_vault_cred_type_t type);

/**
 * @brief Get operation string
 * @param[in] op Operation type
 * @return Operation name string (static, do not free)
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 *
 * @ownership return: BORROW (static string, do not free)
 */
const char *cupolas_vault_operation_string(cupolas_vault_operation_t op);

/**
 * @brief Generate random password
 * @param[out] password_out Output buffer
 * @param[in] length Password length
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads
 * @reentrant Yes
 * @ownership password_out: BORROW (caller-owned buffer, function writes to it)
 */
int cupolas_vault_generate_password(char *password_out, size_t length);

/**
 * @brief Generate key pair
 * @param[out] public_key_out Public key output buffer
 * @param[in,out] pub_len Buffer size / actual length
 * @param[out] private_key_out Private key output buffer
 * @param[in,out] priv_len Buffer size / actual length
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads
 * @reentrant Yes
 * @ownership public_key_out: BORROW (caller-owned buffer, function writes to it)
 * @ownership private_key_out: BORROW (caller-owned buffer, function writes to it)
 */
int cupolas_vault_generate_keypair(char *public_key_out, size_t *pub_len, char *private_key_out,
                                   size_t *priv_len);

/**
 * @brief 凭证轮换: 根据指定策略从凭证池中选择下一个凭证
 * 
 * 符合编码契约要求: 支持四种轮换策略。
 * 
 * @param[in] vault          Vault 上下文 (BORROW - caller retains ownership)
 * @param[in] cred_group     凭证组标识 (BORROW - not stored, copied internally)
 * @param[in] strategy        轮换策略
 * @param[out] selected_id    选中的凭证 ID (BORROW - caller-owned buffer, function writes to it)
 * @param[in] id_buf_size     selected_id 缓冲区大小
 * @return 0 成功, 负数失败
 *
 * @ownership vault: BORROW, cred_group: BORROW, selected_id: BORROW
 */
int cupolas_vault_rotate_credential(cupolas_vault_t *vault, const char *cred_group,
                                    cupolas_vault_rotation_strategy_t strategy,
                                    char *selected_id, size_t id_buf_size);

#ifdef __cplusplus
}
#endif

#endif /* CUPOLAS_VAULT_H */

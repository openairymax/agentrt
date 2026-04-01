/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * cupolas_entitlements.h - Entitlements Permission Declarations: Fine-grained Permission System
 */

#ifndef CUPOLAS_ENTITLEMENTS_H
#define CUPOLAS_ENTITLEMENTS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Entitlements verification result codes
 * 
 * Design principles:
 * - Declarative permissions: All permissions must be explicitly declared
 * - Least privilege: Default deny for undeclared permissions
 * - Signature verification: Entitlements file must be signed
 * - Runtime enforcement: All operations check entitlements
 */
typedef enum {
    CUPOLAS_ENT_OK = 0,               /**< Verification successful */
    CUPOLAS_ENT_INVALID = -1,         /**< Invalid format */
    CUPOLAS_ENT_SIGNATURE_INVALID = -2, /**< Invalid signature */
    CUPOLAS_ENT_EXPIRED = -3,         /**< Expired */
    CUPOLAS_ENT_DENIED = -4,          /**< Permission denied */
    CUPOLAS_ENT_NOT_FOUND = -5,       /**< Not found */
    CUPOLAS_ENT_PARSE_ERROR = -6      /**< Parse error */
} cupolas_ent_result_t;

/**
 * @brief File system permission structure
 */
typedef struct {
    char* path;                     /**< Path pattern (supports wildcards) */
    char** permissions;             /**< Permission list (read, write, create, delete, execute) */
    size_t perm_count;              /**< Number of permissions */
} cupolas_ent_fs_permission_t;

/**
 * @brief Network permission structure
 */
typedef struct {
    char* host;                     /**< Host pattern (supports wildcards) */
    uint16_t port;                  /**< Port (0 = any) */
    char* protocol;                 /**< Protocol (tcp, udp, http, https) */
    char* direction;                /**< Direction (inbound, outbound, both) */
} cupolas_ent_net_permission_t;

/**
 * @brief IPC permission structure
 */
typedef struct {
    char* target;                   /**< Target service */
    char** permissions;             /**< Permission list (send, receive, call) */
    size_t perm_count;              /**< Number of permissions */
} cupolas_ent_ipc_permission_t;

/**
 * @brief Resource limits structure
 */
typedef struct {
    uint32_t max_cpu_percent;       /**< Max CPU percentage */
    uint32_t max_cpu_cores;         /**< Max CPU cores */
    uint64_t max_memory_bytes;      /**< Max memory in bytes */
    uint64_t max_disk_bytes;        /**< Max disk in bytes */
    uint32_t max_processes;         /**< Max processes */
    uint32_t max_threads;           /**< Max threads */
    uint32_t max_open_files;        /**< Max open files */
    uint32_t max_network_connections; /**< Max network connections */
} cupolas_ent_resource_limits_t;

/**
 * @brief Vault access permission structure
 */
typedef struct {
    char* cred_id;                  /**< Credential identifier */
    char** permissions;             /**< Permission list (read, write, delete) */
    size_t perm_count;              /**< Number of permissions */
} cupolas_ent_vault_permission_t;

/**
 * @brief Entitlements context (opaque)
 */
typedef struct cupolas_entitlements cupolas_entitlements_t;

/**
 * @brief Complete entitlements structure
 */
typedef struct {
    char* agent_id;                 /**< Agent identifier */
    char* version;                  /**< Version string */
    uint64_t not_before;            /**< Validity start */
    uint64_t not_after;             /**< Validity end */
    
    cupolas_ent_fs_permission_t* fs_permissions;
    size_t fs_count;
    
    cupolas_ent_net_permission_t* net_permissions;
    size_t net_count;
    
    cupolas_ent_ipc_permission_t* ipc_permissions;
    size_t ipc_count;
    
    cupolas_ent_resource_limits_t resources;
    
    cupolas_ent_vault_permission_t* vault_permissions;
    size_t vault_count;
    
    char** allowed_syscalls;
    size_t syscall_count;
    
    char** allowed_capabilities;
    size_t cap_count;
} cupolas_entitlements_info_t;

/**
 * @brief Initialize Entitlements module
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (initialization only)
 * @reentrant No
 */
int cupolas_entitlements_init(void);

/**
 * @brief Shutdown Entitlements module
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 */
void cupolas_entitlements_cleanup(void);

/**
 * @brief Load entitlements from YAML file
 * @param[in] yaml_path Path to YAML file
 * @param[out] entitlements Output context
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (initialization only)
 * @reentrant No
 * @ownership yaml_path: caller retains ownership
 * @ownership entitlements: caller provides buffer, function writes to it
 */
int cupolas_entitlements_load(const char* yaml_path,
                             cupolas_entitlements_t** entitlements);

/**
 * @brief Load entitlements from JSON file
 * @param[in] json_path Path to JSON file
 * @param[out] entitlements Output context
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (initialization only)
 * @reentrant No
 * @ownership json_path: caller retains ownership
 * @ownership entitlements: caller provides buffer, function writes to it
 */
int cupolas_entitlements_load_json(const char* json_path,
                                  cupolas_entitlements_t** entitlements);

/**
 * @brief Load entitlements from string
 * @param[in] yaml_content YAML content string
 * @param[out] entitlements Output context
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (initialization only)
 * @reentrant No
 * @ownership yaml_content: caller retains ownership
 * @ownership entitlements: caller provides buffer, function writes to it
 */
int cupolas_entitlements_load_string(const char* yaml_content,
                                    cupolas_entitlements_t** entitlements);

/**
 * @brief Free entitlements context
 * @param[in] entitlements Entitlements context (may be NULL)
 * @note Thread-safe: Safe to call from multiple threads
 * @reentrant No
 * @ownership entitlements: transferred to this function, will be freed
 */
void cupolas_entitlements_free(cupolas_entitlements_t* entitlements);

/**
 * @brief Verify entitlements signature
 * @param[in] entitlements Entitlements context
 * @param[in] public_key Verification public key (PEM format)
 * @return 0 if valid, negative if invalid
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership entitlements and public_key: caller retains ownership
 */
int cupolas_entitlements_verify(cupolas_entitlements_t* entitlements,
                               const char* public_key);

/**
 * @brief Sign entitlements
 * @param[in] entitlements Entitlements context
 * @param[in] private_key Signing private key (PEM format)
 * @param[out] signature_out Signature output buffer
 * @param[in,out] sig_len Buffer size / actual length
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 * @ownership entitlements and private_key: caller retains ownership
 * @ownership signature_out: caller provides buffer, function writes to it
 */
int cupolas_entitlements_sign(cupolas_entitlements_t* entitlements,
                             const char* private_key,
                             char* signature_out, size_t* sig_len);

/**
 * @brief Check if entitlements is signed
 * @param[in] entitlements Entitlements context
 * @return true if signed, false otherwise
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 */
bool cupolas_entitlements_is_signed(cupolas_entitlements_t* entitlements);

/**
 * @brief Check file system permission
 * @param[in] entitlements Entitlements context
 * @param[in] path File path
 * @param[in] operation Operation type (read, write, create, delete, execute)
 * @return 1 if allowed, 0 if denied
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership entitlements, path, and operation: caller retains ownership
 */
int cupolas_entitlements_check_fs(cupolas_entitlements_t* entitlements,
                                 const char* path,
                                 const char* operation);

/**
 * @brief Check network permission
 * @param[in] entitlements Entitlements context
 * @param[in] host Target host
 * @param[in] port Port number
 * @param[in] protocol Protocol
 * @param[in] direction Direction (inbound, outbound)
 * @return 1 if allowed, 0 if denied
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership entitlements, host, protocol, and direction: caller retains ownership
 */
int cupolas_entitlements_check_net(cupolas_entitlements_t* entitlements,
                                  const char* host,
                                  uint16_t port,
                                  const char* protocol,
                                  const char* direction);

/**
 * @brief Check IPC permission
 * @param[in] entitlements Entitlements context
 * @param[in] target Target service
 * @param[in] operation Operation type
 * @return 1 if allowed, 0 if denied
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership entitlements, target, and operation: caller retains ownership
 */
int cupolas_entitlements_check_ipc(cupolas_entitlements_t* entitlements,
                                  const char* target,
                                  const char* operation);

/**
 * @brief Check syscall permission
 * @param[in] entitlements Entitlements context
 * @param[in] syscall_name Syscall name
 * @return 1 if allowed, 0 if denied
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership entitlements and syscall_name: caller retains ownership
 */
int cupolas_entitlements_check_syscall(cupolas_entitlements_t* entitlements,
                                      const char* syscall_name);

/**
 * @brief Check capability permission
 * @param[in] entitlements Entitlements context
 * @param[in] capability Capability name
 * @return 1 if allowed, 0 if denied
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership entitlements and capability: caller retains ownership
 */
int cupolas_entitlements_check_capability(cupolas_entitlements_t* entitlements,
                                         const char* capability);

/**
 * @brief Check vault access permission
 * @param[in] entitlements Entitlements context
 * @param[in] cred_id Credential identifier
 * @param[in] operation Operation type
 * @return 1 if allowed, 0 if denied
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership entitlements, cred_id, and operation: caller retains ownership
 */
int cupolas_entitlements_check_vault(cupolas_entitlements_t* entitlements,
                                    const char* cred_id,
                                    const char* operation);

/**
 * @brief Get resource limits
 * @param[in] entitlements Entitlements context
 * @param[out] limits Resource limits output
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership entitlements: caller retains ownership
 * @ownership limits: caller provides buffer, function writes to it
 */
int cupolas_entitlements_get_resource_limits(cupolas_entitlements_t* entitlements,
                                            cupolas_ent_resource_limits_t* limits);

/**
 * @brief Check if resource usage exceeds limit
 * @param[in] entitlements Entitlements context
 * @param[in] resource_type Resource type (cpu, memory, disk, process, thread, file, connection)
 * @param[in] current_value Current usage value
 * @return 1 if within limit, 0 if exceeded
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership entitlements and resource_type: caller retains ownership
 */
int cupolas_entitlements_check_resource(cupolas_entitlements_t* entitlements,
                                       const char* resource_type,
                                       uint64_t current_value);

/**
 * @brief Get complete entitlements information
 * @param[in] entitlements Entitlements context
 * @param[out] info Information output
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership entitlements: caller retains ownership
 * @ownership info: caller provides buffer, function writes to it
 */
int cupolas_entitlements_get_info(cupolas_entitlements_t* entitlements,
                                 cupolas_entitlements_info_t* info);

/**
 * @brief Free entitlements info structure
 * @param[in] info Info pointer (may be NULL)
 * @note Thread-safe: Safe to call from multiple threads
 * @reentrant No
 * @ownership info: transferred to this function, will be freed
 */
void cupolas_entitlements_free_info(cupolas_entitlements_info_t* info);

/**
 * @brief Get Agent ID
 * @param[in] entitlements Entitlements context
 * @return Agent ID string (static, do not free)
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 */
const char* cupolas_entitlements_get_agent_id(cupolas_entitlements_t* entitlements);

/**
 * @brief Check validity period
 * @param[in] entitlements Entitlements context
 * @return 0 if valid, negative if invalid
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 */
int cupolas_entitlements_check_validity(cupolas_entitlements_t* entitlements);

/**
 * @brief Export to YAML format
 * @param[in] entitlements Entitlements context
 * @param[out] yaml_out YAML output buffer
 * @param[in,out] len Buffer size / actual length
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership entitlements: caller retains ownership
 * @ownership yaml_out: caller provides buffer, function writes to it
 */
int cupolas_entitlements_export_yaml(cupolas_entitlements_t* entitlements,
                                    char* yaml_out, size_t* len);

/**
 * @brief Export to JSON format
 * @param[in] entitlements Entitlements context
 * @param[out] json_out JSON output buffer
 * @param[in,out] len Buffer size / actual length
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership entitlements: caller retains ownership
 * @ownership json_out: caller provides buffer, function writes to it
 */
int cupolas_entitlements_export_json(cupolas_entitlements_t* entitlements,
                                    char* json_out, size_t* len);

/**
 * @brief Get error description string
 * @param[in] result Error code
 * @return Error description (static, do not free)
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 */
const char* cupolas_entitlements_result_string(cupolas_ent_result_t result);

/**
 * @brief Match path pattern
 * @param[in] pattern Pattern (supports * and ?)
 * @param[in] path Actual path
 * @return 1 if matches, 0 otherwise
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership pattern and path: caller retains ownership
 */
int cupolas_entitlements_match_path(const char* pattern, const char* path);

/**
 * @brief Match host pattern
 * @param[in] pattern Pattern (supports * prefix wildcard)
 * @param[in] host Actual host
 * @return 1 if matches, 0 otherwise
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership pattern and host: caller retains ownership
 */
int cupolas_entitlements_match_host(const char* pattern, const char* host);

#ifdef __cplusplus
}
#endif

#endif /* CUPOLAS_ENTITLEMENTS_H */

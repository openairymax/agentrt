/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * daemon_security.h - Daemon Layer Security Integration with cupolas Module
 *
 * Design Principles:
 * - Security by Default: All daemon services must use cupolas security features
 * - Zero Trust: Every request must be validated and sanitized
 * - Defense in Depth: Multiple security layers for comprehensive protection
 * - Audit Trail: All operations must be logged for compliance
 */

#ifndef DAEMON_SECURITY_H
#define DAEMON_SECURITY_H

/*
 * External Dependency Guard (E-1 安全内生 + E-3 资源确定性 + S-2 层次分解)
 *
 * daemon_security 依赖 cupolas 安全框架。提供两种编译模式:
 * 1. 正常模式 (默认): 直接包含 cupolas 头文件
 * 2. 存根模式 (DAEMON_SECURITY_STUB_MODE): 不链接 cupolas，函数降级为安全默认值
 *
 * 设计决策依据 ARCHITECTURAL_PRINCIPLES.md:
 * - K-4 零信任: 即使 cupolas 不可用也不应导致编译失败
 * - E-6 错误可追溯: 存根模式下记录明确的降级警告日志
 */
#ifdef DAEMON_SECURITY_STUB_MODE
    #define CUPOLAS_AVAILABLE 0
#else
    #ifdef __has_include
        #if __has_include("../../../agentos/cupolas/include/cupolas.h")
            #define CUPOLAS_AVAILABLE 1
            #include "../../../agentos/cupolas/include/cupolas.h"
            #include "../../../agentos/cupolas/src/sanitizer/sanitizer.h"
            #include "../../../agentos/cupolas/src/permission/permission.h"
            #include "../../../agentos/cupolas/src/security/cupolas_signature.h"
            #include "../../../agentos/cupolas/src/security/cupolas_vault.h"
        #else
            #define CUPOLAS_AVAILABLE 0
            #warning "daemon_security: cupolas headers not found, degraded mode"
        #endif
    #else
        #define CUPOLAS_AVAILABLE 1
        #include "../../agentos/cupolas/include/cupolas.h"
        #include "../../agentos/cupolas/src/sanitizer/sanitizer.h"
        #include "../../agentos/cupolas/src/permission/permission.h"
        #include "../../agentos/cupolas/src/security/cupolas_signature.h"
        #include "../../agentos/cupolas/src/security/cupolas_vault.h"
    #endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Daemon security configuration structure
 * 
 * Provides unified security configuration for all daemon services:
 * - Input sanitization level
 * - Permission checking mode
 * - Code signature verification
 * - Audit logging settings
 */
typedef struct daemon_security_config {
    /* Sanitizer configuration */
    sanitize_level_t sanitize_level;        /**< Input sanitization level */
    const char* sanitizer_rules_path;      /**< Path to sanitizer rules file */

    /* Permission configuration */
    const char* permission_rules_path;     /**< Path to permission rules file */
    bool enable_permission_cache;           /**< Enable permission result caching */

    /* Signature verification configuration */
    bool enable_signature_verification;     /**< Enable code signature verification */
    const char* trusted_ca_path;            /**< Trusted CA bundle path */
    const char* expected_signer;             /**< Expected signer CN (optional) */

    /* Vault configuration */
    bool enable_vault;                      /**< Enable secure credential storage */
    const char* vault_storage_path;          /**< Path to vault storage */

    /* Audit configuration */
    bool enable_audit_logging;              /**< Enable audit logging */
    const char* audit_log_dir;               /**< Directory for audit logs */
} daemon_security_config_t;

/**
 * @brief Initialize daemon security layer
 * 
 * This function initializes all cupolas security components for daemon usage.
 * It should be called once during daemon startup before any service initialization.
 * 
 * @param[in] config Security configuration (NULL for defaults)
 * @param[out] error Optional error code output
 * @return 0 on success, negative on failure
 * 
 * @note Thread-safe: Safe to call from main thread only during initialization
 * @reentrant No
 * 
 * @ownership config: caller retains ownership, may be NULL
 * @ownership error: caller provides buffer, function writes to it
 * 
 * @details
 * This function performs the following initializations:
 * 1. Initializes the core cupolas module
 * 2. Loads sanitizer rules from configuration
 * 3. Loads permission rules from configuration
 * 4. Initializes code signature verification (if enabled)
 * 5. Opens secure vault (if enabled)
 * 6. Configures audit logging
 * 
 * Example usage:
 * @code
 * daemon_security_config_t sec_config = {
 *     .sanitize_level = SANITIZE_LEVEL_STRICT,
 *     .sanitizer_rules_path = "/etc/agentos/cupolas/sanitizer_rules.yaml",
 *     .permission_rules_path = "/etc/agentos/cupolas/permission_rules.yaml",
 *     .enable_permission_cache = true,
 *     .enable_signature_verification = true,
 *     .trusted_ca_path = "/etc/agentos/cupolas/ca",
 *     .expected_signer = "SPHARX Trusted Signer",
 *     .enable_vault = true,
 *     .vault_storage_path = "/var/lib/agentos/cupolas/vault",
 *     .enable_audit_logging = true,
 *     .audit_log_dir = "/var/log/cupolas"
 * };
 * 
 * agentos_error_t error;
 * int ret = daemon_security_init(&sec_config, &error);
 * if (ret != 0) {
 *     fprintf(stderr, "Security init failed: %s\n", error.message);
 * }
 * @endcode
 */
int daemon_security_init(const daemon_security_config_t* config, agentos_error_t* error);

/**
 * @brief Shutdown daemon security layer
 * 
 * Cleans up all cupolas security components. Should be called during daemon shutdown.
 * 
 * @note Thread-safe: Safe to call from main thread only during shutdown
 * @reentrant No
 */
void daemon_security_shutdown(void);

/**
 * @brief Sanitize input string for LLM service requests
 * 
 * Sanitizes user input to prevent injection attacks before passing to LLM providers.
 * Uses the configured sanitization level from daemon_security_init().
 * 
 * @param[in] input Raw input string from user or external source
 * @param[out] output Sanitized output buffer
 * @param[in] output_size Size of output buffer
 * @return 0 on success, negative on failure
 * 
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * 
 * @ownership input: caller retains ownership
 * @ownership output: caller provides buffer, function writes to it
 * 
 * @details
 * Sanitizes against:
 * - SQL injection attacks
 * - XSS (Cross-Site Scripting)
 * - Command injection attacks
 * - Path traversal attacks
 * - Special character attacks
 * 
 * Example usage:
 * @code
 * char sanitized[4096];
 * const char* user_input = "SELECT * FROM users WHERE id=1 OR 1=1";
 * 
 * if (daemon_sanitize_llm_input(user_input, sanitized, sizeof(sanitized)) == 0) {
 *     // Use sanitized input for LLM call
 * } else {
 *     // Handle sanitization failure
 * }
 * @endcode
 */
int daemon_sanitize_llm_input(const char* input, char* output, size_t output_size);

/**
 * @brief Sanitize tool execution parameters
 * 
 * Sanitizes tool name and parameters to prevent command injection.
 * Should be called before any tool execution in the tool_d service.
 * 
 * @param[in] tool_name Tool name to sanitize
 * @param[in] params Tool parameters (JSON format)
 * @param[out] sanitized_tool Output buffer for sanitized tool name
 * @param[in] tool_buf_size Size of tool output buffer
 * @param[out] sanitized_params Output buffer for sanitized parameters
 * @param[in] param_buf_size Size of parameters output buffer
 * @return 0 on success, negative on failure
 * 
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * 
 * @ownership All parameters: caller retains ownership or provides buffers as appropriate
 * 
 * @details
 * Performs comprehensive sanitization including:
 * - Tool name validation against whitelist
 * - Parameter type validation
 * - Command argument escaping
 * - Path traversal prevention
 */
int daemon_sanitize_tool_params(const char* tool_name, const char* params,
                                  char* sanitized_tool, size_t tool_buf_size,
                                  char* sanitized_params, size_t param_buf_size);

/**
 * @brief Check tool execution permission
 * 
 * Verifies that the requesting agent has permission to execute a specific tool.
 * Should be called before tool execution in the tool_d service.
 * 
 * @param[in] agent_id Agent identifier requesting access
 * @param[in] tool_name Name of the tool to execute
 * @param[in] action Action type ("execute", "read", "write")
 * @return 1 if allowed, 0 if denied
 * 
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * 
 * @ownership All parameters: caller retains ownership
 * 
 * @details
 * Checks against configured permission rules:
 * - Agent identity verification
 * - Tool-specific permissions
 * - Rate limiting checks
 * - Context-aware authorization
 * 
 * Example usage:
 * @code
 * if (!daemon_check_tool_permission("agent-001", "file_read", "execute")) {
 *     return AGENTOS_ERR_PERMISSION_DENIED;
 * }
 * // Proceed with tool execution
 * @endcode
 */
int daemon_check_tool_permission(const char* agent_id, const char* tool_name,
                                 const char* action);

/**
 * @brief Check LLM API call permission
 * 
 * Verifies that the requesting agent has permission to make LLM API calls.
 * Should be called before any LLM provider interaction in the llm_d service.
 * 
 * @param[in] agent_id Agent identifier requesting access
 * @param[in] model_name Name of the LLM model to use
 * @param[in] action Action type ("query", "stream", "embed")
 * @return 1 if allowed, 0 if denied
 * 
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * 
 * @ownership All parameters: caller retains ownership
 */
int daemon_check_llm_permission(const char* agent_id, const char* model_name,
                                 const char* action);

/**
 * @brief Verify Agent/Skill package signature
 * 
 * Verifies the digital signature of an Agent or Skill package before installation.
 * Should be called in the market_d service during package installation.
 * 
 * @param[in] package_path Path to the package file (binary or archive)
 * @param[out] is_valid Output flag indicating valid signature
 * @param[out] signer_info Signer information output (may be NULL)
 * @return 0 on success, negative on failure
 * 
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * 
 * @ownership package_path: caller retains ownership
 * @ownership is_valid: caller provides buffer, function writes to it
 * @ownership signer_info: caller provides buffer, function writes to it (may be NULL)
 * 
 * @details
 * Performs comprehensive signature verification:
 * - Cryptographic signature verification
 * - Certificate chain validation
 * - Revocation status check
 * - Timestamp verification
 * - Integrity hash comparison
 */
int daemon_verify_package_signature(const char* package_path, bool* is_valid,
                                     cupolas_signer_info_t* signer_info);

/**
 * @brief Store secure credential in vault
 * 
 * Stores sensitive credentials (API keys, tokens, passwords) securely.
 * Should be used instead of storing credentials in plain text files.
 * 
 * @param[in] cred_id Unique credential identifier
 * @param[in] cred_type Credential type (password/token/key/cert/etc.)
 * @param[in] data Credential data
 * @param[in] data_len Length of credential data
 * @param[in] agent_id Owner agent ID (for ACL)
 * @return 0 on success, negative on failure
 * 
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * 
 * @ownership cred_id, agent_id: caller retains ownership
 * @ownership data: caller retains ownership during this call
 * 
 * @example
 * @code
 * const char* api_key = "sk-abc123xyz456";
 * int ret = daemon_store_credential("openai_api_key", CUPOLAS_VAULT_CRED_TOKEN,
 *                                   api_key, strlen(api_key), "agent-001");
 * @endcode
 */
int daemon_store_credential(const char* cred_id, cupolas_vault_cred_type_t cred_type,
                           const uint8_t* data, size_t data_len,
                           const char* agent_id);

/**
 * @brief Retrieve secure credential from vault
 * 
 * Retrieves previously stored secure credential.
 * 
 * @param[in] cred_id Credential identifier
 * @param[in] agent_id Requesting agent ID (for ACL check)
 * @param[out] data Output buffer for credential data
 * @param[in,out] data_len Buffer size / actual length
 * @return 0 on success, negative on failure
 * 
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * 
 * @ownership cred_id, agent_id: caller retains ownership
 * @ownership data: caller provides buffer, function writes to it
 */
int daemon_retrieve_credential(const char* cred_id, const char* agent_id,
                                uint8_t* data, size_t* data_len);

/**
 * @brief Log audit event for daemon operation
 * 
 * Records an audit event for compliance tracking.
 * Should be called after important operations (tool execution, LLM calls, etc.).
 * 
 * @param[in] service_name Service name (e.g., "tool_d", "llm_d", "market_d")
 * @param[in] operation Operation performed
 * @param[in] resource Resource accessed
 * @param[in] result Result of operation (success/failure)
 * @param[in] agent_id Agent performing the operation
 * @return 0 on success, negative on failure
 * 
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * 
 * @ownership All parameters: caller retains ownership
 * 
 * @details
 * Logs include:
 * - Timestamp
 * - Service identification
 * - Operation details
 * - Resource information
 * - Result status
 * - Agent identification
 */
int daemon_audit_log_event(const char* service_name, const char* operation,
                             const char* resource, int result,
                             const char* agent_id);

/**
 * @brief Get daemon security status
 * 
 * Returns the current status of all security components.
 * Useful for health checks and monitoring.
 * 
 * @param[out] sanitizer_status Sanitizer status (1=active, 0=inactive)
 * @param[out] permission_status Permission engine status
 * @param[out] signature_status Signature verification status
 * @param[out] vault_status Vault status
 * @param[out] audit_status Audit logging status
 * @return 0 on success, negative on failure
 * 
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * 
 * @ownership All output parameters: caller provides buffers, function writes to them
 */
int daemon_security_get_status(int* sanitizer_status, int* permission_status,
                               int* signature_status, int* vault_status,
                               int* audit_status);

#ifdef __cplusplus
}
#endif

#endif /* DAEMON_SECURITY_H */

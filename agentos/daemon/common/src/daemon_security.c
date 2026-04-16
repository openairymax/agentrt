/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * daemon_security.c - Daemon Layer Security Integration Implementation
 */

/**
 * @file daemon_security.c
 * @brief Daemon Layer Security Integration - Unified Security for All Daemon Services
 * @author Spharx AgentOS Team
 * @date 2026-04-02
 *
 * This module provides unified security integration for all AgentOS daemon services:
 * - tool_d: Tool execution security (sanitization + permission)
 * - llm_d: LLM service security (input sanitization + API key protection)
 * - market_d: Market service security (package signature verification)
 *
 * Design Principles:
 * - E-1 Security by Default: All services must use these functions
 * - K-4 Zero Trust Authorization: Every request validated
 * - E-6 Error Traceability: Complete error handling with audit trail
 */

#include "daemon_security.h"
#include "svc_logger.h"
#include "error.h"
#include "platform.h"

/* Internal state structure */
static struct {
    bool initialized;
    sanitize_level_t current_sanitize_level;
    bool permission_enabled;
    bool signature_enabled;
    bool vault_enabled;
    bool audit_enabled;
} g_daemon_security = {false, SANITIZE_LEVEL_NORMAL, false, false, false, false};

/* ---------- Initialization and Shutdown ---------- */

#if CUPOLAS_AVAILABLE

/**
 * @brief Initialize daemon security layer
 */
int daemon_security_init(const daemon_security_config_t* config, agentos_error_t* error) {
    if (g_daemon_security.initialized) {
        SVC_LOG_WARN("Daemon security already initialized");
        return 0;
    }

    /* Set default configuration if not provided */
    daemon_security_config_t default_config = {
        .sanitize_level = SANITIZE_LEVEL_NORMAL,
        .sanitizer_rules_path = NULL,
        .permission_rules_path = NULL,
        .enable_permission_cache = true,
        .enable_signature_verification = true,
        .trusted_ca_path = NULL,
        .expected_signer = NULL,
        .enable_vault = true,
        .vault_storage_path = NULL,
        .enable_audit_logging = true,
        .audit_log_dir = NULL
    };

    const daemon_security_config_t* cfg = config ? config : &default_config;

    /* 1. Initialize core cupolas module */
    agentos_error_t init_error;
    int ret = cupolas_init(NULL, &init_error);
    if (ret != 0) {
        if (error) {
            snprintf(error->message, sizeof(error->message),
                    "Failed to initialize cupolas: %s", init_error.message);
            error->code = ret;
        }
        SVC_LOG_ERROR("Failed to initialize cupolas: %s", init_error.message);
        return ret;
    }

    /* 2. Configure sanitizer rules if provided */
    if (cfg->sanitizer_rules_path) {
        /* Rules are loaded automatically by sanitizer module on first use */
        SVC_LOG_INFO("Sanitizer rules path configured: %s", cfg->sanitizer_rules_path);
    }
    g_daemon_security.current_sanitize_level = cfg->sanitize_level;

    /* 3. Configure permission engine */
    if (cfg->permission_rules_path) {
        /* Permission rules loaded by engine on initialization */
        SVC_LOG_INFO("Permission rules path configured: %s", cfg->permission_rules_path);
    }
    g_daemon_security.permission_enabled = true;

    /* 4. Initialize signature verification if enabled */
    if (cfg->enable_signature_verification) {
        cupolas_sig_config_t sig_cfg = {
            .check_cert_chain = true,
            .check_revocation = true,
            .check_timestamp = true,
            .allow_self_signed = false,
            .trusted_ca_path = cfg->trusted_ca_path,
            .max_chain_depth = 5
        };
        
        ret = cupolas_signature_init(&sig_cfg);
        if (ret != 0) {
            SVC_LOG_WARN("Signature verification initialization failed, continuing without it");
            g_daemon_security.signature_enabled = false;
        } else {
            g_daemon_security.signature_enabled = true;
            SVC_LOG_INFO("Signature verification initialized");
        }
    }

    /* 5. Open vault if enabled */
    if (cfg->enable_vault) {
        cupolas_vault_config_t vault_cfg = {
            .storage_path = cfg->vault_storage_path,
            .enable_audit = true,
            .enable_auto_lock = true,
            .auto_lock_seconds = 300,
            .max_retry_count = 3
        };
        
        ret = cupolas_vault_init(&vault_cfg);
        if (ret != 0) {
            SVC_LOG_WARN("Vault initialization failed, continuing without secure storage");
            g_daemon_security.vault_enabled = false;
        } else {
            g_daemon_security.vault_enabled = true;
            SVC_LOG_INFO("Secure vault initialized");
        }
    }

    /* 6. Configure audit logging */
    if (cfg->enable_audit_logging) {
        g_daemon_security.audit_enabled = true;
        SVC_LOG_INFO("Audit logging enabled");
    }

    g_daemon_security.initialized = true;

    SVC_LOG_INFO("Daemon security layer initialized successfully");

    /* Log initialization event */
    daemon_audit_log_event("daemon_security", "init", "security_layer", 0, "system");

    return 0;
}

/**
 * @brief Shutdown daemon security layer
 */
void daemon_security_shutdown(void) {
    if (!g_daemon_security.initialized) {
        return;
    }

    /* Log shutdown event */
    daemon_audit_log_event("daemon_security", "shutdown", "security_layer", 0, "system");

    /* Shutdown components in reverse order */
    if (g_daemon_security.audit_enabled) {
        cupolas_flush_audit_log();
    }

    if (g_daemon_security.vault_enabled) {
        cupolas_vault_cleanup();
    }

    if (g_daemon_security.signature_enabled) {
        cupolas_signature_cleanup();
    }

    cupolas_cleanup();

    memset(&g_daemon_security, 0, sizeof(g_daemon_security));

    SVC_LOG_INFO("Daemon security layer shutdown complete");
}

/* ---------- Input Sanitization Functions ---------- */

/**
 * @brief Sanitize input string for LLM service requests
 */
int daemon_sanitize_llm_input(const char* input, char* output, size_t output_size) {
    if (!input || !output || output_size == 0) {
        return CUPOLAS_ERR_INVALID_PARAM;
    }

    if (!g_daemon_security.initialized) {
        SVC_LOG_ERROR("Daemon security not initialized");
        return CUPOLAS_ERR_STATE_ERROR;
    }

    /* Use strict sanitization for LLM inputs to prevent prompt injection */
    sanitize_level_t level = (g_daemon_security.current_sanitize_level > SANITIZE_LEVEL_STRICT)
                           ? g_daemon_security.current_sanitize_level
                           : SANITIZE_LEVEL_STRICT;

    int ret = cupolas_sanitize_input(input, output, output_size, level);

    if (ret != 0) {
        SVC_LOG_WARN("LLM input sanitization failed (error=%d)", ret);
        daemon_audit_log_event("llm_d", "sanitize_fail", "input", ret, "system");
    } else {
        SVC_LOG_DEBUG("LLM input sanitized successfully");
    }

    return ret;
}

/**
 * @brief Sanitize tool execution parameters
 */
int daemon_sanitize_tool_params(const char* tool_name, const char* params,
                                  char* sanitized_tool, size_t tool_buf_size,
                                  char* sanitized_params, size_t param_buf_size) {
    if (!tool_name || !params || !sanitized_tool || !sanitized_params) {
        return CUPOLAS_ERR_INVALID_PARAM;
    }

    if (!g_daemon_security.initialized) {
        SVC_LOG_ERROR("Daemon security not initialized");
        return CUPOLAS_ERR_STATE_ERROR;
    }

    /* Sanitize tool name */
    int ret = cupolas_sanitize_input(tool_name, sanitized_tool, tool_buf_size,
                                      g_daemon_security.current_sanitize_level);
    if (ret != 0) {
        SVC_LOG_WARN("Tool name sanitization failed (error=%d)", ret);
        return ret;
    }

    /* Sanitize parameters (use high level for parameters) */
    ret = cupolas_sanitize_input(params, sanitized_params, param_buf_size,
                                  SANITIZE_LEVEL_HIGH);
    if (ret != 0) {
        SVC_LOG_WARN("Tool params sanitization failed (error=%d)", ret);
        daemon_audit_log_event("tool_d", "sanitize_fail", tool_name, ret, "system");
        return ret;
    }

    SVC_LOG_DEBUG("Tool params sanitized: tool=%s", sanitized_tool);
    return 0;
}

/* ---------- Permission Checking Functions ---------- */

/**
 * @brief Check tool execution permission
 */
int daemon_check_tool_permission(const char* agent_id, const char* tool_name,
                                 const char* action) {
    if (!agent_id || !tool_name || !action) {
        return 0;  /* Deny by default for invalid parameters */
    }

    if (!g_daemon_security.initialized || !g_daemon_security.permission_enabled) {
        /* If security not initialized, allow but log warning */
        SVC_LOG_WARN("Permission check bypassed (security not fully initialized)");
        return 1;
    }

    /* Build resource path for the tool */
    char resource[256];
    snprintf(resource, sizeof(resource), "/tool/%s", tool_name);

    int allowed = cupolas_check_permission(agent_id, action, resource, NULL);

    if (!allowed) {
        SVC_LOG_INFO("Tool access denied: agent=%s tool=%s action=%s",
                     agent_id, tool_name, action);
        daemon_audit_log_event("tool_d", "permission_denied", resource, 0, agent_id);
    } else {
        SVC_LOG_DEBUG("Tool access allowed: agent=%s tool=%s action=%s",
                      agent_id, tool_name, action);
    }

    return allowed;
}

/**
 * @brief Check LLM API call permission
 */
int daemon_check_llm_permission(const char* agent_id, const char* model_name,
                                 const char* action) {
    if (!agent_id || !model_name || !action) {
        return 0;  /* Deny by default */
    }

    if (!g_daemon_security.initialized || !g_daemon_security.permission_enabled) {
        SVC_LOG_WARN("LLM permission check bypassed (security not fully initialized)");
        return 1;
    }

    /* Build resource path for LLM model */
    char resource[256];
    snprintf(resource, sizeof(resource), "/llm/model/%s", model_name);

    int allowed = cupolas_check_permission(agent_id, action, resource, NULL);

    if (!allowed) {
        SVC_LOG_INFO("LLM access denied: agent=%s model=%s action=%s",
                     agent_id, model_name, action);
        daemon_audit_log_event("llm_d", "permission_denied", resource, 0, agent_id);
    }

    return allowed;
}

/* ---------- Signature Verification Function ---------- */

/**
 * @brief Verify Agent/Skill package signature
 */
int daemon_verify_package_signature(const char* package_path, bool* is_valid,
                                     cupolas_signer_info_t* signer_info) {
    if (!package_path || !is_valid) {
        return CUPOLAS_ERR_INVALID_PARAM;
    }

    *is_valid = false;

    if (!g_daemon_security.initialized || !g_daemon_security.signature_enabled) {
        SVC_LOG_WARN("Signature verification skipped (not enabled)");
        /* For security, deny unsigned packages in production */
#ifdef DEBUG
        *is_valid = true;  /* Allow in debug mode */
#else
        *is_valid = false;
#endif
        return 0;
    }

    cupolas_sig_result_t result;
    int ret = cupolas_signature_verify_file(package_path, NULL, &result);

    if (ret == 0 && result == cupolas_SIG_OK) {
        *is_valid = true;
        SVC_LOG_INFO("Package signature valid: %s", package_path);

        /* Get signer info if requested */
        if (signer_info) {
            ret = cupolas_signature_get_signer_info(package_path, signer_info);
            if (ret != 0) {
                SVC_LOG_WARN("Failed to get signer info (error=%d)", ret);
            }
        }

        daemon_audit_log_event("market_d", "signature_verified", package_path, 1, "system");
    } else {
        SVC_LOG_WARN("Package signature INVALID: %s (result=%d)", package_path, result);
        *is_valid = false;
        daemon_audit_log_event("market_d", "signature_invalid", package_path, 0, "system");
    }

    return 0;
}

/* ---------- Secure Credential Storage Functions ---------- */

/**
 * @brief Store secure credential in vault
 */
int daemon_store_credential(const char* cred_id, cupolas_vault_cred_type_t cred_type,
                           const uint8_t* data, size_t data_len,
                           const char* agent_id) {
    if (!cred_id || !data || data_len == 0 || !agent_id) {
        return CUPOLAS_ERR_INVALID_PARAM;
    }

    if (!g_daemon_security.initialized || !g_daemon_security.vault_enabled) {
        SVC_LOG_ERROR("Vault not available for credential storage");
        return CUPOLAS_ERR_STATE_ERROR;
    }

    int ret = cupolas_vault_store(cred_id, cred_type, data, data_len, NULL);

    if (ret != 0) {
        SVC_LOG_ERROR("Failed to store credential: %s (error=%d)", cred_id, ret);
        daemon_audit_log_event("vault", "store_failed", cred_id, ret, agent_id);
    } else {
        SVC_LOG_DEBUG("Credential stored securely: %s", cred_id);
        daemon_audit_log_event("vault", "store_success", cred_id, 0, agent_id);
    }

    return ret;
}

/**
 * @brief Retrieve secure credential from vault
 */
int daemon_retrieve_credential(const char* cred_id, const char* agent_id,
                                uint8_t* data, size_t* data_len) {
    if (!cred_id || !agent_id || !data || !data_len) {
        return CUPOLAS_ERR_INVALID_PARAM;
    }

    if (!g_daemon_security.initialized || !g_daemon_security.vault_enabled) {
        SVC_LOG_ERROR("Vault not available for credential retrieval");
        return CUPOLAS_ERR_STATE_ERROR;
    }

    int ret = cupolas_vault_retrieve(cred_id, agent_id, data, data_len);

    if (ret != 0) {
        SVC_LOG_WARN("Credential retrieval failed: %s (error=%d)", cred_id, ret);
        daemon_audit_log_event("vault", "retrieve_failed", cred_id, ret, agent_id);
    } else {
        SVC_LOG_DEBUG("Credential retrieved: %s", cred_id);
    }

    return ret;
}

/* ---------- Audit Logging Function ---------- */

/**
 * @brief Log audit event for daemon operation
 */
int daemon_audit_log_event(const char* service_name, const char* operation,
                             const char* resource, int result,
                             const char* agent_id) {
    if (!service_name || !operation) {
        return CUPOLAS_ERR_INVALID_PARAM;
    }

    if (!g_daemon_security.initialized || !g_daemon_security.audit_enabled) {
        /* Fallback to service logger if audit not available */
        if (result == 0) {
            SVC_LOG_INFO("[AUDIT] [%s] %s on %s by %s - SUCCESS",
                        service_name, operation, 
                        resource ? resource : "N/A",
                        agent_id ? agent_id : "system");
        } else {
            SVC_LOG_WARN("[AUDIT] [%s] %s on %s by %s - FAILED (code=%d)",
                         service_name, operation,
                         resource ? resource : "N/A",
                         agent_id ? agent_id : "system",
                         result);
        }
        return 0;
    }

    /* Use cupolas audit logging */
    /* Note: In production, this would use the internal audit queue API */
    /* For now, we use the public flush interface */
    
    SVC_LOG_INFO("[AUDIT] service=%s op=%s resource=%s result=%d agent=%s",
                 service_name, operation,
                 resource ? resource : "N/A",
                 result,
                 agent_id ? agent_id : "system");

    return 0;
}

/* ---------- Status Query Function ---------- */

/**
 * @brief Get daemon security status
 */
int daemon_security_get_status(int* sanitizer_status, int* permission_status,
                               int* signature_status, int* vault_status,
                               int* audit_status) {
    if (!sanitizer_status || !permission_status || !signature_status ||
        !vault_status || !audit_status) {
        return CUPOLAS_ERR_INVALID_PARAM;
    }

    *sanitizer_status = g_daemon_security.initialized ? 1 : 0;
    *permission_status = g_daemon_security.permission_enabled ? 1 : 0;
    *signature_status = g_daemon_security.signature_enabled ? 1 : 0;
    *vault_status = g_daemon_security.vault_enabled ? 1 : 0;
    *audit_status = g_daemon_security.audit_enabled ? 1 : 0;

    return 0;
}

/* ==================== Cupolas 不可用时的存根实现 ==================== */

#else /* CUPOLAS_AVAILABLE == 0 */

/**
 * @brief 存根模式: 安全初始化（无 cupolas 时返回成功，功能降级）
 */
int daemon_security_init(const daemon_security_config_t* config, agentos_error_t* error) {
    (void)config;
    (void)error;

    SVC_LOG_WARN("Daemon security: running in STUB mode (cupolas not available)");
    SVC_LOG_WARN("Security features (sanitization/permission/signature/vault) are DISABLED");
    return 0;
}

/**
 * @brief 存根模式: 安全关闭
 */
void daemon_security_shutdown(void) {
    SVC_LOG_INFO("Daemon security stub shutdown");
}

/**
 * @brief 存根模式: LLM 输入净化（仅做基本长度检查）
 */
int daemon_sanitize_llm_input(const char* input, char* output, size_t output_size) {
    if (!input || !output || output_size == 0) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    /* 基本复制: 无 cupolas 时仅做截断保护 */
    size_t len = strlen(input);
    if (len >= output_size) {
        len = output_size - 1;
        SVC_LOG_WARN("LLM input truncated in stub mode (no sanitizer available)");
    }
    memcpy(output, input, len);
    output[len] = '\0';
    return 0;
}

/**
 * @brief 存根模式: 工具参数净化
 */
int daemon_sanitize_tool_params(const char* tool_name, const char* params,
                                  char* sanitized_tool, size_t tool_buf_size,
                                  char* sanitized_params, size_t param_buf_size) {
    if (!tool_name || !params || !sanitized_tool || !sanitized_params) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    /* 基本截断复制 */
    size_t tlen = strlen(tool_name);
    size_t plen = strlen(params);

    if (tlen >= tool_buf_size) tlen = tool_buf_size - 1;
    if (plen >= param_buf_size) plen = param_buf_size - 1;

    memcpy(sanitized_tool, tool_name, tlen);
    sanitized_tool[tlen] = '\0';

    memcpy(sanitized_params, params, plen);
    sanitized_params[plen] = '\0';

    SVC_LOG_WARN("Tool params sanitized in STUB mode (no cupolas sanitizer)");
    return 0;
}

/**
 * @brief 存根模式: 工具权限检查（默认允许，记录警告）
 */
int daemon_check_tool_permission(const char* agent_id, const char* tool_name,
                                 const char* action) {
    (void)agent_id;
    (void)tool_name;
    (void)action;

    SVC_LOG_WARN("[STUB] Tool permission check BYPASSED: tool=%s (no cupolas)", 
                tool_name ? tool_name : "unknown");
    return 1;  /* 默认允许 */
}

/**
 * @brief 存根模式: LLM 权限检查
 */
int daemon_check_llm_permission(const char* agent_id, const char* model_name,
                                 const char* action) {
    (void)agent_id;
    (void)model_name;
    (void)action;

    SVC_LOG_WARN("[STUB] LLM permission check BYPASSED: model=%s (no cupolas)",
                model_name ? model_name : "unknown");
    return 1;  /* 默认允许 */
}

/**
 * @brief 存根模式: 包签名验证
 */
int daemon_verify_package_signature(const char* package_path, bool* is_valid,
                                     cupolas_signer_info_t* signer_info) {
    (void)signer_info;

    if (!package_path || !is_valid) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

#ifdef DEBUG
    *is_valid = true;  /* 调试模式允许未签名包 */
#else
    *is_valid = false; /* 生产环境拒绝未签名包 */
#endif

    SVC_LOG_WARN("[STUB] Package signature verification SKIPPED: %s (no cupolas)", package_path);
    return 0;
}

/**
 * @brief 存根模式: 凭据存储
 */
int daemon_store_credential(const char* cred_id, cupolas_vault_cred_type_t cred_type,
                           const uint8_t* data, size_t data_len,
                           const char* agent_id) {
    (void)cred_id;
    (void)cred_type;
    (void)data;
    (void)data_len;
    (void)agent_id;

    SVC_LOG_ERROR("[STUB] Credential storage FAILED: no vault available (id=%s)", 
                 cred_id ? cred_id : "unknown");
    return AGENTOS_ERR_NOT_SUPPORTED;
}

/**
 * @brief 存根模式: 凭据检索
 */
int daemon_retrieve_credential(const char* cred_id, const char* agent_id,
                                uint8_t* data, size_t* data_len) {
    (void)cred_id;
    (void)agent_id;
    (void)data;
    (void)data_len;

    SVC_LOG_ERROR("[STUB] Credential retrieval FAILED: no vault available (id=%s)",
                 cred_id ? cred_id : "unknown");
    return AGENTOS_ERR_NOT_SUPPORTED;
}

/**
 * @brief 存根模式: 审计日志
 */
int daemon_audit_log_event(const char* service_name, const char* operation,
                             const char* resource, int result,
                             const char* agent_id) {
    if (!service_name || !operation) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    /* 降级到标准日志输出 */
    if (result == 0) {
        SVC_LOG_INFO("[AUDIT-STUB] [%s] %s on %s by %s - SUCCESS",
                    service_name, operation,
                    resource ? resource : "N/A",
                    agent_id ? agent_id : "system");
    } else {
        SVC_LOG_WARN("[AUDIT-STUB] [%s] %s on %s by %s - FAILED (code=%d)",
                     service_name, operation,
                     resource ? resource : "N/A",
                     agent_id ? agent_id : "system", result);
    }
    return 0;
}

/**
 * @brief 存根模式: 状态查询
 */
int daemon_security_get_status(int* sanitizer_status, int* permission_status,
                               int* signature_status, int* vault_status,
                               int* audit_status) {
    if (!sanitizer_status || !permission_status || !signature_status ||
        !vault_status || !audit_status) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    *sanitizer_status = 0;   /* 存根模式下不可用 */
    *permission_status = 0;
    *signature_status = 0;
    *vault_status = 0;
    *audit_status = 1;       /* 审计降级到日志，始终可用 */

    return 0;
}

#endif /* CUPOLAS_AVAILABLE */

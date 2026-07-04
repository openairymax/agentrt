#include "memory_compat.h"
#include "error.h"
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * daemon_security.c - Daemon Layer Security Integration Implementation
 */

/**
 * @file daemon_security.c
 * @brief Daemon Layer Security Integration - Unified Security for All Daemon Services
 * @author SPHARX Ltd. - Airymax Team
 * @date 2026-04-02
 *
 * This module provides unified security integration for all AgentRT daemon services:
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


#ifndef SVC_LOG_SECURITY
#define SVC_LOG_SECURITY(...) LOG_WARN(__VA_ARGS__)
#endif

#include "platform.h"
#include "svc_logger.h"

/* pthread.h provided by platform.h — no direct pthread include (CROSS-01) */

/* Internal state structure */
static struct {
    bool initialized;
    sanitize_level_t current_sanitize_level;
    bool permission_enabled;
    bool signature_enabled;
    bool vault_enabled;
    bool audit_enabled;
} g_daemon_security
    __attribute__((unused)) = {false, SANITIZE_LEVEL_NORMAL, false, false, false, false};

/* ---------- Initialization and Shutdown ---------- */


/* ==================== 生产级安全实现（独立实现，不依赖 cupolas） ==================== */
/* 所有函数均为真实实现，无桩函数；OpenSSL ED25519 验签 + 凭据保险库 + 审计日志 + ACL */

#include "agentos_dirent.h"
#include "cupolas_signer_info.h"
#include "cupolas_vault_cred_type.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifdef AGENTOS_HAS_OPENSSL
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#endif

#define MAX_CREDENTIALS 64
#define MAX_ACL_ENTRIES 128
#define MAX_AUDIT_LOG_SIZE 1024

typedef struct {
    char *cred_id;
    cupolas_vault_cred_type_t type;
    uint8_t *data;
    size_t data_len;
    char *owner_agent_id;
} credential_entry_t;

typedef struct {
    char agent_id[64];
    char resource[128];
    uint32_t operations;
    bool allowed;
} acl_entry_t;

static struct {
    bool initialized;
    sanitize_level_t current_sanitize_level;
    bool permission_enabled;
    bool signature_enabled;
    bool vault_enabled;
    bool audit_enabled;
    credential_entry_t credentials[MAX_CREDENTIALS];
    size_t cred_count;
    acl_entry_t acl_table[MAX_ACL_ENTRIES];
    size_t acl_count;
    FILE *audit_fp;
    char audit_log_path[256];
} g_security_ctx = {0};

static agentos_mutex_t g_security_mutex; /* CROSS-01: initialized via agentos_mutex_init() */
static bool g_security_mutex_initialized = false;

static void ensure_mutex_initialized(void)
{
    if (!g_security_mutex_initialized) {
        agentos_mutex_init(&g_security_mutex);
        g_security_mutex_initialized = true;
    }
}

static const char *DANGEROUS_PATTERNS[] = {";",  "|", "`",  "$(", "${", "&&", "||", ">",
                                           ">>", "<", "<<", "\\", "\n", "\r", NULL};

static bool contains_dangerous_pattern(const char *input)
{
    if (!input)
        return false;
    for (size_t i = 0; DANGEROUS_PATTERNS[i] != NULL; i++) {
        if (strstr(input, DANGEROUS_PATTERNS[i]) != NULL) {
            return true;
        }
    }
    return false;
}

static void sanitize_string(char *output, const char *input, size_t max_len)
{
    if (!output || !input || max_len == 0)
        return;

    size_t j = 0;
    for (size_t i = 0; input[i] && j < max_len - 1; i++) {
        unsigned char c = (unsigned char)input[i];
        if (isprint(c) && c != '\\' && c != '\n' && c != '\r' && c != '\t') {
            output[j++] = (char)c;
        }
    }
    output[j] = '\0';
}

int daemon_security_init(const daemon_security_config_t *config, agentos_error_t *error)
{
    ensure_mutex_initialized();
    agentos_mutex_lock(&g_security_mutex);
    if (g_security_ctx.initialized) {
        agentos_mutex_unlock(&g_security_mutex);
        SVC_LOG_INFO("Daemon security: already initialized");
        return 0;
    }

    __builtin_memset(&g_security_ctx, 0, sizeof(g_security_ctx));
    g_security_ctx.current_sanitize_level = SANITIZE_LEVEL_STRICT;
    g_security_ctx.permission_enabled = true;
    g_security_ctx.signature_enabled = true;
    g_security_ctx.vault_enabled = true;
    g_security_ctx.audit_enabled = true;

    if (config) {
        g_security_ctx.current_sanitize_level = config->sanitize_level;
        g_security_ctx.permission_enabled = config->enable_permission_cache;
        g_security_ctx.signature_enabled = config->enable_signature_verification;
        g_security_ctx.vault_enabled = config->enable_vault;
        g_security_ctx.audit_enabled = config->enable_audit_logging;

        if (config->audit_log_dir && strlen(config->audit_log_dir) > 0) {
            snprintf(g_security_ctx.audit_log_path, sizeof(g_security_ctx.audit_log_path),
                     "%s/daemon_audit.log", config->audit_log_dir);
        }
    }

    if (g_security_ctx.audit_enabled && g_security_ctx.audit_log_path[0] == '\0') {
        snprintf(g_security_ctx.audit_log_path, sizeof(g_security_ctx.audit_log_path),
                 AGENTOS_LOG_DIR "/daemon_audit.log");
    }

    if (g_security_ctx.audit_enabled) {
        g_security_ctx.audit_fp = fopen(g_security_ctx.audit_log_path, "a");
        if (!g_security_ctx.audit_fp) {
            SVC_LOG_WARN("Cannot open audit log: %s, falling back to syslog",
                         g_security_ctx.audit_log_path);
        }
    }

    g_security_ctx.initialized = true;
    agentos_mutex_unlock(&g_security_mutex);
    SVC_LOG_INFO("Daemon security: initialized in production mode (sanitize_level=%d)",
                 g_security_ctx.current_sanitize_level);
    return 0;
}

void daemon_security_shutdown(void)
{
    ensure_mutex_initialized();
    agentos_mutex_lock(&g_security_mutex);
    if (!g_security_ctx.initialized) {
        agentos_mutex_unlock(&g_security_mutex);
        return;
    }

    for (size_t i = 0; i < g_security_ctx.cred_count; i++) {
        AGENTOS_FREE(g_security_ctx.credentials[i].cred_id);
        g_security_ctx.credentials[i].cred_id = NULL;
        AGENTOS_FREE(g_security_ctx.credentials[i].data);
        g_security_ctx.credentials[i].data = NULL;
        AGENTOS_FREE(g_security_ctx.credentials[i].owner_agent_id);
        g_security_ctx.credentials[i].owner_agent_id = NULL;
    }
    g_security_ctx.cred_count = 0;

    for (size_t i = 0; i < g_security_ctx.acl_count; i++) {
        g_security_ctx.acl_table[i].agent_id[0] = '\0';
        g_security_ctx.acl_table[i].resource[0] = '\0';
    }
    g_security_ctx.acl_count = 0;

    if (g_security_ctx.audit_fp) {
        fclose(g_security_ctx.audit_fp);
        g_security_ctx.audit_fp = NULL;
    }

    g_security_ctx.initialized = false;
    g_security_ctx.permission_enabled = false;
    g_security_ctx.signature_enabled = false;
    g_security_ctx.vault_enabled = false;
    g_security_ctx.audit_enabled = false;
    agentos_mutex_unlock(&g_security_mutex);
    agentos_mutex_destroy(&g_security_mutex);
    g_security_mutex_initialized = false;
    SVC_LOG_INFO("Daemon security: shutdown complete");
}

int daemon_sanitize_llm_input(const char *input, char *output, size_t output_size)
{
    if (!input || !output || output_size == 0) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    ensure_mutex_initialized();
    agentos_mutex_lock(&g_security_mutex);
    /* P3.15 ACC-DT16: fail-safe — 不再 lazy-init。未初始化时使用最严格的
     * SANITIZE_LEVEL_STRICT 继续净化（净化是保护性操作，拒绝会降低安全性）。
     * 调用方应在 daemon 启动时通过 daemon_cupolas_init() 显式初始化。 */
    sanitize_level_t level;
    if (!g_security_ctx.initialized) {
        level = SANITIZE_LEVEL_STRICT;
        agentos_mutex_unlock(&g_security_mutex);
        SVC_LOG_WARN("daemon_sanitize_llm_input: daemon_security not initialized — "
                     "call daemon_cupolas_init() during startup. Using SANITIZE_LEVEL_STRICT (fail-safe).");
    } else {
        level = g_security_ctx.current_sanitize_level;
        agentos_mutex_unlock(&g_security_mutex);
    }

    if (contains_dangerous_pattern(input)) {
        SVC_LOG_SECURITY(
            "SEC-011 VIOLATION: LLM input contains shell injection pattern - REJECTED");
        snprintf(output, output_size, "[SANITIZED: input rejected - security violation]");
        return AGENTOS_ERR_PERMISSION_DENIED;
    }

    sanitize_string(output, input, output_size);

    if (level >= SANITIZE_LEVEL_STRICT) {
        for (size_t i = 0; output[i]; i++) {
            if ((unsigned char)output[i] > 127) {
                output[i] = '?';
            }
        }
    }

    return AGENTOS_OK;
}

int daemon_sanitize_tool_params(const char *tool_name, const char *params, char *sanitized_tool,
                                size_t tool_buf_size, char *sanitized_params, size_t param_buf_size)
{
    if (!tool_name || !params || !sanitized_tool || !sanitized_params) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    ensure_mutex_initialized();
    agentos_mutex_lock(&g_security_mutex);
    /* P3.15 ACC-DT16: fail-safe — 不再 lazy-init。未初始化时仍执行净化
     *（净化是保护性操作），仅记录警告。 */
    bool sec_uninitialized = !g_security_ctx.initialized;
    agentos_mutex_unlock(&g_security_mutex);
    if (sec_uninitialized) {
        SVC_LOG_WARN("daemon_sanitize_tool_params: daemon_security not initialized — "
                     "call daemon_cupolas_init() during startup. Proceeding with default sanitize (fail-safe).");
    }

    sanitize_string(sanitized_tool, tool_name, tool_buf_size);

    if (contains_dangerous_pattern(params)) {
        SVC_LOG_SECURITY("SEC-014 VIOLATION: Tool params contain dangerous pattern - REJECTED");
        snprintf(sanitized_params, param_buf_size, "[SANITIZED: params rejected]");
        return AGENTOS_ERR_PERMISSION_DENIED;
    }

    sanitize_string(sanitized_params, params, param_buf_size);

    if (strlen(params) > param_buf_size - 1) {
        SVC_LOG_WARN("Tool params truncated: %zu -> %zu bytes", strlen(params), param_buf_size - 1);
    }

    return AGENTOS_OK;
}

int daemon_check_tool_permission(const char *agent_id, const char *tool_name, const char *action)
{
    if (!agent_id || !tool_name || !action) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    ensure_mutex_initialized();
    agentos_mutex_lock(&g_security_mutex);
    /* P3.15 ACC-DT16: fail-closed — 未初始化时拒绝所有工具执行。 */
    if (!g_security_ctx.initialized) {
        agentos_mutex_unlock(&g_security_mutex);
        SVC_LOG_ERROR("daemon_check_tool_permission: daemon_security not initialized — "
                      "call daemon_cupolas_init() during startup. DENYING %s/%s (fail-closed).",
                      agent_id, tool_name);
        return AGENTOS_EPERM;
    }

    if (!g_security_ctx.permission_enabled) {
        agentos_mutex_unlock(&g_security_mutex);
        SVC_LOG_WARN("Permission check: disabled by configuration, DENYING %s/%s (fail-closed)",
                     agent_id, tool_name);
        return AGENTOS_EPERM;
    }

    int result = AGENTOS_ERR_PERMISSION_DENIED;
    for (size_t i = 0; i < g_security_ctx.acl_count; i++) {
        if (strcmp(g_security_ctx.acl_table[i].agent_id, agent_id) == 0 &&
            strcmp(g_security_ctx.acl_table[i].resource, tool_name) == 0) {

            if (g_security_ctx.acl_table[i].allowed) {
                result = AGENTOS_OK;
            }
            break;
        }
    }
    agentos_mutex_unlock(&g_security_mutex);

    if (result == AGENTOS_OK) {
        SVC_LOG_DEBUG("Permission GRANTED: agent=%s tool=%s action=%s", agent_id, tool_name,
                      action);
    } else {
        SVC_LOG_SECURITY("Permission DENIED (no ACL entry): agent=%s tool=%s action=%s", agent_id,
                         tool_name, action);
    }
    return result;
}

int daemon_check_llm_permission(const char *agent_id, const char *model_name, const char *action)
{
    if (!agent_id || !model_name || !action) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    ensure_mutex_initialized();
    agentos_mutex_lock(&g_security_mutex);
    /* P3.15 ACC-DT16: fail-closed — 未初始化时拒绝所有 LLM 调用。 */
    if (!g_security_ctx.initialized) {
        agentos_mutex_unlock(&g_security_mutex);
        SVC_LOG_ERROR("daemon_check_llm_permission: daemon_security not initialized — "
                      "call daemon_cupolas_init() during startup. DENYING %s/%s (fail-closed).",
                      agent_id, model_name);
        return AGENTOS_EPERM;
    }

    if (!g_security_ctx.permission_enabled) {
        agentos_mutex_unlock(&g_security_mutex);
        SVC_LOG_WARN("LLM permission check: disabled by configuration, DENYING %s/%s (fail-closed)",
                     agent_id, model_name);
        return AGENTOS_EPERM;
    }

    char resource[256];
    snprintf(resource, sizeof(resource), "llm:%s", model_name);

    int result = AGENTOS_ERR_PERMISSION_DENIED;
    for (size_t i = 0; i < g_security_ctx.acl_count; i++) {
        if (strcmp(g_security_ctx.acl_table[i].agent_id, agent_id) == 0 &&
            strstr(g_security_ctx.acl_table[i].resource, resource) != NULL) {

            if (g_security_ctx.acl_table[i].allowed) {
                result = AGENTOS_OK;
            }
            break;
        }
    }
    agentos_mutex_unlock(&g_security_mutex);

    if (result == AGENTOS_OK) {
        SVC_LOG_DEBUG("LLM Permission GRANTED: agent=%s model=%s action=%s", agent_id, model_name,
                      action);
    } else {
        SVC_LOG_SECURITY("LLM Permission DENIED (no ACL): agent=%s model=%s action=%s", agent_id,
                         model_name, action);
    }
    return result;
}

int daemon_verify_package_signature(const char *package_path, bool *is_valid,
                                    cupolas_signer_info_t *signer_info)
{
    if (!package_path || !is_valid) {
        return AGENTOS_ERR_INVALID_PARAM;
    }
    if (signer_info)
        __builtin_memset(signer_info, 0, sizeof(cupolas_signer_info_t));

    *is_valid = false;

    ensure_mutex_initialized();
    agentos_mutex_lock(&g_security_mutex);
    /* P3.15 ACC-DT16: fail-closed — 未初始化时拒绝签名验证（标记为未验证）。 */
    if (!g_security_ctx.initialized) {
        agentos_mutex_unlock(&g_security_mutex);
        SVC_LOG_ERROR("daemon_verify_package_signature: daemon_security not initialized — "
                      "call daemon_cupolas_init() during startup. Marking package UNVERIFIED (fail-closed).");
        *is_valid = false;
        return AGENTOS_ERR_STATE_ERROR;
    }

    if (!g_security_ctx.signature_enabled) {
        agentos_mutex_unlock(&g_security_mutex);
        SVC_LOG_WARN("Signature verification: disabled by configuration, package NOT verified");
        *is_valid = false;
        return AGENTOS_OK;
    }
    agentos_mutex_unlock(&g_security_mutex);

    struct stat st;
    if (stat(package_path, &st) != 0) {
        SVC_LOG_ERROR("Package not found: %s", package_path);
        return AGENTOS_ERR_NOT_FOUND;
    }

    if (st.st_size == 0) {
        SVC_LOG_ERROR("Package is empty: %s", package_path);
        return AGENTOS_ERR_INVALID_PARAM;
    }

    if (st.st_size > 512 * 1024 * 1024) {
        SVC_LOG_ERROR("Package exceeds size limit: %s (%lld bytes)", package_path,
                      (long long)st.st_size);
        *is_valid = false;
        return AGENTOS_OK;
    }

    char sig_path[1024];
    snprintf(sig_path, sizeof(sig_path), "%s.sig", package_path);
    struct stat sig_st;
    if (stat(sig_path, &sig_st) != 0) {
        SVC_LOG_WARN("No signature file found for %s (expected %s), marking as unverified",
                     package_path, sig_path);
        *is_valid = false;
        return AGENTOS_OK;
    }

    FILE *sig_fp = fopen(sig_path, "rb");
    if (!sig_fp) {
        SVC_LOG_ERROR("Cannot open signature file: %s", sig_path);
        *is_valid = false;
        return AGENTOS_OK;
    }

    uint8_t signature[256] = {0};
    size_t sig_len = fread(signature, 1, sizeof(signature), sig_fp);
    fclose(sig_fp);

    if (sig_len < 64 || sig_len > 256) {
        SVC_LOG_ERROR("Invalid signature length: %zu bytes (expected 64 for ED25519)", sig_len);
        *is_valid = false;
        return AGENTOS_OK;
    }

#ifdef AGENTOS_HAS_OPENSSL
    const char *trusted_keys_dir = getenv("AGENTOS_TRUSTED_KEYS_DIR");
    if (!trusted_keys_dir) {
        trusted_keys_dir = AGENTOS_CONFIG_DIR "/trusted_keys";
    }

    DIR *dir = opendir(trusted_keys_dir);
    if (!dir) {
        SVC_LOG_WARN("Trusted keys directory not found: %s, cannot verify signature",
                     trusted_keys_dir);
        *is_valid = false;
        return AGENTOS_OK;
    }

    FILE *pkg_fp = fopen(package_path, "rb");
    if (!pkg_fp) {
        closedir(dir);
        SVC_LOG_ERROR("Cannot open package file: %s", package_path);
        *is_valid = false;
        return AGENTOS_OK;
    }

    uint8_t *pkg_data = (uint8_t *)AGENTOS_MALLOC((size_t)st.st_size);
    if (!pkg_data) {
        fclose(pkg_fp);
        closedir(dir);
        AGENTOS_ERROR(AGENTOS_ERR_OUT_OF_MEMORY, "failed to allocate package data buffer");
    }
    size_t pkg_read = fread(pkg_data, 1, (size_t)st.st_size, pkg_fp);
    fclose(pkg_fp);

    if (pkg_read != (size_t)st.st_size) {
        AGENTOS_FREE(pkg_data);
        closedir(dir);
        SVC_LOG_ERROR("Failed to read entire package: %s", package_path);
        *is_valid = false;
        return AGENTOS_OK;
    }

    bool verified = false;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.')
            continue;

        size_t name_len = strlen(entry->d_name);
        if (name_len < 5 || strcmp(entry->d_name + name_len - 4, ".pem") != 0)
            continue;

        char key_path[1024];
        snprintf(key_path, sizeof(key_path), "%s/%s", trusted_keys_dir, entry->d_name);

        FILE *key_fp = fopen(key_path, "r");
        if (!key_fp)
            continue;

        EVP_PKEY *pkey = PEM_read_PUBKEY(key_fp, NULL, NULL, NULL);
        fclose(key_fp);

        if (!pkey)
            continue;

        if (EVP_PKEY_base_id(pkey) != EVP_PKEY_ED25519) {
            EVP_PKEY_free(pkey);
            continue;
        }

        EVP_MD_CTX *md_ctx = EVP_MD_CTX_new();
        if (!md_ctx) {
            EVP_PKEY_free(pkey);
            continue;
        }

        if (EVP_DigestVerifyInit(md_ctx, NULL, NULL, NULL, pkey) != 1) {
            EVP_MD_CTX_free(md_ctx);
            EVP_PKEY_free(pkey);
            continue;
        }

        int verify_result = EVP_DigestVerify(md_ctx, signature, sig_len, pkg_data, pkg_read);

        EVP_MD_CTX_free(md_ctx);
        EVP_PKEY_free(pkey);

        if (verify_result == 1) {
            verified = true;
            if (signer_info) {
                char *dot = strchr(entry->d_name, '.');
                size_t id_len = dot ? (size_t)(dot - entry->d_name) : name_len;
                if (id_len >= sizeof(signer_info->key_id))
                    id_len = sizeof(signer_info->key_id) - 1;
                __builtin_memcpy(signer_info->key_id, entry->d_name, id_len);
                signer_info->key_id[id_len] = '\0';
                signer_info->algorithm = AGENTOS_STRDUP("ED25519");
            }
            SVC_LOG_INFO("Package signature VERIFIED (ED25519): %s with key %s", package_path,
                         entry->d_name);
            break;
        }
    }

    closedir(dir);
    AGENTOS_FREE(pkg_data);

    *is_valid = verified;
    if (!verified) {
        SVC_LOG_SECURITY("Package signature INVALID: %s (no trusted key matched)", package_path);
    }
    return AGENTOS_OK;

#else
    SVC_LOG_WARN("OpenSSL not available: cannot perform ED25519 verification for %s, "
                 "marking as unverified (size=%lld bytes)",
                 package_path, (long long)st.st_size);
    *is_valid = false;
    return AGENTOS_ENOTSUP;
#endif
}

int daemon_store_credential(const char *cred_id, cupolas_vault_cred_type_t cred_type,
                            const uint8_t *data, size_t data_len, const char *agent_id)
{
    if (!cred_id || !data || data_len == 0) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    ensure_mutex_initialized();
    agentos_mutex_lock(&g_security_mutex);
    /* P3.15 ACC-DT16: fail-closed — 未初始化时拒绝凭据存储。 */
    if (!g_security_ctx.initialized) {
        agentos_mutex_unlock(&g_security_mutex);
        SVC_LOG_ERROR("daemon_store_credential: daemon_security not initialized — "
                      "call daemon_cupolas_init() during startup. DENYING credential storage (fail-closed).");
        return AGENTOS_ERR_STATE_ERROR;
    }

    if (!g_security_ctx.vault_enabled) {
        agentos_mutex_unlock(&g_security_mutex);
        AGENTOS_ERROR(AGENTOS_ERR_NOT_SUPPORTED, "vault is disabled");
    }

    if (g_security_ctx.cred_count >= MAX_CREDENTIALS) {
        agentos_mutex_unlock(&g_security_mutex);
        SVC_LOG_ERROR("Credential storage full (max=%d)", MAX_CREDENTIALS);
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    for (size_t i = 0; i < g_security_ctx.cred_count; i++) {
        if (strcmp(g_security_ctx.credentials[i].cred_id, cred_id) == 0) {
            AGENTOS_FREE(g_security_ctx.credentials[i].data);
            g_security_ctx.credentials[i].data = (uint8_t *)AGENTOS_MALLOC(data_len);
            if (!g_security_ctx.credentials[i].data) {
                agentos_mutex_unlock(&g_security_mutex);
                AGENTOS_ERROR(AGENTOS_ERR_OUT_OF_MEMORY,
                              "failed to allocate credential data buffer");
            }
            __builtin_memcpy(g_security_ctx.credentials[i].data, data, data_len);
            g_security_ctx.credentials[i].data_len = data_len;
            agentos_mutex_unlock(&g_security_mutex);
            SVC_LOG_INFO("Credential updated: %s (type=%d, %zu bytes)", cred_id, cred_type,
                         data_len);
            return AGENTOS_OK;
        }
    }

    credential_entry_t *entry = &g_security_ctx.credentials[g_security_ctx.cred_count++];
    entry->cred_id = AGENTOS_STRDUP(cred_id);
    entry->type = cred_type;
    entry->data = (uint8_t *)AGENTOS_MALLOC(data_len);
    if (!entry->data) {
        agentos_mutex_unlock(&g_security_mutex);
        AGENTOS_ERROR(AGENTOS_ERR_OUT_OF_MEMORY, "failed to allocate credential data buffer");
    }
    __builtin_memcpy(entry->data, data, data_len);
    entry->data_len = data_len;
    entry->owner_agent_id = agent_id ? AGENTOS_STRDUP(agent_id) : AGENTOS_STRDUP("system");

    agentos_mutex_unlock(&g_security_mutex);
    SVC_LOG_INFO("Credential stored: %s (type=%d, %zu bytes, total=%zu)", cred_id, cred_type,
                 data_len, g_security_ctx.cred_count);
    return AGENTOS_OK;
}

int daemon_retrieve_credential(const char *cred_id, const char *agent_id, uint8_t *data,
                               size_t *data_len)
{
    if (!cred_id || !data || !data_len) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    ensure_mutex_initialized();
    agentos_mutex_lock(&g_security_mutex);
    /* P3.15 ACC-DT16: fail-closed — 未初始化时拒绝凭据检索。 */
    if (!g_security_ctx.initialized) {
        agentos_mutex_unlock(&g_security_mutex);
        SVC_LOG_ERROR("daemon_retrieve_credential: daemon_security not initialized — "
                      "call daemon_cupolas_init() during startup. DENYING credential retrieval (fail-closed).");
        return AGENTOS_ERR_STATE_ERROR;
    }

    if (!g_security_ctx.vault_enabled) {
        agentos_mutex_unlock(&g_security_mutex);
        AGENTOS_ERROR(AGENTOS_ERR_NOT_SUPPORTED, "vault is disabled");
    }

    for (size_t i = 0; i < g_security_ctx.cred_count; i++) {
        if (g_security_ctx.credentials[i].cred_id &&
            strcmp(g_security_ctx.credentials[i].cred_id, cred_id) == 0) {
            if (agent_id && g_security_ctx.credentials[i].owner_agent_id &&
                strcmp(g_security_ctx.credentials[i].owner_agent_id, agent_id) != 0 &&
                strcmp(agent_id, "system") != 0) {
                agentos_mutex_unlock(&g_security_mutex);
                SVC_LOG_SECURITY("Credential access DENIED: %s (agent=%s not owner=%s)", cred_id,
                                 agent_id, g_security_ctx.credentials[i].owner_agent_id);
                return AGENTOS_ERR_PERMISSION_DENIED;
            }

            size_t copy_len = g_security_ctx.credentials[i].data_len;
            if (copy_len > *data_len)
                copy_len = *data_len;
            if (g_security_ctx.credentials[i].data) {
                __builtin_memcpy(data, g_security_ctx.credentials[i].data, copy_len);
            }
            *data_len = copy_len;
            agentos_mutex_unlock(&g_security_mutex);
            return AGENTOS_OK;
        }
    }

    agentos_mutex_unlock(&g_security_mutex);
    SVC_LOG_WARN("Credential not found: %s", cred_id);
    return AGENTOS_ERR_NOT_FOUND;
}

int daemon_audit_log_event(const char *service_name, const char *operation, const char *resource,
                           int result, const char *agent_id)
{
    if (!service_name || !operation) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    /* P3.15 ACC-DT16: fail-closed — 未初始化时拒绝审计日志写入。 */
    if (!g_security_ctx.initialized) {
        SVC_LOG_ERROR("daemon_audit_log_event: daemon_security not initialized — "
                      "call daemon_cupolas_init() during startup. Audit event DROPPED (fail-closed).");
        return AGENTOS_ERR_STATE_ERROR;
    }

    ensure_mutex_initialized();
    agentos_mutex_lock(&g_security_mutex);
    if (!g_security_ctx.audit_enabled) {
        agentos_mutex_unlock(&g_security_mutex);
        return 0;
    }

    time_t now = time(NULL);
    struct tm tm_info;
    localtime_r(&now, &tm_info);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S%z", &tm_info);

    const char *result_str = (result == 0) ? "SUCCESS" : "FAILED";
    char log_msg[MAX_AUDIT_LOG_SIZE];

    snprintf(log_msg, sizeof(log_msg),
             "[%s] [%s] service=%s operation=%s resource=%s agent=%s result=%s\n", timestamp,
             result_str, service_name, operation, resource ? resource : "N/A",
             agent_id ? agent_id : "system", result_str);

    if (g_security_ctx.audit_fp) {
        fwrite(log_msg, 1, strlen(log_msg), g_security_ctx.audit_fp);
        fflush(g_security_ctx.audit_fp);
    } else {
        if (result == 0) {
            SVC_LOG_INFO("[AUDIT] %s", log_msg);
        } else {
            SVC_LOG_WARN("[AUDIT] %s", log_msg);
        }
    }
    agentos_mutex_unlock(&g_security_mutex);

    return AGENTOS_OK;
}

int daemon_security_get_status(int *sanitizer_status, int *permission_status, int *signature_status,
                               int *vault_status, int *audit_status)
{
    if (!sanitizer_status || !permission_status || !signature_status || !vault_status ||
        !audit_status) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    ensure_mutex_initialized();
    agentos_mutex_lock(&g_security_mutex);
    *sanitizer_status = g_security_ctx.initialized ? 1 : 0;
    *permission_status = g_security_ctx.permission_enabled ? 1 : 0;
    *signature_status = g_security_ctx.signature_enabled ? 1 : 0;
    *vault_status = g_security_ctx.vault_enabled ? 1 : 0;
    *audit_status = g_security_ctx.audit_enabled ? 1 : 0;
    agentos_mutex_unlock(&g_security_mutex);

    return AGENTOS_OK;
}

int daemon_security_add_acl_rule(const char *agent_id, const char *resource, bool allowed)
{
    if (!agent_id || !resource || agent_id[0] == '\0' || resource[0] == '\0') {
        return AGENTOS_ERR_INVALID_PARAM;
    }
    if (strlen(agent_id) >= sizeof(g_security_ctx.acl_table[0].agent_id) ||
        strlen(resource) >= sizeof(g_security_ctx.acl_table[0].resource)) {
        SVC_LOG_ERROR("ACL rule rejected: agent_id or resource too long (agent=%s resource=%s)",
                      agent_id, resource);
        return AGENTOS_ERR_INVALID_PARAM;
    }

    ensure_mutex_initialized();
    agentos_mutex_lock(&g_security_mutex);
    /* P3.15 ACC-DT16: fail-closed — 未初始化时拒绝 ACL 规则添加。 */
    if (!g_security_ctx.initialized) {
        agentos_mutex_unlock(&g_security_mutex);
        SVC_LOG_ERROR("daemon_security_add_acl_rule: daemon_security not initialized — "
                      "call daemon_cupolas_init() during startup. DENYING ACL rule add (fail-closed).");
        return AGENTOS_ERR_STATE_ERROR;
    }

    /* 查找已有的同 (agent_id, resource) 条目，覆盖其 allowed 状态 */
    for (size_t i = 0; i < g_security_ctx.acl_count; i++) {
        if (strcmp(g_security_ctx.acl_table[i].agent_id, agent_id) == 0 &&
            strcmp(g_security_ctx.acl_table[i].resource, resource) == 0) {
            g_security_ctx.acl_table[i].allowed = allowed;
            agentos_mutex_unlock(&g_security_mutex);
            SVC_LOG_DEBUG("ACL rule updated: agent=%s resource=%s allowed=%d",
                          agent_id, resource, allowed ? 1 : 0);
            return AGENTOS_OK;
        }
    }

    if (g_security_ctx.acl_count >= MAX_ACL_ENTRIES) {
        agentos_mutex_unlock(&g_security_mutex);
        SVC_LOG_ERROR("ACL table full (max=%d), cannot add rule for agent=%s resource=%s",
                      MAX_ACL_ENTRIES, agent_id, resource);
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    acl_entry_t *entry = &g_security_ctx.acl_table[g_security_ctx.acl_count];
    snprintf(entry->agent_id, sizeof(entry->agent_id), "%s", agent_id);
    snprintf(entry->resource, sizeof(entry->resource), "%s", resource);
    entry->operations = 0xFFFFFFFF; /* 所有操作（execute/read/write） */
    entry->allowed = allowed;
    g_security_ctx.acl_count++;
    agentos_mutex_unlock(&g_security_mutex);

    SVC_LOG_INFO("ACL rule added: agent=%s resource=%s allowed=%d (count=%zu/%d)",
                 agent_id, resource, allowed ? 1 : 0, g_security_ctx.acl_count, MAX_ACL_ENTRIES);
    return AGENTOS_OK;
}


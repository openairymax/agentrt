/**
 * @file daemon_cupolas_bootstrap.c
 * @brief P3.14 (ACC-DT15): daemon 统一 cupolas 安全穹顶引导实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "daemon_cupolas_bootstrap.h"

#include "cupolas.h"
#include "daemon_security.h"
#include "svc_logger.h"

/* ==================== 内部状态 ==================== */

static int g_cupolas_initialized = 0;

/* ==================== 实现 ==================== */

agentrt_error_t daemon_cupolas_init(const char *daemon_name)
{
    if (!daemon_name)
    {
        SVC_LOG_ERROR("daemon_cupolas_init: NULL daemon_name");
        return AGENTRT_EINVAL;
    }

    /* 幂等：已初始化则直接返回成功 */
    if (g_cupolas_initialized)
    {
        SVC_LOG_DEBUG("daemon_cupolas_init: cupolas already initialized (daemon=%s)", daemon_name);
        return AGENTRT_SUCCESS;
    }

    /* P3.15 ACC-DT16: 显式初始化 daemon_security 层（非 NULL config）。
     * 历史代码在各 security 函数内部 lazy-init（daemon_security_init(NULL,NULL)），
     * 掩盖了 daemon 未显式初始化安全层的问题。改为在 cupolas bootstrap 时统一显式初始化，
     * security 函数内部改为 fail-closed（未初始化 = 拒绝）。*/
    daemon_security_config_t sec_config;
    __builtin_memset(&sec_config, 0, sizeof(sec_config));
    sec_config.sanitize_level             = SANITIZE_LEVEL_STRICT;
    sec_config.sanitizer_rules_path       = NULL;
    sec_config.permission_rules_path      = NULL;
    sec_config.enable_permission_cache    = true;
    sec_config.enable_signature_verification = false; /* 0.1.1 无签名基础设施，1.0.1 启用 */
    sec_config.trusted_ca_path            = NULL;
    sec_config.expected_signer            = NULL;
    sec_config.enable_vault               = true;
    sec_config.vault_storage_path         = NULL;
    sec_config.enable_audit_logging       = true;
    sec_config.audit_log_dir              = NULL; /* 使用默认 AGENTRT_LOG_DIR */

    agentrt_error_t sec_err = AGENTRT_OK;
    int sec_rc = daemon_security_init(&sec_config, &sec_err);
    if (sec_rc != 0)
    {
        SVC_LOG_ERROR("daemon_cupolas_init: daemon_security_init FAILED for daemon='%s' "
                      "(rc=%d, err=%d) — security layer unavailable, "
                      "service-layer fail-closed will deny all privileged operations",
                      daemon_name, sec_rc, (int) sec_err);
        /* 非致命：继续初始化 cupolas，由各 service 层 fail-closed 拦截 */
    }

    agentrt_error_t cupolas_err = AGENTRT_OK;
    int             rc          = cupolas_init(NULL, &cupolas_err);
    if (rc != 0)
    {
        SVC_LOG_ERROR("daemon_cupolas_init: cupolas_init FAILED for daemon='%s' "
                      "(rc=%d, err=%d) — security dome unavailable, "
                      "service-layer fail-closed will deny all privileged operations",
                      daemon_name, rc, (int) cupolas_err);
        return cupolas_err;
    }

    g_cupolas_initialized = 1;
    SVC_LOG_INFO("daemon_cupolas_init: cupolas security dome initialized for '%s' "
                 "(permission_engine + sanitizer + audit_logger + daemon_security)",
                 daemon_name);
    return AGENTRT_SUCCESS;
}

void daemon_cupolas_cleanup(void)
{
    if (!g_cupolas_initialized)
        return;

    /* 刷新审计日志，确保所有审计记录落盘 */
    cupolas_flush_audit_log();
    cupolas_cleanup();
    daemon_security_shutdown();
    g_cupolas_initialized = 0;
    SVC_LOG_INFO("daemon_cupolas_cleanup: cupolas security dome shut down");
}

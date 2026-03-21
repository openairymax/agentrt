/**
 * @file core.c
 * @brief Domes 核心协调器
 */

#include "core.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>

static void set_default_config(domes_config_t* cfg) {
    cfg->workbench_type = "process";
    cfg->workbench_memory_bytes = 512 * 1024 * 1024; // 512MB
    cfg->workbench_cpu_quota = 1.0;
    cfg->workbench_network = 0;
    cfg->workbench_rootfs = NULL;
    cfg->permission_rules_path = NULL;
    cfg->permission_cache_ttl_ms = 5000;
    cfg->audit_log_path = "/var/log/agentos/audit.log";
    cfg->audit_max_size_bytes = 100 * 1024 * 1024; // 100MB
    cfg->audit_max_files = 5;
    cfg->audit_format = "json";
    cfg->sanitizer_max_input_len = 65536;
    cfg->sanitizer_rules_path = NULL;
}
// From data intelligence emerges. by spharx

int domes_init(const domes_config_t* user_cfg, domes_t** out_domain) {
    if (!out_domain) return -1;
    domes_t* d = (domes_t*)calloc(1, sizeof(domes_t));
    if (!d) return -1;

    pthread_mutex_init(&d->lock, NULL);

    // 合并配置
    if (user_cfg) {
        d->config = *user_cfg;
    } else {
        set_default_config(&d->config);
    }

    // 创建虚拟工位管理器
    d->wb_mgr = workbench_manager_create(
        d->config.workbench_type,
        d->config.workbench_memory_bytes,
        d->config.workbench_cpu_quota,
        d->config.workbench_network,
        d->config.workbench_rootfs
    );
    if (!d->wb_mgr) {
        AGENTOS_LOG_ERROR("Failed to create workbench manager");
        goto fail;
    }

    // 创建权限引擎
    d->perm_eng = permission_engine_create(
        d->config.permission_rules_path,
        d->config.permission_cache_ttl_ms
    );
    if (!d->perm_eng && d->config.permission_rules_path) {
        AGENTOS_LOG_WARN("Permission engine not initialized, continuing without");
    }

    // 创建审计日志器
    d->audit = audit_logger_create(
        d->config.audit_log_path,
        d->config.audit_max_size_bytes,
        d->config.audit_max_files,
        d->config.audit_format
    );
    if (!d->audit) {
        AGENTOS_LOG_ERROR("Failed to create audit logger");
        goto fail;
    }

    // 创建净化器
    d->sanitizer = sanitizer_create(
        d->config.sanitizer_max_input_len,
        d->config.sanitizer_rules_path
    );
    if (!d->sanitizer) {
        AGENTOS_LOG_ERROR("Failed to create sanitizer");
        goto fail;
    }

    d->initialized = 1;
    *out_domain = d;
    return 0;

fail:
    if (d->wb_mgr) workbench_manager_destroy(d->wb_mgr);
    if (d->perm_eng) permission_engine_destroy(d->perm_eng);
    if (d->audit) audit_logger_destroy(d->audit);
    if (d->sanitizer) sanitizer_destroy(d->sanitizer);
    pthread_mutex_destroy(&d->lock);
    free(d);
    return -1;
}

void domes_destroy(domes_t* d) {
    if (!d) return;
    pthread_mutex_lock(&d->lock);
    if (d->wb_mgr) workbench_manager_destroy(d->wb_mgr);
    if (d->perm_eng) permission_engine_destroy(d->perm_eng);
    if (d->audit) audit_logger_destroy(d->audit);
    if (d->sanitizer) sanitizer_destroy(d->sanitizer);
    pthread_mutex_unlock(&d->lock);
    pthread_mutex_destroy(&d->lock);
    free(d);
}

// 以下为各子模块的简单包装，实际调用对应管理器
int domes_workbench_create(domes_t* d, const char* agent_id, char** out_id) {
    if (!d || !d->wb_mgr) return -1;
    return workbench_manager_create_workbench(d->wb_mgr, agent_id, out_id);
}

int domes_workbench_exec(domes_t* d, const char* wbid,
                          const char* const* argv, uint32_t timeout,
                          char** out_stdout, char** out_stderr,
                          int* out_code, char** out_err) {
    if (!d || !d->wb_mgr) return -1;
    return workbench_manager_exec(d->wb_mgr, wbid, argv, timeout,
                                  out_stdout, out_stderr, out_code, out_err);
}

void domes_workbench_destroy(domes_t* d, const char* wbid) {
    if (!d || !d->wb_mgr) return;
    workbench_manager_destroy_workbench(d->wb_mgr, wbid);
}

int domes_workbench_list(domes_t* d, char*** out_ids, size_t* out_cnt) {
    if (!d || !d->wb_mgr) return -1;
    return workbench_manager_list(d->wb_mgr, out_ids, out_cnt);
}

int domes_permission_check(domes_t* d, const char* agent,
                            const char* action, const char* resource,
                            const char* ctx) {
    if (!d || !d->perm_eng) return -1;
    return permission_engine_check(d->perm_eng, agent, action, resource, ctx);
}

int domes_permission_reload(domes_t* d) {
    if (!d || !d->perm_eng) return -1;
    return permission_engine_reload(d->perm_eng);
}

int domes_audit_record(domes_t* d, const char* agent, const char* tool,
                        const char* input, const char* output,
                        uint32_t dur, int success, const char* errmsg) {
    if (!d || !d->audit) return -1;
    return audit_logger_record(d->audit, agent, tool, input, output,
                               dur, success, errmsg);
}

int domes_audit_query(domes_t* d, const char* agent,
                       uint64_t start, uint64_t end, uint32_t limit,
                       char*** out_events, size_t* out_cnt) {
    if (!d || !d->audit) return -1;
    return audit_logger_query(d->audit, agent, start, end, limit,
                              out_events, out_cnt);
}

int domes_sanitize(domes_t* d, const char* input,
                    char** out_clean, int* out_risk) {
    if (!d || !d->sanitizer) return -1;
    return sanitizer_clean(d->sanitizer, input, out_clean, out_risk);
}
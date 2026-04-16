/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * cupolas.c - AgentOS Security Dome Core Implementation (Facade)
 *
 * This module implements the unified public API for the security dome,
 * providing a facade over all four protection layers:
 * - Virtual Workbench (workbench/)
 * - Permission Engine (permission/)
 * - Input Sanitizer (sanitizer/)
 * - Audit Trail (audit/)
 */

#include "cupolas.h"
#include "cupolas_config.h"
#include "security/cupolas_error.h"
#include "permission/permission.h"
#include "sanitizer/sanitizer.h"
#include "workbench/workbench.h"
#include "audit/audit.h"
#include "utils/cupolas_utils.h"
#include "guards/guard_integration.h"
#include "platform/platform.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define CUPOLAS_DEFAULT_AUDIT_MAX_FILE_SIZE (10 * 1024 * 1024)
#define CUPOLAS_DEFAULT_AUDIT_MAX_FILES 5
#define CUPOLAS_CONFIG_PATH_MAX 512

typedef struct {
    char permission_rules_path[CUPOLAS_CONFIG_PATH_MAX];
    char audit_log_dir[CUPOLAS_CONFIG_PATH_MAX];
} cupolas_internal_config_t;

static void cupolas_internal_config_init_defaults(cupolas_internal_config_t* cfg) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
#ifdef _WIN32
    snprintf(cfg->permission_rules_path, sizeof(cfg->permission_rules_path),
             "C:\\ProgramData\\agentos\\cupolas\\permission_rules.yaml");
    snprintf(cfg->audit_log_dir, sizeof(cfg->audit_log_dir),
             "C:\\ProgramData\\agentos\\cupolas\\logs");
#else
    snprintf(cfg->permission_rules_path, sizeof(cfg->permission_rules_path),
             "/etc/agentos/cupolas/permission_rules.yaml");
    snprintf(cfg->audit_log_dir, sizeof(cfg->audit_log_dir),
             "/var/log/agentos/cupolas");
#endif
}

static void cupolas_internal_config_cleanup(cupolas_internal_config_t* cfg) {
    if (!cfg) return;
    cupolas_memset_s(cfg, sizeof(*cfg));
}

static struct {
    int initialized;
    cupolas_internal_config_t config;
    cupolas_config_t* config_mgr;
    permission_engine_t* perm;
    sanitizer_t* san;
    workbench_t* wb;
    audit_logger_t* audit;
    cupolas_mutex_t lock;
} g_cupolas = {0};

int cupolas_init(const char* config_path, agentos_error_t* error) {
    if (g_cupolas.initialized) {
        return CUPOLAS_OK;
    }

    memset(&g_cupolas, 0, sizeof(g_cupolas));

    if (cupolas_mutex_init(&g_cupolas.lock) != 0) {
        if (error) *error = AGENTOS_ERR_IO;
        return cupolas_ERR_UNKNOWN;
    }

    cupolas_mutex_lock(&g_cupolas.lock);

    cupolas_internal_config_init_defaults(&g_cupolas.config);

    if (config_path && config_path[0] != '\0') {
        g_cupolas.config_mgr = cupolas_config_create(NULL);
        if (g_cupolas.config_mgr) {
            int result = cupolas_config_load(g_cupolas.config_mgr,
                                             CONFIG_TYPE_ALL, config_path);
            if (result != 0) {
                if (error) *error = AGENTOS_ERR_IO;
                cupolas_config_destroy(g_cupolas.config_mgr);
                g_cupolas.config_mgr = NULL;
                cupolas_mutex_unlock(&g_cupolas.lock);
                return result;
            }
        }
    }

    g_cupolas.perm = permission_engine_create(
        g_cupolas.config.permission_rules_path[0] ? g_cupolas.config.permission_rules_path : NULL);
    if (!g_cupolas.perm) {
        if (error) *error = AGENTOS_ERR_OUT_OF_MEMORY;
        if (g_cupolas.config_mgr) {
            cupolas_config_destroy(g_cupolas.config_mgr);
            g_cupolas.config_mgr = NULL;
        }
        cupolas_mutex_unlock(&g_cupolas.lock);
        return cupolas_ERR_OUT_OF_MEMORY;
    }

    g_cupolas.san = sanitizer_create(NULL);
    if (!g_cupolas.san) {
        if (error) *error = AGENTOS_ERR_OUT_OF_MEMORY;
        permission_engine_destroy(g_cupolas.perm);
        g_cupolas.perm = NULL;
        if (g_cupolas.config_mgr) {
            cupolas_config_destroy(g_cupolas.config_mgr);
            g_cupolas.config_mgr = NULL;
        }
        cupolas_mutex_unlock(&g_cupolas.lock);
        return cupolas_ERR_OUT_OF_MEMORY;
    }

    g_cupolas.wb = NULL;

    g_cupolas.audit = audit_logger_create(
        g_cupolas.config.audit_log_dir[0] ? g_cupolas.config.audit_log_dir : ".",
        "cupolas_audit", CUPOLAS_DEFAULT_AUDIT_MAX_FILE_SIZE,
        CUPOLAS_DEFAULT_AUDIT_MAX_FILES);
    if (!g_cupolas.audit) {
        if (error) *error = AGENTOS_ERR_OUT_OF_MEMORY;
        sanitizer_destroy(g_cupolas.san);
        g_cupolas.san = NULL;
        permission_engine_destroy(g_cupolas.perm);
        g_cupolas.perm = NULL;
        if (g_cupolas.config_mgr) {
            cupolas_config_destroy(g_cupolas.config_mgr);
            g_cupolas.config_mgr = NULL;
        }
        cupolas_mutex_unlock(&g_cupolas.lock);
        return cupolas_ERR_OUT_OF_MEMORY;
    }

    g_cupolas.initialized = 1;
    cupolas_mutex_unlock(&g_cupolas.lock);

    return CUPOLAS_OK;
}

void cupolas_cleanup(void) {
    if (!g_cupolas.initialized) {
        return;
    }

    cupolas_mutex_lock(&g_cupolas.lock);

    if (g_cupolas.audit) {
        audit_logger_flush(g_cupolas.audit);
        audit_logger_destroy(g_cupolas.audit);
        g_cupolas.audit = NULL;
    }

    if (g_cupolas.wb) {
        workbench_destroy(g_cupolas.wb);
        g_cupolas.wb = NULL;
    }

    if (g_cupolas.san) {
        sanitizer_destroy(g_cupolas.san);
        g_cupolas.san = NULL;
    }

    if (g_cupolas.perm) {
        permission_engine_destroy(g_cupolas.perm);
        g_cupolas.perm = NULL;
    }

    if (g_cupolas.config_mgr) {
        cupolas_config_destroy(g_cupolas.config_mgr);
        g_cupolas.config_mgr = NULL;
    }

    cupolas_internal_config_cleanup(&g_cupolas.config);

    g_cupolas.initialized = 0;
    cupolas_mutex_unlock(&g_cupolas.lock);

    cupolas_mutex_destroy(&g_cupolas.lock);
}

const char* cupolas_version(void) {
    return "1.0.0";
}

int cupolas_check_permission(const char* agent_id, const char* action,
                           const char* resource, const char* context) {
    if (!agent_id || !action || !resource) {
        return cupolas_ERR_INVALID_PARAM;
    }

    if (!g_cupolas.initialized || !g_cupolas.perm) {
        return cupolas_ERR_STATE_ERROR;
    }

    int result = permission_engine_check(g_cupolas.perm, agent_id, action, resource, context);

    if (g_cupolas.audit) {
        audit_logger_log(g_cupolas.audit, AUDIT_EVENT_PERMISSION,
                         agent_id, action, resource, NULL,
                         result >= 0 ? result : -1);
    }

    if (result > 0 && cupolas_guards_is_enabled()) {
        guard_manager_t* guard_manager = cupolas_guards_get_manager();
        if (guard_manager) {
            const size_t max_results = 8;
            guard_result_t results[max_results];
            size_t actual_results = 0;

            guard_context_t guard_ctx = {
                .operation = "permission_check",
                .resource = resource,
                .agent_id = agent_id,
                .session_id = context,
                .input_data = (void*)action,
                .input_size = action ? strlen(action) + 1 : 0,
                .context_data = NULL,
                .timestamp = cupolas_get_timestamp_ns()
            };

            int guard_ret = guard_manager_check_sync(guard_manager, &guard_ctx,
                                                   results, max_results, &actual_results);
            if (guard_ret == 0) {
                for (size_t i = 0; i < actual_results; i++) {
                    guard_result_t* gr = &results[i];
                    if (gr->risk_level >= RISK_LEVEL_MEDIUM &&
                        (gr->recommended_action == GUARD_ACTION_BLOCK ||
                         gr->recommended_action == GUARD_ACTION_ISOLATE ||
                         gr->recommended_action == GUARD_ACTION_TERMINATE)) {
                        if (g_cupolas.audit) {
                            audit_logger_log(g_cupolas.audit, AUDIT_EVENT_PERMISSION,
                                             agent_id, "guard_block",
                                             resource, NULL, 0);
                        }
                        return 0;
                    }
                }
            }
        }
    }

    return result > 0 ? 1 : (result == 0 ? 0 : result);
}

int cupolas_add_permission_rule(const char* agent_id, const char* action,
                               const char* resource, int allow, int priority) {
    if (!g_cupolas.initialized || !g_cupolas.perm) {
        return cupolas_ERR_STATE_ERROR;
    }

    return permission_engine_add_rule(g_cupolas.perm, agent_id, action,
                                     resource, allow, priority);
}

void cupolas_clear_permission_cache(void) {
    if (!g_cupolas.initialized || !g_cupolas.perm) {
        return;
    }

    permission_engine_clear_cache(g_cupolas.perm);
}

int cupolas_sanitize_input(const char* input, char* output, size_t output_size) {
    if (!input || !output || output_size == 0) {
        return cupolas_ERR_INVALID_PARAM;
    }

    cupolas_mutex_lock(&g_cupolas.lock);
    if (!g_cupolas.initialized || !g_cupolas.san) {
        cupolas_mutex_unlock(&g_cupolas.lock);
        return cupolas_ERR_STATE_ERROR;
    }

    sanitize_result_t result = sanitizer_sanitize(g_cupolas.san, input,
                                                  output, output_size, NULL);
    cupolas_mutex_unlock(&g_cupolas.lock);

    if (g_cupolas.audit) {
        audit_logger_log(g_cupolas.audit, AUDIT_EVENT_SANITIZER,
                         "system", "sanitize_input",
                         input, NULL, (int)result);
    }

    if (result == SANITIZE_OK && cupolas_guards_is_enabled()) {
        guard_manager_t* guard_manager = cupolas_guards_get_manager();
        if (guard_manager) {
            const size_t max_results = 8;
            guard_result_t results[max_results];
            size_t actual_results = 0;

            guard_context_t guard_ctx = {
                .operation = "input_sanitization",
                .resource = "sanitizer",
                .agent_id = "system",
                .session_id = NULL,
                .input_data = (void*)output,
                .input_size = strlen(output) + 1,
                .context_data = NULL,
                .timestamp = cupolas_get_timestamp_ns()
            };

            int guard_ret = guard_manager_check_sync(guard_manager, &guard_ctx,
                                                   results, max_results, &actual_results);
            if (guard_ret == 0) {
                for (size_t i = 0; i < actual_results; i++) {
                    guard_result_t* gr = &results[i];
                    if (gr->risk_level >= RISK_LEVEL_CRITICAL) {
                        output[0] = '\0';
                        if (g_cupolas.audit) {
                            audit_logger_log(g_cupolas.audit, AUDIT_EVENT_SANITIZER,
                                             "system", "guard_block",
                                             input, NULL, 0);
                        }
                        return cupolas_ERROR_INVALID_ARG;
                    } else if (gr->risk_level >= RISK_LEVEL_HIGH) {
                        if (g_cupolas.audit) {
                            audit_logger_log(g_cupolas.audit, AUDIT_EVENT_SANITIZER,
                                             "system", "guard_warn",
                                             input, NULL, 0);
                        }
                    }
                }
            }
        }
    }

    return (result == SANITIZE_OK) ? CUPOLAS_OK : cupolas_ERR_PERMISSION_DENIED;
}

int cupolas_execute_command(const char* command, char* const argv[],
                          int* exit_code, char* stdout_buf, size_t stdout_size,
                          char* stderr_buf, size_t stderr_size) {
    if (!command || !argv) {
        return cupolas_ERR_INVALID_PARAM;
    }

    cupolas_mutex_lock(&g_cupolas.lock);
    if (!g_cupolas.initialized) {
        cupolas_mutex_unlock(&g_cupolas.lock);
        return cupolas_ERR_STATE_ERROR;
    }

    if (cupolas_guards_is_enabled()) {
        guard_manager_t* guard_manager = cupolas_guards_get_manager();
        if (guard_manager) {
            char cmd_buffer[1024] = {0};
            size_t pos = 0;

            if (command) {
                pos += snprintf(cmd_buffer + pos, sizeof(cmd_buffer) - pos, "%s", command);
            }

            if (argv) {
                for (int i = 0; argv[i] && pos < sizeof(cmd_buffer) - 1; i++) {
                    pos += snprintf(cmd_buffer + pos, sizeof(cmd_buffer) - pos, " %s", argv[i]);
                }
            }

            const size_t max_results = 8;
            guard_result_t results[max_results];
            size_t actual_results = 0;

            guard_context_t guard_ctx = {
                .operation = "command_execution",
                .resource = "workbench",
                .agent_id = "system",
                .session_id = NULL,
                .input_data = cmd_buffer,
                .input_size = strlen(cmd_buffer) + 1,
                .context_data = NULL,
                .timestamp = cupolas_get_timestamp_ns()
            };

            int guard_ret = guard_manager_check_sync(guard_manager, &guard_ctx,
                                                   results, max_results, &actual_results);
            if (guard_ret == 0) {
                for (size_t i = 0; i < actual_results; i++) {
                    guard_result_t* gr = &results[i];
                    if (gr->risk_level >= RISK_LEVEL_MEDIUM &&
                        (gr->recommended_action == GUARD_ACTION_BLOCK ||
                         gr->recommended_action == GUARD_ACTION_ISOLATE ||
                         gr->recommended_action == GUARD_ACTION_TERMINATE)) {
                        if (g_cupolas.audit) {
                            audit_logger_log(g_cupolas.audit, AUDIT_EVENT_WORKBENCH,
                                             "system", "execute_command",
                                             command, "guard_block",
                                             cupolas_ERR_PERMISSION_DENIED);
                        }
                        cupolas_mutex_unlock(&g_cupolas.lock);
                        return cupolas_ERR_PERMISSION_DENIED;
                    }
                }
            }
        }
    }

    workbench_config_t wbcfg;
    workbench_default_config(&wbcfg);

    if (!g_cupolas.wb) {
        g_cupolas.wb = workbench_create(&wbcfg);
        if (!g_cupolas.wb) {
            cupolas_mutex_unlock(&g_cupolas.lock);
            return cupolas_ERR_OUT_OF_MEMORY;
        }
    }

    workbench_result_t result;
    int ret = workbench_execute(g_cupolas.wb, command, argv, &result);

    if (exit_code) {
        *exit_code = result.exit_code;
    }

    if (stdout_buf && stdout_size > 0 && result.stdout_data) {
        strncpy(stdout_buf, result.stdout_data, stdout_size - 1);
        stdout_buf[stdout_size - 1] = '\0';
    }

    if (stderr_buf && stderr_size > 0 && result.stderr_data) {
        strncpy(stderr_buf, result.stderr_data, stderr_size - 1);
        stderr_buf[stderr_size - 1] = '\0';
    }

    workbench_result_free(&result);

    if (g_cupolas.audit) {
        audit_logger_log(g_cupolas.audit, AUDIT_EVENT_WORKBENCH,
                         "system", "execute_command",
                         command, NULL, ret);
    }

    cupolas_mutex_unlock(&g_cupolas.lock);
    return ret;
}

void cupolas_flush_audit_log(void) {
    if (!g_cupolas.initialized || !g_cupolas.audit) {
        return;
    }

    audit_logger_flush(g_cupolas.audit);
}

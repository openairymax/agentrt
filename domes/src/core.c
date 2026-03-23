/**
 * @file core.c
 * @brief domes 模块核心实现
 * @author Spharx
 * @date 2024
 */

#include "core.h"
#include "permission/permission.h"
#include "audit/audit.h"
#include "sanitizer/sanitizer.h"
#include <stdlib.h>
#include <string.h>

domes_instance_t* g_domes_instance = NULL;

int domes_init(const char* config_path) {
    if (g_domes_instance != NULL) {
        return DOMES_OK;
    }
    
    g_domes_instance = (domes_instance_t*)domes_mem_alloc(sizeof(domes_instance_t));
    if (!g_domes_instance) {
        return DOMES_ERROR_NO_MEMORY;
    }
    
    memset(g_domes_instance, 0, sizeof(domes_instance_t));
    
    if (domes_rwlock_init(&g_domes_instance->lock) != DOMES_OK) {
        domes_mem_free(g_domes_instance);
        g_domes_instance = NULL;
        return DOMES_ERROR_UNKNOWN;
    }
    
    if (config_path) {
        g_domes_instance->config_path = domes_strdup(config_path);
    }
    
    g_domes_instance->permission = permission_engine_create(NULL);
    if (!g_domes_instance->permission) {
        domes_rwlock_destroy(&g_domes_instance->lock);
        domes_mem_free(g_domes_instance->config_path);
        domes_mem_free(g_domes_instance);
        g_domes_instance = NULL;
        return DOMES_ERROR_UNKNOWN;
    }
    
    g_domes_instance->audit = audit_logger_create(NULL, "domes", 10 * 1024 * 1024, 10);
    if (!g_domes_instance->audit) {
        permission_engine_destroy(g_domes_instance->permission);
        domes_rwlock_destroy(&g_domes_instance->lock);
        domes_mem_free(g_domes_instance->config_path);
        domes_mem_free(g_domes_instance);
        g_domes_instance = NULL;
        return DOMES_ERROR_UNKNOWN;
    }
    
    g_domes_instance->sanitizer = sanitizer_create(NULL);
    if (!g_domes_instance->sanitizer) {
        audit_logger_destroy(g_domes_instance->audit);
        permission_engine_destroy(g_domes_instance->permission);
        domes_rwlock_destroy(&g_domes_instance->lock);
        domes_mem_free(g_domes_instance->config_path);
        domes_mem_free(g_domes_instance);
        g_domes_instance = NULL;
        return DOMES_ERROR_UNKNOWN;
    }
    
    domes_atomic_store32(&g_domes_instance->ref_count, 1);
    g_domes_instance->initialized = true;
    
    return DOMES_OK;
}

void domes_cleanup(void) {
    if (!g_domes_instance) return;
    
    if (domes_atomic_sub32(&g_domes_instance->ref_count, 1) > 1) {
        return;
    }
    
    domes_rwlock_wrlock(&g_domes_instance->lock);
    
    g_domes_instance->initialized = false;
    
    if (g_domes_instance->sanitizer) {
        sanitizer_destroy(g_domes_instance->sanitizer);
        g_domes_instance->sanitizer = NULL;
    }
    
    if (g_domes_instance->audit) {
        audit_logger_destroy(g_domes_instance->audit);
        g_domes_instance->audit = NULL;
    }
    
    if (g_domes_instance->permission) {
        permission_engine_destroy(g_domes_instance->permission);
        g_domes_instance->permission = NULL;
    }
    
    domes_mem_free(g_domes_instance->config_path);
    domes_mem_free(g_domes_instance->log_dir);
    
    domes_rwlock_unlock(&g_domes_instance->lock);
    domes_rwlock_destroy(&g_domes_instance->lock);
    
    domes_mem_free(g_domes_instance);
    g_domes_instance = NULL;
}

int domes_check_permission(const char* agent_id, const char* action,
                           const char* resource, const char* context) {
    if (!g_domes_instance || !g_domes_instance->permission) {
        return 0;
    }
    
    int result = permission_engine_check(g_domes_instance->permission, 
                                          agent_id, action, resource, context);
    
    if (g_domes_instance->audit) {
        audit_logger_log_permission(g_domes_instance->audit, agent_id, 
                                     action, resource, result);
    }
    
    return result;
}

int domes_sanitize_input(const char* input, char* output, size_t output_size) {
    if (!g_domes_instance || !g_domes_instance->sanitizer) {
        if (output && output_size > 0 && input) {
            strncpy(output, input, output_size - 1);
            output[output_size - 1] = '\0';
        }
        return DOMES_OK;
    }
    
    sanitize_result_t result = sanitizer_sanitize(g_domes_instance->sanitizer,
                                                   input, output, output_size, NULL);
    
    if (g_domes_instance->audit) {
        audit_logger_log_sanitizer(g_domes_instance->audit, NULL,
                                    input, output, result == SANITIZE_OK);
    }
    
    return result == SANITIZE_REJECTED ? DOMES_ERROR_PERMISSION : DOMES_OK;
}

int domes_execute_command(const char* command, char* const argv[],
                          int* exit_code, char* stdout_buf, size_t stdout_size,
                          char* stderr_buf, size_t stderr_size) {
    if (!g_domes_instance) {
        return DOMES_ERROR_INVALID_ARG;
    }
    
    workbench_config_t config;
    workbench_default_config(&config);
    
    workbench_t* wb = workbench_create(&config);
    if (!wb) {
        return DOMES_ERROR_NO_MEMORY;
    }
    
    workbench_result_t result;
    int ret = workbench_execute(wb, command, argv, &result);
    
    if (ret == DOMES_OK) {
        if (exit_code) *exit_code = result.exit_code;
        if (stdout_buf && stdout_size > 0 && result.stdout_data) {
            strncpy(stdout_buf, result.stdout_data, stdout_size - 1);
            stdout_buf[stdout_size - 1] = '\0';
        }
        if (stderr_buf && stderr_size > 0 && result.stderr_data) {
            strncpy(stderr_buf, result.stderr_data, stderr_size - 1);
            stderr_buf[stderr_size - 1] = '\0';
        }
        
        if (g_domes_instance->audit) {
            audit_logger_log_workbench(g_domes_instance->audit, NULL,
                                        command, result.exit_code);
        }
    }
    
    workbench_result_free(&result);
    workbench_destroy(wb);
    
    return ret;
}

const char* domes_version(void) {
    return "1.0.0";
}

int domes_add_permission_rule(const char* agent_id, const char* action,
                               const char* resource, int allow, int priority) {
    if (!g_domes_instance || !g_domes_instance->permission) {
        return DOMES_ERROR_INVALID_ARG;
    }
    
    return permission_engine_add_rule(g_domes_instance->permission,
                                       agent_id, action, resource, allow, priority);
}

void domes_clear_permission_cache(void) {
    if (!g_domes_instance || !g_domes_instance->permission) {
        return;
    }
    
    permission_engine_clear_cache(g_domes_instance->permission);
}

void domes_flush_audit_log(void) {
    if (!g_domes_instance || !g_domes_instance->audit) {
        return;
    }
    
    audit_logger_flush(g_domes_instance->audit);
}

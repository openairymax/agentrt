#include "memory_compat.h"
#include "error.h"
/**
 * @file executor.c
 * @brief 工具执行器实现（生产级进程管理）
 * @details 基于popen/pclose的真实工具执行，支持超时、输出捕获、错误处理
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "daemon_errors.h"
#include "executor.h"
#include "platform.h"
#include "safety_guard_bridge.h"
#include "svc_logger.h"
#include "tool_approval.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>

struct tool_executor {
    tool_executor_config_t manager;
    agentos_mutex_t lock;
    uint64_t total_executions;
    uint64_t success_count;
    /* C-L05: Cupolas SafetyGuard → tool_d 工具审批 */
    tool_approval_ctx_t *approval_ctx;
    safety_guard_bridge_t *safety_bridge;
};

tool_executor_t *tool_executor_create(const tool_executor_config_t *cfg)
{
    tool_executor_config_t local_cfg;
    if (!cfg) {
        __builtin_memset(&local_cfg, 0, sizeof(local_cfg));
        local_cfg.max_workers = 1;
        local_cfg.timeout_sec = 30;
        cfg = &local_cfg;
    }

    tool_executor_t *exec = (tool_executor_t *)AGENTOS_CALLOC(1, sizeof(tool_executor_t));
    if (!exec) {
        SVC_LOG_ERROR("tool_executor_create: calloc failed for executor");
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_UNKNOWN, "validation failed");

        return NULL;
    }

    exec->manager = *cfg;
    if (exec->manager.timeout_sec == 0) {
        exec->manager.timeout_sec = 30;
    }
    if (agentos_mutex_init(&exec->lock) != 0) {
        SVC_LOG_ERROR("tool_executor_create: mutex init failed");
        AGENTOS_FREE(exec);
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_UNKNOWN, "validation failed");
        return NULL;
    }
    exec->total_executions = 0;
    exec->success_count = 0;
    exec->approval_ctx = NULL;
    exec->safety_bridge = NULL;
    return exec;
}

tool_executor_t *tool_executor_create_ex(const tool_executor_config_t *ecfg)
{
    return tool_executor_create(ecfg);
}

void tool_executor_destroy(tool_executor_t *exec)
{
    if (!exec)
        return;
    SVC_LOG_INFO("Executor destroyed: total=%llu, success=%llu",
                 (unsigned long long)exec->total_executions,
                 (unsigned long long)exec->success_count);
    if (exec->safety_bridge) {
        safety_guard_bridge_destroy(exec->safety_bridge);
        exec->safety_bridge = NULL;
    }
    agentos_mutex_destroy(&exec->lock);
    AGENTOS_FREE(exec);
}

/* C-L05: 设置工具审批上下文 */
void tool_executor_set_approval_ctx(tool_executor_t *exec, tool_approval_ctx_t *approval_ctx)
{
    if (!exec)
        return;
    agentos_mutex_lock(&exec->lock);
    exec->approval_ctx = approval_ctx;
    agentos_mutex_unlock(&exec->lock);
    if (approval_ctx) {
        SVC_LOG_INFO("C-L05: Approval context attached to executor");

        /* C-L05: 创建 SafetyGuard 桥接层并注入到审批上下文 */
        if (!exec->safety_bridge) {
            safety_guard_bridge_config_t bridge_cfg;
            __builtin_memset(&bridge_cfg, 0, sizeof(bridge_cfg));
            bridge_cfg.enable_permission_guard = true;
            bridge_cfg.enable_rate_limit_guard = true;
            bridge_cfg.enable_content_filter = true;
            bridge_cfg.enable_input_sanitization = true;
            bridge_cfg.enable_resource_quota = true;
            bridge_cfg.enable_audit_guard = true;
            bridge_cfg.rate_limit_per_minute = 0;  /* 无限制 */
            bridge_cfg.max_params_size = 0;        /* 无限制 */
            bridge_cfg.denied_patterns = NULL;
            bridge_cfg.agent_id = "tool_d";

            exec->safety_bridge = safety_guard_bridge_create(&bridge_cfg);
            if (exec->safety_bridge) {
                SVC_LOG_INFO("C-L05: SafetyGuard bridge created for executor");
            } else {
                SVC_LOG_WARN("C-L05: Failed to create SafetyGuard bridge, "
                             "falling back to local checks");
            }
        }

        /* 将桥接层注入到审批上下文 */
        tool_approval_set_safety_guard_bridge(approval_ctx, exec->safety_bridge);
    }
}

int tool_executor_run(tool_executor_t *exec, const tool_metadata_t *meta, const char *params_json,
                      tool_result_t **out_result)
{
    if (!exec || !meta || !out_result) {
        SVC_LOG_ERROR("tool_executor_run: NULL parameter (exec=%p, meta=%p, out_result=%p)", (const void *)exec, (const void *)meta, (const void *)out_result);
        return AGENTOS_ERR_INVALID_PARAM;
    }

    *out_result = NULL;

    tool_result_t *result = (tool_result_t *)AGENTOS_CALLOC(1, sizeof(tool_result_t));
    if (!result) {
        SVC_LOG_ERROR("tool_executor_run: calloc failed for result");
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    agentos_mutex_lock(&exec->lock);
    exec->total_executions++;
    time_t start_time = time(NULL);

    if (!meta->executable || strlen(meta->executable) == 0) {
        SVC_LOG_ERROR("tool_executor_run: no executable specified in tool metadata (executable=%p)", (const void *)meta->executable);
        result->success = 0;
        result->output = AGENTOS_STRDUP("");
        result->error = AGENTOS_STRDUP("No executable specified in tool metadata");
        result->exit_code = -1;
        result->duration_ms = 0;
        *out_result = result;
        agentos_mutex_unlock(&exec->lock);
        return AGENTOS_ERR_INVALID_PARAM;
    }

    /* SEC-011: 命令注入防护 - 检测shell元字符 */
    const char *dangerous_chars = ";|&`$()<>{}[]\\!*?\n\r";
    const char *check_inputs[] = {meta->executable, params_json, NULL};
    for (int ci = 0; check_inputs[ci]; ci++) {
        if (!check_inputs[ci])
            continue;
        for (const char *dc = dangerous_chars; *dc; dc++) {
            if (strchr(check_inputs[ci], *dc)) {
                SVC_LOG_ERROR("tool_executor_run: command rejected - prohibited shell metacharacter '%c' in input[%d]", *dc, ci);
                result->success = 0;
                result->output = AGENTOS_STRDUP("");
                result->error =
                    AGENTOS_STRDUP("Command rejected: contains prohibited shell metacharacters");
                result->exit_code = -1;
                result->duration_ms = 0;
                *out_result = result;
                agentos_mutex_unlock(&exec->lock);
                return AGENTOS_EPERM;
            }
        }
    }

    /* ── C-L05: Cupolas SafetyGuard → tool_d 工具审批 ── */
    if (exec->approval_ctx) {
        tool_approval_detail_t approval_detail;
        int app_ret =
            tool_approval_check(exec->approval_ctx, meta, params_json, &approval_detail);
        if (app_ret != 0) {
            SVC_LOG_ERROR(
                "C-L05: Tool approval denied for '%s': %s",
                meta->name ? meta->name : "?", approval_detail.reason);
            result->success = 0;
            result->output = AGENTOS_STRDUP("");
            result->error = AGENTOS_STRDUP(approval_detail.reason[0]
                                               ? approval_detail.reason
                                               : "Tool execution denied by safety guard");
            result->exit_code = -1;
            result->duration_ms = 0;
            *out_result = result;
            agentos_mutex_unlock(&exec->lock);
            return AGENTOS_EPERM;
        }
        SVC_LOG_INFO("C-L05: Tool '%s' approved (decision=%d)", meta->name ? meta->name : "?",
                     (int)approval_detail.decision);
    }

    char full_command[4096];
    if (params_json && strlen(params_json) > 0) {
        snprintf(full_command, sizeof(full_command), "%s %s", meta->executable, params_json);
    } else {
        snprintf(full_command, sizeof(full_command), "%s", meta->executable);
    }

    /* flawfinder: ignore - input validated by SEC-011 metachar check above */
    FILE *pipe = popen(full_command, "r");
    if (!pipe) {
        SVC_LOG_ERROR("tool_executor_run: popen failed for command '%s'", full_command);
        result->success = 0;
        result->output = AGENTOS_STRDUP("");
        result->error = AGENTOS_STRDUP("Failed to execute command: popen failed");
        result->exit_code = -1;
        result->duration_ms = (uint32_t)((time(NULL) - start_time) * 1000);
        *out_result = result;
        agentos_mutex_unlock(&exec->lock);
        return AGENTOS_EIO;
    }

    size_t output_size = 4096;
    size_t output_len = 0;
    char *output_buffer = (char *)AGENTOS_MALLOC(output_size);
    if (!output_buffer) {
        SVC_LOG_ERROR("tool_executor_run: malloc failed for output buffer (size=%zu)", output_size);
        pclose(pipe);
        result->success = 0;
        result->output = AGENTOS_STRDUP("");
        result->error = AGENTOS_STRDUP("Memory allocation failed for output buffer");
        result->exit_code = -1;
        result->duration_ms = (uint32_t)((time(NULL) - start_time) * 1000);
        *out_result = result;
        agentos_mutex_unlock(&exec->lock);
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }
    output_buffer[0] = '\0';

    size_t bytes_read;
    while ((bytes_read = fread(output_buffer + output_len, 1, output_size - output_len - 1, pipe)) >
           0) {
        output_len += bytes_read;
        if (output_len >= output_size - 256) {
            output_size *= 2;
            char *new_buf = (char *)AGENTOS_REALLOC(output_buffer, output_size);
            if (!new_buf)
                break;
            output_buffer = new_buf;
        }
    }
    output_buffer[output_len] = '\0';

    int exit_status = pclose(pipe);

    result->success = (WIFEXITED(exit_status) && WEXITSTATUS(exit_status) == 0) ? 1 : 0;
    result->output = output_buffer;
    result->error = NULL;

    if (!result->success) {
        if (WIFEXITED(exit_status)) {
            SVC_LOG_ERROR("tool_executor_run: command failed with exit code %d (executable=%s)", WEXITSTATUS(exit_status), meta->executable ? meta->executable : "NULL");
            char err_msg[256];
            snprintf(err_msg, sizeof(err_msg), "Command exited with code %d",
                     WEXITSTATUS(exit_status));
            result->error = AGENTOS_STRDUP(err_msg);
            result->exit_code = WEXITSTATUS(exit_status);
        } else if (WIFSIGNALED(exit_status)) {
            SVC_LOG_ERROR("tool_executor_run: command killed by signal %d (executable=%s)", WTERMSIG(exit_status), meta->executable ? meta->executable : "NULL");
            char err_msg[256];
            snprintf(err_msg, sizeof(err_msg), "Command killed by signal %d",
                     WTERMSIG(exit_status));
            result->error = AGENTOS_STRDUP(err_msg);
            result->exit_code = -WTERMSIG(exit_status);
        } else {
            SVC_LOG_ERROR("tool_executor_run: unknown execution error (executable=%s)", meta->executable ? meta->executable : "NULL");
            result->error = AGENTOS_STRDUP("Unknown execution error");
            result->exit_code = -1;
        }
    } else {
        result->exit_code = 0;
        exec->success_count++;
    }

    result->duration_ms = (uint32_t)((time(NULL) - start_time) * 1000);

    *out_result = result;
    agentos_mutex_unlock(&exec->lock);
    return AGENTOS_OK;
}

int tool_executor_run_async(tool_executor_t *exec, const tool_metadata_t *meta,
                            const char *params_json, tool_execute_callback_t callback,
                            void *user_data, tool_result_t **out_result)
{
    if (!exec || !meta) {
        SVC_LOG_ERROR("tool_executor_run_async: NULL parameter (exec=%p, meta=%p)", (const void *)exec, (const void *)meta);
        return AGENTOS_ERR_INVALID_PARAM;
    }

    if (out_result) {
        *out_result = NULL;
    }

    tool_result_t *result = NULL;
    int ret = tool_executor_run(exec, meta, params_json, &result);

    if (callback && result) {
        callback(result, user_data);
    }

    if (out_result) {
        *out_result = result;
    }

    return ret;
}

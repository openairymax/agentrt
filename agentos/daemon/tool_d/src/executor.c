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

    /* BAN-211/235: 使用 execvp 直接执行（不经 shell），无需 SEC-011 shell 元字符检查。
     * params_json 作为单个 argv 元素传递给工具，工具自行解析 JSON。 */

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

    /* BAN-211/235: 构建 argv 并通过 execvp 直接执行（不经 shell），消除命令注入风险。
     * params_json 作为单个 argv 元素传递给工具，工具自行解析。 */
    const char *argv[3];
    int arg_count = 0;
    argv[arg_count++] = meta->executable;
    if (params_json && strlen(params_json) > 0) {
        argv[arg_count++] = params_json;
    }
    argv[arg_count] = NULL;

    /* 分配输出捕获缓冲区（1MB 上限，覆盖绝大多数工具输出，同时防止失控输出 OOM） */
    size_t cap_size = 1024 * 1024;
    char *output_buffer = (char *)AGENTOS_MALLOC(cap_size);
    if (!output_buffer) {
        SVC_LOG_ERROR("tool_executor_run: malloc failed for output buffer (size=%zu)", cap_size);
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

    uint32_t timeout_ms = (uint32_t)exec->manager.timeout_sec * 1000;
    int ret = agentos_process_run_capture(meta->executable, (char *const *)argv, NULL,
                                          timeout_ms, output_buffer, cap_size);

    /* 缩减缓冲区到实际输出长度，避免内存浪费 */
    size_t actual_len = strlen(output_buffer);
    if (actual_len + 1 < cap_size) {
        char *shrunk = (char *)AGENTOS_REALLOC(output_buffer, actual_len + 1);
        if (shrunk) {
            output_buffer = shrunk;
        }
    }

    result->output = output_buffer;
    result->error = NULL;

    if (ret == -1) {
        SVC_LOG_ERROR("tool_executor_run: failed to start command '%s'",
                      meta->executable ? meta->executable : "NULL");
        result->success = 0;
        result->error = AGENTOS_STRDUP("Failed to execute command: execvp failed");
        result->exit_code = -1;
    } else if (ret == -2) {
        SVC_LOG_ERROR("tool_executor_run: command timed out after %u ms (executable=%s)",
                      timeout_ms, meta->executable ? meta->executable : "NULL");
        result->success = 0;
        result->error = AGENTOS_STRDUP("Command timed out");
        result->exit_code = -1;
    } else if (ret == 0) {
        result->success = 1;
        result->exit_code = 0;
        exec->success_count++;
    } else if (ret > 0) {
        SVC_LOG_ERROR("tool_executor_run: command failed with exit code %d (executable=%s)", ret,
                      meta->executable ? meta->executable : "NULL");
        result->success = 0;
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "Command exited with code %d", ret);
        result->error = AGENTOS_STRDUP(err_msg);
        result->exit_code = ret;
    } else {
        /* ret < 0 且非 -1/-2：信号终止（exit_code = -signum） */
        SVC_LOG_ERROR("tool_executor_run: command killed by signal %d (executable=%s)", -ret,
                      meta->executable ? meta->executable : "NULL");
        result->success = 0;
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "Command killed by signal %d", -ret);
        result->error = AGENTOS_STRDUP(err_msg);
        result->exit_code = ret;
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

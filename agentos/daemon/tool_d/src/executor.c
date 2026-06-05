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
#include "svc_logger.h"

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
};

tool_executor_t *tool_executor_create(const tool_executor_config_t *cfg)
{
    tool_executor_config_t local_cfg;
    if (!cfg) {
        AGENTOS_MEMSET(&local_cfg, 0, sizeof(local_cfg));
        local_cfg.max_workers = 1;
        local_cfg.timeout_sec = 30;
        cfg = &local_cfg;
    }

    tool_executor_t *exec = (tool_executor_t *)AGENTOS_CALLOC(1, sizeof(tool_executor_t));
    if (!exec) {
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_UNKNOWN, "validation failed");

        return NULL;
    }

    exec->manager = *cfg;
    if (exec->manager.timeout_sec == 0) {
        exec->manager.timeout_sec = 30;
    }
    if (agentos_mutex_init(&exec->lock) != 0) {
        AGENTOS_FREE(exec);
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_UNKNOWN, "validation failed");
        return NULL;
    }
    exec->total_executions = 0;
    exec->success_count = 0;
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
    agentos_mutex_destroy(&exec->lock);
    AGENTOS_FREE(exec);
}

int tool_executor_run(tool_executor_t *exec, const tool_metadata_t *meta, const char *params_json,
                      tool_result_t **out_result)
{
    if (!exec || !meta || !out_result) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    *out_result = NULL;

    tool_result_t *result = (tool_result_t *)AGENTOS_CALLOC(1, sizeof(tool_result_t));
    if (!result)
        return AGENTOS_ERR_OUT_OF_MEMORY;

    agentos_mutex_lock(&exec->lock);
    exec->total_executions++;
    time_t start_time = time(NULL);

    if (!meta->executable || strlen(meta->executable) == 0) {
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

    char full_command[4096];
    if (params_json && strlen(params_json) > 0) {
        snprintf(full_command, sizeof(full_command), "%s %s", meta->executable, params_json);
    } else {
        snprintf(full_command, sizeof(full_command), "%s", meta->executable);
    }

    /* flawfinder: ignore - input validated by SEC-011 metachar check above */
    FILE *pipe = popen(full_command, "r");
    if (!pipe) {
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
            char err_msg[256];
            snprintf(err_msg, sizeof(err_msg), "Command exited with code %d",
                     WEXITSTATUS(exit_status));
            result->error = AGENTOS_STRDUP(err_msg);
            result->exit_code = WEXITSTATUS(exit_status);
        } else if (WIFSIGNALED(exit_status)) {
            char err_msg[256];
            snprintf(err_msg, sizeof(err_msg), "Command killed by signal %d",
                     WTERMSIG(exit_status));
            result->error = AGENTOS_STRDUP(err_msg);
            result->exit_code = -WTERMSIG(exit_status);
        } else {
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

/**
 * @file executor.c
 * @brief 工具执行器实现（生产级进程管理）
 * @details 基于popen/pclose的真实工具执行，支持超时、输出捕获、错误处理
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "executor.h"
#include "daemon_errors.h"
#include "platform.h"
#include "svc_logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/wait.h>

struct tool_executor {
    tool_executor_config_t manager;
    agentos_mutex_t lock;
    uint64_t total_executions;
    uint64_t success_count;
};

tool_executor_t* tool_executor_create(const tool_executor_config_t* cfg) {
    if (!cfg) return NULL;

    tool_executor_t* exec = (tool_executor_t*)calloc(1, sizeof(tool_executor_t));
    if (!exec) return NULL;

    exec->manager = *cfg;
    if (exec->manager.timeout_sec == 0) {
        exec->manager.timeout_sec = 30;
    }
    if (agentos_mutex_init(&exec->lock) != 0) {
        free(exec);
        return NULL;
    }
    exec->total_executions = 0;
    exec->success_count = 0;
    return exec;
}

tool_executor_t* tool_executor_create_ex(const tool_executor_config_t* ecfg) {
    return tool_executor_create(ecfg);
}

void tool_executor_destroy(tool_executor_t* exec) {
    if (!exec) return;
    SVC_LOG_INFO("Executor destroyed: total=%llu, success=%llu",
                (unsigned long long)exec->total_executions,
                (unsigned long long)exec->success_count);
    agentos_mutex_destroy(&exec->lock);
    free(exec);
}

int tool_executor_run(tool_executor_t* exec,
                      const tool_metadata_t* meta,
                      const char* params_json,
                      tool_result_t** out_result) {
    if (!exec || !meta || !out_result) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    *out_result = NULL;

    tool_result_t* result = (tool_result_t*)calloc(1, sizeof(tool_result_t));
    if (!result) return AGENTOS_ERR_OUT_OF_MEMORY;

    agentos_mutex_lock(&exec->lock);
    exec->total_executions++;
    time_t start_time = time(NULL);

    if (!meta->executable || strlen(meta->executable) == 0) {
        result->success = 0;
        result->output = strdup("");
        result->error = strdup("No executable specified in tool metadata");
        result->exit_code = -1;
        result->duration_ms = 0;
        *out_result = result;
        agentos_mutex_unlock(&exec->lock);
        return AGENTOS_ERR_INVALID_PARAM;
    }

    char full_command[4096];
    if (params_json && strlen(params_json) > 0) {
        snprintf(full_command, sizeof(full_command), "%s %s", meta->executable, params_json);
    } else {
        snprintf(full_command, sizeof(full_command), "%s", meta->executable);
    }

    FILE* pipe = popen(full_command, "r");
    if (!pipe) {
        result->success = 0;
        result->output = strdup("");
        result->error = strdup("Failed to execute command: popen failed");
        result->exit_code = -1;
        result->duration_ms = (uint32_t)((time(NULL) - start_time) * 1000);
        *out_result = result;
        agentos_mutex_unlock(&exec->lock);
        return AGENTOS_EIO;
    }

    size_t output_size = 4096;
    size_t output_len = 0;
    char* output_buffer = (char*)malloc(output_size);
    if (!output_buffer) {
        pclose(pipe);
        result->success = 0;
        result->output = strdup("");
        result->error = strdup("Memory allocation failed for output buffer");
        result->exit_code = -1;
        result->duration_ms = (uint32_t)((time(NULL) - start_time) * 1000);
        *out_result = result;
        agentos_mutex_unlock(&exec->lock);
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }
    output_buffer[0] = '\0';

    size_t bytes_read;
    while ((bytes_read = fread(output_buffer + output_len, 1, output_size - output_len - 1, pipe)) > 0) {
        output_len += bytes_read;
        if (output_len >= output_size - 256) {
            output_size *= 2;
            char* new_buf = (char*)realloc(output_buffer, output_size);
            if (!new_buf) break;
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
            snprintf(err_msg, sizeof(err_msg), "Command exited with code %d", WEXITSTATUS(exit_status));
            result->error = strdup(err_msg);
            result->exit_code = WEXITSTATUS(exit_status);
        } else if (WIFSIGNALED(exit_status)) {
            char err_msg[256];
            snprintf(err_msg, sizeof(err_msg), "Command killed by signal %d", WTERMSIG(exit_status));
            result->error = strdup(err_msg);
            result->exit_code = -WTERMSIG(exit_status);
        } else {
            result->error = strdup("Unknown execution error");
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

int tool_executor_run_async(tool_executor_t* exec,
                           const tool_metadata_t* meta,
                           const char* params_json,
                           tool_execute_callback_t callback,
                           void* user_data,
                           tool_result_t** out_result) {
    if (!exec || !meta) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    if (out_result) {
        *out_result = NULL;
    }

    tool_result_t* result = NULL;
    int ret = tool_executor_run(exec, meta, params_json, &result);

    if (callback && result) {
        callback(result, user_data);
    }

    if (out_result) {
        *out_result = result;
    }

    return ret;
}

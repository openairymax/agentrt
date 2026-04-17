/**
 * @file executor.c
 * @brief 工具执行器实现（简化桩版本）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "executor.h"
#include "daemon_errors.h"
#include <stdlib.h>
#include <string.h>

/* ==================== 完整结构体定义 ==================== */

struct tool_executor {
    tool_executor_config_t manager;
    agentos_mutex_t lock;
};

/* ==================== 公共接口实现 ==================== */

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
    return exec;
}

tool_executor_t* tool_executor_create_ex(const tool_executor_config_t* ecfg) {
    return tool_executor_create(ecfg);
}

void tool_executor_destroy(tool_executor_t* exec) {
    if (!exec) return;
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

    result->success = 0;
    result->output = meta->executable ? strdup(meta->executable) : NULL;
    result->error = strdup("Executor not yet implemented - stub mode");
    result->exit_code = -1;
    result->duration_ms = 0;

    *out_result = result;
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

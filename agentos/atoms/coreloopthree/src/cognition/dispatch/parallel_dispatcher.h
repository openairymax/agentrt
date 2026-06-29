/*
 * parallel_dispatcher.h — 认知层工具并行调度器
 *
 * 职责：CoreLoopThree 认知引擎的工具调用并行调度，供 cognition/dispatch 使用。
 * 字段约定：tool_name / arguments / elapsed_ns（纳秒）。
 *
 * 注意：daemon/common/include/daemon_task_dispatcher.h（原 parallel_dispatcher.h）
 * 同名但职责不同（daemon 层并行任务执行，字段为 tool_id/params_json/duration_ms）。
 * 两者 include guard 已分离：本文件用 AGENTOS_PARALLEL_DISPATCHER_H，
 * daemon 侧用 AGENTOS_DAEMON_TASK_DISPATCHER_H。
 */
#ifndef AGENTOS_PARALLEL_DISPATCHER_H
#define AGENTOS_PARALLEL_DISPATCHER_H

#include "agentos.h"
#include "delegate.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum agentos_tool_safety_class {
    AGENTOS_TOOL_READ_ONLY = 0,
    AGENTOS_TOOL_WRITE_SHARED = 1,
    AGENTOS_TOOL_INTERACTIVE = 2,
    AGENTOS_TOOL_SIDE_EFFECT = 3
} agentos_tool_safety_class_t;

typedef struct agentos_tool_call {
    const char *tool_name;
    const char *arguments;
    size_t arguments_len;
    agentos_tool_safety_class_t safety_class;
    const char *resource_path;
} agentos_tool_call_t;

typedef struct agentos_tool_result {
    char *tool_name;
    agentos_error_t error;
    char *output;
    size_t output_len;
    uint64_t elapsed_ns;
} agentos_tool_result_t;

typedef struct agentos_parallel_dispatcher agentos_parallel_dispatcher_t;

AGENTOS_API agentos_parallel_dispatcher_t *agentos_parallel_dispatcher_create(int max_parallel);
AGENTOS_API void agentos_parallel_dispatcher_destroy(agentos_parallel_dispatcher_t *dispatcher);
AGENTOS_API agentos_error_t agentos_parallel_dispatcher_set_executor(
    agentos_parallel_dispatcher_t *dispatcher, agentos_tool_execute_fn executor, void *user_data);
AGENTOS_API agentos_error_t agentos_parallel_dispatcher_dispatch(
    agentos_parallel_dispatcher_t *dispatcher, const agentos_tool_call_t *calls, size_t call_count,
    agentos_tool_result_t **out_results, size_t *out_result_count);
AGENTOS_API agentos_error_t
agentos_parallel_dispatcher_cancel(agentos_parallel_dispatcher_t *dispatcher);
AGENTOS_API bool
agentos_parallel_dispatcher_is_cancelled(const agentos_parallel_dispatcher_t *dispatcher);

#ifdef __cplusplus
}
#endif

#endif

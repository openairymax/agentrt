/*
 * parallel_dispatcher.h — 认知层工具并行调度器
 *
 * 职责：CoreLoopThree 认知引擎的工具调用并行调度，供 cognition/dispatch 使用。
 * 字段约定：tool_name / arguments / elapsed_ns（纳秒）。
 *
 * 注意：daemons/common/include/daemon_task_dispatcher.h（原 parallel_dispatcher.h）
 * 同名但职责不同（daemon 层并行任务执行，字段为 tool_id/params_json/duration_ms）。
 * 两者 include guard 已分离：本文件用 AGENTRT_PARALLEL_DISPATCHER_H，
 * daemon 侧用 AGENTRT_DAEMON_TASK_DISPATCHER_H。
 */
#ifndef AGENTRT_PARALLEL_DISPATCHER_H
#define AGENTRT_PARALLEL_DISPATCHER_H

#include "agentrt.h"
#include "delegate.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum agentrt_tool_safety_class {
    AGENTRT_TOOL_READ_ONLY = 0,
    AGENTRT_TOOL_WRITE_SHARED = 1,
    AGENTRT_TOOL_INTERACTIVE = 2,
    AGENTRT_TOOL_SIDE_EFFECT = 3
} agentrt_tool_safety_class_t;

typedef struct agentrt_tool_call {
    const char *tool_name;
    const char *arguments;
    size_t arguments_len;
    agentrt_tool_safety_class_t safety_class;
    const char *resource_path;
} agentrt_tool_call_t;

typedef struct agentrt_tool_result {
    char *tool_name;
    agentrt_error_t error;
    char *output;
    size_t output_len;
    uint64_t elapsed_ns;
} agentrt_tool_result_t;

typedef struct agentrt_parallel_dispatcher agentrt_parallel_dispatcher_t;

AGENTRT_API agentrt_parallel_dispatcher_t *agentrt_parallel_dispatcher_create(int max_parallel);
AGENTRT_API void agentrt_parallel_dispatcher_destroy(agentrt_parallel_dispatcher_t *dispatcher);
AGENTRT_API agentrt_error_t agentrt_parallel_dispatcher_set_executor(
    agentrt_parallel_dispatcher_t *dispatcher, agentrt_tool_execute_fn executor, void *user_data);
AGENTRT_API agentrt_error_t agentrt_parallel_dispatcher_dispatch(
    agentrt_parallel_dispatcher_t *dispatcher, const agentrt_tool_call_t *calls, size_t call_count,
    agentrt_tool_result_t **out_results, size_t *out_result_count);
AGENTRT_API agentrt_error_t
agentrt_parallel_dispatcher_cancel(agentrt_parallel_dispatcher_t *dispatcher);
AGENTRT_API bool
agentrt_parallel_dispatcher_is_cancelled(const agentrt_parallel_dispatcher_t *dispatcher);

#ifdef __cplusplus
}
#endif

#endif

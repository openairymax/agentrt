#ifndef AGENTOS_PARALLEL_DISPATCHER_H
#define AGENTOS_PARALLEL_DISPATCHER_H

#include "agentos.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum agentos_tool_safety_class {
    AGENTOS_TOOL_READ_ONLY = 0,
    AGENTOS_TOOL_WRITE_SHARED = 1,
    AGENTOS_TOOL_INTERACTIVE = 2,
    AGENTOS_TOOL_SIDE_EFFECT = 3,
} agentos_tool_safety_class_t;

typedef struct agentos_tool_call {
    const char* tool_name;
    const char* arguments;
    size_t arguments_len;
    agentos_tool_safety_class_t safety_class;
    const char* resource_path;
} agentos_tool_call_t;

typedef struct agentos_tool_result {
    char* tool_name;
    agentos_error_t error;
    char* output;
    size_t output_len;
    uint64_t elapsed_ns;
} agentos_tool_result_t;

typedef struct agentos_parallel_dispatcher agentos_parallel_dispatcher_t;

typedef agentos_error_t (*agentos_tool_execute_fn)(
    const char* tool_name,
    const char* arguments,
    size_t arguments_len,
    char** out_output,
    size_t* out_output_len,
    void* user_data);

AGENTOS_API agentos_parallel_dispatcher_t* agentos_parallel_dispatcher_create(int max_parallel);
AGENTOS_API void agentos_parallel_dispatcher_destroy(agentos_parallel_dispatcher_t* dispatcher);
AGENTOS_API agentos_error_t agentos_parallel_dispatcher_set_executor(
    agentos_parallel_dispatcher_t* dispatcher,
    agentos_tool_execute_fn executor,
    void* user_data);
AGENTOS_API agentos_error_t agentos_parallel_dispatcher_dispatch(
    agentos_parallel_dispatcher_t* dispatcher,
    const agentos_tool_call_t* calls,
    size_t call_count,
    agentos_tool_result_t** out_results,
    size_t* out_result_count);

typedef struct agentos_delegate_config {
    const char* focus_prompt;
    const char** allowed_tools;
    size_t allowed_tool_count;
    int max_depth;
    int max_iterations;
    float token_budget_ratio;
} agentos_delegate_config_t;

typedef struct agentos_delegate_task {
    char* task_id;
    char* description;
    agentos_delegate_config_t config;
    agentos_error_t status;
    char* result;
    size_t result_len;
    int completed;
} agentos_delegate_task_t;

AGENTOS_API agentos_delegate_task_t* agentos_delegate_create(
    const char* task_description,
    const agentos_delegate_config_t* config);
AGENTOS_API void agentos_delegate_destroy(agentos_delegate_task_t* task);
AGENTOS_API agentos_error_t agentos_delegate_assign(
    agentos_delegate_task_t* task,
    agentos_tool_execute_fn executor,
    void* user_data);
AGENTOS_API agentos_error_t agentos_delegate_collect(
    agentos_delegate_task_t* task,
    char** out_result,
    size_t* out_result_len);

#ifdef __cplusplus
}
#endif

#endif

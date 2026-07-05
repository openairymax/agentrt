#ifndef AGENTRT_DELEGATE_H
#define AGENTRT_DELEGATE_H

#include "agentrt.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct agentrt_delegate_config {
    const char *focus_prompt;
    const char **allowed_tools;
    size_t allowed_tool_count;
    int max_depth;
    int max_iterations;
    float token_budget_ratio;
} agentrt_delegate_config_t;

typedef enum agentrt_delegate_state {
    AGENTRT_DELEGATE_IDLE = 0,
    AGENTRT_DELEGATE_RUNNING = 1,
    AGENTRT_DELEGATE_COMPLETED = 2,
    AGENTRT_DELEGATE_CANCELLED = 3,
    AGENTRT_DELEGATE_FAILED = 4
} agentrt_delegate_state_t;

typedef struct agentrt_delegate_task {
    char *task_id;
    char *description;
    agentrt_delegate_config_t config;
    char **owned_tools;
    size_t owned_tool_count;
    agentrt_error_t status;
    agentrt_delegate_state_t state;
    char *result;
    size_t result_len;
} agentrt_delegate_task_t;

typedef agentrt_error_t (*agentrt_tool_execute_fn)(const char *tool_name, const char *arguments,
                                                   size_t arguments_len, char **out_output,
                                                   size_t *out_output_len, void *user_data);

AGENTRT_API agentrt_delegate_task_t *
agentrt_delegate_create(const char *task_description, const agentrt_delegate_config_t *config);

AGENTRT_API void agentrt_delegate_destroy(agentrt_delegate_task_t *task);

AGENTRT_API agentrt_error_t agentrt_delegate_assign(agentrt_delegate_task_t *task,
                                                    agentrt_tool_execute_fn executor,
                                                    void *user_data);

AGENTRT_API agentrt_error_t agentrt_delegate_collect(agentrt_delegate_task_t *task,
                                                     char **out_result, size_t *out_result_len);

AGENTRT_API agentrt_error_t agentrt_delegate_cancel(agentrt_delegate_task_t *task);

AGENTRT_API agentrt_delegate_state_t
agentrt_delegate_get_state(const agentrt_delegate_task_t *task);

#ifdef __cplusplus
}
#endif

#endif

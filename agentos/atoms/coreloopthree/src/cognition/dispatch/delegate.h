#ifndef AGENTOS_DELEGATE_H
#define AGENTOS_DELEGATE_H

#include "agentos.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct agentos_delegate_config {
    const char *focus_prompt;
    const char **allowed_tools;
    size_t allowed_tool_count;
    int max_depth;
    int max_iterations;
    float token_budget_ratio;
} agentos_delegate_config_t;

typedef enum agentos_delegate_state {
    AGENTOS_DELEGATE_IDLE = 0,
    AGENTOS_DELEGATE_RUNNING = 1,
    AGENTOS_DELEGATE_COMPLETED = 2,
    AGENTOS_DELEGATE_CANCELLED = 3,
    AGENTOS_DELEGATE_FAILED = 4
} agentos_delegate_state_t;

typedef struct agentos_delegate_task {
    char *task_id;
    char *description;
    agentos_delegate_config_t config;
    char **owned_tools;
    size_t owned_tool_count;
    agentos_error_t status;
    agentos_delegate_state_t state;
    char *result;
    size_t result_len;
} agentos_delegate_task_t;

typedef agentos_error_t (*agentos_tool_execute_fn)(const char *tool_name, const char *arguments,
                                                   size_t arguments_len, char **out_output,
                                                   size_t *out_output_len, void *user_data);

AGENTOS_API agentos_delegate_task_t *
agentos_delegate_create(const char *task_description, const agentos_delegate_config_t *config);

AGENTOS_API void agentos_delegate_destroy(agentos_delegate_task_t *task);

AGENTOS_API agentos_error_t agentos_delegate_assign(agentos_delegate_task_t *task,
                                                    agentos_tool_execute_fn executor,
                                                    void *user_data);

AGENTOS_API agentos_error_t agentos_delegate_collect(agentos_delegate_task_t *task,
                                                     char **out_result, size_t *out_result_len);

AGENTOS_API agentos_error_t agentos_delegate_cancel(agentos_delegate_task_t *task);

AGENTOS_API agentos_delegate_state_t
agentos_delegate_get_state(const agentos_delegate_task_t *task);

#ifdef __cplusplus
}
#endif

#endif

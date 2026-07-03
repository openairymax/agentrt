#include "parallel_dispatcher.h"

#include "agentos.h"
#include "atomic_compat.h"
#include "delegate.h"
#include "memory_compat.h"
#include "platform.h"

#include <stdlib.h>
#include <string.h>
#include "error.h"
#include "error_compat.h"

#define ATM_RET_ERR(c) \
    do { agentos_error_push_ex((c), __FILE__, __LINE__, __func__, "%s", agentos_error_str(c)); return (c); } while(0)


typedef struct tool_exec_context {
    agentos_tool_execute_fn executor;
    void *user_data;
    const agentos_tool_call_t *call;
    agentos_tool_result_t *result;
    agentos_thread_t thread;
    int started;
    int joined;
} tool_exec_context_t;

struct agentos_parallel_dispatcher {
    int max_parallel;
    agentos_tool_execute_fn executor;
    void *executor_user_data;
    agentos_mutex_t mutex;
    atomic_int cancelled;
};

static int has_path_conflict(const agentos_tool_call_t *a, const agentos_tool_call_t *b)
{
    if (!a->resource_path || !b->resource_path)
        return 0;
    return strcmp(a->resource_path, b->resource_path) == 0;
}

static int must_be_serial(const agentos_tool_call_t *call)
{
    return call->safety_class == AGENTOS_TOOL_INTERACTIVE ||
           call->safety_class == AGENTOS_TOOL_SIDE_EFFECT;
}

static int any_interactive(const agentos_tool_call_t *calls, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (calls[i].safety_class == AGENTOS_TOOL_INTERACTIVE)
            return 1;
    }
    return 0;
}

static void *tool_exec_thread(void *arg)
{
    tool_exec_context_t *ctx = (tool_exec_context_t *)arg;
    if (!ctx || !ctx->executor || !ctx->call || !ctx->result) {
        AGENTOS_ERROR_NULL(AGENTOS_ERR_INVALID_PARAM, "null parameter");
    }

    uint64_t start_ns = agentos_time_ns();

    char *output = NULL;
    size_t output_len = 0;
    agentos_error_t err =
        ctx->executor(ctx->call->tool_name, ctx->call->arguments, ctx->call->arguments_len, &output,
                      &output_len, ctx->user_data);

    uint64_t end_ns = agentos_time_ns();

    ctx->result->error = err;
    ctx->result->output = output;
    ctx->result->output_len = output_len;
    ctx->result->elapsed_ns = end_ns - start_ns;

    return NULL;
}

static agentos_error_t exec_single_tool(agentos_tool_execute_fn executor, void *user_data,
                                        const agentos_tool_call_t *call,
                                        agentos_tool_result_t *result)
{
    uint64_t start_ns = agentos_time_ns();
    char *output = NULL;
    size_t output_len = 0;
    agentos_error_t err = executor(call->tool_name, call->arguments, call->arguments_len, &output,
                                   &output_len, user_data);
    uint64_t end_ns = agentos_time_ns();
    result->error = err;
    result->output = output;
    result->output_len = output_len;
    result->elapsed_ns = end_ns - start_ns;
    return err;
}

agentos_parallel_dispatcher_t *agentos_parallel_dispatcher_create(int max_parallel)
{
    agentos_parallel_dispatcher_t *d =
        (agentos_parallel_dispatcher_t *)AGENTOS_CALLOC(1, sizeof(agentos_parallel_dispatcher_t));
    if (!d) return NULL;
    d->max_parallel = max_parallel > 0 ? max_parallel : 4;
    d->executor = NULL;
    d->executor_user_data = NULL;
    d->cancelled = 0;
    agentos_mutex_init(&d->mutex);
    return d;
}

void agentos_parallel_dispatcher_destroy(agentos_parallel_dispatcher_t *dispatcher)
{
    if (!dispatcher)
        return;
    agentos_mutex_destroy(&dispatcher->mutex);
    AGENTOS_FREE(dispatcher);
}

agentos_error_t agentos_parallel_dispatcher_set_executor(agentos_parallel_dispatcher_t *dispatcher,
                                                         agentos_tool_execute_fn executor,
                                                         void *user_data)
{
    if (!dispatcher || !executor)
        ATM_RET_ERR(AGENTOS_EINVAL);
    agentos_mutex_lock(&dispatcher->mutex);
    dispatcher->executor = executor;
    dispatcher->executor_user_data = user_data;
    agentos_mutex_unlock(&dispatcher->mutex);
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_parallel_dispatcher_dispatch(agentos_parallel_dispatcher_t *dispatcher,
                                                     const agentos_tool_call_t *calls,
                                                     size_t call_count,
                                                     agentos_tool_result_t **out_results,
                                                     size_t *out_result_count)
{
    if (!dispatcher || !calls || call_count == 0 || !out_results || !out_result_count) {
        ATM_RET_ERR(AGENTOS_EINVAL);
    }

    agentos_mutex_lock(&dispatcher->mutex);
    if (!dispatcher->executor) {
        agentos_mutex_unlock(&dispatcher->mutex);
        ATM_RET_ERR(AGENTOS_ENOTINIT);
    }
    dispatcher->cancelled = 0;
    agentos_mutex_unlock(&dispatcher->mutex);

    agentos_tool_result_t *results =
        (agentos_tool_result_t *)AGENTOS_CALLOC(call_count, sizeof(agentos_tool_result_t));
    if (!results)
        ATM_RET_ERR(AGENTOS_ENOMEM);

    for (size_t i = 0; i < call_count; i++) {
        results[i].tool_name = calls[i].tool_name ? AGENTOS_STRDUP(calls[i].tool_name) : NULL;
        results[i].error = AGENTOS_SUCCESS;
        results[i].output = NULL;
        results[i].output_len = 0;
        results[i].elapsed_ns = 0;
    }

    if (any_interactive(calls, call_count)) {
        for (size_t i = 0; i < call_count; i++) {
            if (dispatcher->cancelled) {
                results[i].error = AGENTOS_ECANCELLED;
                continue;
            }
            exec_single_tool(dispatcher->executor, dispatcher->executor_user_data, &calls[i],
                             &results[i]);
        }
        *out_results = results;
        *out_result_count = call_count;
        return AGENTOS_SUCCESS;
    }

    int *group_id;
    SAFE_MALLOC_ARRAY(group_id, call_count, sizeof(int));
    int *serial_flags = (int *)AGENTOS_CALLOC(call_count, sizeof(int));
    if (!group_id || !serial_flags) {
        AGENTOS_FREE(group_id);
        AGENTOS_FREE(serial_flags);
        for (size_t i = 0; i < call_count; i++) {
            AGENTOS_FREE(results[i].tool_name);
        }
        AGENTOS_FREE(results);
        ATM_RET_ERR(AGENTOS_ENOMEM);
    }

    for (size_t i = 0; i < call_count; i++) {
        group_id[i] = -1;
        if (must_be_serial(&calls[i])) {
            serial_flags[i] = 1;
        }
    }

    int group_count = 0;
    for (size_t i = 0; i < call_count; i++) {
        if (group_id[i] >= 0)
            continue;
        group_id[i] = group_count;
        if (serial_flags[i]) {
            group_count++;
            continue;
        }
        for (size_t j = i + 1; j < call_count; j++) {
            if (group_id[j] >= 0)
                continue;
            if (serial_flags[j])
                continue;
            if (calls[i].safety_class == AGENTOS_TOOL_WRITE_SHARED &&
                calls[j].safety_class == AGENTOS_TOOL_WRITE_SHARED &&
                has_path_conflict(&calls[i], &calls[j])) {
                continue;
            }
            int conflict = 0;
            for (size_t k = i; k < j; k++) {
                if (group_id[k] == group_count &&
                    calls[k].safety_class == AGENTOS_TOOL_WRITE_SHARED &&
                    calls[j].safety_class == AGENTOS_TOOL_WRITE_SHARED &&
                    has_path_conflict(&calls[k], &calls[j])) {
                    conflict = 1;
                    break;
                }
            }
            if (!conflict) {
                group_id[j] = group_count;
            }
        }
        group_count++;
    }

    for (int g = 0; g < group_count; g++) {
        if (dispatcher->cancelled) {
            for (size_t i = 0; i < call_count; i++) {
                if (group_id[i] == g) {
                    results[i].error = AGENTOS_ECANCELLED;
                }
            }
            continue;
        }

        size_t group_size = 0;
        for (size_t i = 0; i < call_count; i++) {
            if (group_id[i] == g)
                group_size++;
        }
        if (group_size == 0)
            continue;

        int is_serial = 0;
        for (size_t i = 0; i < call_count; i++) {
            if (group_id[i] == g && serial_flags[i]) {
                is_serial = 1;
                break;
            }
        }

        if (is_serial || group_size == 1) {
            for (size_t i = 0; i < call_count; i++) {
                if (group_id[i] != g)
                    continue;
                if (dispatcher->cancelled) {
                    results[i].error = AGENTOS_ECANCELLED;
                    continue;
                }
                exec_single_tool(dispatcher->executor, dispatcher->executor_user_data, &calls[i],
                                 &results[i]);
            }
        } else {
            size_t parallel_count = group_size;
            if ((int)parallel_count > dispatcher->max_parallel) {
                parallel_count = (size_t)dispatcher->max_parallel;
            }

            tool_exec_context_t *contexts =
                (tool_exec_context_t *)AGENTOS_CALLOC(parallel_count, sizeof(tool_exec_context_t));
            if (!contexts) {
                for (size_t i = 0; i < call_count; i++) {
                    if (group_id[i] != g)
                        continue;
                    results[i].error = AGENTOS_ENOMEM;
                }
                continue;
            }

            size_t ctx_idx = 0;
            for (size_t i = 0; i < call_count && ctx_idx < parallel_count; i++) {
                if (group_id[i] != g)
                    continue;
                if (dispatcher->cancelled) {
                    results[i].error = AGENTOS_ECANCELLED;
                    continue;
                }
                contexts[ctx_idx].executor = dispatcher->executor;
                contexts[ctx_idx].user_data = dispatcher->executor_user_data;
                contexts[ctx_idx].call = &calls[i];
                contexts[ctx_idx].result = &results[i];
                contexts[ctx_idx].started = 0;
                contexts[ctx_idx].joined = 0;
                int ret = agentos_platform_thread_create(&contexts[ctx_idx].thread,
                                                         tool_exec_thread, &contexts[ctx_idx]);
                contexts[ctx_idx].started = (ret == 0);
                if (ret != 0) {
                    results[i].error = AGENTOS_EIO;
                }
                ctx_idx++;
            }

            for (size_t j = 0; j < ctx_idx; j++) {
                if (contexts[j].started && !contexts[j].joined) {
                    agentos_platform_thread_join(contexts[j].thread, NULL);
                    contexts[j].joined = 1;
                }
            }
            AGENTOS_FREE(contexts);
        }
    }

    AGENTOS_FREE(group_id);
    AGENTOS_FREE(serial_flags);

    *out_results = results;
    *out_result_count = call_count;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_parallel_dispatcher_cancel(agentos_parallel_dispatcher_t *dispatcher)
{
    if (!dispatcher)
        ATM_RET_ERR(AGENTOS_EINVAL);
    agentos_mutex_lock(&dispatcher->mutex);
    dispatcher->cancelled = 1;
    dispatcher->executor = NULL;
    dispatcher->executor_user_data = NULL;
    agentos_mutex_unlock(&dispatcher->mutex);
    return AGENTOS_SUCCESS;
}

bool agentos_parallel_dispatcher_is_cancelled(const agentos_parallel_dispatcher_t *dispatcher)
{
    if (!dispatcher)
        return false;
    return dispatcher->cancelled != 0;
}

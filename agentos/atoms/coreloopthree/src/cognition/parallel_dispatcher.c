#include "parallel_dispatcher.h"
#include "agentos.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

typedef struct tool_exec_context {
    agentos_tool_execute_fn executor;
    void* user_data;
    const agentos_tool_call_t* call;
    agentos_tool_result_t* result;
    pthread_t thread;
    int started;
    int joined;
} tool_exec_context_t;

struct agentos_parallel_dispatcher {
    int max_parallel;
    agentos_tool_execute_fn executor;
    void* executor_user_data;
    pthread_mutex_t mutex;
};

static int has_path_conflict(const agentos_tool_call_t* a, const agentos_tool_call_t* b) {
    if (!a->resource_path || !b->resource_path) return 0;
    return strcmp(a->resource_path, b->resource_path) == 0;
}

static int must_be_serial(const agentos_tool_call_t* call) {
    return call->safety_class == AGENTOS_TOOL_INTERACTIVE ||
           call->safety_class == AGENTOS_TOOL_SIDE_EFFECT;
}

static int any_interactive(const agentos_tool_call_t* calls, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (calls[i].safety_class == AGENTOS_TOOL_INTERACTIVE) return 1;
    }
    return 0;
}

static void* tool_exec_thread(void* arg) {
    tool_exec_context_t* ctx = (tool_exec_context_t*)arg;
    if (!ctx || !ctx->executor || !ctx->call || !ctx->result) return NULL;

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    char* output = NULL;
    size_t output_len = 0;
    agentos_error_t err = ctx->executor(
        ctx->call->tool_name,
        ctx->call->arguments,
        ctx->call->arguments_len,
        &output,
        &output_len,
        ctx->user_data);

    clock_gettime(CLOCK_MONOTONIC, &end);

    ctx->result->error = err;
    ctx->result->output = output;
    ctx->result->output_len = output_len;
    ctx->result->elapsed_ns = (uint64_t)(end.tv_sec - start.tv_sec) * 1000000000ULL +
                               (uint64_t)(end.tv_nsec - start.tv_nsec);

    return NULL;
}

agentos_parallel_dispatcher_t* agentos_parallel_dispatcher_create(int max_parallel) {
    agentos_parallel_dispatcher_t* d = (agentos_parallel_dispatcher_t*)calloc(
        1, sizeof(agentos_parallel_dispatcher_t));
    if (!d) return NULL;
    d->max_parallel = max_parallel > 0 ? max_parallel : 4;
    d->executor = NULL;
    d->executor_user_data = NULL;
    pthread_mutex_init(&d->mutex, NULL);
    return d;
}

void agentos_parallel_dispatcher_destroy(agentos_parallel_dispatcher_t* dispatcher) {
    if (!dispatcher) return;
    pthread_mutex_destroy(&dispatcher->mutex);
    free(dispatcher);
}

agentos_error_t agentos_parallel_dispatcher_set_executor(
    agentos_parallel_dispatcher_t* dispatcher,
    agentos_tool_execute_fn executor,
    void* user_data) {
    if (!dispatcher || !executor) return AGENTOS_EINVAL;
    pthread_mutex_lock(&dispatcher->mutex);
    dispatcher->executor = executor;
    dispatcher->executor_user_data = user_data;
    pthread_mutex_unlock(&dispatcher->mutex);
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_parallel_dispatcher_dispatch(
    agentos_parallel_dispatcher_t* dispatcher,
    const agentos_tool_call_t* calls,
    size_t call_count,
    agentos_tool_result_t** out_results,
    size_t* out_result_count) {
    if (!dispatcher || !calls || call_count == 0 || !out_results || !out_result_count) {
        return AGENTOS_EINVAL;
    }
    if (!dispatcher->executor) return AGENTOS_ENOTINIT;

    agentos_tool_result_t* results = (agentos_tool_result_t*)calloc(
        call_count, sizeof(agentos_tool_result_t));
    if (!results) return AGENTOS_ENOMEM;

    for (size_t i = 0; i < call_count; i++) {
        results[i].tool_name = calls[i].tool_name ? strdup(calls[i].tool_name) : NULL;
        results[i].error = AGENTOS_SUCCESS;
        results[i].output = NULL;
        results[i].output_len = 0;
        results[i].elapsed_ns = 0;
    }

    if (any_interactive(calls, call_count)) {
        for (size_t i = 0; i < call_count; i++) {
            struct timespec start, end;
            clock_gettime(CLOCK_MONOTONIC, &start);
            char* output = NULL;
            size_t output_len = 0;
            agentos_error_t err = dispatcher->executor(
                calls[i].tool_name, calls[i].arguments, calls[i].arguments_len,
                &output, &output_len, dispatcher->executor_user_data);
            clock_gettime(CLOCK_MONOTONIC, &end);
            results[i].error = err;
            results[i].output = output;
            results[i].output_len = output_len;
            results[i].elapsed_ns = (uint64_t)(end.tv_sec - start.tv_sec) * 1000000000ULL +
                                     (uint64_t)(end.tv_nsec - start.tv_nsec);
        }
        *out_results = results;
        *out_result_count = call_count;
        return AGENTOS_SUCCESS;
    }

    int* group_id = (int*)calloc(call_count, sizeof(int));
    int group_count = 0;
    int* serial_flags = (int*)calloc(call_count, sizeof(int));

    for (size_t i = 0; i < call_count; i++) {
        if (must_be_serial(&calls[i])) {
            serial_flags[i] = 1;
        }
    }

    for (size_t i = 0; i < call_count; i++) {
        if (group_id[i] >= 0) continue;
        group_id[i] = group_count;
        if (serial_flags[i]) {
            group_count++;
            continue;
        }
        for (size_t j = i + 1; j < call_count; j++) {
            if (group_id[j] >= 0) continue;
            if (serial_flags[j]) continue;
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
        size_t group_size = 0;
        for (size_t i = 0; i < call_count; i++) {
            if (group_id[i] == g) group_size++;
        }
        if (group_size == 0) continue;

        int is_serial = 0;
        for (size_t i = 0; i < call_count; i++) {
            if (group_id[i] == g && serial_flags[i]) { is_serial = 1; break; }
        }

        if (is_serial || group_size == 1) {
            for (size_t i = 0; i < call_count; i++) {
                if (group_id[i] != g) continue;
                struct timespec start, end;
                clock_gettime(CLOCK_MONOTONIC, &start);
                char* output = NULL;
                size_t output_len = 0;
                agentos_error_t err = dispatcher->executor(
                    calls[i].tool_name, calls[i].arguments, calls[i].arguments_len,
                    &output, &output_len, dispatcher->executor_user_data);
                clock_gettime(CLOCK_MONOTONIC, &end);
                results[i].error = err;
                results[i].output = output;
                results[i].output_len = output_len;
                results[i].elapsed_ns = (uint64_t)(end.tv_sec - start.tv_sec) * 1000000000ULL +
                                         (uint64_t)(end.tv_nsec - start.tv_nsec);
            }
        } else {
            size_t parallel_count = group_size;
            if ((int)parallel_count > dispatcher->max_parallel) {
                parallel_count = (size_t)dispatcher->max_parallel;
            }

            tool_exec_context_t* contexts = (tool_exec_context_t*)calloc(
                parallel_count, sizeof(tool_exec_context_t));
            if (!contexts) {
                for (size_t i = 0; i < call_count; i++) {
                    if (group_id[i] != g) continue;
                    results[i].error = AGENTOS_ENOMEM;
                }
                continue;
            }

            size_t ctx_idx = 0;
            for (size_t i = 0; i < call_count && ctx_idx < parallel_count; i++) {
                if (group_id[i] != g) continue;
                contexts[ctx_idx].executor = dispatcher->executor;
                contexts[ctx_idx].user_data = dispatcher->executor_user_data;
                contexts[ctx_idx].call = &calls[i];
                contexts[ctx_idx].result = &results[i];
                contexts[ctx_idx].started = 0;
                contexts[ctx_idx].joined = 0;
                int ret = pthread_create(&contexts[ctx_idx].thread, NULL, tool_exec_thread, &contexts[ctx_idx]);
                contexts[ctx_idx].started = (ret == 0);
                if (ret != 0) {
                    results[i].error = AGENTOS_EIO;
                }
                ctx_idx++;
            }

            for (size_t j = 0; j < ctx_idx; j++) {
                if (contexts[j].started && !contexts[j].joined) {
                    pthread_join(contexts[j].thread, NULL);
                    contexts[j].joined = 1;
                }
            }
            free(contexts);
        }
    }

    free(group_id);
    free(serial_flags);

    *out_results = results;
    *out_result_count = call_count;
    return AGENTOS_SUCCESS;
}

static int g_delegate_depth = 0;

agentos_delegate_task_t* agentos_delegate_create(
    const char* task_description,
    const agentos_delegate_config_t* config) {
    if (!task_description) return NULL;
    if (g_delegate_depth >= 2) return NULL;

    agentos_delegate_task_t* task = (agentos_delegate_task_t*)calloc(
        1, sizeof(agentos_delegate_task_t));
    if (!task) return NULL;

    static uint64_t task_counter = 0;
    task->task_id = (char*)malloc(64);
    if (task->task_id) {
        snprintf(task->task_id, 64, "delegate_%lu", (unsigned long)(task_counter++));
    }
    task->description = strdup(task_description);
    task->status = AGENTOS_SUCCESS;
    task->result = NULL;
    task->result_len = 0;
    task->completed = 0;

    if (config) {
        task->config = *config;
        task->config.focus_prompt = config->focus_prompt ? strdup(config->focus_prompt) : NULL;
    } else {
        memset(&task->config, 0, sizeof(agentos_delegate_config_t));
        task->config.max_depth = 2;
        task->config.max_iterations = 10;
        task->config.token_budget_ratio = 0.3f;
    }

    return task;
}

void agentos_delegate_destroy(agentos_delegate_task_t* task) {
    if (!task) return;
    if (task->task_id) free(task->task_id);
    if (task->description) free(task->description);
    if (task->result) free(task->result);
    if (task->config.focus_prompt) free((void*)task->config.focus_prompt);
    free(task);
}

agentos_error_t agentos_delegate_assign(
    agentos_delegate_task_t* task,
    agentos_tool_execute_fn executor,
    void* user_data) {
    if (!task || !executor) return AGENTOS_EINVAL;
    if (task->completed) return AGENTOS_EBUSY;
    if (g_delegate_depth >= task->config.max_depth) return AGENTOS_EPERM;

    g_delegate_depth++;

    char* output = NULL;
    size_t output_len = 0;
    agentos_error_t err = executor(
        "delegate_task",
        task->description,
        task->description ? strlen(task->description) : 0,
        &output,
        &output_len,
        user_data);

    g_delegate_depth--;

    task->status = err;
    task->result = output;
    task->result_len = output_len;
    task->completed = 1;

    return err;
}

agentos_error_t agentos_delegate_collect(
    agentos_delegate_task_t* task,
    char** out_result,
    size_t* out_result_len) {
    if (!task || !out_result) return AGENTOS_EINVAL;
    if (!task->completed) return AGENTOS_EBUSY;

    if (task->result && task->result_len > 0) {
        *out_result = task->result;
        if (out_result_len) *out_result_len = task->result_len;
        task->result = NULL;
        task->result_len = 0;
    } else {
        *out_result = NULL;
        if (out_result_len) *out_result_len = 0;
    }

    return task->status;
}

#ifndef AGENTOS_PARALLEL_DISPATCHER_H
#define AGENTOS_PARALLEL_DISPATCHER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "thread_pool.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct parallel_dispatcher_s parallel_dispatcher_t;

typedef struct {
    char* tool_id;
    char* params_json;
    void* user_data;
} parallel_task_t;

typedef struct {
    int success;
    char* output;
    char* error;
    uint64_t duration_ms;
    size_t task_index;
} parallel_result_t;

typedef enum {
    PARALLEL_WAIT_ALL = 0,
    PARALLEL_WAIT_ANY = 1,
    PARALLEL_WAIT_MAJORITY = 2
} parallel_wait_mode_t;

typedef struct {
    parallel_wait_mode_t wait_mode;
    uint32_t timeout_ms;
    uint32_t max_concurrency;
    bool cancel_on_error;
} parallel_dispatch_config_t;

typedef void (*parallel_complete_cb_t)(
    size_t index, const parallel_result_t* result, void* user_data);

parallel_dispatcher_t* parallel_dispatcher_create(
    thread_pool_t* pool, const parallel_dispatch_config_t* config);

void parallel_dispatcher_destroy(parallel_dispatcher_t* dispatcher);

int parallel_dispatcher_execute(
    parallel_dispatcher_t* dispatcher,
    const parallel_task_t* tasks,
    size_t task_count,
    parallel_result_t** results,
    size_t* result_count);

int parallel_dispatcher_execute_async(
    parallel_dispatcher_t* dispatcher,
    const parallel_task_t* tasks,
    size_t task_count,
    parallel_complete_cb_t on_complete,
    void* user_data);

void parallel_result_free(parallel_result_t* results, size_t count);

parallel_task_t parallel_task_create(const char* tool_id, const char* params_json);

void parallel_task_free(parallel_task_t* task);

#ifdef __cplusplus
}
#endif

#endif

#include "parallel_dispatcher.h"
#include "ipc_service_bus.h"
#include "platform.h"
#include "include/memory_compat.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

struct parallel_dispatcher_s {
    thread_pool_t* pool;
    parallel_dispatch_config_t config;
    ipc_service_bus_t bus;
};

typedef struct dispatch_context_s dispatch_context_t;

typedef struct {
    agentos_platform_mutex_t lock;
    agentos_cond_t cond;
    parallel_result_t* results;
    dispatch_context_t* contexts;
    size_t task_count;
    volatile int completed_count;
    volatile int error_count;
    volatile int cancel_flag;
    parallel_complete_cb_t on_complete;
    void* cb_user_data;
    parallel_dispatcher_t* dispatcher;
} dispatch_session_t;

struct dispatch_context_s {
    parallel_dispatcher_t* dispatcher;
    size_t task_index;
    parallel_task_t task;
    parallel_result_t* result_slot;
    dispatch_session_t* session;
};

static uint64_t time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static char* json_extract_field(const char* json, const char* key) {
    if (!json || !key) return NULL;
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char* pos = strstr(json, search);
    if (!pos) return NULL;
    pos += strlen(search);
    while (*pos && (*pos == ' ' || *pos == ':' || *pos == '\t')) pos++;
    if (*pos == '"') {
        pos++;
        const char* end = strchr(pos, '"');
        if (!end) return NULL;
        size_t len = (size_t)(end - pos);
        char* val = (char*)AGENTOS_MALLOC(len + 1);
        if (val) { memcpy(val, pos, len); val[len] = '\0'; }
        return val;
    }
    return NULL;
}

static void session_release(dispatch_session_t* session) {
    if (!session) return;
    bool should_free = false;
    agentos_platform_mutex_lock(&session->lock);
    if ((size_t)session->completed_count >= session->task_count) {
        should_free = true;
    }
    agentos_platform_mutex_unlock(&session->lock);

    if (should_free) {
        agentos_platform_mutex_destroy(&session->lock);
        agentos_cond_destroy(&session->cond);
        if (session->contexts) AGENTOS_FREE(session->contexts);
        AGENTOS_FREE(session);
    }
}

static void dispatch_worker(void* arg) {
    dispatch_context_t* ctx = (dispatch_context_t*)arg;
    if (!ctx || !ctx->dispatcher || !ctx->session) return;

    dispatch_session_t* session = ctx->session;

    agentos_platform_mutex_lock(&session->lock);
    if (session->cancel_flag) {
        ctx->result_slot->success = 0;
        ctx->result_slot->error = strdup("cancelled");
        ctx->result_slot->task_index = ctx->task_index;
        session->completed_count++;
        session->error_count++;
        if (session->on_complete) {
            session->on_complete(ctx->task_index, ctx->result_slot, session->cb_user_data);
        }
        agentos_cond_signal(&session->cond);
        agentos_platform_mutex_unlock(&session->lock);
        session_release(session);
        return;
    }
    agentos_platform_mutex_unlock(&session->lock);

    uint64_t start = time_ms();
    ctx->result_slot->task_index = ctx->task_index;
    ctx->result_slot->success = 0;
    ctx->result_slot->output = NULL;
    ctx->result_slot->error = NULL;

    ipc_service_bus_t bus = ctx->dispatcher->bus;
    bool ipc_ok = false;

    if (bus) {
        char request_payload[2048];
        snprintf(request_payload, sizeof(request_payload),
            "{\"jsonrpc\":\"2.0\",\"method\":\"execute\",\"params\":{\"tool_id\":\"%s\",\"params\":%s},\"id\":%zu}",
            ctx->task.tool_id ? ctx->task.tool_id : "",
            ctx->task.params_json ? ctx->task.params_json : "{}",
            ctx->task_index + 1);

        ipc_bus_message_t request;
        memset(&request, 0, sizeof(request));
        request.header.msg_type = IPC_BUS_MSG_REQUEST;
        request.header.protocol = IPC_BUS_PROTO_MCP;
        snprintf(request.header.target, sizeof(request.header.target), "tool_d");
        snprintf(request.header.source, sizeof(request.header.source), "parallel_d");
        request.payload = request_payload;
        request.payload_size = strlen(request_payload) + 1;

        ipc_bus_message_t response;
        memset(&response, 0, sizeof(response));

        agentos_error_t err = ipc_service_bus_request(
            bus, "tool_d", &request, &response,
            ctx->dispatcher->config.timeout_ms > 0 ? ctx->dispatcher->config.timeout_ms : 30000);

        if (err == AGENTOS_SUCCESS && response.payload && response.payload_size > 0) {
            const char* resp_str = (const char*)response.payload;
            char* result_str = json_extract_field(resp_str, "result");
            char* error_str = json_extract_field(resp_str, "error");
            if (result_str) {
                ctx->result_slot->success = 1;
                ctx->result_slot->output = result_str;
                ipc_ok = true;
            }
            if (error_str) {
                ctx->result_slot->error = error_str;
            }
            free(response.payload);
        } else {
            char errbuf[128];
            snprintf(errbuf, sizeof(errbuf), "IPC request failed: error=%d", err);
            ctx->result_slot->error = strdup(errbuf);
        }
    } else {
        ctx->result_slot->success = 0;
        ctx->result_slot->error = strdup("no IPC bus available");
    }

    ctx->result_slot->duration_ms = time_ms() - start;

    agentos_platform_mutex_lock(&session->lock);
    session->completed_count++;
    if (!ctx->result_slot->success) session->error_count++;

    if (ctx->dispatcher->config.cancel_on_error && session->error_count > 0) {
        session->cancel_flag = 1;
    }

    if (session->on_complete) {
        session->on_complete(ctx->task_index, ctx->result_slot, session->cb_user_data);
    }

    agentos_cond_signal(&session->cond);
    agentos_platform_mutex_unlock(&session->lock);

    session_release(session);
}

parallel_dispatcher_t* parallel_dispatcher_create(
    thread_pool_t* pool, const parallel_dispatch_config_t* config) {
    if (!pool) return NULL;

    parallel_dispatcher_t* disp = (parallel_dispatcher_t*)AGENTOS_CALLOC(1, sizeof(parallel_dispatcher_t));
    if (!disp) return NULL;

    disp->pool = pool;
    disp->bus = NULL;

    if (config) {
        disp->config = *config;
    } else {
        disp->config.wait_mode = PARALLEL_WAIT_ALL;
        disp->config.timeout_ms = 30000;
        disp->config.max_concurrency = 4;
        disp->config.cancel_on_error = false;
    }

    if (disp->config.max_concurrency == 0) disp->config.max_concurrency = 4;
    if (disp->config.timeout_ms == 0) disp->config.timeout_ms = 30000;

    return disp;
}

void parallel_dispatcher_destroy(parallel_dispatcher_t* dispatcher) {
    if (!dispatcher) return;
    AGENTOS_FREE(dispatcher);
}

static bool should_return(parallel_wait_mode_t mode, int completed, size_t total) {
    switch (mode) {
        case PARALLEL_WAIT_ALL:
            return (size_t)completed >= total;
        case PARALLEL_WAIT_ANY:
            return completed >= 1;
        case PARALLEL_WAIT_MAJORITY:
            return (size_t)completed >= (total / 2 + 1);
        default:
            return (size_t)completed >= total;
    }
}

static dispatch_session_t* session_create(
    parallel_dispatcher_t* dispatcher,
    const parallel_task_t* tasks,
    size_t task_count,
    parallel_result_t* results,
    parallel_complete_cb_t on_complete,
    void* user_data) {
    dispatch_session_t* session = (dispatch_session_t*)AGENTOS_CALLOC(1, sizeof(dispatch_session_t));
    if (!session) return NULL;

    agentos_platform_mutex_init(&session->lock);
    agentos_cond_init(&session->cond);
    session->results = results;
    session->task_count = task_count;
    session->completed_count = 0;
    session->error_count = 0;
    session->cancel_flag = 0;
    session->on_complete = on_complete;
    session->cb_user_data = user_data;
    session->dispatcher = dispatcher;

    session->contexts = (dispatch_context_t*)AGENTOS_CALLOC(task_count, sizeof(dispatch_context_t));
    if (!session->contexts) {
        agentos_platform_mutex_destroy(&session->lock);
        agentos_cond_destroy(&session->cond);
        AGENTOS_FREE(session);
        return NULL;
    }

    for (size_t i = 0; i < task_count; i++) {
        session->contexts[i].dispatcher = dispatcher;
        session->contexts[i].task_index = i;
        session->contexts[i].task = tasks[i];
        session->contexts[i].result_slot = &results[i];
        session->contexts[i].session = session;
    }

    return session;
}

int parallel_dispatcher_execute(
    parallel_dispatcher_t* dispatcher,
    const parallel_task_t* tasks,
    size_t task_count,
    parallel_result_t** results,
    size_t* result_count) {
    if (!dispatcher || !tasks || task_count == 0 || !results) return -1;

    *results = (parallel_result_t*)AGENTOS_CALLOC(task_count, sizeof(parallel_result_t));
    if (!*results) return -3;
    if (result_count) *result_count = task_count;

    dispatch_session_t* session = session_create(dispatcher, tasks, task_count, *results, NULL, NULL);
    if (!session) {
        AGENTOS_FREE(*results);
        *results = NULL;
        return -3;
    }

    for (size_t i = 0; i < task_count; i++) {
        agentos_platform_mutex_lock(&session->lock);
        if (session->cancel_flag) {
            agentos_platform_mutex_unlock(&session->lock);
            break;
        }

        while (thread_pool_active_count(dispatcher->pool) >= dispatcher->config.max_concurrency) {
            if (should_return(dispatcher->config.wait_mode, session->completed_count, task_count)) {
                agentos_platform_mutex_unlock(&session->lock);
                goto wait_done;
            }
            agentos_cond_timedwait(&session->cond, &session->lock, 100);
        }
        agentos_platform_mutex_unlock(&session->lock);

        int rc = thread_pool_submit(dispatcher->pool, dispatch_worker, &session->contexts[i]);
        if (rc != 0) {
            session->contexts[i].result_slot->success = 0;
            session->contexts[i].result_slot->error = strdup("submit failed");
            session->contexts[i].result_slot->task_index = i;
            agentos_platform_mutex_lock(&session->lock);
            session->completed_count++;
            session->error_count++;
            agentos_cond_signal(&session->cond);
            agentos_platform_mutex_unlock(&session->lock);
        }
    }

wait_done:
    {
        uint64_t deadline = time_ms() + dispatcher->config.timeout_ms;
        while (1) {
            agentos_platform_mutex_lock(&session->lock);
            bool done = should_return(dispatcher->config.wait_mode,
                session->completed_count, task_count);
            agentos_platform_mutex_unlock(&session->lock);

            if (done) break;

            if (time_ms() >= deadline) {
                session->cancel_flag = 1;
                break;
            }

            agentos_platform_mutex_lock(&session->lock);
            agentos_cond_timedwait(&session->cond, &session->lock, 50);
            agentos_platform_mutex_unlock(&session->lock);
        }
    }

    int errors = session->error_count;
    agentos_platform_mutex_destroy(&session->lock);
    agentos_cond_destroy(&session->cond);
    if (session->contexts) AGENTOS_FREE(session->contexts);
    AGENTOS_FREE(session);

    return (errors > 0) ? 1 : 0;
}

int parallel_dispatcher_execute_async(
    parallel_dispatcher_t* dispatcher,
    const parallel_task_t* tasks,
    size_t task_count,
    parallel_complete_cb_t on_complete,
    void* user_data) {
    if (!dispatcher || !tasks || task_count == 0) return -1;

    parallel_result_t* results = (parallel_result_t*)AGENTOS_CALLOC(task_count, sizeof(parallel_result_t));
    if (!results) return -3;

    dispatch_session_t* session = session_create(dispatcher, tasks, task_count, results, on_complete, user_data);
    if (!session) {
        AGENTOS_FREE(results);
        return -3;
    }

    int submitted = 0;
    for (size_t i = 0; i < task_count; i++) {
        int rc = thread_pool_submit(dispatcher->pool, dispatch_worker, &session->contexts[i]);
        if (rc == 0) {
            submitted++;
        } else {
            results[i].success = 0;
            results[i].error = strdup("submit failed");
            results[i].task_index = i;
            if (on_complete) on_complete(i, &results[i], user_data);
            agentos_platform_mutex_lock(&session->lock);
            session->completed_count++;
            session->error_count++;
            agentos_platform_mutex_unlock(&session->lock);
        }
    }

    if (submitted == 0) {
        agentos_platform_mutex_destroy(&session->lock);
        agentos_cond_destroy(&session->cond);
        if (session->contexts) AGENTOS_FREE(session->contexts);
        AGENTOS_FREE(session);
        AGENTOS_FREE(results);
        return -4;
    }

    return 0;
}

void parallel_result_free(parallel_result_t* results, size_t count) {
    if (!results) return;
    for (size_t i = 0; i < count; i++) {
        if (results[i].output) AGENTOS_FREE(results[i].output);
        if (results[i].error) AGENTOS_FREE(results[i].error);
    }
    AGENTOS_FREE(results);
}

parallel_task_t parallel_task_create(const char* tool_id, const char* params_json) {
    parallel_task_t task;
    memset(&task, 0, sizeof(task));
    task.tool_id = tool_id ? strdup(tool_id) : NULL;
    task.params_json = params_json ? strdup(params_json) : NULL;
    task.user_data = NULL;
    return task;
}

void parallel_task_free(parallel_task_t* task) {
    if (!task) return;
    if (task->tool_id) { AGENTOS_FREE(task->tool_id); task->tool_id = NULL; }
    if (task->params_json) { AGENTOS_FREE(task->params_json); task->params_json = NULL; }
}

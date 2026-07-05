#include "daemon_task_dispatcher.h"

#include "atomic_compat.h"
#include "ipc_service_bus.h"
#include "memory_compat.h"
#include "platform.h"
#include "svc_logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "error.h"

struct parallel_dispatcher_s {
    thread_pool_t *pool;
    parallel_dispatch_config_t config;
    ipc_service_bus_t bus;
};

typedef struct dispatch_context_s dispatch_context_t;

typedef struct {
    agentrt_mutex_t lock;
    agentrt_cond_t cond;
    parallel_result_t *results;
    dispatch_context_t *contexts;
    size_t task_count;
    atomic_int completed_count;
    atomic_int error_count;
    atomic_int cancel_flag;
    parallel_complete_cb_t on_complete;
    void *cb_user_data;
    parallel_dispatcher_t *dispatcher;
} dispatch_session_t;

struct dispatch_context_s {
    parallel_dispatcher_t *dispatcher;
    size_t task_index;
    parallel_task_t task;
    parallel_result_t *result_slot;
    dispatch_session_t *session;
};

static uint64_t time_ms(void)
{
    return agentrt_time_ms();
}

static char *json_extract_field(const char *json, const char *key)
{
    if (!json || !key) {
        SVC_LOG_ERROR("json_extract_field: null parameter json=%p key=%p", (void *)json, (void *)key);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_UNKNOWN, "validation failed");
    }
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *pos = strstr(json, search);
    if (!pos) {
        SVC_LOG_WARN("json_extract_field: key '%s' not found in JSON", key);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_UNKNOWN, "validation failed");
    }
    pos += strlen(search);
    while (*pos && (*pos == ' ' || *pos == ':' || *pos == '\t'))
        pos++;
    if (*pos == '"') {
        pos++;
        const char *end = strchr(pos, '"');
        if (!end) {
            SVC_LOG_ERROR("json_extract_field: unterminated string for key '%s'", key);
            AGENTRT_ERROR_NULL(AGENTRT_ERR_UNKNOWN, "validation failed");
        }
        size_t len = (size_t)(end - pos);
        char *val = (char *)AGENTRT_MALLOC(len + 1);
        if (val) {
            __builtin_memcpy(val, pos, len);
            val[len] = '\0';
        }
        return val;
    }
    SVC_LOG_WARN("json_extract_field: value is not a string for key '%s'", key);
    AGENTRT_ERROR_NULL(AGENTRT_ERR_UNKNOWN, "operation failed");
}

static void session_release(dispatch_session_t *session)
{
    if (!session)
        return;
    bool should_free = false;
    agentrt_mutex_lock(&session->lock);
    if ((size_t)session->completed_count >= session->task_count) {
        should_free = true;
    }
    agentrt_mutex_unlock(&session->lock);

    if (should_free) {
        agentrt_mutex_destroy(&session->lock);
        agentrt_cond_destroy(&session->cond);
        if (session->contexts)
            AGENTRT_FREE(session->contexts);
        if (session->results) {
            for (size_t i = 0; i < session->task_count; i++) {
                if (session->results[i].output)
                    AGENTRT_FREE(session->results[i].output);
                if (session->results[i].error)
                    AGENTRT_FREE(session->results[i].error);
            }
            AGENTRT_FREE(session->results);
        }
        AGENTRT_FREE(session);
    }
}

static void dispatch_worker(void *arg)
{
    dispatch_context_t *ctx = (dispatch_context_t *)arg;
    if (!ctx || !ctx->dispatcher || !ctx->session) {
        SVC_LOG_ERROR("dispatch_worker: null context ctx=%p dispatcher=%p session=%p",
                      (void *)ctx, ctx ? (void *)ctx->dispatcher : NULL, ctx ? (void *)ctx->session : NULL);
        return;
    }

    dispatch_session_t *session = ctx->session;

    agentrt_mutex_lock(&session->lock);
    if (session->cancel_flag) {
        ctx->result_slot->success = 0;
        ctx->result_slot->error = AGENTRT_STRDUP("cancelled");
        ctx->result_slot->task_index = ctx->task_index;
        session->completed_count++;
        session->error_count++;
        if (session->on_complete) {
            session->on_complete(ctx->task_index, ctx->result_slot, session->cb_user_data);
        }
        agentrt_cond_signal(&session->cond);
        agentrt_mutex_unlock(&session->lock);
        session_release(session);
        return;
    }
    agentrt_mutex_unlock(&session->lock);

    uint64_t start = time_ms();
    ctx->result_slot->task_index = ctx->task_index;
    ctx->result_slot->success = 0;
    ctx->result_slot->output = NULL;
    ctx->result_slot->error = NULL;

    ipc_service_bus_t bus = ctx->dispatcher->bus;
    bool ipc_ok __attribute__((unused)) = false;

    if (bus) {
        char request_payload[2048];
        snprintf(request_payload, sizeof(request_payload),
                 "{\"jsonrpc\":\"2.0\",\"method\":\"execute\",\"params\":{\"tool_id\":\"%s\","
                 "\"params\":%s},\"id\":%zu}",
                 ctx->task.tool_id ? ctx->task.tool_id : "",
                 ctx->task.params_json ? ctx->task.params_json : "{}", ctx->task_index + 1);

        ipc_bus_message_t request;
        __builtin_memset(&request, 0, sizeof(request));
        request.header.msg_type = IPC_BUS_MSG_REQUEST;
        request.header.protocol = IPC_BUS_PROTO_MCP;
        snprintf(request.header.target, sizeof(request.header.target), "tool_d");
        snprintf(request.header.source, sizeof(request.header.source), "parallel_d");
        request.payload = request_payload;
        request.payload_size = strlen(request_payload) + 1;

        ipc_bus_message_t response;
        __builtin_memset(&response, 0, sizeof(response));

        agentrt_error_t err = ipc_service_bus_request(
            bus, "tool_d", &request, &response,
            ctx->dispatcher->config.timeout_ms > 0 ? ctx->dispatcher->config.timeout_ms : 30000);

        if (err == AGENTRT_SUCCESS && response.payload && response.payload_size > 0) {
            const char *resp_str = (const char *)response.payload;
            char *result_str = json_extract_field(resp_str, "result");
            char *error_str = json_extract_field(resp_str, "error");
            if (result_str) {
                ctx->result_slot->success = 1;
                ctx->result_slot->output = result_str;
                ipc_ok = true;
            }
            if (error_str) {
                ctx->result_slot->error = error_str;
            }
            AGENTRT_FREE(response.payload);
        } else {
            char errbuf[128];
            snprintf(errbuf, sizeof(errbuf), "IPC request failed: error=%d", err);
            SVC_LOG_ERROR("dispatch_worker: IPC request failed for tool '%s' error=%d",
                          ctx->task.tool_id ? ctx->task.tool_id : "null", err);
            ctx->result_slot->error = AGENTRT_STRDUP(errbuf);
        }
    } else {
        SVC_LOG_WARN("dispatch_worker: no IPC bus available for tool '%s'",
                     ctx->task.tool_id ? ctx->task.tool_id : "null");
        ctx->result_slot->success = 0;
        ctx->result_slot->error = AGENTRT_STRDUP("no IPC bus available");
    }

    ctx->result_slot->duration_ms = time_ms() - start;

    agentrt_mutex_lock(&session->lock);
    session->completed_count++;
    if (!ctx->result_slot->success)
        session->error_count++;

    if (ctx->dispatcher->config.cancel_on_error && session->error_count > 0) {
        session->cancel_flag = 1;
    }

    if (session->on_complete) {
        session->on_complete(ctx->task_index, ctx->result_slot, session->cb_user_data);
    }

    agentrt_cond_signal(&session->cond);
    agentrt_mutex_unlock(&session->lock);

    session_release(session);
}

parallel_dispatcher_t *parallel_dispatcher_create(thread_pool_t *pool,
                                                  const parallel_dispatch_config_t *config)
{
    if (!pool) {
        SVC_LOG_ERROR("parallel_dispatcher_create: null pool parameter");
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    parallel_dispatcher_t *disp =
        (parallel_dispatcher_t *)AGENTRT_CALLOC(1, sizeof(parallel_dispatcher_t));
    if (!disp) {
        SVC_LOG_ERROR("parallel_dispatcher_create: memory allocation failed");
        AGENTRT_ERROR_NULL(AGENTRT_ERR_UNKNOWN, "validation failed");
    }

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

    if (disp->config.max_concurrency == 0)
        disp->config.max_concurrency = 4;
    if (disp->config.timeout_ms == 0)
        disp->config.timeout_ms = 30000;

    return disp;
}

void parallel_dispatcher_destroy(parallel_dispatcher_t *dispatcher)
{
    if (!dispatcher)
        return;
    AGENTRT_FREE(dispatcher);
}

static bool should_return(parallel_wait_mode_t mode, int completed, size_t total)
{
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

static dispatch_session_t *session_create(parallel_dispatcher_t *dispatcher,
                                          const parallel_task_t *tasks, size_t task_count,
                                          parallel_result_t *results,
                                          parallel_complete_cb_t on_complete, void *user_data)
{
    dispatch_session_t *session =
        (dispatch_session_t *)AGENTRT_CALLOC(1, sizeof(dispatch_session_t));
    if (!session) {
        SVC_LOG_ERROR("session_create: memory allocation failed for session");
        AGENTRT_ERROR_NULL(AGENTRT_ERR_UNKNOWN, "validation failed");
    }

    agentrt_mutex_init(&session->lock);
    agentrt_cond_init(&session->cond);
    session->results = results;
    session->task_count = task_count;
    session->completed_count = 0;
    session->error_count = 0;
    session->cancel_flag = 0;
    session->on_complete = on_complete;
    session->cb_user_data = user_data;
    session->dispatcher = dispatcher;

    session->contexts =
        (dispatch_context_t *)AGENTRT_CALLOC(task_count, sizeof(dispatch_context_t));
    if (!session->contexts) {
        SVC_LOG_ERROR("session_create: memory allocation failed for contexts (task_count=%zu)", task_count);
        agentrt_mutex_destroy(&session->lock);
        agentrt_cond_destroy(&session->cond);
        AGENTRT_FREE(session);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
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

int parallel_dispatcher_execute(parallel_dispatcher_t *dispatcher, const parallel_task_t *tasks,
                                size_t task_count, parallel_result_t **results,
                                size_t *result_count)
{
    if (!dispatcher || !tasks || task_count == 0 || !results) {
        SVC_LOG_ERROR("parallel_dispatcher_execute: invalid parameter dispatcher=%p tasks=%p task_count=%zu results=%p",
                      (void *)dispatcher, (void *)tasks, task_count, (void *)results);
        return AGENTRT_ERR_INVALID_PARAM;
    }

    *results = (parallel_result_t *)AGENTRT_CALLOC(task_count, sizeof(parallel_result_t));
    if (!*results) {
        SVC_LOG_ERROR("parallel_dispatcher_execute: memory allocation failed for results (task_count=%zu)", task_count);
        return AGENTRT_ERR_OUT_OF_MEMORY;
    }
    if (result_count)
        *result_count = task_count;

    dispatch_session_t *session =
        session_create(dispatcher, tasks, task_count, *results, NULL, NULL);
    if (!session) {
        SVC_LOG_ERROR("parallel_dispatcher_execute: session creation failed (task_count=%zu)", task_count);
        AGENTRT_FREE(*results);
        *results = NULL;
        return AGENTRT_ERR_OUT_OF_MEMORY;
    }

    for (size_t i = 0; i < task_count; i++) {
        agentrt_mutex_lock(&session->lock);
        if (session->cancel_flag) {
            agentrt_mutex_unlock(&session->lock);
            break;
        }

        while (thread_pool_active_count(dispatcher->pool) >= dispatcher->config.max_concurrency) {
            if (should_return(dispatcher->config.wait_mode, session->completed_count, task_count)) {
                agentrt_mutex_unlock(&session->lock);
                goto wait_done;
            }
            agentrt_cond_timedwait(&session->cond, &session->lock, 100);
        }
        agentrt_mutex_unlock(&session->lock);

        int rc = thread_pool_submit(dispatcher->pool, dispatch_worker, &session->contexts[i]);
        if (rc != 0) {
            SVC_LOG_WARN("parallel_dispatcher_execute: task submit failed index=%zu rc=%d", i, rc);
            session->contexts[i].result_slot->success = 0;
            session->contexts[i].result_slot->error = AGENTRT_STRDUP("submit failed");
            session->contexts[i].result_slot->task_index = i;
            agentrt_mutex_lock(&session->lock);
            session->completed_count++;
            session->error_count++;
            agentrt_cond_signal(&session->cond);
            agentrt_mutex_unlock(&session->lock);
        }
    }

wait_done: {
    uint64_t deadline = time_ms() + dispatcher->config.timeout_ms;
    while (1) {
        agentrt_mutex_lock(&session->lock);
        bool done =
            should_return(dispatcher->config.wait_mode, session->completed_count, task_count);
        agentrt_mutex_unlock(&session->lock);

        if (done)
            break;

        if (time_ms() >= deadline) {
            SVC_LOG_WARN("parallel_dispatcher_execute: timeout reached, cancelling remaining tasks (timeout_ms=%u)",
                         dispatcher->config.timeout_ms);
            session->cancel_flag = 1;
            break;
        }

        agentrt_mutex_lock(&session->lock);
        agentrt_cond_timedwait(&session->cond, &session->lock, 50);
        agentrt_mutex_unlock(&session->lock);
    }
}

    int errors = session->error_count;
    bool session_freed_by_worker = false;
    agentrt_mutex_lock(&session->lock);
    if ((size_t)session->completed_count >= session->task_count) {
        session_freed_by_worker = true;
    }
    agentrt_mutex_unlock(&session->lock);

    if (!session_freed_by_worker) {
        agentrt_mutex_destroy(&session->lock);
        agentrt_cond_destroy(&session->cond);
        if (session->contexts)
            AGENTRT_FREE(session->contexts);
        AGENTRT_FREE(session);
    }

    return (errors > 0) ? 1 : 0;
}

int parallel_dispatcher_execute_async(parallel_dispatcher_t *dispatcher,
                                      const parallel_task_t *tasks, size_t task_count,
                                      parallel_complete_cb_t on_complete, void *user_data)
{
    if (!dispatcher || !tasks || task_count == 0) {
        SVC_LOG_ERROR("parallel_dispatcher_execute_async: invalid parameter dispatcher=%p tasks=%p task_count=%zu",
                      (void *)dispatcher, (void *)tasks, task_count);
        return AGENTRT_ERR_INVALID_PARAM;
    }

    parallel_result_t *results =
        (parallel_result_t *)AGENTRT_CALLOC(task_count, sizeof(parallel_result_t));
    if (!results) {
        SVC_LOG_ERROR("parallel_dispatcher_execute_async: memory allocation failed for results (task_count=%zu)", task_count);
        return AGENTRT_ERR_OUT_OF_MEMORY;
    }

    dispatch_session_t *session =
        session_create(dispatcher, tasks, task_count, results, on_complete, user_data);
    if (!session) {
        SVC_LOG_ERROR("parallel_dispatcher_execute_async: session creation failed (task_count=%zu)", task_count);
        AGENTRT_FREE(results);
        return AGENTRT_ERR_OUT_OF_MEMORY;
    }

    int submitted = 0;
    for (size_t i = 0; i < task_count; i++) {
        int rc = thread_pool_submit(dispatcher->pool, dispatch_worker, &session->contexts[i]);
        if (rc == 0) {
            submitted++;
        } else {
            SVC_LOG_WARN("parallel_dispatcher_execute_async: task submit failed index=%zu rc=%d", i, rc);
            results[i].success = 0;
            results[i].error = AGENTRT_STRDUP("submit failed");
            results[i].task_index = i;
            if (on_complete)
                on_complete(i, &results[i], user_data);
            agentrt_mutex_lock(&session->lock);
            session->completed_count++;
            session->error_count++;
            agentrt_mutex_unlock(&session->lock);
        }
    }

    if (submitted == 0) {
        SVC_LOG_ERROR("parallel_dispatcher_execute_async: all task submissions failed (task_count=%zu)", task_count);
        agentrt_mutex_destroy(&session->lock);
        agentrt_cond_destroy(&session->cond);
        if (session->contexts)
            AGENTRT_FREE(session->contexts);
        AGENTRT_FREE(session);
        AGENTRT_FREE(results);
        return AGENTRT_ERR_UNKNOWN;
    }

    return 0;
}

void parallel_result_free(parallel_result_t *results, size_t count)
{
    if (!results)
        return;
    for (size_t i = 0; i < count; i++) {
        if (results[i].output)
            AGENTRT_FREE(results[i].output);
        if (results[i].error)
            AGENTRT_FREE(results[i].error);
    }
    AGENTRT_FREE(results);
}

parallel_task_t parallel_task_create(const char *tool_id, const char *params_json)
{
    parallel_task_t task;
    __builtin_memset(&task, 0, sizeof(task));
    task.tool_id = tool_id ? AGENTRT_STRDUP(tool_id) : NULL;
    task.params_json = params_json ? AGENTRT_STRDUP(params_json) : NULL;
    task.user_data = NULL;
    return task;
}

void parallel_task_free(parallel_task_t *task)
{
    if (!task)
        return;
    if (task->tool_id) {
        AGENTRT_FREE(task->tool_id);
        task->tool_id = NULL;
    }
    if (task->params_json) {
        AGENTRT_FREE(task->params_json);
        task->params_json = NULL;
    }
}

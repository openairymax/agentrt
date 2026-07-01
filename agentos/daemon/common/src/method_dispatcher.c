
#include "daemon_errors.h"
#include "jsonrpc_helpers.h"
#include "memory_compat.h"
#include "method_dispatcher.h"
#include "svc_logger.h"

#include <stdlib.h>
#include <string.h>
#include "error.h"

struct method_handler {
    char *method;
    method_fn handler;
    void *user_data;
};

struct method_dispatcher {
    struct method_handler *handlers;
    size_t max_methods;
    size_t method_count;
    agentos_mutex_t lock;
};

static int find_method_index(method_dispatcher_t *disp, const char *method)
{
    for (size_t i = 0; i < disp->method_count; i++) {
        if (strcmp(disp->handlers[i].method, method) == 0)
            return (int)i;
    }
    return AGENTOS_ERR_NOT_FOUND;
}

method_dispatcher_t *method_dispatcher_create(size_t max_methods)
{
    if (max_methods == 0) {
        SVC_LOG_ERROR("method_dispatcher_create: max_methods is zero");
        AGENTOS_ERROR_NULL(AGENTOS_ERR_OVERFLOW, "limit exceeded");
    }

    method_dispatcher_t *disp =
        (method_dispatcher_t *)AGENTOS_CALLOC(1, sizeof(method_dispatcher_t));
    if (!disp) {
        SVC_LOG_ERROR("method_dispatcher_create: memory allocation failed for dispatcher");
        AGENTOS_ERROR_NULL(AGENTOS_ERR_UNKNOWN, "validation failed");
    }

    disp->handlers =
        (struct method_handler *)AGENTOS_CALLOC(max_methods, sizeof(struct method_handler));
    if (!disp->handlers) {
        SVC_LOG_ERROR("method_dispatcher_create: memory allocation failed for handlers (max_methods=%zu)", max_methods);
        AGENTOS_FREE(disp);
        AGENTOS_ERROR_NULL(AGENTOS_ERR_INVALID_PARAM, "null parameter");
    }

    disp->max_methods = max_methods;
    disp->method_count = 0;
    agentos_mutex_init(&disp->lock);

    return disp;
}

void method_dispatcher_destroy(method_dispatcher_t *disp)
{
    if (!disp)
        return;

    for (size_t i = 0; i < disp->method_count; i++) {
        AGENTOS_FREE(disp->handlers[i].method);
    }
    AGENTOS_FREE(disp->handlers);
    agentos_mutex_destroy(&disp->lock);
    AGENTOS_FREE(disp);
}

int method_dispatcher_register(method_dispatcher_t *disp, const char *method, method_fn handler,
                               void *user_data)
{
    if (!disp || !method || !handler) {
        SVC_LOG_ERROR("method_dispatcher_register: null parameter disp=%p method=%p handler=%p",
                      (void *)disp, (void *)method, (void *)(uintptr_t)handler);
        return AGENTOS_ERR_INVALID_PARAM;
    }
    if (disp->method_count >= disp->max_methods) {
        SVC_LOG_ERROR("method_dispatcher_register: max methods reached count=%zu max=%zu method='%s'",
                      disp->method_count, disp->max_methods, method);
        return AGENTOS_ERR_OVERFLOW;
    }

    int existing = find_method_index(disp, method);
    if (existing >= 0) {
        SVC_LOG_WARN("method_dispatcher_register: method '%s' already registered at index=%d", method, existing);
        return AGENTOS_ERR_UNKNOWN;
    }

    disp->handlers[disp->method_count].method = AGENTOS_STRDUP(method);
    disp->handlers[disp->method_count].handler = handler;
    disp->handlers[disp->method_count].user_data = user_data;
    disp->method_count++;

    return 0;
}

int method_dispatcher_dispatch(method_dispatcher_t *disp, cJSON *request,
                               char *(*error_response_fn)(int, const char *, int), void *user_data)
{
    if (!disp || !request) {
        SVC_LOG_ERROR("method_dispatcher_dispatch: null parameter disp=%p request=%p",
                      (void *)disp, (void *)request);
        return AGENTOS_ERR_INVALID_PARAM;
    }

    char *method = NULL;
    cJSON *params = NULL;
    int id = 0;

    if (jsonrpc_parse_request_ptr(request, &method, &params, &id) != 0) {
        SVC_LOG_ERROR("method_dispatcher_dispatch: failed to parse JSON-RPC request");
        if (error_response_fn) {
            char *err = error_response_fn(JSONRPC_INVALID_REQUEST, "Invalid request", id);
            AGENTOS_FREE(err);
        }
        return AGENTOS_ERR_PARSE_ERROR;
    }

    int index = find_method_index(disp, method);
    if (index < 0) {
        SVC_LOG_WARN("method_dispatcher_dispatch: method '%s' not found (registered=%zu)", method, disp->method_count);
        if (error_response_fn) {
            char *err = error_response_fn(JSONRPC_METHOD_NOT_FOUND, "Method not found", id);
            AGENTOS_FREE(err);
        }
        AGENTOS_FREE(method);
        if (params)
            cJSON_Delete(params);
        return AGENTOS_ERR_NOT_FOUND;
    }

    method_fn handler = disp->handlers[index].handler;
    void *data = disp->handlers[index].user_data ? disp->handlers[index].user_data : user_data;

    handler(params, id, data);

    AGENTOS_FREE(method);
    if (params)
        cJSON_Delete(params);

    return 0;
}

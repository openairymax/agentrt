/**
 * @file method_dispatcher.c
 * @brief JSON-RPC 方法分发器框架实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * @version 1.0.0
 * @date 2026-04-04
 */

#include "method_dispatcher.h"
#include "svc_logger.h"
#include "jsonrpc_helpers.h"
#include <stdlib.h>
#include <string.h>

/* ==================== 内部结构 ==================== */

struct method_handler {
    char* method;
    method_fn handler;
    void* user_data;
};

struct method_dispatcher {
    method_handler_t* handlers;
    size_t max_methods;
    size_t method_count;
    agentos_mutex_t lock;
};

/* ==================== 工具函数 ==================== */

static int find_method_index(method_dispatcher_t* disp, const char* method) {
    for (size_t i = 0; i < disp->method_count; i++) {
        if (strcmp(disp->handlers[i].method, method) == 0) {
            return (int)i;
        }
    }
    return -1;
}

/* ==================== 公共接口实现 ==================== */

method_dispatcher_t* method_dispatcher_create(size_t max_methods) {
    if (max_methods == 0) {
        SVC_LOG_ERROR("max_methods must be greater than 0");
        return NULL;
    }

    method_dispatcher_t* disp = (method_dispatcher_t*)calloc(1, sizeof(method_dispatcher_t));
    if (!disp) {
        SVC_LOG_ERROR("Failed to allocate method dispatcher");
        return NULL;
    }

    disp->handlers = (method_handler_t*)calloc(max_methods, sizeof(method_handler_t));
    if (!disp->handlers) {
        SVC_LOG_ERROR("Failed to allocate method handlers");
        free(disp);
        return NULL;
    }

    disp->max_methods = max_methods;
    disp->method_count = 0;
    agentos_mutex_init(&disp->lock);

    SVC_LOG_DEBUG("Method dispatcher created (max_methods=%zu)", max_methods);
    return disp;
}

void method_dispatcher_destroy(method_dispatcher_t* disp) {
    if (!disp) return;

    agentos_mutex_lock(&disp->lock);

    for (size_t i = 0; i < disp->method_count; i++) {
        free(disp->handlers[i].method);
    }
    free(disp->handlers);

    agentos_mutex_unlock(&disp->lock);
    agentos_mutex_destroy(&disp->lock);

    free(disp);
    SVC_LOG_DEBUG("Method dispatcher destroyed");
}

int method_dispatcher_register(method_dispatcher_t* disp,
                              const char* method,
                              method_fn handler,
                              void* user_data) {
    if (!disp || !method || !handler) {
        SVC_LOG_ERROR("Invalid parameters for register");
        return -1;
    }

    agentos_mutex_lock(&disp->lock);

    if (disp->method_count >= disp->max_methods) {
        SVC_LOG_ERROR("Method dispatcher is full (max=%zu)", disp->max_methods);
        agentos_mutex_unlock(&disp->lock);
        return -1;
    }

    int existing = find_method_index(disp, method);
    if (existing >= 0) {
        SVC_LOG_WARN("Method already registered: %s", method);
        agentos_mutex_unlock(&disp->lock);
        return -1;
    }

    disp->handlers[disp->method_count].method = strdup(method);
    disp->handlers[disp->method_count].handler = handler;
    disp->handlers[disp->method_count].user_data = user_data;
    disp->method_count++;

    agentos_mutex_unlock(&disp->lock);

    SVC_LOG_DEBUG("Method registered: %s", method);
    return 0;
}

int method_dispatcher_unregister(method_dispatcher_t* disp, const char* method) {
    if (!disp || !method) {
        return -1;
    }

    agentos_mutex_lock(&disp->lock);

    int index = find_method_index(disp, method);
    if (index < 0) {
        agentos_mutex_unlock(&disp->lock);
        SVC_LOG_WARN("Method not found: %s", method);
        return -1;
    }

    free(disp->handlers[index].method);

    for (size_t i = index; i < disp->method_count - 1; i++) {
        disp->handlers[i] = disp->handlers[i + 1];
    }
    disp->method_count--;

    agentos_mutex_unlock(&disp->lock);

    SVC_LOG_DEBUG("Method unregistered: %s", method);
    return 0;
}

int method_dispatcher_dispatch(method_dispatcher_t* disp,
                              cJSON* request,
                              char* (*error_response_fn)(int code, const char* msg, int id),
                              void* user_data) {
    if (!disp || !request) {
        SVC_LOG_ERROR("Invalid parameters for dispatch");
        return -1;
    }

    char* method = NULL;
    cJSON* params = NULL;
    int id = 0;

    if (jsonrpc_parse_request_ptr(request, &method, &params, &id) != 0) {
        SVC_LOG_ERROR("Failed to parse request");
        return -1;
    }

    agentos_mutex_lock(&disp->lock);

    int index = find_method_index(disp, method);
    if (index < 0) {
        agentos_mutex_unlock(&disp->lock);
        SVC_LOG_ERROR("Method not found: %s", method);

        if (error_response_fn) {
            char* err_resp = error_response_fn(JSONRPC_METHOD_NOT_FOUND,
                                              "Method not found", id);
            if (err_resp) {
                free(err_resp);
            }
        }

        free(method);
        if (params) cJSON_Delete(params);
        return -1;
    }

    method_handler_t* handler = &disp->handlers[index];
    agentos_mutex_unlock(&disp->lock);

    handler->handler(params, id, handler->user_data ? handler->user_data : user_data);

    free(method);
    if (params) cJSON_Delete(params);

    SVC_LOG_DEBUG("Method dispatched: %s (id=%d)", method, id);
    return 0;
}

size_t method_dispatcher_count(const method_dispatcher_t* disp) {
    if (!disp) return 0;
    return disp->method_count;
}

size_t method_dispatcher_list(const method_dispatcher_t* disp,
                             char*** out_methods,
                             size_t max_methods) {
    if (!disp || !out_methods) return 0;

    agentos_mutex_lock((method_dispatcher_t*)disp);

    size_t count = (disp->method_count < max_methods) ?
                   disp->method_count : max_methods;

    *out_methods = (char**)malloc(count * sizeof(char*));
    if (!*out_methods) {
        agentos_mutex_unlock((method_dispatcher_t*)disp);
        return 0;
    }

    for (size_t i = 0; i < count; i++) {
        (*out_methods)[i] = strdup(disp->handlers[i].method);
    }

    agentos_mutex_unlock((method_dispatcher_t*)disp);
    return count;
}

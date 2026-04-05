/**
 * @file method_dispatcher.c
 * @brief JSON-RPC 方法分发器实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "method_dispatcher.h"
#include "jsonrpc_helpers.h"
#include "svc_logger.h"
#include <stdlib.h>
#include <string.h>

struct method_handler {
    char* method;
    method_fn handler;
    void* user_data;
};

struct method_dispatcher {
    struct method_handler* handlers;
    size_t max_methods;
    size_t method_count;
    agentos_mutex_t lock;
};

static int find_method_index(method_dispatcher_t* disp, const char* method) {
    for (size_t i = 0; i < disp->method_count; i++) {
        if (strcmp(disp->handlers[i].method, method) == 0) return (int)i;
    }
    return -1;
}

method_dispatcher_t* method_dispatcher_create(size_t max_methods) {
    if (max_methods == 0) return NULL;
    
    method_dispatcher_t* disp = (method_dispatcher_t*)calloc(1, sizeof(method_dispatcher_t));
    if (!disp) return NULL;
    
    disp->handlers = (struct method_handler*)calloc(max_methods, sizeof(struct method_handler));
    if (!disp->handlers) {
        free(disp);
        return NULL;
    }
    
    disp->max_methods = max_methods;
    disp->method_count = 0;
    agentos_mutex_init(&disp->lock);
    
    return disp;
}

void method_dispatcher_destroy(method_dispatcher_t* disp) {
    if (!disp) return;
    
    for (size_t i = 0; i < disp->method_count; i++) {
        free(disp->handlers[i].method);
    }
    free(disp->handlers);
    agentos_mutex_destroy(&disp->lock);
    free(disp);
}

int method_dispatcher_register(method_dispatcher_t* disp, const char* method, method_fn handler, void* user_data) {
    if (!disp || !method || !handler) return -1;
    if (disp->method_count >= disp->max_methods) return -1;
    
    int existing = find_method_index(disp, method);
    if (existing >= 0) return -1;
    
    disp->handlers[disp->method_count].method = strdup(method);
    disp->handlers[disp->method_count].handler = handler;
    disp->handlers[disp->method_count].user_data = user_data;
    disp->method_count++;
    
    return 0;
}

int method_dispatcher_dispatch(method_dispatcher_t* disp, cJSON* request,
                              char* (*error_response_fn)(int, const char*, int), void* user_data) {
    if (!disp || !request) return -1;
    
    char* method = NULL;
    cJSON* params = NULL;
    int id = 0;
    
    if (jsonrpc_parse_request_ptr(request, &method, &params, &id) != 0) {
        if (error_response_fn) {
            char* err = error_response_fn(JSONRPC_INVALID_REQUEST, "Invalid request", id);
            free(err);
        }
        return -1;
    }
    
    int index = find_method_index(disp, method);
    if (index < 0) {
        if (error_response_fn) {
            char* err = error_response_fn(JSONRPC_METHOD_NOT_FOUND, "Method not found", id);
            free(err);
        }
        free(method);
        if (params) cJSON_Delete(params);
        return -1;
    }
    
    method_fn handler = disp->handlers[index].handler;
    void* data = disp->handlers[index].user_data ? disp->handlers[index].user_data : user_data;
    
    handler(params, id, data);
    
    free(method);
    if (params) cJSON_Delete(params);
    
    return 0;
}

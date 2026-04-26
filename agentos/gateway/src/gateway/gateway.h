#ifndef AGENTOS_GATEWAY_INTERNAL_H
#define AGENTOS_GATEWAY_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GATEWAY_SUCCESS = 0,
    GATEWAY_ERROR_INVALID = -1,
    GATEWAY_ERROR_MEMORY = -2,
    GATEWAY_ERROR_IO = -3,
    GATEWAY_ERROR_TIMEOUT = -4,
    GATEWAY_ERROR_CLOSED = -5,
    GATEWAY_ERROR_PROTOCOL = -6
} gateway_error_t;

typedef enum {
    GATEWAY_TYPE_HTTP = 0,
    GATEWAY_TYPE_WS,
    GATEWAY_TYPE_STDIO
} gateway_type_t;

typedef int (*gateway_request_handler_t)(
    const char* request_json,
    char** response_json,
    void* user_data
);

typedef char* (*gateway_internal_handler_t)(void* request, void* user_data);

typedef struct gateway_ops {
    int (*start)(void* impl);
    void (*stop)(void* impl);
    void (*destroy)(void* impl);
    const char* (*get_name)(void* impl);
    int (*get_stats)(void* impl, char** out_json);
    bool (*is_running)(void* impl);
    int (*set_handler)(void* impl, gateway_internal_handler_t handler, void* user_data);
} gateway_ops_t;

typedef struct gateway {
    const gateway_ops_t* ops;
    void* impl;
    gateway_type_t type;
    gateway_request_handler_t public_handler;
    void* public_handler_data;
} gateway_t;

gateway_t* gateway_http_create(const char* host, uint16_t port);
gateway_t* gateway_ws_create(const char* host, uint16_t port);
gateway_t* gateway_stdio_create(void);

void gateway_destroy(gateway_t* gw);
int gateway_start(gateway_t* gw);
int gateway_stop(gateway_t* gw);
int gateway_get_stats(gateway_t* gw, char** out_json);
int gateway_set_handler(gateway_t* gw, gateway_request_handler_t handler, void* user_data);
bool gateway_is_running(gateway_t* gw);
gateway_type_t gateway_get_type(gateway_t* gw);
const char* gateway_get_name(gateway_t* gw);

#ifdef __cplusplus
}
#endif

#endif

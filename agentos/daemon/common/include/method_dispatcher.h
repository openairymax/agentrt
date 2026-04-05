/**
 * @file method_dispatcher.h
 * @brief JSON-RPC 方法分发器框架
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_METHOD_DISPATCHER_H
#define AGENTOS_METHOD_DISPATCHER_H

#include <cjson/cJSON.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct method_dispatcher method_dispatcher_t;
typedef void (*method_fn)(cJSON* params, int id, void* user_data);

method_dispatcher_t* method_dispatcher_create(size_t max_methods);
void method_dispatcher_destroy(method_dispatcher_t* disp);
int method_dispatcher_register(method_dispatcher_t* disp, const char* method, method_fn handler, void* user_data);
int method_dispatcher_dispatch(method_dispatcher_t* disp, cJSON* request, char* (*error_response_fn)(int, const char*, int), void* user_data);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_METHOD_DISPATCHER_H */

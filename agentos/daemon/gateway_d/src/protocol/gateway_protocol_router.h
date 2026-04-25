#ifndef AGENTOS_GATEWAY_PROTOCOL_ROUTER_H
#define AGENTOS_GATEWAY_PROTOCOL_ROUTER_H

#include "unified_protocol.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GW_PROTO_MAX_ADAPTERS 8
#define GW_PROTO_MAX_METHOD_LEN 128
#define GW_PROTO_MAX_PATH_LEN 512

typedef enum {
    GW_PROTO_DETECT_UNKNOWN = 0,
    GW_PROTO_DETECT_MCP,
    GW_PROTO_DETECT_A2A,
    GW_PROTO_DETECT_OPENAI,
    GW_PROTO_DETECT_JSONRPC
} gw_proto_detect_result_t;

typedef struct gw_proto_router gw_proto_router_t;

typedef struct {
    char method[GW_PROTO_MAX_METHOD_LEN];
    char path[GW_PROTO_MAX_PATH_LEN];
    char protocol_name[32];
    int status_code;
    double duration_ms;
} gw_proto_route_stats_entry_t;

typedef struct {
    uint64_t total_requests;
    uint64_t mcp_requests;
    uint64_t a2a_requests;
    uint64_t openai_requests;
    uint64_t jsonrpc_requests;
    uint64_t unknown_requests;
    uint64_t route_errors;
} gw_proto_router_stats_t;

typedef int (*gw_proto_request_handler_t)(
    const char* method,
    const char* path,
    const char* body_json,
    char** response_json,
    void* user_data);

gw_proto_router_t* gw_proto_router_create(void);
void gw_proto_router_destroy(gw_proto_router_t* router);

int gw_proto_router_init(gw_proto_router_t* router);
int gw_proto_router_shutdown(gw_proto_router_t* router);

gw_proto_detect_result_t gw_proto_detect(const char* content_type,
                                          const char* path,
                                          const char* body);

int gw_proto_router_register(gw_proto_router_t* router,
                              gw_proto_detect_result_t proto_type,
                              gw_proto_request_handler_t handler,
                              void* user_data);

int gw_proto_router_route(gw_proto_router_t* router,
                           gw_proto_detect_result_t proto_type,
                           const char* method,
                           const char* path,
                           const char* body_json,
                           char** response_json);

int gw_proto_router_route_auto(gw_proto_router_t* router,
                                const char* content_type,
                                const char* method,
                                const char* path,
                                const char* body_json,
                                char** response_json);

int gw_proto_router_get_stats(gw_proto_router_t* router,
                               gw_proto_router_stats_t* stats);

bool gw_proto_router_is_healthy(gw_proto_router_t* router);

#ifdef __cplusplus
}
#endif

#endif

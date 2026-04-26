#ifndef AGENTOS_GATEWAY_MCP_SERVER_H
#define AGENTOS_GATEWAY_MCP_SERVER_H

#include "gateway_protocol_router.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gw_mcp_server gw_mcp_server_t;

typedef struct {
    char server_name[128];
    char server_version[32];
    uint32_t capabilities;
    uint32_t default_timeout_ms;
    bool enable_progress;
    bool enable_cancellation;
    bool enable_sampling;
} gw_mcp_server_config_t;

#define GW_MCP_SERVER_CONFIG_DEFAULTS { \
    .server_name = "agentos-gateway", \
    .server_version = "1.0.0", \
    .capabilities = 0x3F, \
    .default_timeout_ms = 30000, \
    .enable_progress = true, \
    .enable_cancellation = true, \
    .enable_sampling = true }

typedef int (*gw_mcp_tool_exec_fn)(const char* tool_name,
                                    const char* arguments_json,
                                    char** result_json,
                                    void* user_data);

typedef int (*gw_mcp_resource_read_fn)(const char* uri,
                                        char** content,
                                        char** mime_type,
                                        void* user_data);

gw_mcp_server_t* gw_mcp_server_create(const gw_mcp_server_config_t* config);
void gw_mcp_server_destroy(gw_mcp_server_t* server);

int gw_mcp_server_init(gw_mcp_server_t* server);
int gw_mcp_server_shutdown(gw_mcp_server_t* server);

int gw_mcp_server_register_tool(gw_mcp_server_t* server,
                                 const char* name,
                                 const char* description,
                                 const char* input_schema_json,
                                 gw_mcp_tool_exec_fn exec_fn,
                                 void* user_data);

int gw_mcp_server_register_resource(gw_mcp_server_t* server,
                                     const char* uri,
                                     const char* name,
                                     const char* description,
                                     const char* mime_type,
                                     gw_mcp_resource_read_fn read_fn,
                                     void* user_data);

int gw_mcp_server_handle_request(gw_mcp_server_t* server,
                                  const char* method,
                                  const char* path,
                                  const char* body_json,
                                  char** response_json);

int gw_mcp_server_handle_jsonrpc(gw_mcp_server_t* server,
                                  const char* method,
                                  const char* params_json,
                                  char** response_json);

gw_proto_request_handler_t gw_mcp_server_get_handler(gw_mcp_server_t* server);
void* gw_mcp_server_get_handler_data(gw_mcp_server_t* server);

bool gw_mcp_server_is_healthy(gw_mcp_server_t* server);

#ifdef __cplusplus
}
#endif

#endif

#ifndef AGENTOS_MCP_SERVER_H
#define AGENTOS_MCP_SERVER_H

#include <stdint.h>
#include <stdbool.h>
#include <cJSON.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mcp_server_s* mcp_server_t;

typedef struct {
    const char* name;
    const char* version;
    bool        enable_tools;
    bool        enable_resources;
    bool        enable_prompts;
} mcp_server_config_t;

typedef int (*mcp_tool_handler_t)(const char* name, const cJSON* params,
                                  cJSON** result, void* user_data);
typedef int (*mcp_resource_handler_t)(const char* uri, cJSON** result,
                                     void* user_data);
typedef int (*mcp_prompt_handler_t)(const char* name, const cJSON* args,
                                   cJSON** result, void* user_data);

mcp_server_t mcp_server_create(const mcp_server_config_t* config);
void         mcp_server_destroy(mcp_server_t server);

int  mcp_server_register_tool(mcp_server_t server, const char* name,
                              const char* description, cJSON* input_schema,
                              mcp_tool_handler_t handler, void* user_data);
int  mcp_server_register_resource(mcp_server_t server, const char* uri,
                                  const char* name, const char* description,
                                  const char* mime_type,
                                  mcp_resource_handler_t handler, void* user_data);
int  mcp_server_register_prompt(mcp_server_t server, const char* name,
                                const char* description, cJSON* argument_schema,
                                mcp_prompt_handler_t handler, void* user_data);

int  mcp_server_handle_request(mcp_server_t server, const char* method,
                               const cJSON* params, cJSON** response);

int  mcp_server_get_capabilities(cJSON** caps);
const char* mcp_error_to_string(int code);

#define MCP_ERROR_INVALID_PARAMS   (-32602)
#define MCP_ERROR_METHOD_NOT_FOUND (-32601)
#define MCP_ERROR_INTERNAL         (-32603)

#ifdef __cplusplus
}
#endif

#endif

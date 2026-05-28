#include "mcp_server.h"

#include "error.h"
#include "gateway_compat.h"
#include "memory_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MCP_MAX_TOOLS 64
#define MCP_MAX_RESOURCES 64
#define MCP_MAX_PROMPTS 64

typedef struct {
    char *name;
    char *description;
    cJSON *input_schema;
    mcp_tool_handler_t handler;
    void *user_data;
} mcp_tool_entry_t;

typedef struct {
    char *uri;
    char *name;
    char *description;
    char *mime_type;
    mcp_resource_handler_t handler;
    void *user_data;
} mcp_resource_entry_t;

typedef struct {
    char *name;
    char *description;
    cJSON *argument_schema;
    mcp_prompt_handler_t handler;
    void *user_data;
} mcp_prompt_entry_t;

struct mcp_server_s {
    mcp_server_config_t config;

    mcp_tool_entry_t tools[MCP_MAX_TOOLS];
    int tool_count;

    mcp_resource_entry_t resources[MCP_MAX_RESOURCES];
    int resource_count;

    mcp_prompt_entry_t prompts[MCP_MAX_PROMPTS];
    int prompt_count;

    uint64_t request_count;
    time_t created_at;
};

static cJSON *mcp_make_result(const cJSON *content)
{
    cJSON *result = cJSON_CreateObject();
    if (content) {
        cJSON_AddItemToObject(result, "content", (cJSON *)cJSON_Duplicate((cJSON *)content, 1));
    } else {
        cJSON *empty = cJSON_CreateArray();
        cJSON_AddItemToObject(result, "content", empty);
    }
    cJSON_AddBoolToObject(result, "isError", 0);
    return result;
}

static cJSON *mcp_make_error(int code, const char *message, const cJSON *data)
{
    cJSON *error = cJSON_CreateObject();
    cJSON_AddNumberToObject(error, "code", code);
    cJSON_AddStringToObject(error, "message", message ? message : "Unknown error");
    if (data) {
        cJSON_AddItemToObject(error, "data", (cJSON *)cJSON_Duplicate((cJSON *)data, 1));
    }
    return error;
}

mcp_server_t mcp_server_create(const mcp_server_config_t *config)
{
    mcp_server_t server = (mcp_server_t)AGENTOS_CALLOC(1, sizeof(struct mcp_server_s));
    if (!server)
        return NULL;

    if (config) {
        server->config = *config;
    } else {
        server->config.name = "agentos-mcp";
        server->config.version = "0.1.0";
        server->config.enable_tools = true;
        server->config.enable_resources = true;
        server->config.enable_prompts = true;
    }

    server->created_at = time(NULL);
    return server;
}

void mcp_server_destroy(mcp_server_t server)
{
    if (!server)
        return;

    for (int i = 0; i < server->tool_count; i++) {
        AGENTOS_FREE(server->tools[i].name);
        AGENTOS_FREE(server->tools[i].description);
        if (server->tools[i].input_schema)
            cJSON_Delete(server->tools[i].input_schema);
    }

    for (int i = 0; i < server->resource_count; i++) {
        AGENTOS_FREE(server->resources[i].uri);
        AGENTOS_FREE(server->resources[i].name);
        AGENTOS_FREE(server->resources[i].description);
        AGENTOS_FREE(server->resources[i].mime_type);
    }

    for (int i = 0; i < server->prompt_count; i++) {
        AGENTOS_FREE(server->prompts[i].name);
        AGENTOS_FREE(server->prompts[i].description);
        if (server->prompts[i].argument_schema)
            cJSON_Delete(server->prompts[i].argument_schema);
    }

    AGENTOS_FREE(server);
}

int mcp_server_register_tool(mcp_server_t server, const char *name, const char *description,
                             cJSON *input_schema, mcp_tool_handler_t handler, void *user_data)
{
    AGENTOS_CHECK(server != NULL, AGENTOS_EFAIL, "server is NULL");
    AGENTOS_CHECK(name != NULL, AGENTOS_EFAIL, "name is NULL");
    AGENTOS_CHECK(handler != NULL, AGENTOS_EFAIL, "handler is NULL");
    if (server->tool_count >= MCP_MAX_TOOLS)
        return AGENTOS_ERR_OVERFLOW;

    mcp_tool_entry_t *entry = &server->tools[server->tool_count++];
    entry->name = AGENTOS_STRDUP(name);
    entry->description = description ? AGENTOS_STRDUP(description) : AGENTOS_STRDUP("");
    entry->input_schema = input_schema ? cJSON_Duplicate(input_schema, 1) : NULL;
    entry->handler = handler;
    entry->user_data = user_data;
    return 0;
}

int mcp_server_register_resource(mcp_server_t server, const char *uri, const char *name,
                                 const char *description, const char *mime_type,
                                 mcp_resource_handler_t handler, void *user_data)
{
    AGENTOS_CHECK(server != NULL, AGENTOS_EFAIL, "server is NULL");
    AGENTOS_CHECK(uri != NULL, AGENTOS_EFAIL, "uri is NULL");
    AGENTOS_CHECK(handler != NULL, AGENTOS_EFAIL, "handler is NULL");
    if (server->resource_count >= MCP_MAX_RESOURCES)
        return AGENTOS_ERR_OVERFLOW;

    mcp_resource_entry_t *entry = &server->resources[server->resource_count];
    entry->uri = AGENTOS_STRDUP(uri);
    entry->name = name ? AGENTOS_STRDUP(name) : AGENTOS_STRDUP("");
    entry->description = description ? AGENTOS_STRDUP(description) : AGENTOS_STRDUP("");
    entry->mime_type = mime_type ? AGENTOS_STRDUP(mime_type) : AGENTOS_STRDUP("text/plain");
    if (!entry->uri || !entry->name || !entry->description || !entry->mime_type) {
        AGENTOS_FREE(entry->uri);
        AGENTOS_FREE(entry->name);
        AGENTOS_FREE(entry->description);
        AGENTOS_FREE(entry->mime_type);
        entry->uri = NULL;
        entry->name = NULL;
        entry->description = NULL;
        entry->mime_type = NULL;
        return AGENTOS_ERR_INVALID_PARAM;
    }
    entry->handler = handler;
    entry->user_data = user_data;
    server->resource_count++;
    return 0;
}

int mcp_server_register_prompt(mcp_server_t server, const char *name, const char *description,
                               cJSON *argument_schema, mcp_prompt_handler_t handler,
                               void *user_data)
{
    AGENTOS_CHECK(server != NULL, AGENTOS_EFAIL, "server is NULL");
    AGENTOS_CHECK(name != NULL, AGENTOS_EFAIL, "name is NULL");
    AGENTOS_CHECK(handler != NULL, AGENTOS_EFAIL, "handler is NULL");
    if (server->prompt_count >= MCP_MAX_PROMPTS)
        return AGENTOS_ERR_OVERFLOW;

    mcp_prompt_entry_t *entry = &server->prompts[server->prompt_count];
    entry->name = AGENTOS_STRDUP(name);
    entry->description = description ? AGENTOS_STRDUP(description) : AGENTOS_STRDUP("");
    if (!entry->name || !entry->description) {
        AGENTOS_FREE(entry->name);
        AGENTOS_FREE(entry->description);
        entry->name = NULL;
        entry->description = NULL;
        return AGENTOS_ERR_INVALID_PARAM;
    }
    entry->argument_schema = argument_schema ? cJSON_Duplicate(argument_schema, 1) : NULL;
    entry->handler = handler;
    entry->user_data = user_data;
    server->prompt_count++;
    return 0;
}

static int handle_initialize(mcp_server_t server, const cJSON *params __attribute__((unused)),
                             cJSON **response)
{

    cJSON *caps = NULL;
    mcp_server_get_capabilities(&caps);

    cJSON *result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "protocolVersion", "2024-11-05");
    cJSON_AddStringToObject(result, "name",
                            server->config.name ? server->config.name : "agentos-mcp");
    cJSON_AddStringToObject(result, "version",
                            server->config.version ? server->config.version : "1.0.0");

    if (caps) {
        cJSON_AddItemToObject(result, "capabilities", caps);
    } else {
        cJSON *default_caps = cJSON_CreateObject();
        cJSON_AddItemToObject(result, "capabilities", default_caps);
    }

    *response = mcp_make_result(result);
    cJSON_Delete(result);
    return 0;
}

static int handle_ping(mcp_server_t server __attribute__((unused)),
                       const cJSON *params __attribute__((unused)), cJSON **response)
{

    cJSON *content_arr = cJSON_CreateArray();
    cJSON *text_content = cJSON_CreateObject();
    cJSON_AddStringToObject(text_content, "type", "text");
    cJSON_AddStringToObject(text_content, "text", "pong");
    cJSON_AddItemToArray(content_arr, text_content);

    *response = mcp_make_result(content_arr);
    cJSON_Delete(content_arr);
    return 0;
}

static int handle_tools_list(mcp_server_t server, const cJSON *params __attribute__((unused)),
                             cJSON **response)
{

    cJSON *tools_arr = cJSON_CreateArray();

    for (int i = 0; i < server->tool_count && server->config.enable_tools; i++) {
        cJSON *tool = cJSON_CreateObject();
        cJSON_AddStringToObject(tool, "name", server->tools[i].name);
        cJSON_AddStringToObject(tool, "description", server->tools[i].description);

        if (server->tools[i].input_schema) {
            cJSON_AddItemToObject(tool, "inputSchema",
                                  cJSON_Duplicate(server->tools[i].input_schema, 1));
        } else {
            cJSON *default_schema = cJSON_CreateObject();
            cJSON_AddStringToObject(default_schema, "type", "object");
            cJSON_AddItemToObject(tool, "inputSchema", default_schema);
        }

        cJSON_AddItemToArray(tools_arr, tool);
    }

    *response = mcp_make_result(tools_arr);
    cJSON_Delete(tools_arr);
    return 0;
}

static int handle_tools_call(mcp_server_t server, const cJSON *params, cJSON **response)
{
    if (!params) {
        *response =
            mcp_make_result(mcp_make_error(MCP_ERROR_INVALID_PARAMS, "Missing params", NULL));
        return MCP_ERROR_INVALID_PARAMS;
    }

    const char *tool_name = cJSON_GetObjectItem(params, "name")
                                ? cJSON_GetObjectItem(params, "name")->valuestring
                                : NULL;
    cJSON *arguments = cJSON_GetObjectItem(params, "arguments");
    int owns_arguments = 0;
    if (!arguments) {
        arguments = cJSON_CreateObject();
        owns_arguments = 1;
    }

    if (!tool_name) {
        if (owns_arguments)
            cJSON_Delete(arguments);
        *response =
            mcp_make_result(mcp_make_error(MCP_ERROR_INVALID_PARAMS, "Missing tool name", NULL));
        return MCP_ERROR_INVALID_PARAMS;
    }

    bool found = false;
    for (int i = 0; i < server->tool_count; i++) {
        if (strcmp(server->tools[i].name, tool_name) == 0) {
            found = true;
            cJSON *tool_result = NULL;
            int ret = server->tools[i].handler(tool_name, arguments, &tool_result,
                                               server->tools[i].user_data);

            if (ret != 0 || !tool_result) {
                char err_msg[256];
                snprintf(err_msg, sizeof(err_msg), "Tool '%s' execution failed: %d", tool_name,
                         ret);
                *response = mcp_make_result(mcp_make_error(MCP_ERROR_INTERNAL, err_msg, NULL));
                return MCP_ERROR_INTERNAL;
            }

            *response = mcp_make_result(tool_result);
            cJSON_Delete(tool_result);
            if (owns_arguments)
                cJSON_Delete(arguments);
            return 0;
        }
    }

    if (!found) {
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "Tool not found: %s", tool_name);
        *response = mcp_make_result(mcp_make_error(MCP_ERROR_METHOD_NOT_FOUND, err_msg, NULL));
        return MCP_ERROR_METHOD_NOT_FOUND;
    }

    if (arguments != cJSON_GetObjectItem(params, "arguments")) {
        cJSON_Delete(arguments);
    }
    return 0;
}

static int handle_resources_list(mcp_server_t server, const cJSON *params __attribute__((unused)),
                                 cJSON **response)
{

    cJSON *res_arr = cJSON_CreateArray();

    for (int i = 0; i < server->resource_count && server->config.enable_resources; i++) {
        cJSON *res = cJSON_CreateObject();
        cJSON_AddStringToObject(res, "uri", server->resources[i].uri);
        cJSON_AddStringToObject(res, "name", server->resources[i].name);
        cJSON_AddStringToObject(res, "description", server->resources[i].description);
        cJSON_AddStringToObject(res, "mimeType", server->resources[i].mime_type);
        cJSON_AddItemToArray(res_arr, res);
    }

    *response = mcp_make_result(res_arr);
    cJSON_Delete(res_arr);
    return 0;
}

static int handle_resources_read(mcp_server_t server, const cJSON *params, cJSON **response)
{
    if (!params) {
        *response =
            mcp_make_result(mcp_make_error(MCP_ERROR_INVALID_PARAMS, "Missing params", NULL));
        return MCP_ERROR_INVALID_PARAMS;
    }

    const char *uri =
        cJSON_GetObjectItem(params, "uri") ? cJSON_GetObjectItem(params, "uri")->valuestring : NULL;

    if (!uri) {
        *response =
            mcp_make_result(mcp_make_error(MCP_ERROR_INVALID_PARAMS, "Missing resource URI", NULL));
        return MCP_ERROR_INVALID_PARAMS;
    }

    for (int i = 0; i < server->resource_count; i++) {
        if (strcmp(server->resources[i].uri, uri) == 0) {
            cJSON *res_result = NULL;
            int ret =
                server->resources[i].handler(uri, &res_result, server->resources[i].user_data);

            if (ret != 0 || !res_result) {
                char err_msg[512];
                snprintf(err_msg, sizeof(err_msg), "Resource read failed for '%s': %d", uri, ret);
                *response = mcp_make_result(mcp_make_error(MCP_ERROR_INTERNAL, err_msg, NULL));
                return MCP_ERROR_INTERNAL;
            }

            *response = mcp_make_result(res_result);
            cJSON_Delete(res_result);
            return 0;
        }
    }

    char err_msg[512];
    snprintf(err_msg, sizeof(err_msg), "Resource not found: %s", uri);
    *response = mcp_make_result(mcp_make_error(MCP_ERROR_METHOD_NOT_FOUND, err_msg, NULL));
    return MCP_ERROR_METHOD_NOT_FOUND;
}

static int handle_prompts_list(mcp_server_t server, const cJSON *params __attribute__((unused)),
                               cJSON **response)
{

    cJSON *prom_arr = cJSON_CreateArray();

    for (int i = 0; i < server->prompt_count && server->config.enable_prompts; i++) {
        cJSON *prom = cJSON_CreateObject();
        cJSON_AddStringToObject(prom, "name", server->prompts[i].name);
        cJSON_AddStringToObject(prom, "description", server->prompts[i].description);

        if (server->prompts[i].argument_schema) {
            cJSON_AddItemToObject(prom, "arguments",
                                  cJSON_Duplicate(server->prompts[i].argument_schema, 1));
        }

        cJSON_AddItemToArray(prom_arr, prom);
    }

    *response = mcp_make_result(prom_arr);
    cJSON_Delete(prom_arr);
    return 0;
}

static int handle_prompts_get(mcp_server_t server, const cJSON *params, cJSON **response)
{
    if (!params) {
        *response =
            mcp_make_result(mcp_make_error(MCP_ERROR_INVALID_PARAMS, "Missing params", NULL));
        return MCP_ERROR_INVALID_PARAMS;
    }

    const char *prompt_name = cJSON_GetObjectItem(params, "name")
                                  ? cJSON_GetObjectItem(params, "name")->valuestring
                                  : NULL;
    cJSON *arguments = cJSON_GetObjectItem(params, "arguments");
    int owns_arguments = 0;
    if (!arguments) {
        arguments = cJSON_CreateObject();
        owns_arguments = 1;
    }

    if (!prompt_name) {
        if (owns_arguments)
            cJSON_Delete(arguments);
        *response =
            mcp_make_result(mcp_make_error(MCP_ERROR_INVALID_PARAMS, "Missing prompt name", NULL));
        return MCP_ERROR_INVALID_PARAMS;
    }

    for (int i = 0; i < server->prompt_count; i++) {
        if (strcmp(server->prompts[i].name, prompt_name) == 0) {
            cJSON *prom_result = NULL;
            int ret = server->prompts[i].handler(prompt_name, arguments, &prom_result,
                                                 server->prompts[i].user_data);

            if (ret != 0 || !prom_result) {
                char err_msg[256];
                snprintf(err_msg, sizeof(err_msg), "Prompt get failed for '%s': %d", prompt_name,
                         ret);
                if (owns_arguments)
                    cJSON_Delete(arguments);
                *response = mcp_make_result(mcp_make_error(MCP_ERROR_INTERNAL, err_msg, NULL));
                return MCP_ERROR_INTERNAL;
            }

            *response = mcp_make_result(prom_result);
            cJSON_Delete(prom_result);
            if (owns_arguments)
                cJSON_Delete(arguments);
            return 0;
        }
    }

    char err_msg[256];
    snprintf(err_msg, sizeof(err_msg), "Prompt not found: %s", prompt_name);
    *response = mcp_make_result(mcp_make_error(MCP_ERROR_METHOD_NOT_FOUND, err_msg, NULL));
    return MCP_ERROR_METHOD_NOT_FOUND;
}

int mcp_server_handle_request(mcp_server_t server, const char *method, const cJSON *params,
                              cJSON **response)
{
    AGENTOS_CHECK(server != NULL, AGENTOS_EFAIL, "server is NULL");
    AGENTOS_CHECK(method != NULL, AGENTOS_EFAIL, "method is NULL");
    AGENTOS_CHECK(response != NULL, AGENTOS_EFAIL, "response is NULL");

    server->request_count++;

    *response = NULL;

    if (strcmp(method, "initialize") == 0) {
        return handle_initialize(server, params, response);
    }
    if (strcmp(method, "ping") == 0) {
        return handle_ping(server, params, response);
    }
    if (strcmp(method, "tools/list") == 0) {
        return handle_tools_list(server, params, response);
    }
    if (strcmp(method, "tools/call") == 0) {
        return handle_tools_call(server, params, response);
    }
    if (strcmp(method, "resources/list") == 0) {
        return handle_resources_list(server, params, response);
    }
    if (strcmp(method, "resources/read") == 0) {
        return handle_resources_read(server, params, response);
    }
    if (strcmp(method, "prompts/list") == 0) {
        return handle_prompts_list(server, params, response);
    }
    if (strcmp(method, "prompts/get") == 0) {
        return handle_prompts_get(server, params, response);
    }

    char err_msg[256];
    snprintf(err_msg, sizeof(err_msg), "Unknown MCP method: %s", method);
    *response = mcp_make_result(mcp_make_error(MCP_ERROR_METHOD_NOT_FOUND, err_msg, NULL));
    return MCP_ERROR_METHOD_NOT_FOUND;
}

int mcp_server_get_capabilities(cJSON **caps)
{
    AGENTOS_CHECK(caps != NULL, AGENTOS_EFAIL, "caps is NULL");

    *caps = cJSON_CreateObject();

    cJSON *tools_cap = cJSON_CreateObject();
    cJSON_AddBoolToObject(tools_cap, "listChanged", 1);
    cJSON_AddItemToObject(*caps, "tools", tools_cap);

    cJSON *resources_cap = cJSON_CreateObject();
    cJSON_AddBoolToObject(resources_cap, "subscribe", 1);
    cJSON_AddBoolToObject(resources_cap, "listChanged", 1);
    cJSON_AddItemToObject(*caps, "resources", resources_cap);

    cJSON *prompts_cap = cJSON_CreateObject();
    cJSON_AddBoolToObject(prompts_cap, "listChanged", 1);
    cJSON_AddItemToObject(*caps, "prompts", prompts_cap);

    cJSON *logging_cap = cJSON_CreateObject();
    cJSON_AddStringToObject(logging_cap, "level", "info");
    cJSON_AddItemToObject(*caps, "logging", logging_cap);

    return 0;
}

const char *mcp_error_to_string(int code)
{
    switch (code) {
    case MCP_ERROR_INVALID_PARAMS:
        return "Invalid params";
    case MCP_ERROR_METHOD_NOT_FOUND:
        return "Method not found";
    case MCP_ERROR_INTERNAL:
        return "Internal error";
    default:
        return "Unknown error";
    }
}

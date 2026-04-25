#include "gateway_mcp_server.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define GW_MCP_MAX_TOOLS 256
#define GW_MCP_MAX_RESOURCES 128
#define GW_MCP_MAX_PROMPTS 64

typedef struct {
    char name[128];
    char description[512];
    char input_schema[2048];
    gw_mcp_tool_exec_fn exec_fn;
    void* user_data;
} gw_mcp_tool_entry_t;

typedef struct {
    char uri[512];
    char name[256];
    char description[512];
    char mime_type[64];
    gw_mcp_resource_read_fn read_fn;
    void* user_data;
} gw_mcp_resource_entry_t;

struct gw_mcp_server {
    gw_mcp_server_config_t config;
    gw_mcp_tool_entry_t tools[GW_MCP_MAX_TOOLS];
    size_t tool_count;
    gw_mcp_resource_entry_t resources[GW_MCP_MAX_RESOURCES];
    size_t resource_count;
    bool initialized;
    bool healthy;
    uint64_t request_count;
    uint64_t error_count;
};

static int handle_mcp_request(const char* method,
                               const char* path,
                               const char* body_json,
                               char** response_json,
                               void* user_data);

gw_mcp_server_t* gw_mcp_server_create(const gw_mcp_server_config_t* config)
{
    gw_mcp_server_t* server = (gw_mcp_server_t*)calloc(1, sizeof(gw_mcp_server_t));
    if (!server) return NULL;
    if (config) {
        server->config = *config;
    } else {
        gw_mcp_server_config_t defaults = GW_MCP_SERVER_CONFIG_DEFAULTS;
        server->config = defaults;
    }
    return server;
}

void gw_mcp_server_destroy(gw_mcp_server_t* server)
{
    if (!server) return;
    if (server->initialized) {
        gw_mcp_server_shutdown(server);
    }
    free(server);
}

int gw_mcp_server_init(gw_mcp_server_t* server)
{
    if (!server) return -1;
    if (server->initialized) return 0;
    server->initialized = true;
    server->healthy = true;
    server->request_count = 0;
    server->error_count = 0;
    return 0;
}

int gw_mcp_server_shutdown(gw_mcp_server_t* server)
{
    if (!server || !server->initialized) return -1;
    server->tool_count = 0;
    server->resource_count = 0;
    server->initialized = false;
    server->healthy = false;
    return 0;
}

int gw_mcp_server_register_tool(gw_mcp_server_t* server,
                                 const char* name,
                                 const char* description,
                                 const char* input_schema_json,
                                 gw_mcp_tool_exec_fn exec_fn,
                                 void* user_data)
{
    if (!server || !name || !exec_fn) return -1;
    if (server->tool_count >= GW_MCP_MAX_TOOLS) return -2;

    gw_mcp_tool_entry_t* entry = &server->tools[server->tool_count];
    strncpy(entry->name, name, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';
    if (description) {
        strncpy(entry->description, description, sizeof(entry->description) - 1);
        entry->description[sizeof(entry->description) - 1] = '\0';
    }
    if (input_schema_json) {
        strncpy(entry->input_schema, input_schema_json, sizeof(entry->input_schema) - 1);
        entry->input_schema[sizeof(entry->input_schema) - 1] = '\0';
    }
    entry->exec_fn = exec_fn;
    entry->user_data = user_data;
    server->tool_count++;
    return 0;
}

int gw_mcp_server_register_resource(gw_mcp_server_t* server,
                                     const char* uri,
                                     const char* name,
                                     const char* description,
                                     const char* mime_type,
                                     gw_mcp_resource_read_fn read_fn,
                                     void* user_data)
{
    if (!server || !uri || !read_fn) return -1;
    if (server->resource_count >= GW_MCP_MAX_RESOURCES) return -2;

    gw_mcp_resource_entry_t* entry = &server->resources[server->resource_count];
    strncpy(entry->uri, uri, sizeof(entry->uri) - 1);
    entry->uri[sizeof(entry->uri) - 1] = '\0';
    if (name) {
        strncpy(entry->name, name, sizeof(entry->name) - 1);
        entry->name[sizeof(entry->name) - 1] = '\0';
    }
    if (description) {
        strncpy(entry->description, description, sizeof(entry->description) - 1);
        entry->description[sizeof(entry->description) - 1] = '\0';
    }
    if (mime_type) {
        strncpy(entry->mime_type, mime_type, sizeof(entry->mime_type) - 1);
        entry->mime_type[sizeof(entry->mime_type) - 1] = '\0';
    }
    entry->read_fn = read_fn;
    entry->user_data = user_data;
    server->resource_count++;
    return 0;
}

static gw_mcp_tool_entry_t* find_tool(gw_mcp_server_t* server, const char* name)
{
    for (size_t i = 0; i < server->tool_count; i++) {
        if (strcmp(server->tools[i].name, name) == 0) {
            return &server->tools[i];
        }
    }
    return NULL;
}

static gw_mcp_resource_entry_t* find_resource(gw_mcp_server_t* server, const char* uri)
{
    for (size_t i = 0; i < server->resource_count; i++) {
        if (strcmp(server->resources[i].uri, uri) == 0) {
            return &server->resources[i];
        }
    }
    return NULL;
}

static char* build_tools_list_json(gw_mcp_server_t* server)
{
    size_t buf_size = 4096 + server->tool_count * 1024;
    char* buf = (char*)malloc(buf_size);
    if (!buf) return NULL;

    size_t pos = 0;
    pos += snprintf(buf + pos, buf_size - pos, "{\"tools\":[");
    for (size_t i = 0; i < server->tool_count; i++) {
        if (i > 0) pos += snprintf(buf + pos, buf_size - pos, ",");
        gw_mcp_tool_entry_t* t = &server->tools[i];
        pos += snprintf(buf + pos, buf_size - pos,
            "{\"name\":\"%s\",\"description\":\"%s\"}",
            t->name, t->description);
    }
    pos += snprintf(buf + pos, buf_size - pos, "]}");
    return buf;
}

static char* build_resources_list_json(gw_mcp_server_t* server)
{
    size_t buf_size = 4096 + server->resource_count * 1024;
    char* buf = (char*)malloc(buf_size);
    if (!buf) return NULL;

    size_t pos = 0;
    pos += snprintf(buf + pos, buf_size - pos, "{\"resources\":[");
    for (size_t i = 0; i < server->resource_count; i++) {
        if (i > 0) pos += snprintf(buf + pos, buf_size - pos, ",");
        gw_mcp_resource_entry_t* r = &server->resources[i];
        pos += snprintf(buf + pos, buf_size - pos,
            "{\"uri\":\"%s\",\"name\":\"%s\",\"description\":\"%s\",\"mimeType\":\"%s\"}",
            r->uri, r->name, r->description, r->mime_type);
    }
    pos += snprintf(buf + pos, buf_size - pos, "]}");
    return buf;
}

static char* extract_jsonrpc_method(const char* body)
{
    if (!body) return NULL;
    const char* key = "\"method\"";
    const char* p = strstr(body, key);
    if (!p) return NULL;
    p += strlen(key);
    while (*p && (*p == ' ' || *p == ':' || *p == '\t')) p++;
    if (*p != '"') return NULL;
    p++;
    const char* end = strchr(p, '"');
    if (!end) return NULL;
    size_t len = (size_t)(end - p);
    char* method = (char*)malloc(len + 1);
    if (!method) return NULL;
    memcpy(method, p, len);
    method[len] = '\0';
    return method;
}

static char* extract_jsonrpc_id(const char* body)
{
    if (!body) return NULL;
    const char* key = "\"id\"";
    const char* p = strstr(body, key);
    if (!p) return NULL;
    p += strlen(key);
    while (*p && (*p == ' ' || *p == ':' || *p == '\t')) p++;
    if (*p == '"') {
        p++;
        const char* end = strchr(p, '"');
        if (!end) return NULL;
        size_t len = (size_t)(end - p);
        char* id = (char*)malloc(len + 1);
        if (!id) return NULL;
        memcpy(id, p, len);
        id[len] = '\0';
        return id;
    }
    char* endptr = NULL;
    long val = strtol(p, &endptr, 10);
    if (endptr == p) return NULL;
    char* id = (char*)malloc(32);
    if (!id) return NULL;
    snprintf(id, 32, "%ld", val);
    return id;
}

static char* extract_jsonrpc_params(const char* body)
{
    if (!body) return NULL;
    const char* key = "\"params\"";
    const char* p = strstr(body, key);
    if (!p) return strdup("{}");
    p += strlen(key);
    while (*p && (*p == ' ' || *p == ':' || *p == '\t')) p++;
    if (*p != '{' && *p != '[') return strdup("{}");
    char open = *p;
    char close = (open == '{') ? '}' : ']';
    int depth = 0;
    const char* start = p;
    while (*p) {
        if (*p == open) depth++;
        else if (*p == close) {
            depth--;
            if (depth == 0) {
                p++;
                size_t len = (size_t)(p - start);
                char* params = (char*)malloc(len + 1);
                if (!params) return strdup("{}");
                memcpy(params, start, len);
                params[len] = '\0';
                return params;
            }
        }
        p++;
    }
    return strdup("{}");
}

static char* extract_tool_name_from_params(const char* params_json)
{
    if (!params_json) return NULL;
    const char* key = "\"name\"";
    const char* p = strstr(params_json, key);
    if (!p) return NULL;
    p += strlen(key);
    while (*p && (*p == ' ' || *p == ':' || *p == '\t')) p++;
    if (*p != '"') return NULL;
    p++;
    const char* end = strchr(p, '"');
    if (!end) return NULL;
    size_t len = (size_t)(end - p);
    char* name = (char*)malloc(len + 1);
    if (!name) return NULL;
    memcpy(name, p, len);
    name[len] = '\0';
    return name;
}

static char* extract_tool_args_from_params(const char* params_json)
{
    if (!params_json) return strdup("{}");
    const char* key = "\"arguments\"";
    const char* p = strstr(params_json, key);
    if (!p) return strdup("{}");
    p += strlen(key);
    while (*p && (*p == ' ' || *p == ':' || *p == '\t')) p++;
    if (*p != '{' && *p != '[') return strdup("{}");
    char open = *p;
    char close = (open == '{') ? '}' : ']';
    int depth = 0;
    const char* start = p;
    while (*p) {
        if (*p == open) depth++;
        else if (*p == close) {
            depth--;
            if (depth == 0) {
                p++;
                size_t len = (size_t)(p - start);
                char* args = (char*)malloc(len + 1);
                if (!args) return strdup("{}");
                memcpy(args, start, len);
                args[len] = '\0';
                return args;
            }
        }
        p++;
    }
    return strdup("{}");
}

static char* extract_resource_uri_from_params(const char* params_json)
{
    if (!params_json) return NULL;
    const char* key = "\"uri\"";
    const char* p = strstr(params_json, key);
    if (!p) return NULL;
    p += strlen(key);
    while (*p && (*p == ' ' || *p == ':' || *p == '\t')) p++;
    if (*p != '"') return NULL;
    p++;
    const char* end = strchr(p, '"');
    if (!end) return NULL;
    size_t len = (size_t)(end - p);
    char* uri = (char*)malloc(len + 1);
    if (!uri) return NULL;
    memcpy(uri, p, len);
    uri[len] = '\0';
    return uri;
}

int gw_mcp_server_handle_jsonrpc(gw_mcp_server_t* server,
                                  const char* method,
                                  const char* params_json,
                                  char** response_json)
{
    if (!server || !method || !response_json) return -1;
    server->request_count++;

    if (strcmp(method, "initialize") == 0) {
        const char* resp = "{\"jsonrpc\":\"2.0\",\"result\":{"
            "\"protocolVersion\":\"2024-11-05\","
            "\"capabilities\":{\"tools\":{\"listChanged\":true},"
            "\"resources\":{\"subscribe\":true,\"listChanged\":true}},"
            "\"serverInfo\":{\"name\":\"%s\",\"version\":\"%s\"}}}";
        size_t len = snprintf(NULL, 0, resp, server->config.server_name, server->config.server_version);
        char* buf = (char*)malloc(len + 1);
        if (!buf) { server->error_count++; return -1; }
        snprintf(buf, len + 1, resp, server->config.server_name, server->config.server_version);
        *response_json = buf;
        return 0;
    }

    if (strcmp(method, "tools/list") == 0) {
        *response_json = build_tools_list_json(server);
        if (!*response_json) { server->error_count++; return -1; }
        return 0;
    }

    if (strcmp(method, "tools/call") == 0) {
        char* tool_name = extract_tool_name_from_params(params_json);
        char* tool_args = extract_tool_args_from_params(params_json);
        if (!tool_name) {
            free(tool_args);
            server->error_count++;
            return -1;
        }
        gw_mcp_tool_entry_t* tool = find_tool(server, tool_name);
        if (!tool) {
            const char* err = "{\"jsonrpc\":\"2.0\",\"error\":"
                "{\"code\":-32601,\"message\":\"Tool not found: %s\"}}";
            size_t elen = snprintf(NULL, 0, err, tool_name);
            char* ebuf = (char*)malloc(elen + 1);
            if (ebuf) snprintf(ebuf, elen + 1, err, tool_name);
            *response_json = ebuf;
            free(tool_name);
            free(tool_args);
            server->error_count++;
            return -2;
        }
        char* tool_result = NULL;
        int rc = tool->exec_fn(tool_name, tool_args, &tool_result, tool->user_data);
        free(tool_name);
        free(tool_args);
        if (rc != 0 || !tool_result) {
            const char* err = "{\"jsonrpc\":\"2.0\",\"error\":"
                "{\"code\":-32603,\"message\":\"Tool execution failed\"}}";
            *response_json = strdup(err);
            free(tool_result);
            server->error_count++;
            return -3;
        }
        const char* resp_fmt = "{\"jsonrpc\":\"2.0\",\"result\":{"
            "\"content\":[{\"type\":\"text\",\"text\":%s}]}}";
        size_t rlen = snprintf(NULL, 0, resp_fmt, tool_result);
        char* buf = (char*)malloc(rlen + 1);
        if (buf) snprintf(buf, rlen + 1, resp_fmt, tool_result);
        *response_json = buf;
        free(tool_result);
        return 0;
    }

    if (strcmp(method, "resources/list") == 0) {
        *response_json = build_resources_list_json(server);
        if (!*response_json) { server->error_count++; return -1; }
        return 0;
    }

    if (strcmp(method, "resources/read") == 0) {
        char* uri = extract_resource_uri_from_params(params_json);
        if (!uri) { server->error_count++; return -1; }
        gw_mcp_resource_entry_t* res = find_resource(server, uri);
        if (!res) {
            free(uri);
            server->error_count++;
            return -2;
        }
        char* content = NULL;
        char* mime = NULL;
        int rc = res->read_fn(uri, &content, &mime, res->user_data);
        free(uri);
        if (rc != 0 || !content) {
            free(content);
            free(mime);
            server->error_count++;
            return -3;
        }
        const char* resp_fmt = "{\"jsonrpc\":\"2.0\",\"result\":{"
            "\"contents\":[{\"uri\":\"%s\",\"mimeType\":\"%s\",\"text\":%s}]}}";
        size_t rlen = snprintf(NULL, 0, resp_fmt, res->uri,
                                mime ? mime : "text/plain", content);
        char* buf = (char*)malloc(rlen + 1);
        if (buf) snprintf(buf, rlen + 1, resp_fmt, res->uri,
                           mime ? mime : "text/plain", content);
        *response_json = buf;
        free(content);
        free(mime);
        return 0;
    }

    if (strcmp(method, "ping") == 0) {
        *response_json = strdup("{\"jsonrpc\":\"2.0\",\"result\":{}}");
        return 0;
    }

    server->error_count++;
    return -1;
}

int gw_mcp_server_handle_request(gw_mcp_server_t* server,
                                  const char* method,
                                  const char* path,
                                  const char* body_json,
                                  char** response_json)
{
    if (!server || !body_json || !response_json) return -1;

    char* rpc_method = extract_jsonrpc_method(body_json);
    if (!rpc_method) {
        server->error_count++;
        return -1;
    }

    char* rpc_params = extract_jsonrpc_params(body_json);
    int rc = gw_mcp_server_handle_jsonrpc(server, rpc_method, rpc_params, response_json);
    free(rpc_method);
    free(rpc_params);
    return rc;
}

static int handle_mcp_request(const char* method,
                               const char* path,
                               const char* body_json,
                               char** response_json,
                               void* user_data)
{
    gw_mcp_server_t* server = (gw_mcp_server_t*)user_data;
    if (!server) return -1;
    return gw_mcp_server_handle_request(server, method, path, body_json, response_json);
}

gw_proto_request_handler_t gw_mcp_server_get_handler(gw_mcp_server_t* server)
{
    if (!server) return NULL;
    return handle_mcp_request;
}

void* gw_mcp_server_get_handler_data(gw_mcp_server_t* server)
{
    return (void*)server;
}

bool gw_mcp_server_is_healthy(gw_mcp_server_t* server)
{
    if (!server) return false;
    return server->healthy;
}

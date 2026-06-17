// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file mcp_v1_adapter.c
 * @brief MCP v1.0 Protocol Adapter Implementation
 */

#include "mcp_v1_adapter.h"

#include "mcp_transport.h"
#include "memory_compat.h"
#include "error.h"
#include "types.h"
#include "unified_protocol.h"

#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Fallback logging macros if not provided by error.h */
#ifndef AGENTOS_LOG_ERROR
#define AGENTOS_LOG_ERROR(fmt, ...) __builtin_fprintf(stderr, "[ERROR] %s: " fmt "\n", __func__, ##__VA_ARGS__)
#endif
#ifndef AGENTOS_LOG_WARN
#define AGENTOS_LOG_WARN(fmt, ...) __builtin_fprintf(stderr, "[WARN] %s: " fmt "\n", __func__, ##__VA_ARGS__)
#endif

typedef struct {
    mcp_tool_t tool;
    mcp_tool_handler_t handler;
    void *user_data;
} mcp_tool_entry_t;

typedef struct {
    mcp_resource_t resource;
    mcp_resource_handler_t handler;
    void *user_data;
} mcp_resource_entry_t;

typedef struct {
    mcp_prompt_t prompt;
    mcp_prompt_handler_t handler;
    void *user_data;
} mcp_prompt_entry_t;

struct mcp_v1_context_s {
    mcp_v1_config_t config;

    mcp_tool_entry_t *tools;
    size_t tool_count;
    size_t tool_capacity;

    mcp_resource_entry_t *resources;
    size_t resource_count;
    size_t resource_capacity;

    mcp_resource_template_t *resource_templates;
    size_t template_count;
    size_t template_capacity;

    mcp_prompt_entry_t *prompts;
    size_t prompt_count;
    size_t prompt_capacity;

    mcp_sampling_handler_t sampling_handler;
    void *sampling_user_data;

    mcp_completion_handler_t completion_handler;
    void *completion_user_data;

    mcp_progress_callback_t progress_callback;
    void *progress_user_data;

    mcp_log_callback_t log_callback;
    void *log_user_data;

    mcp_log_level_t log_level;

    mcp_transport_t *transport;

    uint64_t request_counter;

    mcp_stream_config_t stream_config;
    int stream_active;
};

static char *json_string_escape(const char *str)
{
    if (!str)
        return AGENTOS_STRDUP("null");
    size_t len = strlen(str);
    size_t escaped_len = len * 2 + 3;
    char *escaped = AGENTOS_MALLOC(escaped_len);
    if (!escaped)
        return NULL;
    size_t j = 0;
    escaped[j++] = '"';
    for (size_t i = 0; i < len; i++) {
        switch (str[i]) {
        case '"':
            escaped[j++] = '\\';
            escaped[j++] = '"';
            break;
        case '\\':
            escaped[j++] = '\\';
            escaped[j++] = '\\';
            break;
        case '\n':
            escaped[j++] = '\\';
            escaped[j++] = 'n';
            break;
        case '\r':
            escaped[j++] = '\\';
            escaped[j++] = 'r';
            break;
        case '\t':
            escaped[j++] = '\\';
            escaped[j++] = 't';
            break;
        default:
            escaped[j++] = str[i];
            break;
        }
    }
    escaped[j++] = '"';
    escaped[j] = '\0';
    return escaped;
}

static char *strdup_safe(const char *s)
{
    return s ? AGENTOS_STRDUP(s) : NULL;
}

mcp_v1_config_t mcp_v1_config_default(void)
{
    mcp_v1_config_t config = {0};
    config.capabilities = MCP_CAP_TOOLS | MCP_CAP_RESOURCES | MCP_CAP_PROMPTS | MCP_CAP_LOGGING;
    config.default_timeout_ms = MCP_V1_DEFAULT_TIMEOUT_MS;
    config.max_tools = MCP_V1_MAX_TOOLS;
    config.max_resources = MCP_V1_MAX_RESOURCES;
    config.max_message_size = MCP_V1_MAX_MESSAGE_SIZE;
    config.enable_progress_notifications = true;
    config.enable_cancellation = true;
    config.enable_sampling = false;
    config.server_name = strdup_safe("AgentOS MCP Server");
    config.server_version = strdup_safe(MCP_V1_VERSION);
    return config;
}

mcp_v1_context_t *mcp_v1_context_create(const mcp_v1_config_t *config)
{
    mcp_v1_context_t *ctx = AGENTOS_CALLOC(1, sizeof(mcp_v1_context_t));
    if (!ctx) {
        AGENTOS_LOG_ERROR("context allocation failed, size=%zu", sizeof(mcp_v1_context_t));
        return NULL;
    }

    if (config) {
        ctx->config = *config;
        ctx->config.server_name = strdup_safe(config->server_name);
        ctx->config.server_version = strdup_safe(config->server_version);
    } else {
        ctx->config = mcp_v1_config_default();
    }

    ctx->tool_capacity = 32;
    ctx->tools = AGENTOS_CALLOC(ctx->tool_capacity, sizeof(mcp_tool_entry_t));
    if (!ctx->tools) {
        AGENTOS_LOG_ERROR("tools array allocation failed, capacity=%zu", ctx->tool_capacity);
        AGENTOS_FREE(ctx);
        return NULL;
    }

    ctx->resource_capacity = 16;
    ctx->resources = AGENTOS_CALLOC(ctx->resource_capacity, sizeof(mcp_resource_entry_t));
    if (!ctx->resources) {
        AGENTOS_LOG_ERROR("resources array allocation failed, capacity=%zu", ctx->resource_capacity);
        AGENTOS_FREE(ctx->tools);
        AGENTOS_FREE(ctx);
        return NULL;
    }

    ctx->template_capacity = 16;
    ctx->resource_templates =
        AGENTOS_CALLOC(ctx->template_capacity, sizeof(mcp_resource_template_t));
    if (!ctx->resource_templates) {
        AGENTOS_LOG_ERROR("resource_templates allocation failed, capacity=%zu", ctx->template_capacity);
        AGENTOS_FREE(ctx->resources);
        AGENTOS_FREE(ctx->tools);
        AGENTOS_FREE(ctx);
        return NULL;
    }

    ctx->prompt_capacity = 16;
    ctx->prompts = AGENTOS_CALLOC(ctx->prompt_capacity, sizeof(mcp_prompt_entry_t));
    if (!ctx->prompts) {
        AGENTOS_LOG_ERROR("prompts array allocation failed, capacity=%zu", ctx->prompt_capacity);
        AGENTOS_FREE(ctx->resource_templates);
        AGENTOS_FREE(ctx->resources);
        AGENTOS_FREE(ctx->tools);
        AGENTOS_FREE(ctx);
        return NULL;
    }

    ctx->log_level = MCP_LOG_INFO;
    ctx->request_counter = 0;

    return ctx;
}

void mcp_v1_context_destroy(mcp_v1_context_t *ctx)
{
    if (!ctx)
        return;

    for (size_t i = 0; i < ctx->tool_count; i++) {
        AGENTOS_FREE(ctx->tools[i].tool.name);
        AGENTOS_FREE(ctx->tools[i].tool.description);
        AGENTOS_FREE(ctx->tools[i].tool.input_schema_json);
    }
    AGENTOS_FREE(ctx->tools);

    for (size_t i = 0; i < ctx->resource_count; i++) {
        AGENTOS_FREE(ctx->resources[i].resource.uri);
        AGENTOS_FREE(ctx->resources[i].resource.name);
        AGENTOS_FREE(ctx->resources[i].resource.description);
        AGENTOS_FREE(ctx->resources[i].resource.mime_type);
    }
    AGENTOS_FREE(ctx->resources);

    for (size_t i = 0; i < ctx->template_count; i++) {
        AGENTOS_FREE(ctx->resource_templates[i].uri_template);
        AGENTOS_FREE(ctx->resource_templates[i].name);
        AGENTOS_FREE(ctx->resource_templates[i].description);
        AGENTOS_FREE(ctx->resource_templates[i].mime_type);
    }
    AGENTOS_FREE(ctx->resource_templates);

    for (size_t i = 0; i < ctx->prompt_count; i++) {
        AGENTOS_FREE(ctx->prompts[i].prompt.name);
        AGENTOS_FREE(ctx->prompts[i].prompt.description);
        AGENTOS_FREE(ctx->prompts[i].prompt.arguments_schema_json);
    }
    AGENTOS_FREE(ctx->prompts);

    AGENTOS_FREE(ctx->config.server_name);
    AGENTOS_FREE(ctx->config.server_version);
    AGENTOS_FREE(ctx);
}

int mcp_v1_register_tool(mcp_v1_context_t *ctx, const mcp_tool_t *tool, mcp_tool_handler_t handler,
                         void *user_data)
{
    if (!ctx || !tool || !handler)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "mcp_v1_register_tool: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    if (ctx->tool_count >= ctx->config.max_tools) {
        agentos_error_push_ex(AGENTOS_ERR_INVALID_PARAM, __FILE__, __LINE__, __func__, "mcp_v1_adapter: invalid parameter");
        return AGENTOS_ERR_INVALID_PARAM;
        }

    if (ctx->tool_count >= ctx->tool_capacity) {
        size_t new_cap = ctx->tool_capacity * 2;
        mcp_tool_entry_t *new_tools =
            AGENTOS_REALLOC(ctx->tools, new_cap * sizeof(mcp_tool_entry_t));
        if (!new_tools) {
            agentos_error_push_ex(AGENTOS_ERR_NULL_POINTER, __FILE__, __LINE__, __func__, "mcp_v1_adapter: null pointer");
            return AGENTOS_ERR_NULL_POINTER;
            }
        ctx->tools = new_tools;
        ctx->tool_capacity = new_cap;
    }

    mcp_tool_entry_t *entry = &ctx->tools[ctx->tool_count];
    entry->tool.name = strdup_safe(tool->name);
    entry->tool.description = strdup_safe(tool->description);
    entry->tool.input_schema_json = strdup_safe(tool->input_schema_json);
    entry->tool.required_caps = tool->required_caps;
    entry->handler = handler;
    entry->user_data = user_data;
    ctx->tool_count++;

    return 0;
}

int mcp_v1_register_resource(mcp_v1_context_t *ctx, const mcp_resource_t *resource,
                             mcp_resource_handler_t handler, void *user_data)
{
    if (!ctx || !resource)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "mcp_v1_register_resource: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    if (ctx->resource_count >= ctx->config.max_resources) {
        agentos_error_push_ex(AGENTOS_ERR_INVALID_PARAM, __FILE__, __LINE__, __func__, "mcp_v1_adapter: invalid parameter");
        return AGENTOS_ERR_INVALID_PARAM;
        }

    if (ctx->resource_count >= ctx->resource_capacity) {
        size_t new_cap = ctx->resource_capacity * 2;
        mcp_resource_entry_t *new_res =
            AGENTOS_REALLOC(ctx->resources, new_cap * sizeof(mcp_resource_entry_t));
        if (!new_res) {
            agentos_error_push_ex(AGENTOS_ERR_NULL_POINTER, __FILE__, __LINE__, __func__, "mcp_v1_adapter: null pointer");
            return AGENTOS_ERR_NULL_POINTER;
            }
        ctx->resources = new_res;
        ctx->resource_capacity = new_cap;
    }

    mcp_resource_entry_t *entry = &ctx->resources[ctx->resource_count];
    entry->resource.uri = strdup_safe(resource->uri);
    entry->resource.name = strdup_safe(resource->name);
    entry->resource.description = strdup_safe(resource->description);
    entry->resource.mime_type = strdup_safe(resource->mime_type);
    entry->handler = handler;
    entry->user_data = user_data;
    ctx->resource_count++;

    return 0;
}

int mcp_v1_register_resource_template(mcp_v1_context_t *ctx,
                                      const mcp_resource_template_t *template)
{
    if (!ctx || !template)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "mcp_v1_register_resource_template: failed");
        return AGENTOS_ERR_UNKNOWN;
        }

    if (ctx->template_count >= ctx->template_capacity) {
        size_t new_cap = ctx->template_capacity * 2;
        mcp_resource_template_t *new_tpl =
            AGENTOS_REALLOC(ctx->resource_templates, new_cap * sizeof(mcp_resource_template_t));
        if (!new_tpl) {
            agentos_error_push_ex(AGENTOS_ERR_NULL_POINTER, __FILE__, __LINE__, __func__, "mcp_v1_adapter: null pointer");
            return AGENTOS_ERR_NULL_POINTER;
            }
        ctx->resource_templates = new_tpl;
        ctx->template_capacity = new_cap;
    }

    mcp_resource_template_t *entry = &ctx->resource_templates[ctx->template_count];
    entry->uri_template = strdup_safe(template->uri_template);
    entry->name = strdup_safe(template->name);
    entry->description = strdup_safe(template->description);
    entry->mime_type = strdup_safe(template->mime_type);
    ctx->template_count++;

    return 0;
}

int mcp_v1_register_prompt(mcp_v1_context_t *ctx, const mcp_prompt_t *prompt,
                           mcp_prompt_handler_t handler, void *user_data)
{
    if (!ctx || !prompt)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "mcp_v1_register_prompt: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    if (ctx->prompt_count >= MCP_V1_MAX_PROMPTS) {
        agentos_error_push_ex(AGENTOS_ERR_INVALID_PARAM, __FILE__, __LINE__, __func__, "mcp_v1_adapter: invalid parameter");
        return AGENTOS_ERR_INVALID_PARAM;
        }

    if (ctx->prompt_count >= ctx->prompt_capacity) {
        size_t new_cap = ctx->prompt_capacity * 2;
        mcp_prompt_entry_t *new_prompts =
            AGENTOS_REALLOC(ctx->prompts, new_cap * sizeof(mcp_prompt_entry_t));
        if (!new_prompts) {
            agentos_error_push_ex(AGENTOS_ERR_NULL_POINTER, __FILE__, __LINE__, __func__, "mcp_v1_adapter: null pointer");
            return AGENTOS_ERR_NULL_POINTER;
            }
        ctx->prompts = new_prompts;
        ctx->prompt_capacity = new_cap;
    }

    mcp_prompt_entry_t *entry = &ctx->prompts[ctx->prompt_count];
    entry->prompt.name = strdup_safe(prompt->name);
    entry->prompt.description = strdup_safe(prompt->description);
    entry->prompt.arguments_schema_json = strdup_safe(prompt->arguments_schema_json);
    entry->handler = handler;
    entry->user_data = user_data;
    ctx->prompt_count++;

    return 0;
}

int mcp_v1_set_sampling_handler(mcp_v1_context_t *ctx, mcp_sampling_handler_t handler,
                                void *user_data)
{
    if (!ctx)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "mcp_v1_set_sampling_handler: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    ctx->sampling_handler = handler;
    ctx->sampling_user_data = user_data;
    if (handler)
        ctx->config.capabilities |= MCP_CAP_SAMPLING;
    return 0;
}

int mcp_v1_set_completion_handler(mcp_v1_context_t *ctx, mcp_completion_handler_t handler,
                                  void *user_data)
{
    if (!ctx)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "mcp_v1_set_completion_handler: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    ctx->completion_handler = handler;
    ctx->completion_user_data = user_data;
    if (handler)
        ctx->config.capabilities |= MCP_CAP_COMPLETION;
    return 0;
}

int mcp_v1_set_progress_callback(mcp_v1_context_t *ctx, mcp_progress_callback_t callback,
                                 void *user_data)
{
    if (!ctx)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "mcp_v1_set_progress_callback: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    ctx->progress_callback = callback;
    ctx->progress_user_data = user_data;
    return 0;
}

int mcp_v1_set_log_callback(mcp_v1_context_t *ctx, mcp_log_callback_t callback, void *user_data)
{
    if (!ctx)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "mcp_v1_set_log_callback: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    ctx->log_callback = callback;
    ctx->log_user_data = user_data;
    return 0;
}

int mcp_v1_set_log_level(mcp_v1_context_t *ctx, mcp_log_level_t level)
{
    if (!ctx)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "mcp_v1_set_log_level: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    ctx->log_level = level;
    return 0;
}

int mcp_v1_handle_tools_list(mcp_v1_context_t *ctx, char **response_json)
{
    if (!ctx || !response_json)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "mcp_v1_handle_tools_list: failed");
        return AGENTOS_ERR_UNKNOWN;
        }

    size_t buf_size = 4096 + ctx->tool_count * 512;
    char *buf = AGENTOS_MALLOC(buf_size);
    if (!buf) {
        agentos_error_push_ex(AGENTOS_ERR_NULL_POINTER, __FILE__, __LINE__, __func__, "mcp_v1_adapter: null pointer");
        return AGENTOS_ERR_NULL_POINTER;
        }

    size_t offset = 0;
    offset += snprintf(buf + offset, buf_size - offset, "{\"tools\":[");

    for (size_t i = 0; i < ctx->tool_count; i++) {
        if (i > 0)
            offset += snprintf(buf + offset, buf_size - offset, ",");
        char *name = json_string_escape(ctx->tools[i].tool.name);
        char *desc = json_string_escape(ctx->tools[i].tool.description);
        offset += snprintf(
            buf + offset, buf_size - offset, "{\"name\":%s,\"description\":%s,\"inputSchema\":%s}",
            name, desc,
            ctx->tools[i].tool.input_schema_json ? ctx->tools[i].tool.input_schema_json : "{}");
        AGENTOS_FREE(name);
        AGENTOS_FREE(desc);
    }

    offset += snprintf(buf + offset, buf_size - offset, "]}");
    *response_json = buf;
    return 0;
}

int mcp_v1_handle_tools_call(mcp_v1_context_t *ctx, const char *name, const char *arguments_json,
                             char **response_json)
{
    if (!ctx || !name || !response_json)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "mcp_v1_handle_tools_call: failed");
        return AGENTOS_ERR_UNKNOWN;
        }

    mcp_tool_entry_t *found = NULL;
    for (size_t i = 0; i < ctx->tool_count; i++) {
        if (strcmp(ctx->tools[i].tool.name, name) == 0) {
            found = &ctx->tools[i];
            break;
        }
    }

    if (!found) {
        AGENTOS_LOG_WARN("tool not found: name=%s, tool_count=%zu", name, ctx->tool_count);
        char *name_esc = json_string_escape(name);
        const char *safe_name = name_esc ? name_esc : name;
        size_t len = snprintf(
            NULL, 0,
            "{\"isError\":true,\"content\":[{\"type\":\"text\",\"text\":\"Tool not found: %s\"}]}",
            safe_name);
        char *resp = AGENTOS_MALLOC(len + 1);
        if (resp) {
            snprintf(resp, len + 1,
                     "{\"isError\":true,\"content\":[{\"type\":\"text\",\"text\":\"Tool not found: "
                     "%s\"}]}",
                     safe_name);
        }
        AGENTOS_FREE(name_esc);
        *response_json = resp;
        agentos_error_push_ex(AGENTOS_ERR_INVALID_PARAM, __FILE__, __LINE__, __func__, "mcp_v1_adapter: invalid parameter");
        return AGENTOS_ERR_INVALID_PARAM;
    }

    mcp_content_t *results = NULL;
    size_t result_count = 0;
    bool is_error = false;

    found->handler(name, arguments_json, &results, &result_count, &is_error, found->user_data);

    size_t buf_size = 4096 + result_count * 1024;
    char *buf = AGENTOS_MALLOC(buf_size);
    if (!buf) {
        mcp_content_destroy(results, result_count);
        agentos_error_push_ex(AGENTOS_ERR_NULL_POINTER, __FILE__, __LINE__, __func__, "mcp_v1_adapter: null pointer");
        return AGENTOS_ERR_NULL_POINTER;
    }

    size_t offset = 0;
    offset += snprintf(buf + offset, buf_size - offset, "{\"isError\":%s,\"content\":[",
                       is_error ? "true" : "false");

    for (size_t i = 0; i < result_count; i++) {
        if (i > 0)
            offset += snprintf(buf + offset, buf_size - offset, ",");
        const char *type_str = "text";
        switch (results[i].type) {
        case MCP_CONTENT_IMAGE:
            type_str = "image";
            break;
        case MCP_CONTENT_RESOURCE:
            type_str = "resource";
            break;
        case MCP_CONTENT_EMBEDDED:
            type_str = "embedded";
            break;
        default:
            break;
        }
        char *text_esc = json_string_escape(results[i].text);
        offset += snprintf(buf + offset, buf_size - offset, "{\"type\":\"%s\",\"text\":%s}",
                           type_str, text_esc);
        AGENTOS_FREE(text_esc);
    }

    offset += snprintf(buf + offset, buf_size - offset, "]}");
    *response_json = buf;

    mcp_content_destroy(results, result_count);
    return 0;
}

int mcp_v1_handle_resources_list(mcp_v1_context_t *ctx, char **response_json)
{
    if (!ctx || !response_json)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "mcp_v1_handle_resources_list: failed");
        return AGENTOS_ERR_UNKNOWN;
        }

    size_t buf_size = 4096 + ctx->resource_count * 512;
    char *buf = AGENTOS_MALLOC(buf_size);
    if (!buf) {
        agentos_error_push_ex(AGENTOS_ERR_NULL_POINTER, __FILE__, __LINE__, __func__, "mcp_v1_adapter: null pointer");
        return AGENTOS_ERR_NULL_POINTER;
        }

    size_t offset = 0;
    offset += snprintf(buf + offset, buf_size - offset, "{\"resources\":[");

    for (size_t i = 0; i < ctx->resource_count; i++) {
        if (i > 0)
            offset += snprintf(buf + offset, buf_size - offset, ",");
        char *uri = json_string_escape(ctx->resources[i].resource.uri);
        char *name = json_string_escape(ctx->resources[i].resource.name);
        char *desc = json_string_escape(ctx->resources[i].resource.description);
        char *mime = json_string_escape(ctx->resources[i].resource.mime_type);
        offset += snprintf(buf + offset, buf_size - offset,
                           "{\"uri\":%s,\"name\":%s,\"description\":%s,\"mimeType\":%s}", uri, name,
                           desc, mime);
        AGENTOS_FREE(uri);
        AGENTOS_FREE(name);
        AGENTOS_FREE(desc);
        AGENTOS_FREE(mime);
    }

    offset += snprintf(buf + offset, buf_size - offset, "]}");
    *response_json = buf;
    return 0;
}

int mcp_v1_handle_resources_read(mcp_v1_context_t *ctx, const char *uri, char **response_json)
{
    if (!ctx || !uri || !response_json)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "mcp_v1_handle_resources_read: failed");
        return AGENTOS_ERR_UNKNOWN;
        }

    mcp_resource_entry_t *found = NULL;
    for (size_t i = 0; i < ctx->resource_count; i++) {
        if (strcmp(ctx->resources[i].resource.uri, uri) == 0) {
            found = &ctx->resources[i];
            break;
        }
    }

    if (!found || !found->handler) {
        AGENTOS_LOG_WARN("resource not found or no handler: uri=%s, resource_count=%zu", uri, ctx->resource_count);
        char *uri_esc = json_string_escape(uri);
        size_t len = snprintf(
            NULL, 0, "{\"contents\":[{\"uri\":%s,\"text\":\"Resource not found\"}]}", uri_esc);
        char *resp = AGENTOS_MALLOC(len + 1);
        if (resp)
            snprintf(resp, len + 1, "{\"contents\":[{\"uri\":%s,\"text\":\"Resource not found\"}]}",
                     uri_esc);
        AGENTOS_FREE(uri_esc);
        *response_json = resp;
        agentos_error_push_ex(AGENTOS_ERR_INVALID_PARAM, __FILE__, __LINE__, __func__, "mcp_v1_adapter: invalid parameter");
        return AGENTOS_ERR_INVALID_PARAM;
    }

    char *content = NULL;
    char *mime_type = NULL;
    found->handler(uri, &content, &mime_type, found->user_data);

    char *uri_esc = json_string_escape(uri);
    char *content_esc = json_string_escape(content);
    char *mime_esc = json_string_escape(mime_type);

    size_t len = snprintf(NULL, 0, "{\"contents\":[{\"uri\":%s,\"mimeType\":%s,\"text\":%s}]}",
                          uri_esc, mime_esc, content_esc);
    char *resp = AGENTOS_MALLOC(len + 1);
    if (resp)
        snprintf(resp, len + 1, "{\"contents\":[{\"uri\":%s,\"mimeType\":%s,\"text\":%s}]}",
                 uri_esc, mime_esc, content_esc);

    AGENTOS_FREE(uri_esc);
    AGENTOS_FREE(content_esc);
    AGENTOS_FREE(mime_esc);
    AGENTOS_FREE(content);
    AGENTOS_FREE(mime_type);
    *response_json = resp;
    return 0;
}

int mcp_v1_handle_resources_templates(mcp_v1_context_t *ctx, char **response_json)
{
    if (!ctx || !response_json)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "mcp_v1_handle_resources_templates: failed");
        return AGENTOS_ERR_UNKNOWN;
        }

    size_t buf_size = 4096 + ctx->template_count * 512;
    char *buf = AGENTOS_MALLOC(buf_size);
    if (!buf) {
        agentos_error_push_ex(AGENTOS_ERR_NULL_POINTER, __FILE__, __LINE__, __func__, "mcp_v1_adapter: null pointer");
        return AGENTOS_ERR_NULL_POINTER;
        }

    size_t offset = 0;
    offset += snprintf(buf + offset, buf_size - offset, "{\"resourceTemplates\":[");

    for (size_t i = 0; i < ctx->template_count; i++) {
        if (i > 0)
            offset += snprintf(buf + offset, buf_size - offset, ",");
        char *uri = json_string_escape(ctx->resource_templates[i].uri_template);
        char *name = json_string_escape(ctx->resource_templates[i].name);
        char *desc = json_string_escape(ctx->resource_templates[i].description);
        offset += snprintf(buf + offset, buf_size - offset,
                           "{\"uriTemplate\":%s,\"name\":%s,\"description\":%s}", uri, name, desc);
        AGENTOS_FREE(uri);
        AGENTOS_FREE(name);
        AGENTOS_FREE(desc);
    }

    offset += snprintf(buf + offset, buf_size - offset, "]}");
    *response_json = buf;
    return 0;
}

int mcp_v1_handle_prompts_list(mcp_v1_context_t *ctx, char **response_json)
{
    if (!ctx || !response_json)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "mcp_v1_handle_prompts_list: failed");
        return AGENTOS_ERR_UNKNOWN;
        }

    size_t buf_size = 4096 + ctx->prompt_count * 512;
    char *buf = AGENTOS_MALLOC(buf_size);
    if (!buf) {
        agentos_error_push_ex(AGENTOS_ERR_NULL_POINTER, __FILE__, __LINE__, __func__, "mcp_v1_adapter: null pointer");
        return AGENTOS_ERR_NULL_POINTER;
        }

    size_t offset = 0;
    offset += snprintf(buf + offset, buf_size - offset, "{\"prompts\":[");

    for (size_t i = 0; i < ctx->prompt_count; i++) {
        if (i > 0)
            offset += snprintf(buf + offset, buf_size - offset, ",");
        char *name = json_string_escape(ctx->prompts[i].prompt.name);
        char *desc = json_string_escape(ctx->prompts[i].prompt.description);
        offset += snprintf(buf + offset, buf_size - offset,
                           "{\"name\":%s,\"description\":%s,\"arguments\":%s}", name, desc,
                           ctx->prompts[i].prompt.arguments_schema_json
                               ? ctx->prompts[i].prompt.arguments_schema_json
                               : "[]");
        AGENTOS_FREE(name);
        AGENTOS_FREE(desc);
    }

    offset += snprintf(buf + offset, buf_size - offset, "]}");
    *response_json = buf;
    return 0;
}

int mcp_v1_handle_prompts_get(mcp_v1_context_t *ctx, const char *name, const char *arguments_json,
                              char **response_json)
{
    if (!ctx || !name || !response_json)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "mcp_v1_handle_prompts_get: failed");
        return AGENTOS_ERR_UNKNOWN;
        }

    mcp_prompt_entry_t *found = NULL;
    for (size_t i = 0; i < ctx->prompt_count; i++) {
        if (strcmp(ctx->prompts[i].prompt.name, name) == 0) {
            found = &ctx->prompts[i];
            break;
        }
    }

    if (!found || !found->handler) {
        AGENTOS_LOG_WARN("prompt not found or no handler: name=%s, prompt_count=%zu", name, ctx->prompt_count);
        *response_json = AGENTOS_STRDUP("{\"description\":\"Prompt not found\",\"messages\":[]}");
        agentos_error_push_ex(AGENTOS_ERR_INVALID_PARAM, __FILE__, __LINE__, __func__, "mcp_v1_adapter: invalid parameter");
        return AGENTOS_ERR_INVALID_PARAM;
    }

    mcp_sampling_message_t *messages = NULL;
    size_t message_count = 0;
    found->handler(name, arguments_json, &messages, &message_count, found->user_data);

    size_t buf_size = 4096 + message_count * 1024;
    char *buf = AGENTOS_MALLOC(buf_size);
    if (!buf) {
        agentos_error_push_ex(AGENTOS_ERR_NULL_POINTER, __FILE__, __LINE__, __func__, "mcp_v1_adapter: null pointer");
        return AGENTOS_ERR_NULL_POINTER;
        }

    size_t offset = 0;
    char *name_esc = json_string_escape(name);
    offset += snprintf(buf + offset, buf_size - offset,
                       "{\"description\":\"Prompt: %s\",\"messages\":[", name_esc);
    AGENTOS_FREE(name_esc);
    name_esc = NULL;

    for (size_t i = 0; i < message_count && i < 100; i++) {
        if (i > 0)
            offset += snprintf(buf + offset, buf_size - offset, ",");
        char *role = json_string_escape(messages[i].role);
        char *content =
            json_string_escape(messages[i].content ? (const char *)messages[i].content : "");
        offset += snprintf(buf + offset, buf_size - offset,
                           "{\"role\":%s,\"content\":{\"type\":\"text\",\"text\":\"%s\"}}", role,
                           content);
        AGENTOS_FREE(role);
        AGENTOS_FREE(content);
    }

    offset += snprintf(buf + offset, buf_size - offset, "]}");
    *response_json = buf;
    return 0;
}

int mcp_v1_handle_sampling(mcp_v1_context_t *ctx, const mcp_sampling_params_t *params,
                           mcp_sampling_result_t *result)
{
    if (!ctx || !params || !result)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "mcp_v1_handle_sampling: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    if (!ctx->sampling_handler) {
        AGENTOS_LOG_WARN("sampling handler not registered, cannot handle sampling request");
        agentos_error_push_ex(AGENTOS_ERR_INVALID_PARAM, __FILE__, __LINE__, __func__, "mcp_v1_adapter: invalid parameter");
        return AGENTOS_ERR_INVALID_PARAM;
        }
    if (!(ctx->config.capabilities & MCP_CAP_SAMPLING)) {
        AGENTOS_LOG_WARN("sampling capability not enabled, caps=0x%x", ctx->config.capabilities);
        agentos_error_push_ex(AGENTOS_ERR_NULL_POINTER, __FILE__, __LINE__, __func__, "mcp_v1_adapter: null pointer");
        return AGENTOS_ERR_NULL_POINTER;
        }

    ctx->sampling_handler(params, result, ctx->sampling_user_data);
    return 0;
}

int mcp_v1_handle_completion(mcp_v1_context_t *ctx, const mcp_completion_request_t *request,
                             mcp_completion_result_t *result)
{
    if (!ctx || !request || !result)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "mcp_v1_handle_completion: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    if (!ctx->completion_handler) {
        agentos_error_push_ex(AGENTOS_ERR_INVALID_PARAM, __FILE__, __LINE__, __func__, "mcp_v1_adapter: invalid parameter");
        return AGENTOS_ERR_INVALID_PARAM;
        }

    ctx->completion_handler(request, result, ctx->completion_user_data);
    return 0;
}

int mcp_v1_send_progress(mcp_v1_context_t *ctx, const char *progress_token, double progress,
                         double total)
{
    if (!ctx || !progress_token)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "mcp_v1_send_progress: IO error");
        return AGENTOS_ERR_UNKNOWN;
        }
    if (ctx->progress_callback) {
        ctx->progress_callback(progress_token, progress, total, ctx->progress_user_data);
    }
    return 0;
}

int mcp_v1_notify_cancelled(mcp_v1_context_t *ctx, const char *request_id, const char *reason)
{
    if (!ctx || !request_id)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "mcp_v1_notify_cancelled: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    if (ctx->log_callback) {
        ctx->log_callback(MCP_LOG_INFO, "mcp", reason ? reason : "Request cancelled",
                          ctx->log_user_data);
    }
    return 0;
}

/* ========== Streaming Support Implementation (PROTO-001) ========== */

typedef struct {
    mcp_stream_config_t config;
    bool active;
} mcp_stream_state_t;

int mcp_v1_stream_config(mcp_v1_context_t *ctx, const mcp_stream_config_t *config)
{
    if (!ctx)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "mcp_v1_stream_config: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    if (!config) {
        agentos_error_push_ex(AGENTOS_ERR_INVALID_PARAM, __FILE__, __LINE__, __func__, "mcp_v1_adapter: invalid parameter");
        return AGENTOS_ERR_INVALID_PARAM;
        }

    ctx->stream_config = *config;
    return 0;
}

const char *mcp_stream_event_type_string(mcp_stream_event_type_t type)
{
    switch (type) {
    case MCP_STREAM_EVENT_CONTENT:
        return "content";
    case MCP_STREAM_EVENT_ERROR:
        return "error";
    case MCP_STREAM_EVENT_DONE:
        return "done";
    case MCP_STREAM_EVENT_PROGRESS:
        return "progress";
    case MCP_STREAM_EVENT_CANCELLED:
        return "cancelled";
    default:
        return "unknown";
    }
}

void mcp_stream_event_init(mcp_stream_event_t *event, mcp_stream_event_type_t type,
                           const char *data)
{
    if (!event)
        return;
    AGENTOS_MEMSET(event, 0, sizeof(*event));
    event->type = type;
    event->event_data = data ? AGENTOS_STRDUP(data) : NULL;
    event->data_size = data ? strlen(data) : 0;
}

static void emit_sse_line(mcp_stream_callback_t callback, void *user_data, const char *field,
                          const char *value)
{
    if (!callback)
        return;

    size_t len = strlen(field) + (value ? strlen(value) : 0) + 16;
    char *sse_line = AGENTOS_MALLOC(len);
    if (sse_line) {
        snprintf(sse_line, len, "%s: %s\n", field, value ? value : "");
        mcp_stream_event_t event;
        mcp_stream_event_init(&event, MCP_STREAM_EVENT_CONTENT, sse_line);
        callback(&event, user_data);
        AGENTOS_FREE(sse_line);
    }
}

static int emit_sse_event(mcp_stream_callback_t callback, void *user_data, const char *event_type,
                          const char *json_data)
{
    if (!callback)
        return 0;

    emit_sse_line(callback, user_data, "event", event_type);
    emit_sse_line(callback, user_data, "data", json_data);
    emit_sse_line(callback, user_data, "", NULL);

    return 0;
}

int mcp_v1_handle_tools_call_streaming(mcp_v1_context_t *ctx, const char *name,
                                       const char *arguments_json, mcp_stream_callback_t callback,
                                       void *user_data)
{
    if (!ctx || !name || !callback)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "mcp_v1_handle_tools_call_streaming: failed");
        return AGENTOS_ERR_UNKNOWN;
        }

    mcp_stream_event_t start_event;
    mcp_stream_event_init(&start_event, MCP_STREAM_EVENT_PROGRESS, "{\"status\":\"started\"}");
    start_event.progress = 0;
    start_event.total = 100;
    callback(&start_event, user_data);
    AGENTOS_FREE(start_event.event_data);
    start_event.event_data = NULL;

    mcp_tool_entry_t *found = NULL;
    for (size_t i = 0; i < ctx->tool_count; i++) {
        if (strcmp(ctx->tools[i].tool.name, name) == 0) {
            found = &ctx->tools[i];
            break;
        }
    }

    if (!found) {
        char *name_esc = json_string_escape(name);
        char error_json[512];
        snprintf(
            error_json, sizeof(error_json),
            "{\"isError\":true,\"content\":[{\"type\":\"text\",\"text\":\"Tool not found: %s\"}]}",
            name_esc);
        AGENTOS_FREE(name_esc);
        name_esc = NULL;
        emit_sse_event(callback, user_data, "tools/call/result", error_json);

        mcp_stream_event_t done_event;
        mcp_stream_event_init(&done_event, MCP_STREAM_EVENT_ERROR, "Tool not found");
        done_event.progress = 0;
        done_event.total = 100;
        callback(&done_event, user_data);
        AGENTOS_FREE(done_event.event_data);
        agentos_error_push_ex(AGENTOS_ERR_INVALID_PARAM, __FILE__, __LINE__, __func__, "mcp_v1_adapter: invalid parameter");
        return AGENTOS_ERR_INVALID_PARAM;
    }

    mcp_content_t *results = NULL;
    size_t result_count = 0;
    bool is_error = false;

    found->handler(name, arguments_json, &results, &result_count, &is_error, found->user_data);

    for (size_t i = 0; i < result_count; i++) {
        size_t buf_size = 512 + (results[i].text ? strlen(results[i].text) : 0);
        char *chunk_json = AGENTOS_MALLOC(buf_size);
        if (chunk_json) {
            const char *type_str = "text";
            switch (results[i].type) {
            case MCP_CONTENT_IMAGE:
                type_str = "image";
                break;
            case MCP_CONTENT_RESOURCE:
                type_str = "resource";
                break;
            case MCP_CONTENT_EMBEDDED:
                type_str = "embedded";
                break;
            default:
                break;
            }
            snprintf(chunk_json, buf_size, "{\"index\":%zu,\"type\":\"%s\",\"partial\":true}", i,
                     type_str);

            size_t text_len = results[i].text ? strlen(results[i].text) : 0;
            size_t offset = 0;
            while (offset < text_len) {
                size_t chunk_size = 128;
                if (offset + chunk_size > text_len)
                    chunk_size = text_len - offset;

                char chunk_buf[256];
                __builtin_memcpy(chunk_buf, results[i].text + offset, chunk_size);
                chunk_buf[chunk_size] = '\0';

                char *escaped_chunk = json_string_escape(chunk_buf);
                char sse_data[1024];
                snprintf(sse_data, sizeof(sse_data), "%s{\"delta\":{\"text\":\"%s\"}}",
                         offset == 0 ? chunk_json : "", escaped_chunk);
                AGENTOS_FREE(escaped_chunk);

                emit_sse_event(callback, user_data, "tools/call/chunk", sse_data);
                offset += chunk_size;

                double pct = (double)offset / (double)(text_len > 0 ? text_len : 1) * 80.0;
                mcp_stream_event_t progress_evt;
                mcp_stream_event_init(&progress_evt, MCP_STREAM_EVENT_PROGRESS, NULL);
                progress_evt.progress = pct;
                progress_evt.total = 100;
                callback(&progress_evt, user_data);
                AGENTOS_FREE(progress_evt.event_data);
            }

            AGENTOS_FREE(chunk_json);
        }
    }

    size_t final_buf_size = 4096 + result_count * 1024;
    char *final_json = AGENTOS_MALLOC(final_buf_size);
    if (final_json) {
        size_t offset = 0;
        offset += snprintf(final_json + offset, final_buf_size - offset,
                           "{\"isError\":%s,\"content\":[", is_error ? "true" : "false");
        for (size_t i = 0; i < result_count; i++) {
            if (i > 0)
                offset += snprintf(final_json + offset, final_buf_size - offset, ",");
            const char *type_str = "text";
            switch (results[i].type) {
            case MCP_CONTENT_IMAGE:
                type_str = "image";
                break;
            case MCP_CONTENT_RESOURCE:
                type_str = "resource";
                break;
            case MCP_CONTENT_EMBEDDED:
                type_str = "embedded";
                break;
            default:
                break;
            }
            char *text_esc = json_string_escape(results[i].text);
            offset += snprintf(final_json + offset, final_buf_size - offset,
                               "{\"type\":\"%s\",\"text\":%s}", type_str, text_esc);
            AGENTOS_FREE(text_esc);
        }
        offset += snprintf(final_json + offset, final_buf_size - offset, "]}");

        emit_sse_event(callback, user_data, "tools/call/result", final_json);
        AGENTOS_FREE(final_json);
    }

    mcp_stream_event_t done_event;
    mcp_stream_event_init(&done_event, is_error ? MCP_STREAM_EVENT_ERROR : MCP_STREAM_EVENT_DONE,
                          is_error ? "completed with errors" : "completed successfully");
    done_event.progress = 100;
    done_event.total = 100;
    callback(&done_event, user_data);
    AGENTOS_FREE(done_event.event_data);

    mcp_content_destroy(results, result_count);
    return 0;
}

int mcp_v1_handle_sampling_streaming(mcp_v1_context_t *ctx, const mcp_sampling_params_t *params,
                                     mcp_stream_callback_t callback, void *user_data)
{
    if (!ctx || !params || !callback)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "mcp_v1_handle_sampling_streaming: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    if (!ctx->sampling_handler) {
        AGENTOS_LOG_WARN("sampling handler not registered for streaming request");
        agentos_error_push_ex(AGENTOS_ERR_INVALID_PARAM, __FILE__, __LINE__, __func__, "mcp_v1_adapter: invalid parameter");
        return AGENTOS_ERR_INVALID_PARAM;
        }
    if (!(ctx->config.capabilities & MCP_CAP_SAMPLING)) {
        AGENTOS_LOG_WARN("sampling capability not enabled for streaming, caps=0x%x", ctx->config.capabilities);
        return AGENTOS_ERR_NULL_POINTER;
        }

    mcp_stream_event_t start_event;
    mcp_stream_event_init(&start_event, MCP_STREAM_EVENT_PROGRESS,
                          "{\"status\":\"sampling_started\"}");
    start_event.progress = 0;
    start_event.total = 100;
    callback(&start_event, user_data);
    AGENTOS_FREE(start_event.event_data);
    start_event.event_data = NULL;

    mcp_sampling_result_t result;
    AGENTOS_MEMSET(&result, 0, sizeof(result));

    ctx->sampling_handler(params, &result, ctx->sampling_user_data);

    if (result.content && result.content_count > 0) {
        for (size_t i = 0; i < result.content_count; i++) {
            if (result.content[i].text && result.content[i].text[0]) {
                size_t text_len = strlen(result.content[i].text);
                size_t offset = 0;
                while (offset < text_len) {
                    size_t chunk_size = 128;
                    if (offset + chunk_size > text_len)
                        chunk_size = text_len - offset;

                    char chunk_buf[256];
                    __builtin_memcpy(chunk_buf, result.content[i].text + offset, chunk_size);
                    chunk_buf[chunk_size] = '\0';

                    char *escaped_chunk = json_string_escape(chunk_buf);
                    char *model_esc = json_string_escape(result.model ? result.model : "unknown");
                    char sse_data[512];
                    snprintf(sse_data, sizeof(sse_data),
                             "{\"model\":%s,\"delta\":{\"type\":\"text\",\"text\":\"%s\"},"
                             "\"index\":%zu}",
                             model_esc, escaped_chunk, i);
                    AGENTOS_FREE(escaped_chunk);
                    AGENTOS_FREE(model_esc);
                    emit_sse_event(callback, user_data, "sampling/chunk", sse_data);

                    offset += chunk_size;

                    double pct = (double)offset / (double)(text_len > 0 ? text_len : 1) * 90.0;
                    mcp_stream_event_t progress_evt;
                    mcp_stream_event_init(&progress_evt, MCP_STREAM_EVENT_PROGRESS, NULL);
                    progress_evt.progress = pct;
                    progress_evt.total = 100;
                    callback(&progress_evt, user_data);
                    AGENTOS_FREE(progress_evt.event_data);
                }
            }
        }
    }

    char *model_esc = json_string_escape(result.model ? result.model : "unknown");
    char *stop_esc = json_string_escape(result.stop_reason ? result.stop_reason : "end_turn");
    char final_result[2048];
    snprintf(final_result, sizeof(final_result),
             "{\"model\":%s,\"stopReason\":%s,\"stoppedEarly\":%s}", model_esc, stop_esc,
             result.stopped_early ? "true" : "false");
    AGENTOS_FREE(model_esc);
    model_esc = NULL;
    AGENTOS_FREE(stop_esc);
    stop_esc = NULL;
    emit_sse_event(callback, user_data, "sampling/result", final_result);

    mcp_stream_event_t done_event;
    mcp_stream_event_init(&done_event, MCP_STREAM_EVENT_DONE, "Sampling complete");
    done_event.progress = 100;
    done_event.total = 100;
    callback(&done_event, user_data);
    AGENTOS_FREE(done_event.event_data);

    mcp_sampling_result_destroy(&result);
    return 0;
}

int mcp_v1_route_request(mcp_v1_context_t *ctx, const char *method, const char *params_json,
                         char **response_json)
{
    if (!ctx || !method || !response_json)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "mcp_v1_route_request: failed");
        return AGENTOS_ERR_UNKNOWN;
        }

    ctx->request_counter++;

    if (strcmp(method, "tools/list") == 0) {
        return mcp_v1_handle_tools_list(ctx, response_json);
    } else if (strcmp(method, "tools/call") == 0) {
        char tool_name[256] = {0};
        if (params_json) {
            cJSON *pj = cJSON_Parse(params_json);
            if (pj) {
                cJSON *name_item = cJSON_GetObjectItem(pj, "name");
                if (cJSON_IsString(name_item) && name_item->valuestring) {
                    AGENTOS_STRNCPY_TERM(tool_name, name_item->valuestring, sizeof(tool_name));
                    tool_name[sizeof(tool_name) - 1] = '\0';
                }
                cJSON_Delete(pj);
            }
        }
        return mcp_v1_handle_tools_call(ctx, tool_name[0] ? tool_name : "unknown", params_json,
                                        response_json);
    } else if (strcmp(method, "resources/list") == 0) {
        return mcp_v1_handle_resources_list(ctx, response_json);
    } else if (strcmp(method, "resources/read") == 0) {
        char resource_uri[512] = {0};
        if (params_json) {
            cJSON *pj = cJSON_Parse(params_json);
            if (pj) {
                cJSON *uri_item = cJSON_GetObjectItem(pj, "uri");
                if (cJSON_IsString(uri_item) && uri_item->valuestring) {
                    AGENTOS_STRNCPY_TERM(resource_uri, uri_item->valuestring, sizeof(resource_uri));
                }
                cJSON_Delete(pj);
            }
        }
        return mcp_v1_handle_resources_read(ctx, resource_uri[0] ? resource_uri : "unknown",
                                            response_json);
    } else if (strcmp(method, "resources/templates/list") == 0) {
        return mcp_v1_handle_resources_templates(ctx, response_json);
    } else if (strcmp(method, "prompts/list") == 0) {
        return mcp_v1_handle_prompts_list(ctx, response_json);
    } else if (strcmp(method, "prompts/get") == 0) {
        char prompt_name[256] = {0};
        if (params_json) {
            cJSON *pj = cJSON_Parse(params_json);
            if (pj) {
                cJSON *name_item = cJSON_GetObjectItem(pj, "name");
                if (cJSON_IsString(name_item) && name_item->valuestring) {
                    AGENTOS_STRNCPY_TERM(prompt_name, name_item->valuestring, sizeof(prompt_name));
                }
                cJSON_Delete(pj);
            }
        }
        return mcp_v1_handle_prompts_get(ctx, prompt_name[0] ? prompt_name : "unknown", params_json,
                                         response_json);
    } else if (strcmp(method, "logging/setLogLevel") == 0) {
        mcp_log_level_t level = MCP_LOG_INFO;
        if (params_json) {
            char *level_str = strstr(params_json, "\"level\"");
            if (level_str) {
                level_str = strchr(level_str + 7, '"');
                if (level_str) {
                    level_str++;
                    if (strncmp(level_str, "debug", 5) == 0)
                        level = MCP_LOG_DEBUG;
                    else if (strncmp(level_str, "notice", 6) == 0)
                        level = MCP_LOG_NOTICE;
                    else if (strncmp(level_str, "warning", 7) == 0)
                        level = MCP_LOG_WARNING;
                    else if (strncmp(level_str, "error", 5) == 0)
                        level = MCP_LOG_ERROR;
                    else if (strncmp(level_str, "critical", 8) == 0)
                        level = MCP_LOG_CRITICAL;
                    else if (strncmp(level_str, "alert", 5) == 0)
                        level = MCP_LOG_ALERT;
                    else if (strncmp(level_str, "emergency", 9) == 0)
                        level = MCP_LOG_EMERGENCY;
                }
            }
        }
        return mcp_v1_set_log_level(ctx, level);
    } else if (strcmp(method, "initialize") == 0) {
        size_t len =
            snprintf(NULL, 0,
                     "{\"protocolVersion\":\"%s\",\"capabilities\":{\"tools\":%s,\"resources\":%s,"
                     "\"prompts\":%s,\"sampling\":%s,\"logging\":%s},\"serverInfo\":{\"name\":\"%"
                     "s\",\"version\":\"%s\"}}",
                     MCP_V1_VERSION, (ctx->config.capabilities & MCP_CAP_TOOLS) ? "true" : "false",
                     (ctx->config.capabilities & MCP_CAP_RESOURCES) ? "true" : "false",
                     (ctx->config.capabilities & MCP_CAP_PROMPTS) ? "true" : "false",
                     (ctx->config.capabilities & MCP_CAP_SAMPLING) ? "true" : "false",
                     (ctx->config.capabilities & MCP_CAP_LOGGING) ? "true" : "false",
                     ctx->config.server_name ? ctx->config.server_name : "AgentRT",
                     ctx->config.server_version ? ctx->config.server_version : MCP_V1_VERSION);
        char *resp = AGENTOS_MALLOC(len + 1);
        if (resp)
            snprintf(resp, len + 1,
                     "{\"protocolVersion\":\"%s\",\"capabilities\":{\"tools\":%s,\"resources\":%s,"
                     "\"prompts\":%s,\"sampling\":%s,\"logging\":%s},\"serverInfo\":{\"name\":\"%"
                     "s\",\"version\":\"%s\"}}",
                     MCP_V1_VERSION, (ctx->config.capabilities & MCP_CAP_TOOLS) ? "true" : "false",
                     (ctx->config.capabilities & MCP_CAP_RESOURCES) ? "true" : "false",
                     (ctx->config.capabilities & MCP_CAP_PROMPTS) ? "true" : "false",
                     (ctx->config.capabilities & MCP_CAP_SAMPLING) ? "true" : "false",
                     (ctx->config.capabilities & MCP_CAP_LOGGING) ? "true" : "false",
                     ctx->config.server_name ? ctx->config.server_name : "AgentRT",
                     ctx->config.server_version ? ctx->config.server_version : MCP_V1_VERSION);
        *response_json = resp;
        return 0;
    }

    *response_json =
        AGENTOS_STRDUP("{\"error\":{\"code\":-32601,\"message\":\"Method not found\"}}");
    AGENTOS_LOG_WARN("method not found in route_request: method=%s, request_counter=%llu", method, (unsigned long long)ctx->request_counter);
    agentos_error_push_ex(AGENTOS_ERR_INVALID_PARAM, __FILE__, __LINE__, __func__, "mcp_v1_adapter: invalid parameter");
    return AGENTOS_ERR_INVALID_PARAM;
}

static int mcp_adapter_init(void *context)
{
    mcp_v1_context_t *ctx = (mcp_v1_context_t *)context;
    if (!ctx)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "mcp_adapter_init: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    mcp_v1_config_t config = mcp_v1_config_default();
    mcp_v1_context_t *new_ctx = mcp_v1_context_create(&config);
    if (!new_ctx) {
        agentos_error_push_ex(AGENTOS_ERR_INVALID_PARAM, __FILE__, __LINE__, __func__, "mcp_v1_adapter: invalid parameter");
        return AGENTOS_ERR_INVALID_PARAM;
        }
    __builtin_memcpy(ctx, new_ctx, sizeof(mcp_v1_context_t));
    AGENTOS_FREE(new_ctx);
    return 0;
}

static int mcp_adapter_destroy(void *context)
{
    if (context) {
        mcp_v1_context_destroy((mcp_v1_context_t *)context);
    }
    return 0;
}

static int mcp_adapter_encode(void *context, const void *msg, void **encoded, size_t *size)
{
    if (!context || !msg || !encoded || !size)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "mcp_adapter_encode: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    mcp_v1_context_t *ctx = (mcp_v1_context_t *)context;
    const unified_message_t *umsg = (const unified_message_t *)msg;

    char *response_json = NULL;
    int result =
        mcp_v1_route_request(ctx, umsg->endpoint, (const char *)umsg->payload, &response_json);
    if (result != 0 || !response_json) {
        AGENTOS_LOG_ERROR("route_request failed in encode: endpoint=%s, result=%d", umsg->endpoint, result);
        *encoded = NULL;
        *size = 0;
        return result;
    }

    *encoded = response_json;
    *size = strlen(response_json);
    return 0;
}

static int mcp_adapter_decode(void *context, const void *data, size_t data_size, void *out_msg)
{
    if (!context || !data || !out_msg)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "mcp_adapter_decode: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    if (data_size == 0) {
        AGENTOS_LOG_WARN("decode called with zero data_size");
        agentos_error_push_ex(AGENTOS_ERR_INVALID_PARAM, __FILE__, __LINE__, __func__, "mcp_v1_adapter: invalid parameter");
        return AGENTOS_ERR_INVALID_PARAM;
        }

    mcp_v1_context_t *ctx = (mcp_v1_context_t *)context;
    unified_message_t *msg = (unified_message_t *)out_msg;

    char *input_copy = AGENTOS_MALLOC(data_size + 1);
    if (!input_copy) {
        agentos_error_push_ex(AGENTOS_ERR_NULL_POINTER, __FILE__, __LINE__, __func__, "mcp_v1_adapter: null pointer");
        return AGENTOS_ERR_NULL_POINTER;
        }
    __builtin_memcpy(input_copy, data, data_size);
    input_copy[data_size] = '\0';

    char *method = "tools/call";
    bool method_allocated = false;
    char *method_start = strstr(input_copy, "\"method\"");
    if (method_start) {
        method_start = strchr(method_start + 8, '"');
        if (method_start) {
            method_start++;
            char *method_end = strchr(method_start, '"');
            if (method_end) {
                size_t method_len = (size_t)(method_end - method_start);
                char *method_buf = AGENTOS_MALLOC(method_len + 1);
                if (method_buf) {
                    __builtin_memcpy(method_buf, method_start, method_len);
                    method_buf[method_len] = '\0';
                    method = method_buf;
                    method_allocated = true;
                }
            }
        }
    }

    char *response_json = NULL;
    int result = mcp_v1_route_request(ctx, method, input_copy, &response_json);
    if (method_allocated) {
        AGENTOS_FREE((void *)method);
        method = NULL;
    }
    AGENTOS_FREE(input_copy);
    input_copy = NULL;

    if (result == 0 && response_json) {
        msg->payload = response_json;
        msg->payload_size = strlen(response_json);
        msg->protocol = PROTOCOL_CUSTOM;
        msg->direction = DIRECTION_RESPONSE;
        msg->timestamp = (uint64_t)time(NULL);
    }

    return result;
}

static int mcp_adapter_connect(void *context, const char *address)
{
    if (!context)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "mcp_adapter_connect: IO error");
        return AGENTOS_ERR_UNKNOWN;
        }
    mcp_v1_context_t *ctx = (mcp_v1_context_t *)context;
    if (address) {
        char *old_name = (char *)ctx->config.server_name;
        ctx->config.server_name = AGENTOS_STRDUP(address);
        AGENTOS_FREE(old_name);
    }
    if (ctx->transport) {
        mcp_transport_config_t tcfg;
        AGENTOS_MEMSET(&tcfg, 0, sizeof(tcfg));
        tcfg.type = MCP_TRANSPORT_HTTP_SSE;
        tcfg.config.http.base_url = address;
        tcfg.config.http.sse_endpoint = "/sse";
        tcfg.config.http.post_endpoint = "/message";
        tcfg.read_timeout_ms = (uint32_t)ctx->config.default_timeout_ms;
        tcfg.write_timeout_ms = (uint32_t)ctx->config.default_timeout_ms;
        return mcp_transport_start(ctx->transport);
    }
    return 0;
}

static int mcp_adapter_disconnect(void *context)
{
    if (!context)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "mcp_adapter_disconnect: IO error");
        return AGENTOS_ERR_UNKNOWN;
        }
    mcp_v1_context_t *ctx = (mcp_v1_context_t *)context;
    if (ctx->config.server_name) {
        AGENTOS_FREE((void *)ctx->config.server_name);
        ctx->config.server_name = NULL;
    }
    return 0;
}

static int mcp_adapter_is_connected(void *context)
{
    if (!context)
        return 0;
    mcp_v1_context_t *ctx = (mcp_v1_context_t *)context;
    if (ctx->transport) {
        return mcp_transport_get_state(ctx->transport) == MCP_TRANSPORT_CONNECTED ? 1 : 0;
    }
    return (ctx->tool_count > 0 || ctx->resource_count > 0) ? 1 : 0;
}

static int mcp_adapter_send(void *context, const void *data, size_t size)
{
    if (!context || !data)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "mcp_adapter_send: IO error");
        return AGENTOS_ERR_UNKNOWN;
        }
    mcp_v1_context_t *ctx = (mcp_v1_context_t *)context;
    if (!ctx->transport) {
        AGENTOS_LOG_WARN("send called but no transport configured");
        agentos_error_push_ex(AGENTOS_ERR_INVALID_PARAM, __FILE__, __LINE__, __func__, "mcp_v1_adapter: invalid parameter");
        return AGENTOS_ERR_INVALID_PARAM;
        }
    return mcp_transport_send(ctx->transport, (const char *)data, size);
}

static int mcp_adapter_receive(void *context, void **data, size_t *size)
{
    if (!context || !data || !size)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "mcp_adapter_receive: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    mcp_v1_context_t *ctx = (mcp_v1_context_t *)context;
    if (!ctx->transport) {
        AGENTOS_LOG_WARN("receive called but no transport configured");
        return AGENTOS_ERR_INVALID_PARAM;
        }
    char *msg = NULL;
    size_t msg_len = 0;
    int ret = mcp_transport_receive(ctx->transport, &msg, &msg_len, ctx->config.default_timeout_ms);
    if (ret == 0 && msg) {
        *data = msg;
        *size = msg_len;
    }
    return ret;
}

static int mcp_adapter_receive_adapter(void *context, void **data, size_t *size,
                                       uint32_t timeout_ms)
{
    (void)timeout_ms;
    return mcp_adapter_receive(context, data, size);
}

static int mcp_adapter_get_stats(void *context, char *stats_json, size_t max_size)
{
    (void)context;
    if (!stats_json || max_size < 64)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "mcp_adapter_get_stats: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    int written = snprintf(stats_json, max_size,
                           "{\"adapter_version\":\"%s\",\"protocol\":\"mcp\"}", MCP_V1_VERSION);
    return (written >= 0 && (size_t)written < max_size) ? 0 : -2;
}

static int mcp_adapter_handle_request(void *context, const void *req, void **resp)
{
    if (!context || !req || !resp)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "mcp_adapter_handle_request: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    mcp_v1_context_t *ctx = (mcp_v1_context_t *)context;
    const unified_message_t *msg = (const unified_message_t *)req;

    const char *method = msg->method[0] ? msg->method : "tools/call";
    const char *params = (const char *)(msg->payload ? msg->payload : "{}");

    char *response_json = NULL;
    int result = mcp_v1_route_request(ctx, method, params, &response_json);

    if (result == 0 && response_json) {
        *resp = response_json;
    } else {
        AGENTOS_FREE(response_json);
        response_json = NULL;
        *resp = AGENTOS_STRDUP("{\"error\":\"request failed\"}");
        result = -1;
    }
    return result;
}

static int mcp_adapter_get_version(void *context, char *buf, size_t max_size)
{
    (void)context;
    if (!buf || max_size == 0)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "mcp_adapter_get_version: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    size_t len = strlen(MCP_V1_VERSION);
    if (len >= max_size)
        len = max_size - 1;
    __builtin_memcpy(buf, MCP_V1_VERSION, len);
    buf[len] = '\0';
    return 0;
}

static uint32_t mcp_adapter_capabilities(void *context)
{
    (void)context;
    return (uint32_t)(MCP_CAP_TOOLS | MCP_CAP_RESOURCES | MCP_CAP_PROMPTS | MCP_CAP_LOGGING |
                      MCP_CAP_SAMPLING);
}

static protocol_adapter_t mcp_v1_adapter_internal = {.type = AGENTOS_PROTOCOL_MCP,
                                                     .name = "MCP v1.0 Protocol Adapter",
                                                     .version = MCP_V1_VERSION,
                                                     .description =
                                                         "Model Context Protocol v1.0 adapter",
                                                     .init = mcp_adapter_init,
                                                     .destroy = mcp_adapter_destroy,
                                                     .encode = mcp_adapter_encode,
                                                     .decode = mcp_adapter_decode,
                                                     .connect = mcp_adapter_connect,
                                                     .disconnect = mcp_adapter_disconnect,
                                                     .is_connected = mcp_adapter_is_connected,
                                                     .send = mcp_adapter_send,
                                                     .receive = mcp_adapter_receive_adapter,
                                                     .handle_request = mcp_adapter_handle_request,
                                                     .get_version = mcp_adapter_get_version,
                                                     .capabilities = mcp_adapter_capabilities,
                                                     .get_stats = mcp_adapter_get_stats,
                                                     .context = NULL,
                                                     .user_data = NULL};

const protocol_adapter_t *mcp_v1_get_adapter(void)
{
    return &mcp_v1_adapter_internal;
}

int mcp_v1_set_transport(mcp_v1_context_t *ctx, mcp_transport_t *transport)
{
    if (!ctx)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "mcp_v1_set_transport: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    ctx->transport = transport;
    return 0;
}

mcp_transport_t *mcp_v1_get_transport(mcp_v1_context_t *ctx)
{
    if (!ctx)
        return NULL;
    return ctx->transport;
}

size_t mcp_v1_get_tool_count(mcp_v1_context_t *ctx)
{
    return ctx ? ctx->tool_count : 0;
}

size_t mcp_v1_get_resource_count(mcp_v1_context_t *ctx)
{
    return ctx ? ctx->resource_count : 0;
}

size_t mcp_v1_get_prompt_count(mcp_v1_context_t *ctx)
{
    return ctx ? ctx->prompt_count : 0;
}

uint32_t mcp_v1_get_capabilities(mcp_v1_context_t *ctx)
{
    return ctx ? ctx->config.capabilities : 0;
}

void mcp_content_destroy(mcp_content_t *content, size_t count)
{
    if (!content)
        return;
    for (size_t i = 0; i < count; i++) {
        AGENTOS_FREE(content[i].text);
        AGENTOS_FREE(content[i].mime_type);
        AGENTOS_FREE(content[i].uri);
        AGENTOS_FREE(content[i].data);
    }
    AGENTOS_FREE(content);
}

void mcp_sampling_result_destroy(mcp_sampling_result_t *result)
{
    if (!result)
        return;
    AGENTOS_FREE(result->model);
    AGENTOS_FREE(result->stop_reason);
    mcp_content_destroy(result->content, result->content_count);
}

void mcp_completion_result_destroy(mcp_completion_result_t *result)
{
    if (!result)
        return;
    for (size_t i = 0; i < result->value_count; i++) {
        AGENTOS_FREE(result->values[i]);
    }
    AGENTOS_FREE(result->values);
}
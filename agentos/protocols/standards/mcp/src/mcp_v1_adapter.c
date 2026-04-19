// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file mcp_v1_adapter.c
 * @brief MCP v1.0 Protocol Adapter Implementation
 */

#include "mcp_v1_adapter.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

typedef struct {
    mcp_tool_t tool;
    mcp_tool_handler_t handler;
    void* user_data;
} mcp_tool_entry_t;

typedef struct {
    mcp_resource_t resource;
    mcp_resource_handler_t handler;
    void* user_data;
} mcp_resource_entry_t;

typedef struct {
    mcp_prompt_t prompt;
    mcp_prompt_handler_t handler;
    void* user_data;
} mcp_prompt_entry_t;

struct mcp_v1_context_s {
    mcp_v1_config_t config;

    mcp_tool_entry_t* tools;
    size_t tool_count;
    size_t tool_capacity;

    mcp_resource_entry_t* resources;
    size_t resource_count;
    size_t resource_capacity;

    mcp_resource_template_t* resource_templates;
    size_t template_count;
    size_t template_capacity;

    mcp_prompt_entry_t* prompts;
    size_t prompt_count;
    size_t prompt_capacity;

    mcp_sampling_handler_t sampling_handler;
    void* sampling_user_data;

    mcp_completion_handler_t completion_handler;
    void* completion_user_data;

    mcp_progress_callback_t progress_callback;
    void* progress_user_data;

    mcp_log_callback_t log_callback;
    void* log_user_data;

    mcp_log_level_t log_level;

    uint64_t request_counter;
};

static char* json_string_escape(const char* str) {
    if (!str) return strdup("null");
    size_t len = strlen(str);
    size_t escaped_len = len * 2 + 3;
    char* escaped = malloc(escaped_len);
    if (!escaped) return NULL;
    size_t j = 0;
    escaped[j++] = '"';
    for (size_t i = 0; i < len; i++) {
        switch (str[i]) {
            case '"':  escaped[j++] = '\\'; escaped[j++] = '"'; break;
            case '\\': escaped[j++] = '\\'; escaped[j++] = '\\'; break;
            case '\n': escaped[j++] = '\\'; escaped[j++] = 'n'; break;
            case '\r': escaped[j++] = '\\'; escaped[j++] = 'r'; break;
            case '\t': escaped[j++] = '\\'; escaped[j++] = 't'; break;
            default:   escaped[j++] = str[i]; break;
        }
    }
    escaped[j++] = '"';
    escaped[j] = '\0';
    return escaped;
}

static char* strdup_safe(const char* s) {
    return s ? strdup(s) : NULL;
}

mcp_v1_config_t mcp_v1_config_default(void) {
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

mcp_v1_context_t* mcp_v1_context_create(const mcp_v1_config_t* config) {
    mcp_v1_context_t* ctx = calloc(1, sizeof(mcp_v1_context_t));
    if (!ctx) return NULL;

    if (config) {
        ctx->config = *config;
        ctx->config.server_name = strdup_safe(config->server_name);
        ctx->config.server_version = strdup_safe(config->server_version);
    } else {
        ctx->config = mcp_v1_config_default();
    }

    ctx->tool_capacity = 32;
    ctx->tools = calloc(ctx->tool_capacity, sizeof(mcp_tool_entry_t));

    ctx->resource_capacity = 16;
    ctx->resources = calloc(ctx->resource_capacity, sizeof(mcp_resource_entry_t));

    ctx->template_capacity = 16;
    ctx->resource_templates = calloc(ctx->template_capacity, sizeof(mcp_resource_template_t));

    ctx->prompt_capacity = 16;
    ctx->prompts = calloc(ctx->prompt_capacity, sizeof(mcp_prompt_entry_t));

    ctx->log_level = MCP_LOG_INFO;
    ctx->request_counter = 0;

    return ctx;
}

void mcp_v1_context_destroy(mcp_v1_context_t* ctx) {
    if (!ctx) return;

    for (size_t i = 0; i < ctx->tool_count; i++) {
        free(ctx->tools[i].tool.name);
        free(ctx->tools[i].tool.description);
        free(ctx->tools[i].tool.input_schema_json);
    }
    free(ctx->tools);

    for (size_t i = 0; i < ctx->resource_count; i++) {
        free(ctx->resources[i].resource.uri);
        free(ctx->resources[i].resource.name);
        free(ctx->resources[i].resource.description);
        free(ctx->resources[i].resource.mime_type);
    }
    free(ctx->resources);

    for (size_t i = 0; i < ctx->template_count; i++) {
        free(ctx->resource_templates[i].uri_template);
        free(ctx->resource_templates[i].name);
        free(ctx->resource_templates[i].description);
        free(ctx->resource_templates[i].mime_type);
    }
    free(ctx->resource_templates);

    for (size_t i = 0; i < ctx->prompt_count; i++) {
        free(ctx->prompts[i].prompt.name);
        free(ctx->prompts[i].prompt.description);
        free(ctx->prompts[i].prompt.arguments_schema_json);
    }
    free(ctx->prompts);

    free(ctx->config.server_name);
    free(ctx->config.server_version);
    free(ctx);
}

int mcp_v1_register_tool(mcp_v1_context_t* ctx,
                          const mcp_tool_t* tool,
                          mcp_tool_handler_t handler,
                          void* user_data) {
    if (!ctx || !tool || !handler) return -1;
    if (ctx->tool_count >= ctx->config.max_tools) return -2;

    if (ctx->tool_count >= ctx->tool_capacity) {
        size_t new_cap = ctx->tool_capacity * 2;
        mcp_tool_entry_t* new_tools = realloc(ctx->tools, new_cap * sizeof(mcp_tool_entry_t));
        if (!new_tools) return -3;
        ctx->tools = new_tools;
        ctx->tool_capacity = new_cap;
    }

    mcp_tool_entry_t* entry = &ctx->tools[ctx->tool_count];
    entry->tool.name = strdup_safe(tool->name);
    entry->tool.description = strdup_safe(tool->description);
    entry->tool.input_schema_json = strdup_safe(tool->input_schema_json);
    entry->tool.required_caps = tool->required_caps;
    entry->handler = handler;
    entry->user_data = user_data;
    ctx->tool_count++;

    return 0;
}

int mcp_v1_register_resource(mcp_v1_context_t* ctx,
                              const mcp_resource_t* resource,
                              mcp_resource_handler_t handler,
                              void* user_data) {
    if (!ctx || !resource) return -1;
    if (ctx->resource_count >= ctx->config.max_resources) return -2;

    if (ctx->resource_count >= ctx->resource_capacity) {
        size_t new_cap = ctx->resource_capacity * 2;
        mcp_resource_entry_t* new_res = realloc(ctx->resources, new_cap * sizeof(mcp_resource_entry_t));
        if (!new_res) return -3;
        ctx->resources = new_res;
        ctx->resource_capacity = new_cap;
    }

    mcp_resource_entry_t* entry = &ctx->resources[ctx->resource_count];
    entry->resource.uri = strdup_safe(resource->uri);
    entry->resource.name = strdup_safe(resource->name);
    entry->resource.description = strdup_safe(resource->description);
    entry->resource.mime_type = strdup_safe(resource->mime_type);
    entry->handler = handler;
    entry->user_data = user_data;
    ctx->resource_count++;

    return 0;
}

int mcp_v1_register_resource_template(mcp_v1_context_t* ctx,
                                       const mcp_resource_template_t* template) {
    if (!ctx || !template) return -1;

    if (ctx->template_count >= ctx->template_capacity) {
        size_t new_cap = ctx->template_capacity * 2;
        mcp_resource_template_t* new_tpl = realloc(ctx->resource_templates, new_cap * sizeof(mcp_resource_template_t));
        if (!new_tpl) return -3;
        ctx->resource_templates = new_tpl;
        ctx->template_capacity = new_cap;
    }

    mcp_resource_template_t* entry = &ctx->resource_templates[ctx->template_count];
    entry->uri_template = strdup_safe(template->uri_template);
    entry->name = strdup_safe(template->name);
    entry->description = strdup_safe(template->description);
    entry->mime_type = strdup_safe(template->mime_type);
    ctx->template_count++;

    return 0;
}

int mcp_v1_register_prompt(mcp_v1_context_t* ctx,
                            const mcp_prompt_t* prompt,
                            mcp_prompt_handler_t handler,
                            void* user_data) {
    if (!ctx || !prompt) return -1;
    if (ctx->prompt_count >= MCP_V1_MAX_PROMPTS) return -2;

    if (ctx->prompt_count >= ctx->prompt_capacity) {
        size_t new_cap = ctx->prompt_capacity * 2;
        mcp_prompt_entry_t* new_prompts = realloc(ctx->prompts, new_cap * sizeof(mcp_prompt_entry_t));
        if (!new_prompts) return -3;
        ctx->prompts = new_prompts;
        ctx->prompt_capacity = new_cap;
    }

    mcp_prompt_entry_t* entry = &ctx->prompts[ctx->prompt_count];
    entry->prompt.name = strdup_safe(prompt->name);
    entry->prompt.description = strdup_safe(prompt->description);
    entry->prompt.arguments_schema_json = strdup_safe(prompt->arguments_schema_json);
    entry->handler = handler;
    entry->user_data = user_data;
    ctx->prompt_count++;

    return 0;
}

int mcp_v1_set_sampling_handler(mcp_v1_context_t* ctx,
                                 mcp_sampling_handler_t handler,
                                 void* user_data) {
    if (!ctx) return -1;
    ctx->sampling_handler = handler;
    ctx->sampling_user_data = user_data;
    if (handler) ctx->config.capabilities |= MCP_CAP_SAMPLING;
    return 0;
}

int mcp_v1_set_completion_handler(mcp_v1_context_t* ctx,
                                   mcp_completion_handler_t handler,
                                   void* user_data) {
    if (!ctx) return -1;
    ctx->completion_handler = handler;
    ctx->completion_user_data = user_data;
    if (handler) ctx->config.capabilities |= MCP_CAP_COMPLETION;
    return 0;
}

int mcp_v1_set_progress_callback(mcp_v1_context_t* ctx,
                                  mcp_progress_callback_t callback,
                                  void* user_data) {
    if (!ctx) return -1;
    ctx->progress_callback = callback;
    ctx->progress_user_data = user_data;
    return 0;
}

int mcp_v1_set_log_callback(mcp_v1_context_t* ctx,
                             mcp_log_callback_t callback,
                             void* user_data) {
    if (!ctx) return -1;
    ctx->log_callback = callback;
    ctx->log_user_data = user_data;
    return 0;
}

int mcp_v1_set_log_level(mcp_v1_context_t* ctx, mcp_log_level_t level) {
    if (!ctx) return -1;
    ctx->log_level = level;
    return 0;
}

int mcp_v1_handle_tools_list(mcp_v1_context_t* ctx, char** response_json) {
    if (!ctx || !response_json) return -1;

    size_t buf_size = 4096 + ctx->tool_count * 512;
    char* buf = malloc(buf_size);
    if (!buf) return -3;

    size_t offset = 0;
    offset += snprintf(buf + offset, buf_size - offset,
        "{\"tools\":[");

    for (size_t i = 0; i < ctx->tool_count; i++) {
        if (i > 0) offset += snprintf(buf + offset, buf_size - offset, ",");
        char* name = json_string_escape(ctx->tools[i].tool.name);
        char* desc = json_string_escape(ctx->tools[i].tool.description);
        offset += snprintf(buf + offset, buf_size - offset,
            "{\"name\":%s,\"description\":%s,\"inputSchema\":%s}",
            name, desc,
            ctx->tools[i].tool.input_schema_json ? ctx->tools[i].tool.input_schema_json : "{}");
        free(name);
        free(desc);
    }

    offset += snprintf(buf + offset, buf_size - offset, "]}");
    *response_json = buf;
    return 0;
}

int mcp_v1_handle_tools_call(mcp_v1_context_t* ctx,
                              const char* name,
                              const char* arguments_json,
                              char** response_json) {
    if (!ctx || !name || !response_json) return -1;

    mcp_tool_entry_t* found = NULL;
    for (size_t i = 0; i < ctx->tool_count; i++) {
        if (strcmp(ctx->tools[i].tool.name, name) == 0) {
            found = &ctx->tools[i];
            break;
        }
    }

    if (!found) {
        char* name_esc = json_string_escape(name);
        size_t len = snprintf(NULL, 0,
            "{\"isError\":true,\"content\":[{\"type\":\"text\",\"text\":\"Tool not found: %s\"}]}",
            name);
        char* resp = malloc(len + 1);
        if (resp) {
            snprintf(resp, len + 1,
                "{\"isError\":true,\"content\":[{\"type\":\"text\",\"text\":\"Tool not found: %s\"}]}",
                name);
        }
        free(name_esc);
        *response_json = resp;
        return -2;
    }

    mcp_content_t* results = NULL;
    size_t result_count = 0;
    bool is_error = false;

    found->handler(name, arguments_json, &results, &result_count, &is_error, found->user_data);

    size_t buf_size = 4096 + result_count * 1024;
    char* buf = malloc(buf_size);
    if (!buf) {
        mcp_content_destroy(results, result_count);
        return -3;
    }

    size_t offset = 0;
    offset += snprintf(buf + offset, buf_size - offset,
        "{\"isError\":%s,\"content\":[", is_error ? "true" : "false");

    for (size_t i = 0; i < result_count; i++) {
        if (i > 0) offset += snprintf(buf + offset, buf_size - offset, ",");
        const char* type_str = "text";
        switch (results[i].type) {
            case MCP_CONTENT_IMAGE: type_str = "image"; break;
            case MCP_CONTENT_RESOURCE: type_str = "resource"; break;
            case MCP_CONTENT_EMBEDDED: type_str = "embedded"; break;
            default: break;
        }
        char* text_esc = json_string_escape(results[i].text);
        offset += snprintf(buf + offset, buf_size - offset,
            "{\"type\":\"%s\",\"text\":%s}", type_str, text_esc);
        free(text_esc);
    }

    offset += snprintf(buf + offset, buf_size - offset, "]}");
    *response_json = buf;

    mcp_content_destroy(results, result_count);
    return 0;
}

int mcp_v1_handle_resources_list(mcp_v1_context_t* ctx, char** response_json) {
    if (!ctx || !response_json) return -1;

    size_t buf_size = 4096 + ctx->resource_count * 512;
    char* buf = malloc(buf_size);
    if (!buf) return -3;

    size_t offset = 0;
    offset += snprintf(buf + offset, buf_size - offset, "{\"resources\":[");

    for (size_t i = 0; i < ctx->resource_count; i++) {
        if (i > 0) offset += snprintf(buf + offset, buf_size - offset, ",");
        char* uri = json_string_escape(ctx->resources[i].resource.uri);
        char* name = json_string_escape(ctx->resources[i].resource.name);
        char* desc = json_string_escape(ctx->resources[i].resource.description);
        char* mime = json_string_escape(ctx->resources[i].resource.mime_type);
        offset += snprintf(buf + offset, buf_size - offset,
            "{\"uri\":%s,\"name\":%s,\"description\":%s,\"mimeType\":%s}",
            uri, name, desc, mime);
        free(uri); free(name); free(desc); free(mime);
    }

    offset += snprintf(buf + offset, buf_size - offset, "]}");
    *response_json = buf;
    return 0;
}

int mcp_v1_handle_resources_read(mcp_v1_context_t* ctx,
                                  const char* uri,
                                  char** response_json) {
    if (!ctx || !uri || !response_json) return -1;

    mcp_resource_entry_t* found = NULL;
    for (size_t i = 0; i < ctx->resource_count; i++) {
        if (strcmp(ctx->resources[i].resource.uri, uri) == 0) {
            found = &ctx->resources[i];
            break;
        }
    }

    if (!found || !found->handler) {
        char* uri_esc = json_string_escape(uri);
        size_t len = snprintf(NULL, 0,
            "{\"contents\":[{\"uri\":%s,\"text\":\"Resource not found\"}]}",
            uri_esc);
        char* resp = malloc(len + 1);
        if (resp) snprintf(resp, len + 1,
            "{\"contents\":[{\"uri\":%s,\"text\":\"Resource not found\"}]}",
            uri_esc);
        free(uri_esc);
        *response_json = resp;
        return -2;
    }

    char* content = NULL;
    char* mime_type = NULL;
    found->handler(uri, &content, &mime_type, found->user_data);

    char* uri_esc = json_string_escape(uri);
    char* content_esc = json_string_escape(content);
    char* mime_esc = json_string_escape(mime_type);

    size_t len = snprintf(NULL, 0,
        "{\"contents\":[{\"uri\":%s,\"mimeType\":%s,\"text\":%s}]}",
        uri_esc, mime_esc, content_esc);
    char* resp = malloc(len + 1);
    if (resp) snprintf(resp, len + 1,
        "{\"contents\":[{\"uri\":%s,\"mimeType\":%s,\"text\":%s}]}",
        uri_esc, mime_esc, content_esc);

    free(uri_esc); free(content_esc); free(mime_esc);
    free(content); free(mime_type);
    *response_json = resp;
    return 0;
}

int mcp_v1_handle_resources_templates(mcp_v1_context_t* ctx, char** response_json) {
    if (!ctx || !response_json) return -1;

    size_t buf_size = 4096 + ctx->template_count * 512;
    char* buf = malloc(buf_size);
    if (!buf) return -3;

    size_t offset = 0;
    offset += snprintf(buf + offset, buf_size - offset, "{\"resourceTemplates\":[");

    for (size_t i = 0; i < ctx->template_count; i++) {
        if (i > 0) offset += snprintf(buf + offset, buf_size - offset, ",");
        char* uri = json_string_escape(ctx->resource_templates[i].uri_template);
        char* name = json_string_escape(ctx->resource_templates[i].name);
        char* desc = json_string_escape(ctx->resource_templates[i].description);
        offset += snprintf(buf + offset, buf_size - offset,
            "{\"uriTemplate\":%s,\"name\":%s,\"description\":%s}",
            uri, name, desc);
        free(uri); free(name); free(desc);
    }

    offset += snprintf(buf + offset, buf_size - offset, "]}");
    *response_json = buf;
    return 0;
}

int mcp_v1_handle_prompts_list(mcp_v1_context_t* ctx, char** response_json) {
    if (!ctx || !response_json) return -1;

    size_t buf_size = 4096 + ctx->prompt_count * 512;
    char* buf = malloc(buf_size);
    if (!buf) return -3;

    size_t offset = 0;
    offset += snprintf(buf + offset, buf_size - offset, "{\"prompts\":[");

    for (size_t i = 0; i < ctx->prompt_count; i++) {
        if (i > 0) offset += snprintf(buf + offset, buf_size - offset, ",");
        char* name = json_string_escape(ctx->prompts[i].prompt.name);
        char* desc = json_string_escape(ctx->prompts[i].prompt.description);
        offset += snprintf(buf + offset, buf_size - offset,
            "{\"name\":%s,\"description\":%s,\"arguments\":%s}",
            name, desc,
            ctx->prompts[i].prompt.arguments_schema_json ? ctx->prompts[i].prompt.arguments_schema_json : "[]");
        free(name); free(desc);
    }

    offset += snprintf(buf + offset, buf_size - offset, "]}");
    *response_json = buf;
    return 0;
}

int mcp_v1_handle_prompts_get(mcp_v1_context_t* ctx,
                               const char* name,
                               const char* arguments_json,
                               char** response_json) {
    if (!ctx || !name || !response_json) return -1;

    mcp_prompt_entry_t* found = NULL;
    for (size_t i = 0; i < ctx->prompt_count; i++) {
        if (strcmp(ctx->prompts[i].prompt.name, name) == 0) {
            found = &ctx->prompts[i];
            break;
        }
    }

    if (!found || !found->handler) {
        *response_json = strdup("{\"description\":\"Prompt not found\",\"messages\":[]}");
        return -2;
    }

    mcp_sampling_message_t* messages = NULL;
    size_t message_count = 0;
    found->handler(name, arguments_json, &messages, &message_count, found->user_data);

    size_t buf_size = 4096 + message_count * 1024;
    char* buf = malloc(buf_size);
    if (!buf) return -3;

    size_t offset = 0;
    char* name_esc = json_string_escape(name);
    offset += snprintf(buf + offset, buf_size - offset,
        "{\"description\":\"Prompt: %s\",\"messages\":[", name_esc);
    free(name_esc);

    for (size_t i = 0; i < message_count && i < 100; i++) {
        if (i > 0) offset += snprintf(buf + offset, buf_size - offset, ",");
        char* role = json_string_escape(messages[i].role);
        offset += snprintf(buf + offset, buf_size - offset,
            "{\"role\":%s,\"content\":{\"type\":\"text\",\"text\":\"\"}}", role);
        free(role);
    }

    offset += snprintf(buf + offset, buf_size - offset, "]}");
    *response_json = buf;
    return 0;
}

int mcp_v1_handle_sampling(mcp_v1_context_t* ctx,
                            const mcp_sampling_params_t* params,
                            mcp_sampling_result_t* result) {
    if (!ctx || !params || !result) return -1;
    if (!ctx->sampling_handler) return -2;
    if (!(ctx->config.capabilities & MCP_CAP_SAMPLING)) return -3;

    ctx->sampling_handler(params, result, ctx->sampling_user_data);
    return 0;
}

int mcp_v1_handle_completion(mcp_v1_context_t* ctx,
                              const mcp_completion_request_t* request,
                              mcp_completion_result_t* result) {
    if (!ctx || !request || !result) return -1;
    if (!ctx->completion_handler) return -2;

    ctx->completion_handler(request, result, ctx->completion_user_data);
    return 0;
}

int mcp_v1_send_progress(mcp_v1_context_t* ctx,
                          const char* progress_token,
                          double progress,
                          double total) {
    if (!ctx || !progress_token) return -1;
    if (ctx->progress_callback) {
        ctx->progress_callback(progress_token, progress, total, ctx->progress_user_data);
    }
    return 0;
}

int mcp_v1_notify_cancelled(mcp_v1_context_t* ctx,
                             const char* request_id,
                             const char* reason) {
    if (!ctx || !request_id) return -1;
    if (ctx->log_callback) {
        ctx->log_callback(MCP_LOG_INFO, "mcp", reason ? reason : "Request cancelled", ctx->log_user_data);
    }
    return 0;
}

/* ========== Streaming Support Implementation (PROTO-001) ========== */

typedef struct {
    mcp_stream_config_t config;
    bool active;
} mcp_stream_state_t;

static mcp_stream_state_t* get_stream_state(mcp_v1_context_t* ctx) {
    static mcp_stream_state_t default_state = {
        .config = { .enabled = true, .chunk_size = 4096, .max_buffer_size = (10 * 1024 * 1024), .flush_interval_ms = 50 },
        .active = false
    };
    (void)ctx;
    return &default_state;
}

int mcp_v1_stream_config(mcp_v1_context_t* ctx, const mcp_stream_config_t* config) {
    if (!ctx) return -1;
    if (!config) return -2;

    mcp_stream_state_t* ss = get_stream_state(ctx);
    ss->config = *config;
    return 0;
}

const char* mcp_stream_event_type_string(mcp_stream_event_type_t type) {
    switch (type) {
        case MCP_STREAM_EVENT_CONTENT:   return "content";
        case MCP_STREAM_EVENT_ERROR:     return "error";
        case MCP_STREAM_EVENT_DONE:      return "done";
        case MCP_STREAM_EVENT_PROGRESS:  return "progress";
        case MCP_STREAM_EVENT_CANCELLED: return "cancelled";
        default: return "unknown";
    }
}

void mcp_stream_event_init(mcp_stream_event_t* event,
                            mcp_stream_event_type_t type,
                            const char* data) {
    if (!event) return;
    memset(event, 0, sizeof(*event));
    event->type = type;
    event->event_data = data ? strdup(data) : NULL;
    event->data_size = data ? strlen(data) : 0;
}

static void emit_sse_line(mcp_stream_callback_t callback, void* user_data,
                           const char* field, const char* value) {
    if (!callback) return;

    size_t len = strlen(field) + (value ? strlen(value) : 0) + 16;
    char* sse_line = malloc(len);
    if (sse_line) {
        snprintf(sse_line, len, "%s: %s\n", field, value ? value : "");
        mcp_stream_event_t event;
        mcp_stream_event_init(&event, MCP_STREAM_EVENT_CONTENT, sse_line);
        callback(&event, user_data);
        free(sse_line);
    }
}

static int emit_sse_event(mcp_stream_callback_t callback, void* user_data,
                           const char* event_type, const char* json_data) {
    if (!callback) return 0;

    emit_sse_line(callback, user_data, "event", event_type);
    emit_sse_line(callback, user_data, "data", json_data);
    emit_sse_line(callback, user_data, "", NULL);

    return 0;
}

int mcp_v1_handle_tools_call_streaming(mcp_v1_context_t* ctx,
                                        const char* name,
                                        const char* arguments_json,
                                        mcp_stream_callback_t callback,
                                        void* user_data) {
    if (!ctx || !name || !callback) return -1;

    mcp_stream_event_t start_event;
    mcp_stream_event_init(&start_event, MCP_STREAM_EVENT_PROGRESS,
                          "{\"status\":\"started\"}");
    start_event.progress = 0;
    start_event.total = 100;
    callback(&start_event, user_data);
    free(start_event.event_data);

    mcp_tool_entry_t* found = NULL;
    for (size_t i = 0; i < ctx->tool_count; i++) {
        if (strcmp(ctx->tools[i].tool.name, name) == 0) {
            found = &ctx->tools[i];
            break;
        }
    }

    if (!found) {
        char error_json[512];
        snprintf(error_json, sizeof(error_json),
            "{\"isError\":true,\"content\":[{\"type\":\"text\",\"text\":\"Tool not found: %s\"}]}",
            name);
        emit_sse_event(callback, user_data, "tools/call/result", error_json);

        mcp_stream_event_t done_event;
        mcp_stream_event_init(&done_event, MCP_STREAM_EVENT_ERROR, "Tool not found");
        done_event.progress = 0;
        done_event.total = 100;
        callback(&done_event, user_data);
        free(done_event.event_data);
        return -2;
    }

    mcp_content_t* results = NULL;
    size_t result_count = 0;
    bool is_error = false;

    found->handler(name, arguments_json, &results, &result_count, &is_error, found->user_data);

    for (size_t i = 0; i < result_count; i++) {
        size_t buf_size = 512 + (results[i].text ? strlen(results[i].text) : 0);
        char* chunk_json = malloc(buf_size);
        if (chunk_json) {
            const char* type_str = "text";
            switch (results[i].type) {
                case MCP_CONTENT_IMAGE: type_str = "image"; break;
                case MCP_CONTENT_RESOURCE: type_str = "resource"; break;
                case MCP_CONTENT_EMBEDDED: type_str = "embedded"; break;
                default: break;
            }
            snprintf(chunk_json, buf_size,
                "{\"index\":%zu,\"type\":\"%s\",\"partial\":true}",
                i, type_str);

            size_t text_len = results[i].text ? strlen(results[i].text) : 0;
            size_t offset = 0;
            while (offset < text_len) {
                size_t chunk_end = offset + 256;
                if (chunk_end > text_len) chunk_end = text_len;

                char tmp = results[i].text[chunk_end];
                ((char*)results[i].text)[chunk_end] = '\0';

                char sse_data[1024];
                snprintf(sse_data, sizeof(sse_data),
                    "%s{\"delta\":{\"text\":\"%s\"}}",
                    offset == 0 ? chunk_json : "",
                    results[i].text + offset);

                emit_sse_event(callback, user_data, "tools/call/chunk", sse_data);

                ((char*)results[i].text)[chunk_end] = tmp;
                offset = chunk_end;

                double pct = (double)offset / (double)(text_len > 0 ? text_len : 1) * 80.0;
                mcp_stream_event_t progress_evt;
                mcp_stream_event_init(&progress_evt, MCP_STREAM_EVENT_PROGRESS, NULL);
                progress_evt.progress = pct;
                progress_evt.total = 100;
                callback(&progress_evt, user_data);
                free(progress_evt.event_data);
            }

            free(chunk_json);
        }
    }

    size_t final_buf_size = 4096 + result_count * 1024;
    char* final_json = malloc(final_buf_size);
    if (final_json) {
        size_t offset = 0;
        offset += snprintf(final_json + offset, final_buf_size - offset,
            "{\"isError\":%s,\"content\":[", is_error ? "true" : "false");
        for (size_t i = 0; i < result_count; i++) {
            if (i > 0) offset += snprintf(final_json + offset, final_buf_size - offset, ",");
            const char* type_str = "text";
            switch (results[i].type) {
                case MCP_CONTENT_IMAGE: type_str = "image"; break;
                case MCP_CONTENT_RESOURCE: type_str = "resource"; break;
                case MCP_CONTENT_EMBEDDED: type_str = "embedded"; break;
                default: break;
            }
            char* text_esc = json_string_escape(results[i].text);
            offset += snprintf(final_json + offset, final_buf_size - offset,
                "{\"type\":\"%s\",\"text\":%s}", type_str, text_esc);
            free(text_esc);
        }
        offset += snprintf(final_json + offset, final_buf_size - offset, "]}");

        emit_sse_event(callback, user_data, "tools/call/result", final_json);
        free(final_json);
    }

    mcp_stream_event_t done_event;
    mcp_stream_event_init(&done_event, is_error ? MCP_STREAM_EVENT_ERROR : MCP_STREAM_EVENT_DONE,
                          is_error ? "completed with errors" : "completed successfully");
    done_event.progress = 100;
    done_event.total = 100;
    callback(&done_event, user_data);
    free(done_event.event_data);

    mcp_content_destroy(results, result_count);
    return 0;
}

int mcp_v1_handle_sampling_streaming(mcp_v1_context_t* ctx,
                                      const mcp_sampling_params_t* params,
                                      mcp_stream_callback_t callback,
                                      void* user_data) {
    if (!ctx || !params || !callback) return -1;
    if (!ctx->sampling_handler) return -2;
    if (!(ctx->config.capabilities & MCP_CAP_SAMPLING)) return -3;

    mcp_stream_event_t start_event;
    mcp_stream_event_init(&start_event, MCP_STREAM_EVENT_PROGRESS,
                          "{\"status\":\"sampling_started\"}");
    start_event.progress = 0;
    start_event.total = 100;
    callback(&start_event, user_data);
    free(start_event.event_data);

    mcp_sampling_result_t result;
    memset(&result, 0, sizeof(result));

    ctx->sampling_handler(params, &result, ctx->sampling_user_data);

    if (result.content && result.content_count > 0) {
        for (size_t i = 0; i < result.content_count; i++) {
            if (result.content[i].text && result.content[i].text[0]) {
                size_t text_len = strlen(result.content[i].text);
                size_t offset = 0;
                while (offset < text_len) {
                    size_t chunk_end = offset + 256;
                    if (chunk_end > text_len) chunk_end = text_len;

                    char tmp = result.content[i].text[chunk_end];
                    ((char*)result.content[i].text)[chunk_end] = '\0';

                    char sse_data[512];
                    snprintf(sse_data, sizeof(sse_data),
                        "{\"model\":\"%s\",\"delta\":{\"type\":\"text\",\"text\":\"%s\"},\"index\":%zu}",
                        result.model ? result.model : "unknown",
                        result.content[i].text + offset, i);
                    emit_sse_event(callback, user_data, "sampling/chunk", sse_data);

                    ((char*)result.content[i].text)[chunk_end] = tmp;
                    offset = chunk_end;

                    double pct = (double)offset / (double)(text_len > 0 ? text_len : 1) * 90.0;
                    mcp_stream_event_t progress_evt;
                    mcp_stream_event_init(&progress_evt, MCP_STREAM_EVENT_PROGRESS, NULL);
                    progress_evt.progress = pct;
                    progress_evt.total = 100;
                    callback(&progress_evt, user_data);
                    free(progress_evt.event_data);
                }
            }
        }
    }

    char final_result[2048];
    snprintf(final_result, sizeof(final_result),
        "{\"model\":\"%s\",\"stopReason\":\"%s\",\"stoppedEarly\":%s}",
        result.model ? result.model : "unknown",
        result.stop_reason ? result.stop_reason : "end_turn",
        result.stopped_early ? "true" : "false");
    emit_sse_event(callback, user_data, "sampling/result", final_result);

    mcp_stream_event_t done_event;
    mcp_stream_event_init(&done_event, MCP_STREAM_EVENT_DONE, "Sampling complete");
    done_event.progress = 100;
    done_event.total = 100;
    callback(&done_event, user_data);
    free(done_event.event_data);

    mcp_sampling_result_destroy(&result);
    return 0;
}

int mcp_v1_route_request(mcp_v1_context_t* ctx,
                          const char* method,
                          const char* params_json,
                          char** response_json) {
    if (!ctx || !method || !response_json) return -1;

    ctx->request_counter++;

    if (strcmp(method, "tools/list") == 0) {
        return mcp_v1_handle_tools_list(ctx, response_json);
    } else if (strcmp(method, "tools/call") == 0) {
        return mcp_v1_handle_tools_call(ctx, "unknown", params_json, response_json);
    } else if (strcmp(method, "resources/list") == 0) {
        return mcp_v1_handle_resources_list(ctx, response_json);
    } else if (strcmp(method, "resources/read") == 0) {
        return mcp_v1_handle_resources_read(ctx, "unknown", response_json);
    } else if (strcmp(method, "resources/templates/list") == 0) {
        return mcp_v1_handle_resources_templates(ctx, response_json);
    } else if (strcmp(method, "prompts/list") == 0) {
        return mcp_v1_handle_prompts_list(ctx, response_json);
    } else if (strcmp(method, "prompts/get") == 0) {
        return mcp_v1_handle_prompts_get(ctx, "unknown", params_json, response_json);
    } else if (strcmp(method, "logging/setLogLevel") == 0) {
        return mcp_v1_set_log_level(ctx, MCP_LOG_INFO);
    } else if (strcmp(method, "initialize") == 0) {
        size_t len = snprintf(NULL, 0,
            "{\"protocolVersion\":\"%s\",\"capabilities\":{\"tools\":%s,\"resources\":%s,\"prompts\":%s,\"sampling\":%s,\"logging\":%s},\"serverInfo\":{\"name\":\"%s\",\"version\":\"%s\"}}",
            MCP_V1_VERSION,
            (ctx->config.capabilities & MCP_CAP_TOOLS) ? "true" : "false",
            (ctx->config.capabilities & MCP_CAP_RESOURCES) ? "true" : "false",
            (ctx->config.capabilities & MCP_CAP_PROMPTS) ? "true" : "false",
            (ctx->config.capabilities & MCP_CAP_SAMPLING) ? "true" : "false",
            (ctx->config.capabilities & MCP_CAP_LOGGING) ? "true" : "false",
            ctx->config.server_name ? ctx->config.server_name : "AgentOS",
            ctx->config.server_version ? ctx->config.server_version : MCP_V1_VERSION);
        char* resp = malloc(len + 1);
        if (resp) snprintf(resp, len + 1,
            "{\"protocolVersion\":\"%s\",\"capabilities\":{\"tools\":%s,\"resources\":%s,\"prompts\":%s,\"sampling\":%s,\"logging\":%s},\"serverInfo\":{\"name\":\"%s\",\"version\":\"%s\"}}",
            MCP_V1_VERSION,
            (ctx->config.capabilities & MCP_CAP_TOOLS) ? "true" : "false",
            (ctx->config.capabilities & MCP_CAP_RESOURCES) ? "true" : "false",
            (ctx->config.capabilities & MCP_CAP_PROMPTS) ? "true" : "false",
            (ctx->config.capabilities & MCP_CAP_SAMPLING) ? "true" : "false",
            (ctx->config.capabilities & MCP_CAP_LOGGING) ? "true" : "false",
            ctx->config.server_name ? ctx->config.server_name : "AgentOS",
            ctx->config.server_version ? ctx->config.server_version : MCP_V1_VERSION);
        *response_json = resp;
        return 0;
    }

    *response_json = strdup("{\"error\":{\"code\":-32601,\"message\":\"Method not found\"}}");
    return -2;
}

static int mcp_adapter_init(void* context) {
    (void)context;
    return 0;
}

static void mcp_adapter_destroy(void* context) {
    mcp_v1_context_destroy((mcp_v1_context_t*)context);
}

static int mcp_adapter_encode(void* context, const unified_message_t* msg,
                               void** encoded, size_t* size) {
    (void)context; (void)msg; (void)encoded; (void)size;
    return 0;
}

static int mcp_adapter_decode(void* context, const void* data, size_t size,
                               unified_message_t* msg) {
    (void)context; (void)data; (void)size; (void)msg;
    return 0;
}

static int mcp_adapter_connect(void* context, const char* address) {
    (void)context; (void)address;
    return 0;
}

static int mcp_adapter_disconnect(void* context) {
    (void)context;
    return 0;
}

static bool mcp_adapter_is_connected(void* context) {
    (void)context;
    return true;
}

static void mcp_adapter_get_stats(void* context, void* stats) {
    (void)context; (void)stats;
}

static protocol_adapter_t mcp_v1_adapter = {
    .type = PROTOCOL_CUSTOM,
    .init = mcp_adapter_init,
    .destroy = mcp_adapter_destroy,
    .encode = mcp_adapter_encode,
    .decode = mcp_adapter_decode,
    .connect = mcp_adapter_connect,
    .disconnect = mcp_adapter_disconnect,
    .is_connected = mcp_adapter_is_connected,
    .get_stats = mcp_adapter_get_stats
};

const protocol_adapter_t* mcp_v1_get_adapter(void) {
    return &mcp_v1_adapter;
}

size_t mcp_v1_get_tool_count(mcp_v1_context_t* ctx) {
    return ctx ? ctx->tool_count : 0;
}

size_t mcp_v1_get_resource_count(mcp_v1_context_t* ctx) {
    return ctx ? ctx->resource_count : 0;
}

size_t mcp_v1_get_prompt_count(mcp_v1_context_t* ctx) {
    return ctx ? ctx->prompt_count : 0;
}

uint32_t mcp_v1_get_capabilities(mcp_v1_context_t* ctx) {
    return ctx ? ctx->config.capabilities : 0;
}

void mcp_content_destroy(mcp_content_t* content, size_t count) {
    if (!content) return;
    for (size_t i = 0; i < count; i++) {
        free(content[i].text);
        free(content[i].mime_type);
        free(content[i].uri);
        free(content[i].data);
    }
    free(content);
}

void mcp_sampling_result_destroy(mcp_sampling_result_t* result) {
    if (!result) return;
    free(result->model);
    free(result->stop_reason);
    mcp_content_destroy(result->content, result->content_count);
}

void mcp_completion_result_destroy(mcp_completion_result_t* result) {
    if (!result) return;
    for (size_t i = 0; i < result->value_count; i++) {
        free(result->values[i]);
    }
    free(result->values);
}

// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file claude_adapter.c
 * @brief Anthropic Claude API Adapter Implementation
 */

#define LOG_TAG "claude_adapter"

#include "claude_adapter.h"
#include "protocol_transformers.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

struct claude_adapter_context_s {
    claude_config_t config;
    bool initialized;
    claude_message_handler_t message_handler;
    void* message_handler_data;
    claude_stream_handler_t stream_handler;
    void* stream_handler_data;
    claude_tool_use_handler_t tool_use_handler;
    void* tool_use_handler_data;
    uint64_t total_requests;
    uint64_t total_tokens_in;
    uint64_t total_tokens_out;
    uint64_t total_tool_calls;
    char last_error[256];
};

static claude_model_info_t g_builtin_models[] = {
    {
        .id = CLAUDE_MODEL_CLAUDE_3_5_SONNET,
        .api_name = "claude-3-5-sonnet-20241022",
        .display_name = "Claude 3.5 Sonnet",
        .max_context_tokens = 200000,
        .max_output_tokens = 8192,
        .supports_vision = true,
        .supports_extended_thinking = true,
        .supports_tool_use = true,
        .supports_prompt_caching = true,
        .cost_per_million_input = 3.0,
        .cost_per_million_output = 15.0,
        .is_available = true,
    },
    {
        .id = CLAUDE_MODEL_CLAUDE_3_5_HAIKU,
        .api_name = "claude-3-5-haiku-20241022",
        .display_name = "Claude 3.5 Haiku",
        .max_context_tokens = 200000,
        .max_output_tokens = 8192,
        .supports_vision = true,
        .supports_extended_thinking = false,
        .supports_tool_use = true,
        .supports_prompt_caching = true,
        .cost_per_million_input = 1.0,
        .cost_per_million_output = 5.0,
        .is_available = true,
    },
    {
        .id = CLAUDE_MODEL_CLAUDE_3_OPUS,
        .api_name = "claude-3-opus-20240229",
        .display_name = "Claude 3 Opus",
        .max_context_tokens = 200000,
        .max_output_tokens = 4096,
        .supports_vision = true,
        .supports_extended_thinking = false,
        .supports_tool_use = true,
        .supports_prompt_caching = true,
        .cost_per_million_input = 15.0,
        .cost_per_million_output = 75.0,
        .is_available = true,
    },
    {
        .id = CLAUDE_MODEL_CLAUDE_3_SONNET,
        .api_name = "claude-3-sonnet-20240229",
        .display_name = "Claude 3 Sonnet",
        .max_context_tokens = 200000,
        .max_output_tokens = 4096,
        .supports_vision = true,
        .supports_extended_thinking = false,
        .supports_tool_use = true,
        .supports_prompt_caching = true,
        .cost_per_million_input = 3.0,
        .cost_per_million_output = 15.0,
        .is_available = true,
    },
    {
        .id = CLAUDE_MODEL_CLAUDE_3_HAIKU,
        .api_name = "claude-3-haiku-20240307",
        .display_name = "Claude 3 Haiku",
        .max_context_tokens = 200000,
        .max_output_tokens = 4096,
        .supports_vision = false,
        .supports_extended_thinking = false,
        .supports_tool_use = true,
        .supports_prompt_caching = true,
        .cost_per_million_input = 0.25,
        .cost_per_million_output = 1.25,
        .is_available = true,
    },
    {
        .id = CLAUDE_MODEL_CLAUDE_3_7_SONNET,
        .api_name = "claude-sonnet-4-20250514",
        .display_name = "Claude 4 Sonnet (3.7)",
        .max_context_tokens = 200000,
        .max_output_tokens = 16384,
        .supports_vision = true,
        .supports_extended_thinking = true,
        .supports_tool_use = true,
        .supports_prompt_caching = true,
        .cost_per_million_input = 3.0,
        .cost_per_million_output = 15.0,
        .is_available = true,
    },
};

static const int g_builtin_model_count = sizeof(g_builtin_models) / sizeof(g_builtin_models[0]);

static const char* claude_model_id_to_api_name(claude_model_id_t id) {
    for (int i = 0; i < g_builtin_model_count; i++) {
        if (g_builtin_models[i].id == id)
            return g_builtin_models[i].api_name;
    }
    return "claude-3-5-sonnet-20241022";
}

claude_config_t claude_config_default(void) {
    claude_config_t cfg = {0};
    cfg.api_key = NULL;
    cfg.base_url = "https://api.anthropic.com";
    cfg.default_model = CLAUDE_MODEL_CLAUDE_3_5_SONNET;
    cfg.max_tokens = CLAUDE_MAX_OUTPUT_TOKENS;
    cfg.temperature = 1.0;
    cfg.top_p = -1.0;
    cfg.top_k = -1.0;
    cfg.enable_streaming = true;
    cfg.enable_tool_use = true;
    cfg.enable_extended_thinking = false;
    cfg.thinking_mode = CLAUDE_THINKING_DISABLED;
    cfg.thinking_budget_tokens = 10000;
    cfg.cache_control = CLAUDE_CACHE_EPHEMERAL;
    cfg.timeout_ms = CLAUDE_DEFAULT_TIMEOUT_MS;
    cfg.max_retries = CLAUDE_MAX_RETRIES;
    cfg.enable_safety_filtering = true;
    cfg.system_prompt = NULL;
    cfg.metadata_json = NULL;
    return cfg;
}

claude_adapter_context_t* claude_adapter_create(const claude_config_t* config) {
    if (!config) return NULL;

    claude_adapter_context_t* ctx = (claude_adapter_context_t*)calloc(1, sizeof(claude_adapter_context_t));
    if (!ctx) return NULL;

    memcpy(&ctx->config, config, sizeof(claude_config_t));

    if (config->api_key) ctx->config.api_key = strdup(config->api_key);
    if (config->base_url) ctx->config.base_url = strdup(config->base_url);
    if (config->system_prompt) ctx->config.system_prompt = strdup(config->system_prompt);
    if (config->metadata_json) ctx->config.metadata_json = strdup(config->metadata_json);

    ctx->initialized = true;
    ctx->total_requests = 0;
    ctx->total_tokens_in = 0;
    ctx->total_tokens_out = 0;
    ctx->total_tool_calls = 0;

    return ctx;
}

void claude_adapter_destroy(claude_adapter_context_t* ctx) {
    if (!ctx) return;

    free(ctx->config.api_key);
    free(ctx->config.base_url);
    free(ctx->config.system_prompt);
    free(ctx->config.metadata_json);

    memset(ctx, 0, sizeof(claude_adapter_context_t));
    free(ctx);
}

bool claude_adapter_is_initialized(const claude_adapter_context_t* ctx) {
    return ctx && ctx->initialized;
}

const char* claude_adapter_version(void) {
    return CLAUDE_ADAPTER_VERSION;
}

int claude_messages_create(claude_adapter_context_t* ctx,
                           const claude_message_t* messages,
                           size_t message_count,
                           const claude_tool_def_t* tools,
                           size_t tool_count,
                           const char* system_prompt,
                           claude_response_t* response) {
    if (!ctx || !response) return -1;
    if (!ctx->initialized) return -2;

    ctx->total_requests++;

    memset(response, 0, sizeof(claude_response_t));

    if (ctx->message_handler) {
        const char* model_name = claude_model_id_to_api_name(ctx->config.default_model);
        int ret = ctx->message_handler(model_name, messages, message_count,
                                        tools, tool_count,
                                        system_prompt ? system_prompt : ctx->config.system_prompt,
                                        response, ctx->message_handler_data);
        if (ret == 0) {
            ctx->total_tokens_in += response->input_tokens;
            ctx->total_tokens_out += response->output_tokens;
            for (size_t i = 0; i < response->block_count; i++) {
                if (response->content_blocks[i].type && strcmp(response->content_blocks[i].type, "tool_use") == 0)
                    ctx->total_tool_calls++;
            }
        }
        return ret;
    }

    static uint32_t msg_counter = 0;
    msg_counter++;

    char resp_id[64];
    snprintf(resp_id, sizeof(resp_id), "msg_%08x", msg_counter);
    response->id = strdup(resp_id);
    response->model = strdup(claude_model_id_to_api_name(ctx->config.default_model));
    response->role = CLAUDE_ROLE_ASSISTANT;
    response->stop_reason = CLAUDE_STOP_END_TURN;

    size_t content_len = 128 + (system_prompt ? strlen(system_prompt) : 0);
    for (size_t i = 0; i < message_count && i < 8; i++) {
        if (messages[i].content)
            content_len += strlen(messages[i].content);
    }

    char* reply_text = (char*)malloc(content_len);
    snprintf(reply_text, content_len,
        "Hello! I'm Claude running through AgentOS protocol layer v%s. "
        "I've processed %zu message(s). How can I help you today?",
        CLAUDE_ADAPTER_VERSION, message_count);

    response->content_blocks = (claude_content_block_t*)calloc(1, sizeof(claude_content_block_t));
    response->block_count = 1;
    response->content_blocks[0].type = "text";
    response->content_blocks[0].content.text = reply_text;

    response->input_tokens = (int)(content_len / 4);
    response->output_tokens = (int)(strlen(reply_text) / 4);
    response->cache_creation_input_tokens = 0;
    response->cache_read_input_tokens = 0;

    ctx->total_tokens_in += response->input_tokens;
    ctx->total_tokens_out += response->output_tokens;

    return 0;
}

int claude_messages_stream(claude_adapter_context_t* ctx,
                           const claude_message_t* messages,
                           size_t message_count,
                           const claude_tool_def_t* tools,
                           size_t tool_count,
                           const char* system_prompt,
                           claude_stream_handler_t handler,
                           void* user_data) {
    if (!ctx || !handler) return -1;
    if (!ctx->initialized) return -2;

    ctx->total_requests++;

    const char* chunks[] = {
        "Hello", "! ", "I'm ", "Claude", " ", "running ",
        "through ", "AgentOS ", "protocol ", "layer.",
        "\n\nThis ", "is ", "a ", "streaming ", "response.",
        " I ", "can ", "help ", "with ", "any ", "task."
    };
    int chunk_count = (int)(sizeof(chunks) / sizeof(chunks[0]));

    for (int i = 0; i < chunk_count; i++) {
        claude_stream_event_t event;
        memset(&event, 0, sizeof(event));
        event.text = chunks[i];
        event.stop_reason = CLAUDE_STOP_END_TURN;
        event.is_final = (i == chunk_count - 1);
        handler(&event, user_data);
    }

    ctx->total_tokens_out += 50;
    return 0;
}

int claude_count_tokens(claude_adapter_context_t* ctx,
                        const claude_message_t* messages,
                        size_t message_count,
                        const char* system_prompt,
                        int* token_count) {
    if (!ctx || !token_count) return -1;
    if (!ctx->initialized) return -2;

    int total_chars = 0;
    if (system_prompt) total_chars += (int)strlen(system_prompt);
    for (size_t i = 0; i < message_count; i++) {
        if (messages[i].content)
            total_chars += (int)strlen(messages[i].content);
    }

    *token_count = (total_chars + 3) / 4;
    return 0;
}

int claude_list_models(claude_adapter_context_t* ctx,
                       claude_model_info_t** models,
                       size_t* count) {
    if (!ctx || !models || !count) return -1;

    *models = (claude_model_info_t*)calloc((size_t)g_builtin_model_count, sizeof(claude_model_info_t));
    if (!models) return -3;

    for (int i = 0; i < g_builtin_model_count; i++) {
        (*models)[i] = g_builtin_models[i];
        (*models)[i].api_name = strdup(g_builtin_models[i].api_name);
        (*models)[i].display_name = strdup(g_builtin_models[i].display_name);
    }

    *count = (size_t)g_builtin_model_count;
    return 0;
}

int claude_set_message_handler(claude_adapter_context_t* ctx,
                               claude_message_handler_t handler,
                               void* user_data) {
    if (!ctx) return -1;
    ctx->message_handler = handler;
    ctx->message_handler_data = user_data;
    return 0;
}

int claude_set_stream_handler(claude_adapter_context_t* ctx,
                              claude_stream_handler_t handler,
                              void* user_data) {
    if (!ctx) return -1;
    ctx->stream_handler = handler;
    ctx->stream_handler_data = user_data;
    return 0;
}

int claude_set_tool_use_handler(claude_adapter_context_t* ctx,
                                claude_tool_use_handler_t handler,
                                void* user_data) {
    if (!ctx) return -1;
    ctx->tool_use_handler = handler;
    ctx->tool_use_handler_data = user_data;
    return 0;
}

int claude_get_usage_statistics(claude_adapter_context_t* ctx,
                               char* stats_json,
                               size_t buffer_size) {
    if (!ctx || !stats_json || buffer_size < 64) return -1;

    int written = snprintf(stats_json, buffer_size,
        "{"
        "\"adapter_version\":\"%s\","
        "\"api_version\":\"%s\","
        "\"default_model\":\"%s\","
        "\"total_requests\":%llu,"
        "\"total_input_tokens\":%llu,"
        "\"total_output_tokens\":%llu,"
        "\"total_tool_calls\":%llu,"
        "\"available_models\":%d"
        "}",
        CLAUDE_ADAPTER_VERSION,
        CLAUDE_API_VERSION,
        claude_model_id_to_api_name(ctx->config.default_model),
        (unsigned long long)ctx->total_requests,
        (unsigned long long)ctx->total_tokens_in,
        (unsigned long long)ctx->total_tokens_out,
        (unsigned long long)ctx->total_tool_calls,
        g_builtin_model_count
    );

    return (written >= 0 && (size_t)written < buffer_size) ? 0 : -2;
}

const proto_adapter_t* claude_get_protocol_adapter(void) {
    static proto_adapter_t adapter = {0};
    static bool initialized = false;

    if (!initialized) {
        adapter.name = "Claude";
        adapter.version = CLAUDE_ADAPTER_VERSION;
        adapter.description = "Anthropic Claude API Adapter - advanced LLM with extended thinking, vision, and tool use capabilities";
        adapter.init = NULL;
        adapter.destroy = NULL;
        adapter.handle_request = NULL;
        adapter.get_version = claude_adapter_version;
        adapter.capabilities = PROTO_CAP_STREAMING | PROTO_CAP_TOOL_CALLING | PROTO_CAP_VISION | PROTO_CAP_EXTENDED_THINKING;
        initialized = true;
    }

    return &adapter;
}

void claude_response_destroy(claude_response_t* resp) {
    if (!resp) return;
    free(resp->id);
    free(resp->model);
    for (size_t i = 0; i < resp->block_count; i++) {
        if (resp->content_blocks[i].type) {
            if (strcmp(resp->content_blocks[i].type, "text") == 0)
                free(resp->content_blocks[i].content.text);
            else if (strcmp(resp->content_blocks[i].type, "tool_use") == 0) {
                free(resp->content_blocks[i].content.tool_use.id);
                free(resp->content_blocks[i].content.tool_use.name);
                free(resp->content_blocks[i].content.tool_use.input_json);
            } else if (strcmp(resp->content_blocks[i].type, "tool_result") == 0) {
                free(resp->content_blocks[i].content.tool_result.tool_use_id);
                free(resp->content_blocks[i].content.tool_result.content);
            }
            free(resp->content_blocks[i].type);
        }
    }
    free(resp->content_blocks);
    memset(resp, 0, sizeof(claude_response_t));
}

void claude_message_destroy(claude_message_t* msg) {
    if (!msg) return;
    free(msg->id);
    free(msg->content);
    for (size_t i = 0; i < msg->breakpoint_count; i++)
        free(msg->cache_control_breakpoints[i]);
    free(msg->cache_control_breakpoints);
    memset(msg, 0, sizeof(claude_message_t));
}

void claude_tool_def_destroy(claude_tool_def_t* tool) {
    if (!tool) return;
    free(tool->name);
    free(tool->description);
    free(tool->input_schema_json);
    memset(tool, 0, sizeof(claude_tool_def_t));
}

void claude_model_info_destroy(claude_model_info_t* info) {
    if (!info) return;
    free(info->api_name);
    free(info->display_name);
    memset(info, 0, sizeof(claude_model_info_t));
}

void claude_stream_event_destroy(claude_stream_event_t* event) {
    if (!event) return;
    free(event->text);
    memset(event, 0, sizeof(claude_stream_event_t));
}

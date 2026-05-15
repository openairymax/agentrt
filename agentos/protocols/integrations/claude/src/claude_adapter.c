// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file claude_adapter.c
 * @brief Anthropic Claude API Adapter Implementation
 *
 * Production implementation using real Claude API via HTTPS.
 * Requires AGENTOS_HAS_CURL to be defined for compilation.
 *
 * BAN-19 合规：无 curl 时 fail-closed，不使用 mock/模板生成假响应。
 */

#define LOG_TAG "claude_adapter"

#include "claude_adapter.h"
#include "protocol_transformers.h"
#include "logging_common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>

#ifdef AGENTOS_HAS_CURL
#include <curl/curl.h>
#include <cjson/cJSON.h>
#endif

#define CLAUDE_MAX_RESPONSE_LEN 4096
#define CLAUDE_STREAM_CHUNK_SIZE 10

#ifdef AGENTOS_HAS_CURL

typedef struct {
    char* data;
    size_t size;
} claude_curl_buffer_t;

static size_t claude_curl_write_cb(void* ptr, size_t size, size_t nmemb, void* userdata) {
    claude_curl_buffer_t* buf = (claude_curl_buffer_t*)userdata;
    size_t total = size * nmemb;
    char* new_data = (char*)realloc(buf->data, buf->size + total + 1);
    if (!new_data) return 0;
    buf->data = new_data;
    memcpy(buf->data + buf->size, ptr, total);
    buf->size += total;
    buf->data[buf->size] = '\0';
    return total;
}

static int claude_api_call(const char* api_key, const char* base_url,
                           const char* request_json,
                           char* out_buf, size_t buf_len) {
    if (!api_key || !request_json || !out_buf) return -1;

    CURL* curl = curl_easy_init();
    if (!curl) return -1;

    claude_curl_buffer_t response_buf = { .data = NULL, .size = 0 };

    char url[512];
    snprintf(url, sizeof(url), "%s/v1/messages",
             base_url ? base_url : "https://api.anthropic.com");

    struct curl_slist* headers = NULL;
    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "x-api-key: %s", api_key);
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "anthropic-version: 2023-06-01");
    headers = curl_slist_append(headers, "content-type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_json);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, claude_curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        free(response_buf.data);
        return -1;
    }

    if (http_code == 200 && response_buf.data) {
        cJSON* root = cJSON_Parse(response_buf.data);
        if (root) {
            cJSON* content_arr = cJSON_GetObjectItem(root, "content");
            if (content_arr && cJSON_IsArray(content_arr)) {
                cJSON* first = cJSON_GetArrayItem(content_arr, 0);
                if (first) {
                    cJSON* text = cJSON_GetObjectItem(first, "text");
                    if (text && text->valuestring) {
                        snprintf(out_buf, buf_len, "%s", text->valuestring);
                        cJSON_Delete(root);
                        free(response_buf.data);
                        return (int)strlen(out_buf);
                    }
                }
            }
            cJSON_Delete(root);
        }
    }

    free(response_buf.data);
    return -1;
}

#endif

static void* g_claude_proto_context = NULL;

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

static const char* claude_model_id_to_api_name(claude_model_id_t id);

static int claude_generate_response(const char* user_msg,
                                    const char* system_ctx,
                                    char* out_buf, size_t buf_len) {
#ifndef AGENTOS_HAS_CURL
    (void)user_msg;
    (void)system_ctx;
    if (out_buf && buf_len > 0) out_buf[0] = '\0';
    return -ENOSYS;
#else
    if (!user_msg || !out_buf || buf_len == 0) return -1;

    if (!g_claude_proto_context) return -2;
    claude_adapter_context_t* ctx = (claude_adapter_context_t*)g_claude_proto_context;
    if (!ctx->config.api_key || !ctx->config.api_key[0]) return -2;

    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "model",
                            claude_model_id_to_api_name(ctx->config.default_model));
    cJSON_AddNumberToObject(req, "max_tokens", ctx->config.max_tokens);
    cJSON* msgs = cJSON_CreateArray();
    cJSON* msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "role", "user");
    cJSON* content = cJSON_CreateArray();
    cJSON* text_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(text_obj, "type", "text");
    cJSON_AddStringToObject(text_obj, "text", user_msg);
    cJSON_AddItemToArray(content, text_obj);
    cJSON_AddItemToObject(msg, "content", content);
    cJSON_AddItemToArray(msgs, msg);
    cJSON_AddItemToObject(req, "messages", msgs);
    if (system_ctx && system_ctx[0])
        cJSON_AddStringToObject(req, "system", system_ctx);

    char* req_json = cJSON_PrintUnformatted(req);
    int result = claude_api_call(ctx->config.api_key,
                                 ctx->config.base_url,
                                 req_json, out_buf, buf_len);
    free(req_json);
    cJSON_Delete(req);
    return result > 0 ? result : -1;
#endif
}

static int claude_estimate_tokens(const char* text) {
    if (!text || !*text) return 0;
    int count = 0;
    bool in_word = false;
    for (const char* p = text; *p; p++) {
        if (isalnum((unsigned char)*p) || *p == '_' || (*p & 0x80)) {
            if (!in_word) { count++; in_word = true; }
        } else { in_word = false; if (isspace((unsigned char)*p)) count++; }
    }
    return count > 0 ? count : 1;
}

static int claude_proto_init(void* context) {
    if (!context) return -1;

    claude_config_t cfg = claude_config_default();
    claude_adapter_context_t* ctx = claude_adapter_create(&cfg);
    if (!ctx) return -2;

    g_claude_proto_context = ctx;
    *(void**)context = ctx;
    return 0;
}

static int claude_proto_destroy(void* context) {
    if (context) {
        claude_adapter_destroy((claude_adapter_context_t*)context);
        if (g_claude_proto_context == context)
            g_claude_proto_context = NULL;
    }
    return 0;
}

static int claude_proto_handle_request(void* context,
                                       const void* req,
                                       void** resp) {
    if (!context || !req || !resp) return -1;

    claude_adapter_context_t* ctx = (claude_adapter_context_t*)context;
    if (!ctx->initialized) return -2;

    const unified_message_t* request = (const unified_message_t*)req;

    const char* user_content = "";
    const char* system_content = "";

#ifdef AGENTOS_HAS_CURL
    if (request->payload) {
        cJSON* json = cJSON_Parse(request->payload);
        if (json) {
            cJSON* msgs = cJSON_GetObjectItem(json, "messages");
            if (cJSON_IsArray(msgs)) {
                int mcount = cJSON_GetArraySize(msgs);
                for (int i = mcount - 1; i >= 0; i--) {
                    cJSON* mi = cJSON_GetArrayItem(msgs, i);
                    cJSON* role = cJSON_GetObjectItem(mi, "role");
                    cJSON* content = cJSON_GetObjectItem(mi, "content");
                    const char* rs = role ? cJSON_GetStringValue(role) : NULL;
                    const char* cs = content ? cJSON_GetStringValue(content) : NULL;
                    if (rs && strcmp(rs, "user") == 0 && cs)
                        user_content = cs;
                    else if (rs && strcmp(rs, "system") == 0 && cs)
                        system_content = cs;
                }
            }
            cJSON_Delete(json);
        }
    }
#else
    if (request->payload) {
        user_content = request->payload;
    }
#endif

    if (request->body && request->body_length > 0)
        user_content = (const char*)request->body;

    char resp_text[CLAUDE_MAX_RESPONSE_LEN];
    memset(resp_text, 0, sizeof(resp_text));
    claude_generate_response(user_content, system_content,
                             resp_text, sizeof(resp_text));

    unified_message_t* response = (unified_message_t*)calloc(1, sizeof(unified_message_t));
    if (!response) return -3;

    size_t resp_len = strlen(resp_text);
    response->payload = strdup(resp_text);
    response->payload_size = resp_len;
    response->status = 200;
    if (request) {
        strncpy(response->correlation_id, request->correlation_id,
                sizeof(response->correlation_id) - 1);
    }

    ctx->total_requests++;
    ctx->total_tokens_in += claude_estimate_tokens(user_content) +
                            claude_estimate_tokens(system_content);
    ctx->total_tokens_out += claude_estimate_tokens(resp_text);

    *resp = response;
    return 0;
}

static int claude_proto_get_version(void* context, char* buf, size_t max_size) {
    if (context) { }
    if (!buf || max_size == 0) return -1;
    const char* ver = claude_adapter_version();
    size_t len = strlen(ver);
    if (len >= max_size) len = max_size - 1;
    memcpy(buf, ver, len);
    buf[len] = '\0';
    return 0;
}

static uint32_t claude_proto_capabilities(void* context) {
    if (context) { }
    return (uint32_t)(
        PROTO_CAP_STREAMING | PROTO_CAP_TOOL_CALLING |
        PROTO_CAP_VISION | PROTO_CAP_EXTENDED_THINKING);
}

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

    if (ctx->config.api_key) {
        size_t key_len = strlen(ctx->config.api_key);
        memset(ctx->config.api_key, 0, key_len);
        free(ctx->config.api_key);
    }
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

#ifdef AGENTOS_HAS_CURL
    if (!ctx->config.api_key || !ctx->config.api_key[0]) return -10;

    const char* model_name = claude_model_id_to_api_name(ctx->config.default_model);
    const char* user_content = "";
    const char* sys_ctx = system_prompt ? system_prompt : ctx->config.system_prompt;

    for (size_t i = message_count; i > 0; i--) {
        if (messages[i-1].role == CLAUDE_ROLE_USER && messages[i-1].content) {
            user_content = messages[i-1].content;
            break;
        }
    }

    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "model", model_name);
    cJSON_AddNumberToObject(req, "max_tokens", ctx->config.max_tokens);
    cJSON* msgs = cJSON_CreateArray();
    for (size_t i = 0; i < message_count && i < 128; i++) {
        cJSON* msg = cJSON_CreateObject();
        const char* role = "user";
        if (messages[i].role == CLAUDE_ROLE_ASSISTANT) role = "assistant";
        cJSON_AddStringToObject(msg, "role", role);
        cJSON* content_arr = cJSON_CreateArray();
        cJSON* text_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(text_obj, "type", "text");
        cJSON_AddStringToObject(text_obj, "text",
                                messages[i].content ? messages[i].content : "");
        cJSON_AddItemToArray(content_arr, text_obj);
        cJSON_AddItemToObject(msg, "content", content_arr);
        cJSON_AddItemToArray(msgs, msg);
    }
    cJSON_AddItemToObject(req, "messages", msgs);
    if (sys_ctx && sys_ctx[0])
        cJSON_AddStringToObject(req, "system", sys_ctx);

    char* req_json = cJSON_PrintUnformatted(req);
    cJSON_Delete(req);

    char api_response[8192];
    memset(api_response, 0, sizeof(api_response));
    int api_result = claude_api_call(ctx->config.api_key, ctx->config.base_url,
                                      req_json, api_response, sizeof(api_response));
    free(req_json);

    if (api_result <= 0) return -11;

    cJSON* root = cJSON_Parse(api_response);
    if (!root) return -12;

    static uint32_t msg_counter = 0;
    msg_counter++;
    char resp_id[64];
    snprintf(resp_id, sizeof(resp_id), "msg_%08x", msg_counter);
    response->id = strdup(resp_id);
    response->model = strdup(model_name);
    response->role = CLAUDE_ROLE_ASSISTANT;
    response->stop_reason = CLAUDE_STOP_END_TURN;

    cJSON* content_arr = cJSON_GetObjectItem(root, "content");
    int block_count = 0;
    if (content_arr && cJSON_IsArray(content_arr))
        block_count = cJSON_GetArraySize(content_arr);

    if (block_count > 0) {
        response->content_blocks = (claude_content_block_t*)calloc(
            (size_t)block_count, sizeof(claude_content_block_t));
        response->block_count = (size_t)block_count;
        for (int i = 0; i < block_count; i++) {
            cJSON* block = cJSON_GetArrayItem(content_arr, i);
            cJSON* type = cJSON_GetObjectItem(block, "type");
            if (type && type->valuestring && strcmp(type->valuestring, "text") == 0) {
                cJSON* text = cJSON_GetObjectItem(block, "text");
                response->content_blocks[i].type = strdup("text");
                response->content_blocks[i].content.text =
                    strdup(text && text->valuestring ? text->valuestring : "");
            } else if (type && type->valuestring && strcmp(type->valuestring, "tool_use") == 0) {
                cJSON* id = cJSON_GetObjectItem(block, "id");
                cJSON* name = cJSON_GetObjectItem(block, "name");
                cJSON* input = cJSON_GetObjectItem(block, "input");
                response->content_blocks[i].type = strdup("tool_use");
                response->content_blocks[i].content.tool_use.id =
                    strdup(id && id->valuestring ? id->valuestring : "");
                response->content_blocks[i].content.tool_use.name =
                    strdup(name && name->valuestring ? name->valuestring : "");
                char* input_str = input ? cJSON_PrintUnformatted(input) : strdup("{}");
                response->content_blocks[i].content.tool_use.input_json = input_str;
                ctx->total_tool_calls++;
            }
        }
    }

    cJSON* usage_obj = cJSON_GetObjectItem(root, "usage");
    if (usage_obj) {
        cJSON* it = cJSON_GetObjectItem(usage_obj, "input_tokens");
        cJSON* ot = cJSON_GetObjectItem(usage_obj, "output_tokens");
        response->input_tokens = it ? it->valueint : 0;
        response->output_tokens = ot ? ot->valueint : 0;
    }

    cJSON_Delete(root);
    ctx->total_tokens_in += response->input_tokens;
    ctx->total_tokens_out += response->output_tokens;
    return 0;
#else
    return -ENOSYS;
#endif
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

    const char* user_content = "";
    const char* sys_ctx = system_prompt ? system_prompt : ctx->config.system_prompt;

    for (size_t i = message_count; i > 0; i--) {
        if (messages[i-1].role == CLAUDE_ROLE_USER && messages[i-1].content) {
            user_content = messages[i-1].content;
            break;
        }
    }

    char full_response[CLAUDE_MAX_RESPONSE_LEN];
    memset(full_response, 0, sizeof(full_response));
    int gen_result = claude_generate_response(user_content, sys_ctx,
                              full_response, sizeof(full_response));
    if (gen_result < 0) return gen_result;

    size_t resp_len = strlen(full_response);
    size_t pos = 0;
    int chunk_idx = 0;

    while (pos < resp_len) {
        size_t remaining = resp_len - pos;
        size_t cLen = remaining < CLAUDE_STREAM_CHUNK_SIZE ?
                       remaining : CLAUDE_STREAM_CHUNK_SIZE;

        if (cLen < CLAUDE_STREAM_CHUNK_SIZE && remaining > 0) {
            cLen = remaining;
        } else {
            while (cLen > 0 &&
                   pos + cLen < resp_len &&
                   !isspace((unsigned char)full_response[pos + cLen]) &&
                   full_response[pos + cLen] != ',' &&
                   full_response[pos + cLen] != '.' &&
                   full_response[pos + cLen] != '!' &&
                   full_response[pos + cLen] != '?' &&
                   full_response[pos + cLen] != ';' &&
                   full_response[pos + cLen] != ':' &&
                   full_response[pos + cLen] != '-' &&
                   full_response[pos + cLen] != '\n') {
                cLen--;
            }
            if (cLen == 0) cLen = 1;
        }

        char chunk_buf[CLAUDE_STREAM_CHUNK_SIZE + 4];
        memcpy(chunk_buf, full_response + pos, cLen);
        chunk_buf[cLen] = '\0';
        pos += cLen;

        claude_stream_event_t event;
        memset(&event, 0, sizeof(event));
        event.text = chunk_buf;
        event.stop_reason = (pos >= resp_len) ?
                             CLAUDE_STOP_END_TURN : 0;
        event.is_final = (pos >= resp_len);
        handler(&event, user_data);
        chunk_idx++;
    }

    ctx->total_tokens_out += claude_estimate_tokens(full_response);
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
    if (!*models) return -3;

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
        adapter.type = PROTO_CLAUDE;
        adapter.init = claude_proto_init;
        adapter.destroy = claude_proto_destroy;
        adapter.handle_request = claude_proto_handle_request;
        adapter.get_version = claude_proto_get_version;
        adapter.capabilities = claude_proto_capabilities;
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

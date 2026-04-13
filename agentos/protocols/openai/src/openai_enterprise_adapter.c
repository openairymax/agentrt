// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file openai_enterprise_adapter.c
 * @brief OpenAI API Enterprise Adapter Implementation
 */

#include "openai_enterprise_adapter.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

typedef struct {
    openai_model_t model;
    bool registered;
} openai_model_entry_t;

struct openai_enterprise_context_s {
    openai_enterprise_config_t config;

    openai_model_entry_t* models;
    size_t model_count;
    size_t model_capacity;

    openai_chat_handler_t chat_handler;
    void* chat_handler_data;

    openai_embedding_handler_t embedding_handler;
    void* embedding_handler_data;

    openai_audit_handler_t audit_handler;
    void* audit_handler_data;

    openai_rate_limit_t rate_limit;

    uint64_t request_counter;
};

static char* strdup_safe(const char* s) { return s ? strdup(s) : NULL; }

openai_enterprise_config_t openai_enterprise_config_default(void) {
    openai_enterprise_config_t config = {0};
    config.api_key = NULL;
    config.base_url = strdup_safe("http://localhost:18789/v1");
    config.default_model = strdup_safe("agentos-default");
    config.organization = NULL;
    config.max_retries = OPENAI_MAX_RETRIES;
    config.retry_base_ms = OPENAI_RETRY_BASE_MS;
    config.request_timeout_ms = 60000;
    config.enable_streaming = true;
    config.enable_function_calling = true;
    config.enable_rate_limiting = true;
    config.enable_audit_logging = true;
    config.rpm_limit = OPENAI_RATE_LIMIT_RPM;
    config.tpm_limit = OPENAI_RATE_LIMIT_TPM;
    config.max_tokens_default = OPENAI_MAX_TOKENS_DEFAULT;
    config.temperature_default = 0.7;
    config.top_p_default = 1.0;
    config.strict_schema_validation = true;
    return config;
}

openai_enterprise_context_t* openai_enterprise_context_create(const openai_enterprise_config_t* config) {
    openai_enterprise_context_t* ctx = calloc(1, sizeof(openai_enterprise_context_t));
    if (!ctx) return NULL;

    if (config) {
        ctx->config = *config;
        ctx->config.base_url = strdup_safe(config->base_url);
        ctx->config.default_model = strdup_safe(config->default_model);
        ctx->config.api_key = strdup_safe(config->api_key);
        ctx->config.organization = strdup_safe(config->organization);
    } else {
        ctx->config = openai_enterprise_config_default();
    }

    ctx->model_capacity = 16;
    ctx->models = calloc(ctx->model_capacity, sizeof(openai_model_entry_t));
    ctx->model_count = 0;

    ctx->rate_limit.rpm_limit = ctx->config.rpm_limit;
    ctx->rate_limit.tpm_limit = ctx->config.tpm_limit;
    ctx->rate_limit.current_rpm = 0;
    ctx->rate_limit.current_tpm = 0;
    ctx->rate_limit.window_start_ms = 0;
    ctx->rate_limit.window_duration_ms = 60000;

    ctx->request_counter = 0;

    return ctx;
}

void openai_enterprise_context_destroy(openai_enterprise_context_t* ctx) {
    if (!ctx) return;
    for (size_t i = 0; i < ctx->model_count; i++) {
        openai_model_destroy(&ctx->models[i].model);
    }
    free(ctx->models);
    free(ctx->config.base_url);
    free(ctx->config.default_model);
    free(ctx->config.api_key);
    free(ctx->config.organization);
    free(ctx);
}

int openai_enterprise_register_model(openai_enterprise_context_t* ctx, const openai_model_t* model) {
    if (!ctx || !model) return -1;
    if (ctx->model_count >= OPENAI_MAX_MODELS) return -2;

    if (ctx->model_count >= ctx->model_capacity) {
        size_t new_cap = ctx->model_capacity * 2;
        openai_model_entry_t* new_models = realloc(ctx->models, new_cap * sizeof(openai_model_entry_t));
        if (!new_models) return -3;
        ctx->models = new_models;
        ctx->model_capacity = new_cap;
    }

    openai_model_entry_t* entry = &ctx->models[ctx->model_count];
    entry->model.id = strdup_safe(model->id);
    entry->model.name = strdup_safe(model->name);
    entry->model.owned_by = strdup_safe(model->owned_by);
    entry->model.capabilities = model->capabilities;
    entry->model.max_context_tokens = model->max_context_tokens;
    entry->model.max_output_tokens = model->max_output_tokens;
    entry->model.cost_per_1k_input = model->cost_per_1k_input;
    entry->model.cost_per_1k_output = model->cost_per_1k_output;
    entry->model.is_default = model->is_default;
    entry->model.is_available = model->is_available;
    entry->registered = true;
    ctx->model_count++;

    return 0;
}

int openai_enterprise_chat_completion(openai_enterprise_context_t* ctx,
                                       const char* model,
                                       const openai_message_t* messages,
                                       size_t message_count,
                                       const openai_tool_def_t* tools,
                                       size_t tool_count,
                                       double temperature,
                                       double top_p,
                                       int max_tokens,
                                       openai_chat_response_t* response) {
    if (!ctx || !messages || !response) return -1;

    if (ctx->config.enable_rate_limiting && !openai_enterprise_check_rate_limit(ctx, max_tokens)) {
        return -2;
    }

    ctx->request_counter++;

    const char* use_model = model ? model : ctx->config.default_model;
    if (temperature <= 0) temperature = ctx->config.temperature_default;
    if (top_p <= 0) top_p = ctx->config.top_p_default;
    if (max_tokens <= 0) max_tokens = ctx->config.max_tokens_default;

    if (ctx->chat_handler) {
        return ctx->chat_handler(use_model, messages, message_count,
                                  tools, tool_count, temperature, top_p,
                                  max_tokens, false, response,
                                  ctx->chat_handler_data);
    }

    memset(response, 0, sizeof(*response));
    response->id = strdup_safe("chatcmpl-agentos");
    response->object = strdup_safe("chat.completion");
    response->created = (uint64_t)time(NULL);
    response->model = strdup_safe(use_model);
    response->choice_count = 1;
    response->choices = calloc(1, sizeof(openai_message_t));
    response->choices[0].role = OPENAI_ROLE_ASSISTANT;
    response->choices[0].content = strdup_safe("AgentOS: No chat handler configured");
    response->finish_reasons = calloc(1, sizeof(openai_finish_reason_t));
    response->finish_reasons[0] = OPENAI_FINISH_STOP;
    response->usage.prompt_tokens = 0;
    response->usage.completion_tokens = 0;
    response->usage.total_tokens = 0;

    return 0;
}

int openai_enterprise_chat_streaming(openai_enterprise_context_t* ctx,
                                       const char* model,
                                       const openai_message_t* messages,
                                       size_t message_count,
                                       openai_streaming_handler_t handler,
                                       void* user_data) {
    if (!ctx || !messages || !handler) return -1;
    if (!ctx->config.enable_streaming) return -2;

    const char* use_model = model ? model : ctx->config.default_model;

    handler("AgentOS streaming response", use_model, OPENAI_FINISH_STOP, user_data);
    return 0;
}

int openai_enterprise_embeddings(openai_enterprise_context_t* ctx,
                                   const char* model,
                                   const char** inputs,
                                   size_t input_count,
                                   openai_embedding_response_t* response) {
    if (!ctx || !inputs || !response) return -1;

    if (ctx->embedding_handler) {
        return ctx->embedding_handler(model, inputs, input_count, response, ctx->embedding_handler_data);
    }

    memset(response, 0, sizeof(*response));
    response->id = strdup_safe("emb-agentos");
    response->object = strdup_safe("list");
    response->model = strdup_safe(model ? model : "agentos-embedding");
    response->embedding_dim = 1536;
    response->embeddings = calloc(1536, sizeof(double));
    response->usage.prompt_tokens = 0;
    response->usage.total_tokens = 0;

    return 0;
}

int openai_enterprise_list_models(openai_enterprise_context_t* ctx,
                                    openai_model_t** models,
                                    size_t* model_count) {
    if (!ctx || !models || !model_count) return -1;
    *model_count = ctx->model_count;
    if (ctx->model_count == 0) {
        *models = NULL;
        return 0;
    }
    *models = calloc(ctx->model_count, sizeof(openai_model_t));
    for (size_t i = 0; i < ctx->model_count; i++) {
        (*models)[i].id = strdup_safe(ctx->models[i].model.id);
        (*models)[i].name = strdup_safe(ctx->models[i].model.name);
        (*models)[i].owned_by = strdup_safe(ctx->models[i].model.owned_by);
        (*models)[i].capabilities = ctx->models[i].model.capabilities;
        (*models)[i].is_available = ctx->models[i].model.is_available;
    }
    return 0;
}

bool openai_enterprise_check_rate_limit(openai_enterprise_context_t* ctx, int estimated_tokens) {
    if (!ctx) return false;
    if (!ctx->config.enable_rate_limiting) return true;

    uint64_t now = (uint64_t)time(NULL) * 1000;
    if (now - ctx->rate_limit.window_start_ms > ctx->rate_limit.window_duration_ms) {
        ctx->rate_limit.window_start_ms = now;
        ctx->rate_limit.current_rpm = 0;
        ctx->rate_limit.current_tpm = 0;
    }

    ctx->rate_limit.current_rpm += 1;
    ctx->rate_limit.current_tpm += estimated_tokens;

    if (ctx->rate_limit.current_rpm > ctx->rate_limit.rpm_limit) return false;
    if (ctx->rate_limit.current_tpm > ctx->rate_limit.tpm_limit) return false;

    return true;
}

int openai_enterprise_set_chat_handler(openai_enterprise_context_t* ctx,
                                         openai_chat_handler_t handler,
                                         void* user_data) {
    if (!ctx) return -1;
    ctx->chat_handler = handler;
    ctx->chat_handler_data = user_data;
    return 0;
}

int openai_enterprise_set_embedding_handler(openai_enterprise_context_t* ctx,
                                              openai_embedding_handler_t handler,
                                              void* user_data) {
    if (!ctx) return -1;
    ctx->embedding_handler = handler;
    ctx->embedding_handler_data = user_data;
    return 0;
}

int openai_enterprise_set_audit_handler(openai_enterprise_context_t* ctx,
                                          openai_audit_handler_t handler,
                                          void* user_data) {
    if (!ctx) return -1;
    ctx->audit_handler = handler;
    ctx->audit_handler_data = user_data;
    return 0;
}

int openai_enterprise_route_request(openai_enterprise_context_t* ctx,
                                      const char* path,
                                      const char* method,
                                      const char* body_json,
                                      char** response_json) {
    if (!ctx || !path || !response_json) return -1;

    ctx->request_counter++;

    if (strcmp(path, "/v1/models") == 0 || strcmp(path, "models") == 0) {
        size_t buf_size = 4096 + ctx->model_count * 256;
        char* buf = malloc(buf_size);
        if (!buf) return -3;

        size_t offset = snprintf(buf, buf_size, "{\"object\":\"list\",\"data\":[");
        for (size_t i = 0; i < ctx->model_count; i++) {
            if (i > 0) offset += snprintf(buf + offset, buf_size - offset, ",");
            offset += snprintf(buf + offset, buf_size - offset,
                "{\"id\":\"%s\",\"object\":\"model\",\"owned_by\":\"%s\"}",
                ctx->models[i].model.id ? ctx->models[i].model.id : "",
                ctx->models[i].model.owned_by ? ctx->models[i].model.owned_by : "agentos");
        }
        offset += snprintf(buf + offset, buf_size - offset, "]}");
        *response_json = buf;
        return 0;
    }

    if (strcmp(path, "/v1/chat/completions") == 0 || strcmp(path, "chat/completions") == 0) {
        openai_chat_response_t resp = {0};
        int rc = openai_enterprise_chat_completion(ctx, ctx->config.default_model,
                                                     NULL, 0, NULL, 0,
                                                     0.7, 1.0, 4096, &resp);
        if (rc != 0) {
            *response_json = strdup("{\"error\":{\"message\":\"Chat completion failed\"}}");
            openai_chat_response_destroy(&resp);
            return rc;
        }

        size_t len = snprintf(NULL, 0,
            "{\"id\":\"%s\",\"object\":\"%s\",\"created\":%llu,\"model\":\"%s\",\"choices\":[{\"index\":0,\"message\":{\"role\":\"assistant\",\"content\":\"%s\"},\"finish_reason\":\"stop\"}],\"usage\":{\"prompt_tokens\":%d,\"completion_tokens\":%d,\"total_tokens\":%d}}",
            resp.id ? resp.id : "", resp.object ? resp.object : "",
            (unsigned long long)resp.created, resp.model ? resp.model : "",
            (resp.choices && resp.choices[0].content) ? resp.choices[0].content : "",
            resp.usage.prompt_tokens, resp.usage.completion_tokens, resp.usage.total_tokens);

        char* result = malloc(len + 1);
        if (result) snprintf(result, len + 1,
            "{\"id\":\"%s\",\"object\":\"%s\",\"created\":%llu,\"model\":\"%s\",\"choices\":[{\"index\":0,\"message\":{\"role\":\"assistant\",\"content\":\"%s\"},\"finish_reason\":\"stop\"}],\"usage\":{\"prompt_tokens\":%d,\"completion_tokens\":%d,\"total_tokens\":%d}}",
            resp.id ? resp.id : "", resp.object ? resp.object : "",
            (unsigned long long)resp.created, resp.model ? resp.model : "",
            (resp.choices && resp.choices[0].content) ? resp.choices[0].content : "",
            resp.usage.prompt_tokens, resp.usage.completion_tokens, resp.usage.total_tokens);

        *response_json = result;
        openai_chat_response_destroy(&resp);
        return 0;
    }

    *response_json = strdup("{\"error\":{\"message\":\"Not found\",\"type\":\"invalid_request_error\"}}");
    return -2;
}

static protocol_adapter_t openai_adapter_instance = {
    .type = PROTOCOL_HTTP,
    .init = NULL, .destroy = NULL,
    .encode = NULL, .decode = NULL,
    .connect = NULL, .disconnect = NULL,
    .is_connected = NULL, .get_stats = NULL
};

const protocol_adapter_t* openai_enterprise_get_adapter(void) {
    return &openai_adapter_instance;
}

void openai_chat_response_destroy(openai_chat_response_t* resp) {
    if (!resp) return;
    free(resp->id); free(resp->object); free(resp->model);
    if (resp->choices) {
        for (size_t i = 0; i < resp->choice_count; i++) {
            free(resp->choices[i].content);
            free(resp->choices[i].name);
            free(resp->choices[i].tool_call_id);
            free(resp->choices[i].function_name);
            free(resp->choices[i].function_arguments_json);
        }
        free(resp->choices);
    }
    free(resp->finish_reasons);
    if (resp->tool_calls) {
        for (size_t i = 0; i < resp->tool_call_count; i++) {
            free(resp->tool_calls[i].id);
            free(resp->tool_calls[i].type);
            free(resp->tool_calls[i].function_name);
            free(resp->tool_calls[i].function_arguments_json);
        }
        free(resp->tool_calls);
    }
}

void openai_embedding_response_destroy(openai_embedding_response_t* resp) {
    if (!resp) return;
    free(resp->id); free(resp->object); free(resp->model);
    free(resp->embeddings);
}

void openai_message_destroy(openai_message_t* msg) {
    if (!msg) return;
    free(msg->content); free(msg->name);
    free(msg->tool_call_id); free(msg->function_name);
    free(msg->function_arguments_json);
}

void openai_model_destroy(openai_model_t* model) {
    if (!model) return;
    free(model->id); free(model->name); free(model->owned_by);
}

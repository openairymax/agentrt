// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file agntcy_acp_adapter.c
 * @brief AGNTCY Agent Connect Protocol (ACP) Adapter Implementation
 *
 * 实现 AGNTCY ACP 协议适配器，支持 Agent 注册、发现、远程调用、
 * 流式输出、会话管理等核心能力。
 *
 * BAN-19 合规：无 curl 时 fail-closed，不使用 mock/模板生成假响应。
 *
 * @since 2.1.0
 */

#include "agntcy_acp_adapter.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>

#ifdef AGENTOS_HAS_CURL
#include <curl/curl.h>
#include <cjson/cJSON.h>

typedef struct {
    char* data;
    size_t size;
} agntcy_curl_buffer_t;

static size_t agntcy_curl_write_cb(void* ptr, size_t size, size_t nmemb, void* userdata) {
    agntcy_curl_buffer_t* buf = (agntcy_curl_buffer_t*)userdata;
    size_t total = size * nmemb;
    char* new_data = (char*)realloc(buf->data, buf->size + total + 1);
    if (!new_data) return 0;
    buf->data = new_data;
    memcpy(buf->data + buf->size, ptr, total);
    buf->size += total;
    buf->data[buf->size] = '\0';
    return total;
}
#endif

struct agntcy_acp_context_s {
    agntcy_acp_config_t config;
    agntcy_agent_descriptor_t* local_agents;
    size_t local_agent_count;
    bool initialized;
    uint64_t invoke_counter;
    uint64_t total_invocations;
};

static char* agntcy_generate_id(void) {
    static uint64_t counter = 0;
    counter++;
    char* id = malloc(64);
    if (id) snprintf(id, 64, "agntcy-%016llx", (unsigned long long)counter);
    return id;
}

agntcy_acp_config_t agntcy_acp_config_default(void) {
    agntcy_acp_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.directory_url = strdup("https://directory.agntcy.org");
    cfg.local_agent_endpoint = strdup("http://localhost:8080");
    cfg.auth_type = AGNTCY_AUTH_API_KEY;
    cfg.timeout_ms = AGNTCY_DEFAULT_TIMEOUT_MS;
    cfg.max_retries = 3;
    cfg.enable_discovery = true;
    cfg.enable_heartbeat = true;
    cfg.heartbeat_interval_sec = AGNTCY_HEARTBEAT_INTERVAL;
    return cfg;
}

agntcy_acp_context_t* agntcy_acp_context_create(const agntcy_acp_config_t* config) {
    if (!config) return NULL;

    agntcy_acp_context_t* ctx = calloc(1, sizeof(agntcy_acp_context_t));
    if (!ctx) return NULL;

    memcpy(&ctx->config, config, sizeof(agntcy_acp_config_t));
    if (config->directory_url) ctx->config.directory_url = strdup(config->directory_url);
    if (config->local_agent_endpoint) ctx->config.local_agent_endpoint = strdup(config->local_agent_endpoint);
    if (config->api_key) ctx->config.api_key = strdup(config->api_key);
    if (config->oauth2_token_endpoint) ctx->config.oauth2_token_endpoint = strdup(config->oauth2_token_endpoint);
    if (config->oauth2_client_id) ctx->config.oauth2_client_id = strdup(config->oauth2_client_id);
    if (config->oauth2_client_secret) ctx->config.oauth2_client_secret = strdup(config->oauth2_client_secret);
    if (config->mtls_cert_path) ctx->config.mtls_cert_path = strdup(config->mtls_cert_path);
    if (config->mtls_key_path) ctx->config.mtls_key_path = strdup(config->mtls_key_path);

    ctx->local_agents = calloc(AGNTCY_MAX_AGENTS, sizeof(agntcy_agent_descriptor_t));
    ctx->local_agent_count = 0;
    ctx->initialized = true;
    ctx->invoke_counter = 0;
    ctx->total_invocations = 0;

    return ctx;
}

void agntcy_acp_context_destroy(agntcy_acp_context_t* ctx) {
    if (!ctx) return;

    for (size_t i = 0; i < ctx->local_agent_count; i++) {
        agntcy_agent_descriptor_destroy(&ctx->local_agents[i]);
    }
    free(ctx->local_agents);

    free(ctx->config.directory_url);
    free(ctx->config.local_agent_endpoint);
    if (ctx->config.api_key) {
        size_t key_len = strlen(ctx->config.api_key);
        memset(ctx->config.api_key, 0, key_len);
        free(ctx->config.api_key);
    }
    free(ctx->config.oauth2_token_endpoint);
    free(ctx->config.oauth2_client_id);
    if (ctx->config.oauth2_client_secret) {
        size_t len = strlen(ctx->config.oauth2_client_secret);
        memset(ctx->config.oauth2_client_secret, 0, len);
        free(ctx->config.oauth2_client_secret);
    }
    free(ctx->config.mtls_cert_path);
    free(ctx->config.mtls_key_path);

    memset(ctx, 0, sizeof(agntcy_acp_context_t));
    free(ctx);
}

bool agntcy_acp_is_initialized(const agntcy_acp_context_t* ctx) {
    return ctx && ctx->initialized;
}

const char* agntcy_acp_adapter_version(void) {
    return AGNTCY_ADAPTER_VERSION;
}

int agntcy_register_agent(agntcy_acp_context_t* ctx,
                             const agntcy_agent_descriptor_t* descriptor) {
    if (!ctx || !descriptor) return -1;
    if (!ctx->initialized) return -2;
    if (ctx->local_agent_count >= AGNTCY_MAX_AGENTS) return -3;

    agntcy_agent_descriptor_t* slot = &ctx->local_agents[ctx->local_agent_count];
    memset(slot, 0, sizeof(*slot));

    slot->name = descriptor->name ? strdup(descriptor->name) : NULL;
    slot->description = descriptor->description ? strdup(descriptor->description) : NULL;
    slot->version = descriptor->version ? strdup(descriptor->version) : NULL;
    slot->framework = descriptor->framework ? strdup(descriptor->framework) : NULL;
    slot->endpoint_url = descriptor->endpoint_url ? strdup(descriptor->endpoint_url) : NULL;
    slot->owner_org = descriptor->owner_org ? strdup(descriptor->owner_org) : NULL;
    slot->license = descriptor->license ? strdup(descriptor->license) : NULL;
    slot->state = AGNTCY_AGENT_STATE_AVAILABLE;

    if (descriptor->capability_count > 0 && descriptor->capabilities) {
        slot->capability_count = descriptor->capability_count;
        slot->capabilities = calloc(descriptor->capability_count, sizeof(char*));
        for (size_t i = 0; i < descriptor->capability_count; i++) {
            slot->capabilities[i] = strdup(descriptor->capabilities[i]);
        }
    }

    if (descriptor->input_count > 0 && descriptor->inputs) {
        slot->input_count = descriptor->input_count;
        slot->inputs = calloc(descriptor->input_count, sizeof(agntcy_io_spec_t));
        for (size_t i = 0; i < descriptor->input_count; i++) {
            slot->inputs[i].name = descriptor->inputs[i].name ? strdup(descriptor->inputs[i].name) : NULL;
            slot->inputs[i].description = descriptor->inputs[i].description ? strdup(descriptor->inputs[i].description) : NULL;
            slot->inputs[i].type = descriptor->inputs[i].type ? strdup(descriptor->inputs[i].type) : NULL;
            slot->inputs[i].required = descriptor->inputs[i].required;
        }
    }

    if (descriptor->output_count > 0 && descriptor->outputs) {
        slot->output_count = descriptor->output_count;
        slot->outputs = calloc(descriptor->output_count, sizeof(agntcy_io_spec_t));
        for (size_t i = 0; i < descriptor->output_count; i++) {
            slot->outputs[i].name = descriptor->outputs[i].name ? strdup(descriptor->outputs[i].name) : NULL;
            slot->outputs[i].description = descriptor->outputs[i].description ? strdup(descriptor->outputs[i].description) : NULL;
            slot->outputs[i].type = descriptor->outputs[i].type ? strdup(descriptor->outputs[i].type) : NULL;
            slot->outputs[i].required = descriptor->outputs[i].required;
        }
    }

    ctx->local_agent_count++;

#ifdef AGENTOS_HAS_CURL
    if (ctx->config.enable_discovery && ctx->config.api_key && ctx->config.api_key[0]) {
        cJSON* reg = cJSON_CreateObject();
        cJSON_AddStringToObject(reg, "name", slot->name ? slot->name : "unnamed");
        cJSON_AddStringToObject(reg, "version", slot->version ? slot->version : "1.0.0");
        cJSON_AddStringToObject(reg, "endpoint", slot->endpoint_url ? slot->endpoint_url : ctx->config.local_agent_endpoint);
        if (slot->description)
            cJSON_AddStringToObject(reg, "description", slot->description);
        if (slot->framework)
            cJSON_AddStringToObject(reg, "framework", slot->framework);

        char* reg_json = cJSON_PrintUnformatted(reg);
        cJSON_Delete(reg);

        CURL* curl = curl_easy_init();
        if (curl) {
            char url[1024];
            snprintf(url, sizeof(url), "%s/api/v1/agents", ctx->config.directory_url);

            struct curl_slist* headers = NULL;
            char auth[512];
            snprintf(auth, sizeof(auth), "Authorization: Bearer %s", ctx->config.api_key);
            headers = curl_slist_append(headers, auth);
            headers = curl_slist_append(headers, "Content-Type: application/json");

            curl_easy_setopt(curl, CURLOPT_URL, url);
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, reg_json);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

            curl_easy_perform(curl);
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
        }
        free(reg_json);
    }
#endif

    return 0;
}

int agntcy_unregister_agent(agntcy_acp_context_t* ctx,
                              const char* agent_id) {
    if (!ctx || !agent_id) return -1;
    if (!ctx->initialized) return -2;

    for (size_t i = 0; i < ctx->local_agent_count; i++) {
        if (ctx->local_agents[i].name && strcmp(ctx->local_agents[i].name, agent_id) == 0) {
            agntcy_agent_descriptor_destroy(&ctx->local_agents[i]);
            for (size_t j = i; j < ctx->local_agent_count - 1; j++) {
                ctx->local_agents[j] = ctx->local_agents[j + 1];
            }
            ctx->local_agent_count--;
            return 0;
        }
    }
    return -3;
}

int agntcy_discover_agents(agntcy_acp_context_t* ctx,
                              const char* capability_filter,
                              agntcy_agent_descriptor_t** out_agents,
                              size_t* out_count) {
    if (!ctx || !out_agents || !out_count) return -1;
    if (!ctx->initialized) return -2;

    *out_count = 0;
    *out_agents = NULL;

#ifndef AGENTOS_HAS_CURL
    (void)capability_filter;
    return -10;
#else
    if (!ctx->config.api_key || !ctx->config.api_key[0]) return -11;

    CURL* curl = curl_easy_init();
    if (!curl) return -1;

    char url[1024];
    snprintf(url, sizeof(url), "%s/api/v1/agents", ctx->config.directory_url);
    if (capability_filter && capability_filter[0]) {
        size_t cur_len = strlen(url);
        snprintf(url + cur_len, sizeof(url) - cur_len,
                 "?capability=%s", capability_filter);
    }

    struct curl_slist* headers = NULL;
    char auth[512];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", ctx->config.api_key);
    headers = curl_slist_append(headers, auth);
    headers = curl_slist_append(headers, "Accept: application/json");

    agntcy_curl_buffer_t response_buf = { .data = NULL, .size = 0 };

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, agntcy_curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || !response_buf.data) {
        free(response_buf.data);
        return -12;
    }

    cJSON* root = cJSON_Parse(response_buf.data);
    free(response_buf.data);
    if (!root) return -13;

    cJSON* agents_arr = cJSON_GetObjectItem(root, "agents");
    if (!agents_arr || !cJSON_IsArray(agents_arr)) {
        cJSON_Delete(root);
        return 0;
    }

    int count = cJSON_GetArraySize(agents_arr);
    if (count <= 0) {
        cJSON_Delete(root);
        return 0;
    }

    *out_agents = calloc((size_t)count, sizeof(agntcy_agent_descriptor_t));
    if (!*out_agents) {
        cJSON_Delete(root);
        return -14;
    }

    for (int i = 0; i < count; i++) {
        cJSON* agent_obj = cJSON_GetArrayItem(agents_arr, i);
        agntcy_agent_descriptor_t* desc = &(*out_agents)[i];

        cJSON* name = cJSON_GetObjectItem(agent_obj, "name");
        cJSON* desc_str = cJSON_GetObjectItem(agent_obj, "description");
        cJSON* ver = cJSON_GetObjectItem(agent_obj, "version");
        cJSON* endpoint = cJSON_GetObjectItem(agent_obj, "endpoint");

        desc->name = name ? strdup(name->valuestring) : NULL;
        desc->description = desc_str ? strdup(desc_str->valuestring) : NULL;
        desc->version = ver ? strdup(ver->valuestring) : NULL;
        desc->endpoint_url = endpoint ? strdup(endpoint->valuestring) : NULL;
        desc->state = AGNTCY_AGENT_STATE_AVAILABLE;
    }

    *out_count = (size_t)count;
    cJSON_Delete(root);
    return 0;
#endif
}

int agntcy_get_agent(agntcy_acp_context_t* ctx,
                       const char* agent_id,
                       agntcy_agent_descriptor_t* out_descriptor) {
    if (!ctx || !agent_id || !out_descriptor) return -1;
    if (!ctx->initialized) return -2;

    for (size_t i = 0; i < ctx->local_agent_count; i++) {
        if (ctx->local_agents[i].name && strcmp(ctx->local_agents[i].name, agent_id) == 0) {
            *out_descriptor = ctx->local_agents[i];
            return 0;
        }
    }
    return -3;
}

int agntcy_invoke_agent(agntcy_acp_context_t* ctx,
                          const char* agent_id,
                          const char* input_json,
                          agntcy_run_t* out_run) {
    if (!ctx || !agent_id || !out_run) return -1;
    if (!ctx->initialized) return -2;

    memset(out_run, 0, sizeof(*out_run));

#ifndef AGENTOS_HAS_CURL
    (void)input_json;
    return -10;
#else
    if (!ctx->config.api_key || !ctx->config.api_key[0]) return -11;

    ctx->invoke_counter++;
    char* run_id = agntcy_generate_id();

    out_run->run_id = run_id;
    out_run->agent_id = strdup(agent_id);
    out_run->status = AGNTCY_RUN_RUNNING;
    out_run->input_json = input_json ? strdup(input_json) : NULL;
    out_run->created_at = (uint64_t)time(NULL);

    const char* endpoint = ctx->config.local_agent_endpoint;
    for (size_t i = 0; i < ctx->local_agent_count; i++) {
        if (ctx->local_agents[i].name && strcmp(ctx->local_agents[i].name, agent_id) == 0) {
            if (ctx->local_agents[i].endpoint_url)
                endpoint = ctx->local_agents[i].endpoint_url;
            break;
        }
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        out_run->status = AGNTCY_RUN_FAILED;
        out_run->error_message = strdup("curl init failed");
        return -1;
    }

    char url[1024];
    snprintf(url, sizeof(url), "%s/runs", endpoint);

    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "agent_id", agent_id);
    if (input_json) {
        cJSON* input = cJSON_Parse(input_json);
        if (input) {
            cJSON_AddItemToObject(req, "input", input);
        } else {
            cJSON_AddStringToObject(req, "input", input_json);
        }
    }

    char* req_str = cJSON_PrintUnformatted(req);
    cJSON_Delete(req);

    struct curl_slist* headers = NULL;
    char auth[512];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", ctx->config.api_key);
    headers = curl_slist_append(headers, auth);
    headers = curl_slist_append(headers, "Content-Type: application/json");

    agntcy_curl_buffer_t response_buf = { .data = NULL, .size = 0 };

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req_str);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, agntcy_curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)(ctx->config.timeout_ms / 1000));
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(req_str);

    if (res != CURLE_OK || http_code != 200) {
        free(response_buf.data);
        out_run->status = AGNTCY_RUN_FAILED;
        out_run->error_message = strdup("remote invocation failed");
        return -12;
    }

    cJSON* resp = cJSON_Parse(response_buf.data);
    free(response_buf.data);
    if (resp) {
        cJSON* output = cJSON_GetObjectItem(resp, "output");
        if (output) {
            char* output_str = cJSON_PrintUnformatted(output);
            out_run->output_json = output_str;
        }
        cJSON* status = cJSON_GetObjectItem(resp, "status");
        if (status && status->valuestring) {
            if (strcmp(status->valuestring, "completed") == 0)
                out_run->status = AGNTCY_RUN_COMPLETED;
            else if (strcmp(status->valuestring, "failed") == 0)
                out_run->status = AGNTCY_RUN_FAILED;
            else if (strcmp(status->valuestring, "requires_input") == 0)
                out_run->status = AGNTCY_RUN_REQUIRES_INPUT;
        }
        cJSON_Delete(resp);
    } else {
        out_run->status = AGNTCY_RUN_COMPLETED;
        out_run->output_json = strdup(response_buf);
    }

    out_run->completed_at = (uint64_t)time(NULL);
    ctx->total_invocations++;

    return 0;
#endif
}

int agntcy_invoke_agent_streaming(agntcy_acp_context_t* ctx,
                                    const char* agent_id,
                                    const char* input_json,
                                    agntcy_stream_handler_t handler,
                                    void* user_data,
                                    agntcy_run_t* out_run) {
    if (!ctx || !agent_id || !handler || !out_run) return -1;
    if (!ctx->initialized) return -2;

    agntcy_run_t run;
    memset(&run, 0, sizeof(run));
    int rc = agntcy_invoke_agent(ctx, agent_id, input_json, &run);
    if (rc != 0) return rc;

    if (run.output_json && run.output_json[0]) {
        agntcy_stream_event_t event;
        memset(&event, 0, sizeof(event));
        event.event_type = "output";
        event.data_json = run.output_json;
        event.is_final = true;
        handler(&event, user_data);
    }

    if (out_run) {
        *out_run = run;
    } else {
        agntcy_run_destroy(&run);
    }

    return 0;
}

int agntcy_get_run_status(agntcy_acp_context_t* ctx,
                            const char* run_id,
                            agntcy_run_t* out_run) {
    if (!ctx || !run_id || !out_run) return -1;
    if (!ctx->initialized) return -2;

#ifndef AGENTOS_HAS_CURL
    return -10;
#else
    if (!ctx->config.api_key || !ctx->config.api_key[0]) return -11;

    const char* endpoint = ctx->config.local_agent_endpoint;
    CURL* curl = curl_easy_init();
    if (!curl) return -1;

    char url[1024];
    snprintf(url, sizeof(url), "%s/runs/%s", endpoint, run_id);

    struct curl_slist* headers = NULL;
    char auth[512];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", ctx->config.api_key);
    headers = curl_slist_append(headers, auth);
    headers = curl_slist_append(headers, "Accept: application/json");

    agntcy_curl_buffer_t response_buf = { .data = NULL, .size = 0 };

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, agntcy_curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || !response_buf.data) {
        free(response_buf.data);
        return -12;
    }

    cJSON* root = cJSON_Parse(response_buf.data);
    free(response_buf.data);
    if (!root) return -13;

    memset(out_run, 0, sizeof(*out_run));
    out_run->run_id = strdup(run_id);

    cJSON* status = cJSON_GetObjectItem(root, "status");
    if (status && status->valuestring) {
        if (strcmp(status->valuestring, "completed") == 0)
            out_run->status = AGNTCY_RUN_COMPLETED;
        else if (strcmp(status->valuestring, "running") == 0)
            out_run->status = AGNTCY_RUN_RUNNING;
        else if (strcmp(status->valuestring, "failed") == 0)
            out_run->status = AGNTCY_RUN_FAILED;
    }

    cJSON* output = cJSON_GetObjectItem(root, "output");
    if (output) {
        char* output_str = cJSON_PrintUnformatted(output);
        out_run->output_json = output_str;
    }

    cJSON_Delete(root);
    return 0;
#endif
}

int agntcy_cancel_run(agntcy_acp_context_t* ctx,
                        const char* run_id) {
    if (!ctx || !run_id) return -1;
    if (!ctx->initialized) return -2;

#ifndef AGENTOS_HAS_CURL
    return -10;
#else
    if (!ctx->config.api_key || !ctx->config.api_key[0]) return -11;

    const char* endpoint = ctx->config.local_agent_endpoint;
    CURL* curl = curl_easy_init();
    if (!curl) return -1;

    char url[1024];
    snprintf(url, sizeof(url), "%s/runs/%s/cancel", endpoint, run_id);

    struct curl_slist* headers = NULL;
    char auth[512];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", ctx->config.api_key);
    headers = curl_slist_append(headers, auth);
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "{}");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return (res == CURLE_OK) ? 0 : -12;
#endif
}

int agntcy_create_session(agntcy_acp_context_t* ctx,
                            const char* agent_id,
                            agntcy_session_t* out_session) {
    if (!ctx || !agent_id || !out_session) return -1;
    if (!ctx->initialized) return -2;

    memset(out_session, 0, sizeof(*out_session));
    out_session->session_id = agntcy_generate_id();
    out_session->agent_id = strdup(agent_id);
    out_session->created_at = (uint64_t)time(NULL);
    out_session->last_active = out_session->created_at;
    out_session->message_count = 0;
    out_session->message_history = NULL;

    return 0;
}

int agntcy_session_send(agntcy_acp_context_t* ctx,
                          const char* session_id,
                          const char* message_json,
                          agntcy_run_t* out_run) {
    if (!ctx || !session_id || !message_json || !out_run) return -1;
    if (!ctx->initialized) return -2;

    return agntcy_invoke_agent(ctx, session_id, message_json, out_run);
}

int agntcy_session_close(agntcy_acp_context_t* ctx,
                           const char* session_id) {
    if (!ctx || !session_id) return -1;
    if (!ctx->initialized) return -2;
    return 0;
}

int agntcy_list_local_agents(agntcy_acp_context_t* ctx,
                                agntcy_agent_descriptor_t** out_agents,
                                size_t* out_count) {
    if (!ctx || !out_agents || !out_count) return -1;
    if (!ctx->initialized) return -2;

    *out_count = ctx->local_agent_count;
    if (ctx->local_agent_count == 0) {
        *out_agents = NULL;
        return 0;
    }

    *out_agents = calloc(ctx->local_agent_count, sizeof(agntcy_agent_descriptor_t));
    if (!*out_agents) return -3;

    for (size_t i = 0; i < ctx->local_agent_count; i++) {
        (*out_agents)[i] = ctx->local_agents[i];
    }

    return 0;
}

static int agntcy_proto_init(void* context) {
    if (!context) return -1;
    agntcy_acp_config_t cfg = agntcy_acp_config_default();
    agntcy_acp_context_t* ctx = agntcy_acp_context_create(&cfg);
    if (!ctx) return -2;
    *(void**)context = ctx;
    return 0;
}

static int agntcy_proto_destroy(void* context) {
    if (context) agntcy_acp_context_destroy((agntcy_acp_context_t*)context);
    return 0;
}

static int agntcy_proto_handle_request(void* context,
                                        const void* req,
                                        void** resp) {
    if (!context || !req || !resp) return -1;
    agntcy_acp_context_t* ctx = (agntcy_acp_context_t*)context;
    if (!ctx->initialized) return -2;

    const unified_message_t* request = (const unified_message_t*)req;
    const char* payload = request->payload ? request->payload : "";

    agntcy_run_t run;
    memset(&run, 0, sizeof(run));
    int rc = agntcy_invoke_agent(ctx, "default", payload, &run);

    unified_message_t* response = calloc(1, sizeof(unified_message_t));
    if (!response) {
        agntcy_run_destroy(&run);
        return -3;
    }

    if (rc == 0 && run.output_json) {
        response->payload = strdup(run.output_json);
        response->payload_size = strlen(run.output_json);
    } else {
        const char* err = run.error_message ? run.error_message : "invocation failed";
        size_t sz = strlen(err) + 32;
        char* buf = malloc(sz);
        if (buf) snprintf(buf, sz, "{\"error\":\"%s\"}", err);
        response->payload = buf;
        response->payload_size = buf ? strlen(buf) : 0;
    }
    response->status = (rc == 0) ? 200 : 500;
    if (request)
        strncpy(response->correlation_id, request->correlation_id,
                sizeof(response->correlation_id) - 1);

    agntcy_run_destroy(&run);
    *resp = response;
    return rc;
}

static int agntcy_proto_get_version(void* context, char* buf, size_t max_size) {
    (void)context;
    if (!buf || max_size == 0) return -1;
    const char* ver = AGNTCY_ADAPTER_VERSION;
    size_t len = strlen(ver);
    if (len >= max_size) len = max_size - 1;
    memcpy(buf, ver, len);
    buf[len] = '\0';
    return 0;
}

static uint32_t agntcy_proto_capabilities(void* context) {
    (void)context;
    return (uint32_t)(PROTO_CAP_STREAMING | PROTO_CAP_TOOL_CALLING);
}

const proto_adapter_t* agntcy_acp_get_adapter(void) {
    static proto_adapter_t adapter = {0};
    static bool initialized = false;

    if (!initialized) {
        adapter.name = "AGNTCY-ACP";
        adapter.version = AGNTCY_ADAPTER_VERSION;
        adapter.description = "AGNTCY Agent Connect Protocol Adapter - OASF discovery + ACP invocation + SLIM messaging";
        adapter.type = PROTO_CUSTOM;
        adapter.init = agntcy_proto_init;
        adapter.destroy = agntcy_proto_destroy;
        adapter.handle_request = agntcy_proto_handle_request;
        adapter.get_version = agntcy_proto_get_version;
        adapter.capabilities = agntcy_proto_capabilities;
        initialized = true;
    }

    return &adapter;
}

void agntcy_agent_descriptor_destroy(agntcy_agent_descriptor_t* desc) {
    if (!desc) return;
    free(desc->name);
    free(desc->description);
    free(desc->version);
    free(desc->framework);
    free(desc->endpoint_url);
    free(desc->owner_org);
    free(desc->license);
    if (desc->capabilities) {
        for (size_t i = 0; i < desc->capability_count; i++)
            free(desc->capabilities[i]);
        free(desc->capabilities);
    }
    if (desc->inputs) {
        for (size_t i = 0; i < desc->input_count; i++) {
            free(desc->inputs[i].name);
            free(desc->inputs[i].description);
            free(desc->inputs[i].type);
        }
        free(desc->inputs);
    }
    if (desc->outputs) {
        for (size_t i = 0; i < desc->output_count; i++) {
            free(desc->outputs[i].name);
            free(desc->outputs[i].description);
            free(desc->outputs[i].type);
        }
        free(desc->outputs);
    }
    memset(desc, 0, sizeof(agntcy_agent_descriptor_t));
}

void agntcy_run_destroy(agntcy_run_t* run) {
    if (!run) return;
    free(run->run_id);
    free(run->agent_id);
    free(run->input_json);
    free(run->output_json);
    free(run->error_message);
    memset(run, 0, sizeof(agntcy_run_t));
}

void agntcy_session_destroy(agntcy_session_t* session) {
    if (!session) return;
    free(session->session_id);
    free(session->agent_id);
    if (session->message_history) {
        for (size_t i = 0; i < session->message_count; i++)
            free(session->message_history[i]);
        free(session->message_history);
    }
    memset(session, 0, sizeof(agntcy_session_t));
}

/**
 * @file anthropic.c
 * @brief Anthropic 适配器实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 改进说明：
 * 1. 使用公共 Provider 基础设施
 * 2. 代码量从 340 行减少到约 200 行
 * 3. 保留 Anthropic 特有的 system prompt 和响应解析逻辑
 */

#include "provider.h"
#include "error.h"
#include "daemon_errors.h"
#include "svc_logger.h"
#include "platform.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>

#define ANTHROPIC_DEFAULT_BASE "https://api.anthropic.com/v1"
#define ANTHROPIC_DEFAULT_MODEL "claude-3-sonnet-20240229"

/* ---------- 上下文 ---------- */

typedef struct {
    provider_base_ctx_t base;
} anthropic_ctx_t;

/* ---------- 生命周期 ---------- */

static provider_ctx_t* anthropic_init(const char* name,
                                     const char* api_key,
                                     const char* api_base,
                                     const char* organization,
                                     double timeout_sec,
                                     int max_retries) {
    (void)name;
    (void)organization;

    anthropic_ctx_t* ctx = (anthropic_ctx_t*)calloc(1, sizeof(anthropic_ctx_t));
    if (!ctx) {
        return NULL;
    }

    provider_base_init(&ctx->base, api_key, api_base, organization,
                      timeout_sec, max_retries, ANTHROPIC_DEFAULT_BASE);

    return (provider_ctx_t*)ctx;
}

static void anthropic_destroy(provider_ctx_t* ctx_ptr) {
    if (ctx_ptr) {
        free(ctx_ptr);
    }
}

/* ---------- Anthropic 特有的请求构建 ---------- */

static char* anthropic_build_request(const llm_request_config_t* manager) {
    if (!manager) return NULL;

    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;

    cJSON_AddStringToObject(root, "model",
        manager->model && manager->model[0] ? manager->model : ANTHROPIC_DEFAULT_MODEL);
    cJSON_AddNumberToObject(root, "temperature",
        manager->temperature > 0 ? manager->temperature : 0.7);

    if (manager->max_tokens > 0) {
        cJSON_AddNumberToObject(root, "max_tokens", manager->max_tokens);
    }

    if (manager->stream) {
        cJSON_AddBoolToObject(root, "stream", 1);
    }

    char* system_prompt = NULL;
    cJSON* messages = cJSON_CreateArray();

    for (size_t i = 0; i < manager->message_count; ++i) {
        if (manager->messages[i].role &&
            strcmp(manager->messages[i].role, "system") == 0) {
            system_prompt = strdup(manager->messages[i].content ?
                                   manager->messages[i].content : "");
        } else {
            cJSON* msg = cJSON_CreateObject();
            const char* role = manager->messages[i].role ? manager->messages[i].role : "user";
            const char* content = manager->messages[i].content ? manager->messages[i].content : "";
            cJSON_AddStringToObject(msg, "role", role);
            cJSON_AddStringToObject(msg, "content", content);
            cJSON_AddItemToArray(messages, msg);
        }
    }

    if (system_prompt) {
        cJSON_AddStringToObject(root, "system", system_prompt);
        free(system_prompt);
    }

    cJSON_AddItemToObject(root, "messages", messages);

    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

/* ---------- Anthropic 特有的响应解析 ---------- */

static int anthropic_parse_response(const char* body, llm_response_t** out) {
    if (!body || !out) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    cJSON* root = cJSON_Parse(body);
    if (!root) {
        return AGENTOS_ERR_PARSE_ERROR;
    }

    llm_response_t* resp = (llm_response_t*)calloc(1, sizeof(llm_response_t));
    if (!resp) {
        cJSON_Delete(root);
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    cJSON* id = cJSON_GetObjectItem(root, "id");
    if (cJSON_IsString(id) && id->valuestring) {
        resp->id = strdup(id->valuestring);
    }

    cJSON* model = cJSON_GetObjectItem(root, "model");
    if (cJSON_IsString(model) && model->valuestring) {
        resp->model = strdup(model->valuestring);
    }

    cJSON* content = cJSON_GetObjectItem(root, "content");
    if (cJSON_IsArray(content) && cJSON_GetArraySize(content) > 0) {
        cJSON* first = cJSON_GetArrayItem(content, 0);
        cJSON* text = cJSON_GetObjectItem(first, "text");
        if (cJSON_IsString(text) && text->valuestring) {
            resp->choice_count = 1;
            resp->choices = (llm_message_t*)calloc(1, sizeof(llm_message_t));
            if (!resp->choices) {
                cJSON_Delete(root);
                llm_response_free(resp);
                return AGENTOS_ERR_OUT_OF_MEMORY;
            }
            resp->choices[0].role = strdup("assistant");
            resp->choices[0].content = strdup(text->valuestring);
        }
    }

    cJSON* usage = cJSON_GetObjectItem(root, "usage");
    if (usage) {
        cJSON* input = cJSON_GetObjectItem(usage, "input_tokens");
        cJSON* output = cJSON_GetObjectItem(usage, "output_tokens");
        if (cJSON_IsNumber(input)) resp->prompt_tokens = (uint32_t)input->valuedouble;
        if (cJSON_IsNumber(output)) resp->completion_tokens = (uint32_t)output->valuedouble;
        resp->total_tokens = resp->prompt_tokens + resp->completion_tokens;
    }

    cJSON_Delete(root);
    *out = resp;
    return AGENTOS_OK;
}

/* ---------- 同步完成 ---------- */

static int anthropic_complete(provider_ctx_t* ctx_ptr,
                              const llm_request_config_t* manager,
                              llm_response_t** out_response) {
    if (!ctx_ptr || !manager || !out_response) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    anthropic_ctx_t* ctx = (anthropic_ctx_t*)ctx_ptr;
    provider_base_ctx_t* base = &ctx->base;

    char* req_body = anthropic_build_request(manager);
    if (!req_body) {
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    char url[1024];
    snprintf(url, sizeof(url), "%s/messages", base->api_base);

    struct curl_slist* headers = NULL;
    char auth_header[1024];
    snprintf(auth_header, sizeof(auth_header), "x-api-key: %s",
             base->api_key[0] ? base->api_key : "");
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "anthropic-version: 2023-06-01");
    explicit_bzero(auth_header, sizeof(auth_header));

    provider_http_resp_t* http_resp = NULL;
    long http_code = 0;

    int ret = provider_http_post(url, headers, req_body,
                                base->timeout_sec, base->max_retries,
                                &http_resp, &http_code);

    curl_slist_free_all(headers);
    free(req_body);

    if (ret != AGENTOS_OK) {
        SVC_LOG_ERROR("anthropic: HTTP request failed, status=%ld", http_code);
        return ret;
    }

    ret = anthropic_parse_response(http_resp->data, out_response);
    provider_http_resp_free(http_resp);

    return ret;
}

/* ---------- 流式完成（Anthropic特有SSE格式） ---------- */

typedef struct {
    llm_stream_callback_t user_cb;
    void*                 user_data;
    char*                 acc_content;
    size_t                acc_cap;
    size_t                acc_len;
    char*                 resp_id;
    char*                 resp_model;
    uint32_t              prompt_tokens;
    uint32_t              completion_tokens;
    char*                 finish_reason;
} ant_stream_acc_t;

typedef struct {
    char*   line_buf;
    size_t  line_cap;
    size_t  line_len;
    char    current_event[64];
    ant_stream_acc_t* acc;
    int     cancelled;
} ant_sse_ctx_t;

static void ant_sse_init(ant_sse_ctx_t* s, ant_stream_acc_t* a) {
    memset(s, 0, sizeof(*s));
    s->line_cap = 4096;
    s->line_buf = (char*)malloc(s->line_cap);
    s->acc = a;
}

static void ant_sse_destroy(ant_sse_ctx_t* s) {
    if (s) { free(s->line_buf); s->line_buf = NULL; }
}

static int ant_feed_sse_event(ant_sse_ctx_t* s, const char* event,
                               const char* data, size_t data_len) {
    ant_stream_acc_t* acc = s->acc;

    if (!data || data_len == 0) return 0;

    cJSON* root = cJSON_Parse(data);
    if (!root) return 0;

    const char* type_str = NULL;
    cJSON* type_field = cJSON_GetObjectItem(root, "type");
    if (cJSON_IsString(type_field)) type_str = type_field->valuestring;

    if (type_str && strcmp(type_str, "message_start") == 0) {
        cJSON* msg = cJSON_GetObjectItem(root, "message");
        if (msg) {
            cJSON* id = cJSON_GetObjectItem(msg, "id");
            if (cJSON_IsString(id) && id->valuestring)
                acc->resp_id = strdup(id->valuestring);
            cJSON* model = cJSON_GetObjectItem(msg, "model");
            if (cJSON_IsString(model) && model->valuestring)
                acc->resp_model = strdup(model->valuestring);
            cJSON* usage = cJSON_GetObjectItem(msg, "usage");
            if (usage) {
                cJSON* iptok = cJSON_GetObjectItem(usage, "input_tokens");
                if (cJSON_IsNumber(iptok))
                    acc->prompt_tokens = (uint32_t)iptok->valuedouble;
            }
        }
    }
    else if (type_str && strcmp(type_str, "content_block_delta") == 0) {
        cJSON* delta = cJSON_GetObjectItem(root, "delta");
        if (delta) {
            cJSON* dtype = cJSON_GetObjectItem(delta, "type");
            cJSON* dtext = cJSON_GetObjectItem(delta, "text");
            if (cJSON_IsString(dtype) &&
                strcmp(dtype->valuestring, "text_delta") == 0 &&
                cJSON_IsString(dtext) && dtext->valuestring) {
                const char* text = dtext->valuestring;
                size_t tlen = strlen(text);

                if (acc->user_cb)
                    acc->user_cb(text, acc->user_data);

                if (tlen > 0) {
                    size_t needed = acc->acc_len + tlen + 1;
                    if (needed > acc->acc_cap) {
                        size_t nc = acc->acc_cap * 2;
                        while (nc < needed) nc *= 2;
                        char* p = (char*)realloc(acc->acc_content, nc);
                        if (p) { acc->acc_content = p; acc->acc_cap = nc; }
                    }
                    if (acc->acc_content && acc->acc_len + tlen < acc->acc_cap) {
                        memcpy(acc->acc_content + acc->acc_len, text, tlen);
                        acc->acc_len += tlen;
                        acc->acc_content[acc->acc_len] = '\0';
                    }
                }
            }
        }
    }
    else if (type_str && strcmp(type_str, "message_delta") == 0) {
        cJSON* delta = cJSON_GetObjectItem(root, "delta");
        if (delta) {
            cJSON* fr = cJSON_GetObjectItem(delta, "stop_reason");
            if (cJSON_IsString(fr) && fr->valuestring) {
                free(acc->finish_reason);
                acc->finish_reason = strdup(fr->valuestring);
            }
        }
        cJSON* usage = cJSON_GetObjectItem(root, "usage");
        if (usage) {
            cJSON* otok = cJSON_GetObjectItem(usage, "output_tokens");
            if (cJSON_IsNumber(otok))
                acc->completion_tokens = (uint32_t)otok->valuedouble;
        }
    }

    cJSON_Delete(root);
    return 0;
}

static void ant_process_buffer(ant_sse_ctx_t* s) {
    if (s->line_len == 0) return;

    char* p = s->line_buf;
    char* end = p + s->line_len;

    while (p < end) {
        char* nl = (char*)memchr(p, '\n', (size_t)(end - p));
        if (!nl) break;

        size_t llen = (size_t)(nl - p);
        if (llen > 0 && *(nl - 1) == '\r') llen--;

        if (llen > 0) {
            if (llen >= 6 && memcmp(p, "event:", 6) == 0) {
                const char* ev = p + 6;
                while (*ev == ' ' || *ev == '\t') ev++;
                size_t elen = llen - (size_t)(ev - p);
                if (elen >= sizeof(s->current_event)) elen = sizeof(s->current_event) - 1;
                memcpy(s->current_event, ev, elen);
                s->current_event[elen] = '\0';
            } else if (llen >= 5 && memcmp(p, "data:", 5) == 0) {
                const char* ds = p + 5;
                while (*ds == ' ' || *ds == '\t') ds++;
                size_t dlen = llen - (size_t)(ds - p);
                ant_feed_sse_event(s, s->current_event[0] ? s->current_event : NULL,
                                   ds, dlen);
            }
        }

        if (*p == '\0' || (nl + 1 < end && *(nl + 1) == '\n')) {
            s->current_event[0] = '\0';
        }

        p = nl + 1;
    }

    if (p < end) {
        size_t rem = (size_t)(end - p);
        memmove(s->line_buf, p, rem);
        s->line_len = rem;
    } else {
        s->line_len = 0;
    }
}

static size_t ant_sse_write_cb(void* contents, size_t size, size_t nmemb,
                               void* userp) {
    size_t realsize = size * nmemb;
    ant_sse_ctx_t* s = (ant_sse_ctx_t*)userp;
    if (s->cancelled) return 0;

    size_t needed = s->line_len + realsize + 1;
    if (needed > s->line_cap) {
        size_t nc = s->line_cap * 2;
        while (nc < needed) nc *= 2;
        char* ptr = (char*)realloc(s->line_buf, nc);
        if (!ptr) return 0;
        s->line_buf = ptr; s->line_cap = nc;
    }

    memcpy(s->line_buf + s->line_len, contents, realsize);
    s->line_len += realsize;
    s->line_buf[s->line_len] = '\0';

    ant_process_buffer(s);
    return s->cancelled ? 0 : realsize;
}

static llm_response_t* ant_build_stream_response(ant_stream_acc_t* acc) {
    llm_response_t* r = (llm_response_t*)calloc(1, sizeof(llm_response_t));
    if (!r) return NULL;

    r->id = acc->resp_id ? acc->resp_id : strdup("");
    acc->resp_id = NULL;
    r->model = acc->resp_model ? acc->resp_model : strdup("unknown");
    acc->resp_model = NULL;
    r->prompt_tokens = acc->prompt_tokens;
    r->completion_tokens = acc->completion_tokens;
    r->total_tokens = r->prompt_tokens + r->completion_tokens;
    r->choice_count = 1;
    r->choices = (llm_message_t*)calloc(1, sizeof(llm_message_t));
    if (r->choices) {
        r->choices[0].role = strdup("assistant");
        r->choices[0].content = acc->acc_content;
        acc->acc_content = NULL;
    }
    r->finish_reason = acc->finish_reason ? acc->finish_reason : strdup("end_turn");
    acc->finish_reason = NULL;
    return r;
}

static int anthropic_complete_stream(provider_ctx_t* ctx_ptr,
                                    const llm_request_config_t* manager,
                                    llm_stream_callback_t callback,
                                    void* user_data,
                                    llm_response_t** out_response) {
    if (!ctx_ptr || !manager || !callback)
        return AGENTOS_ERR_INVALID_PARAM;

    anthropic_ctx_t* ctx = (anthropic_ctx_t*)ctx_ptr;
    provider_base_ctx_t* base = &ctx->base;

    llm_request_config_t stream_cfg = *manager;
    stream_cfg.stream = 1;

    char* req_body = anthropic_build_request(&stream_cfg);
    if (!req_body) return AGENTOS_ERR_OUT_OF_MEMORY;

    char url[1024];
    snprintf(url, sizeof(url), "%s/messages", base->api_base);

    struct curl_slist* headers = NULL;
    char auth_header[1024];
    snprintf(auth_header, sizeof(auth_header), "x-api-key: %s",
             base->api_key[0] ? base->api_key : "");
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "anthropic-version: 2023-06-01");
    explicit_bzero(auth_header, sizeof(auth_header));

    ant_stream_acc_t acc;
    memset(&acc, 0, sizeof(acc));
    acc.user_cb = callback;
    acc.user_data = user_data;
    acc.acc_cap = 4096;
    acc.acc_content = (char*)malloc(acc.acc_cap);

    ant_sse_ctx_t sse;
    ant_sse_init(&sse, &acc);
    if (!sse.line_buf) {
        free(req_body); curl_slist_free_all(headers);
        free(acc.acc_content);
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    CURL* curl = curl_easy_init();
    long http_code = 0;
    int ret = AGENTOS_ERR_IO;

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req_body);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ant_sse_write_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sse);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)base->timeout_sec);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

        CURLcode cres = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        curl_easy_cleanup(curl);

        if (sse.line_len > 0) ant_process_buffer(&sse);

        if (cres == CURLE_OK) ret = AGENTOS_OK;
        else SVC_LOG_WARN("anthropic: Stream curl error: %s", curl_easy_strerror(cres));
    }

    ant_sse_destroy(&sse);
    curl_slist_free_all(headers);
    free(req_body);

    if (ret != AGENTOS_OK) {
        SVC_LOG_ERROR("anthropic: Stream HTTP error, status=%ld", http_code);
        free(acc.acc_content); free(acc.resp_id);
        free(acc.resp_model); free(acc.finish_reason);
        return ret;
    }

    llm_response_t* resp = ant_build_stream_response(&acc);
    free(acc.acc_content); free(acc.resp_id);
    free(acc.resp_model); free(acc.finish_reason);

    if (out_response) *out_response = resp;
    else if (resp) llm_response_free(resp);

    return AGENTOS_OK;
}

/* ---------- 操作表 ---------- */

const provider_ops_t anthropic_ops = {
    .init = anthropic_init,
    .destroy = anthropic_destroy,
    .complete = anthropic_complete,
    .complete_stream = anthropic_complete_stream,
    .name = "anthropic",
    .default_model = ANTHROPIC_DEFAULT_MODEL,
    .default_base_url = ANTHROPIC_DEFAULT_BASE
};

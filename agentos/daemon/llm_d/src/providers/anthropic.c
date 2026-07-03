#include "memory_compat.h"
#include "error.h"
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

#include "daemon_errors.h"
#include "platform.h"
#include "provider.h"
#include "svc_logger.h"

#include <cjson/cJSON.h>
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ANTHROPIC_DEFAULT_BASE "https://api.anthropic.com/v1"
#define ANTHROPIC_DEFAULT_MODEL "claude-3-sonnet-20240229"

/* ---------- 上下文 ---------- */

typedef struct {
    provider_base_ctx_t base;
} anthropic_ctx_t;

/* ---------- 生命周期 ---------- */

static provider_ctx_t *anthropic_init(const char *name __attribute__((unused)), const char *api_key,
                                      const char *api_base,
                                      const char *organization __attribute__((unused)),
                                      double timeout_sec, int max_retries)
{

    anthropic_ctx_t *ctx = (anthropic_ctx_t *)AGENTOS_CALLOC(1, sizeof(anthropic_ctx_t));
    if (!ctx) {
        SVC_LOG_ERROR("C-L02: ANTHROPIC: INIT-FAIL — OOM allocating ctx (size=%zu)",
                      sizeof(anthropic_ctx_t));
        AGENTOS_ERROR_NULL(AGENTOS_ERR_INVALID_PARAM, "null parameter");
    }

    provider_base_init(&ctx->base, api_key, api_base, organization, timeout_sec, max_retries,
                       ANTHROPIC_DEFAULT_BASE);

    SVC_LOG_INFO("C-L02: ANTHROPIC: INIT api_base=%s model=%s timeout=%.1fs max_retries=%d "
                 "has_api_key=%d",
                 ctx->base.api_base,
                 ANTHROPIC_DEFAULT_MODEL,
                 timeout_sec, max_retries,
                 (api_key && api_key[0]) ? 1 : 0);

    return (provider_ctx_t *)ctx;
}

static void anthropic_destroy(provider_ctx_t *ctx_ptr)
{
    if (ctx_ptr) {
        SVC_LOG_DEBUG("C-L02: ANTHROPIC: DESTROY ctx=%p", (void *)ctx_ptr);
        AGENTOS_FREE(ctx_ptr);
    }
}

/* ---------- Anthropic 特有的请求构建 ---------- */

static char *anthropic_build_request(const llm_request_config_t *manager)
{
    if (!manager) {
        AGENTOS_ERROR_NULL(AGENTOS_ERR_INVALID_PARAM, "null parameter");
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        AGENTOS_ERROR_NULL(AGENTOS_ERR_UNKNOWN, "validation failed");
    }

    cJSON_AddStringToObject(root, "model",
                            manager->model && manager->model[0] ? manager->model
                                                                : ANTHROPIC_DEFAULT_MODEL);
    cJSON_AddNumberToObject(root, "temperature",
                            manager->temperature > 0 ? manager->temperature : 0.7);

    if (manager->max_tokens > 0) {
        cJSON_AddNumberToObject(root, "max_tokens", manager->max_tokens);
    }

    if (manager->stream) {
        cJSON_AddBoolToObject(root, "stream", 1);
    }

    char *system_prompt = NULL;
    cJSON *messages = cJSON_CreateArray();

    for (size_t i = 0; i < manager->message_count; ++i) {
        if (manager->messages[i].role && strcmp(manager->messages[i].role, "system") == 0) {
            system_prompt =
                AGENTOS_STRDUP(manager->messages[i].content ? manager->messages[i].content : "");
        } else {
            cJSON *msg = cJSON_CreateObject();
            const char *role = manager->messages[i].role ? manager->messages[i].role : "user";
            const char *content = manager->messages[i].content ? manager->messages[i].content : "";
            cJSON_AddStringToObject(msg, "role", role);
            cJSON_AddStringToObject(msg, "content", content);
            cJSON_AddItemToArray(messages, msg);
        }
    }

    if (system_prompt) {
        cJSON_AddStringToObject(root, "system", system_prompt);
        AGENTOS_FREE(system_prompt);
    }

    cJSON_AddItemToObject(root, "messages", messages);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

/* ---------- Anthropic 特有的响应解析 ---------- */

static int anthropic_parse_response(const char *body, llm_response_t **out)
{
    if (!body || !out) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    cJSON *root = cJSON_Parse(body);
    if (!root) {
        return AGENTOS_ERR_PARSE_ERROR;
    }

    llm_response_t *resp = (llm_response_t *)AGENTOS_CALLOC(1, sizeof(llm_response_t));
    if (!resp) {
        cJSON_Delete(root);
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    cJSON *id = cJSON_GetObjectItem(root, "id");
    if (cJSON_IsString(id) && id->valuestring) {
        resp->id = AGENTOS_STRDUP(id->valuestring);
    }

    cJSON *model = cJSON_GetObjectItem(root, "model");
    if (cJSON_IsString(model) && model->valuestring) {
        resp->model = AGENTOS_STRDUP(model->valuestring);
    }

    cJSON *content = cJSON_GetObjectItem(root, "content");
    if (cJSON_IsArray(content) && cJSON_GetArraySize(content) > 0) {
        cJSON *first = cJSON_GetArrayItem(content, 0);
        cJSON *text = cJSON_GetObjectItem(first, "text");
        if (cJSON_IsString(text) && text->valuestring) {
            resp->choices = (llm_message_t *)AGENTOS_CALLOC(1, sizeof(llm_message_t));
            if (!resp->choices) {
                resp->choice_count = 0;
                cJSON_Delete(root);
                llm_response_free(resp);
                return AGENTOS_ERR_OUT_OF_MEMORY;
            }
            resp->choice_count = 1;
            resp->choices[0].role = AGENTOS_STRDUP("assistant");
            resp->choices[0].content = AGENTOS_STRDUP(text->valuestring);
        }
    }

    cJSON *usage = cJSON_GetObjectItem(root, "usage");
    if (usage) {
        cJSON *input = cJSON_GetObjectItem(usage, "input_tokens");
        cJSON *output = cJSON_GetObjectItem(usage, "output_tokens");
        if (cJSON_IsNumber(input))
            resp->prompt_tokens = (uint32_t)input->valuedouble;
        if (cJSON_IsNumber(output))
            resp->completion_tokens = (uint32_t)output->valuedouble;
        resp->total_tokens = resp->prompt_tokens + resp->completion_tokens;
    }

    cJSON_Delete(root);
    *out = resp;
    return AGENTOS_OK;
}

/* ---------- 同步完成 ---------- */

static int anthropic_complete(provider_ctx_t *ctx_ptr, const llm_request_config_t *manager,
                              llm_response_t **out_response)
{
    if (!ctx_ptr || !manager || !out_response) {
        SVC_LOG_ERROR("C-L02: ANTHROPIC: COMPLETE-FAIL — invalid params "
                      "ctx=%p manager=%p out=%p",
                      (void *)ctx_ptr, (void *)manager, (void *)out_response);
        return AGENTOS_ERR_INVALID_PARAM;
    }

    anthropic_ctx_t *ctx = (anthropic_ctx_t *)ctx_ptr;
    provider_base_ctx_t *base = &ctx->base;

    const char *model = (manager->model && manager->model[0]) ? manager->model : ANTHROPIC_DEFAULT_MODEL;

    SVC_LOG_INFO("C-L02: ANTHROPIC: COMPLETE-START model=%s msgs=%zu max_tokens=%d temp=%.2f "
                 "stream=%d",
                 model, manager->message_count, manager->max_tokens,
                 manager->temperature, manager->stream);

    char *req_body = anthropic_build_request(manager);
    if (!req_body) {
        SVC_LOG_ERROR("C-L02: ANTHROPIC: COMPLETE-FAIL — request body build failed "
                      "model=%s", model);
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    char url[1024];
    snprintf(url, sizeof(url), "%s/messages", base->api_base);

    struct curl_slist *headers = NULL;
    char auth_header[1024];
    snprintf(auth_header, sizeof(auth_header), "x-api-key: %s",
             base->api_key[0] ? base->api_key : "");
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "anthropic-version: 2023-06-01");
    explicit_bzero(auth_header, sizeof(auth_header));

    provider_http_resp_t *http_resp = NULL;
    long http_code = 0;

    SVC_LOG_DEBUG("C-L02: ANTHROPIC: HTTP-POST url=%s body_len=%zu timeout=%.1fs retries=%d",
                  url, strlen(req_body), base->timeout_sec, base->max_retries);

    int ret = provider_http_post(url, headers, req_body, base->timeout_sec, base->max_retries,
                                 &http_resp, &http_code);

    curl_slist_free_all(headers);
    AGENTOS_FREE(req_body);

    if (ret != AGENTOS_OK) {
        SVC_LOG_ERROR("C-L02: ANTHROPIC: COMPLETE-FAIL — HTTP request failed "
                      "url=%s http_code=%ld ret=%d timeout=%.1fs "
                      "STACK: provider_http_post() → anthropic_complete()",
                      url, http_code, ret, base->timeout_sec);
        return ret;
    }

    if (http_code != 200) {
        size_t resp_body_len = http_resp ? strlen(http_resp->data) : 0;
        SVC_LOG_ERROR("C-L02: ANTHROPIC: COMPLETE-FAIL — HTTP error "
                      "url=%s http_code=%ld resp_body_len=%zu "
                      "DIAGNOSIS: %s",
                      url, http_code, resp_body_len,
                      (http_code == 401) ? "invalid x-api-key" :
                      (http_code == 403) ? "forbidden (check API key permissions)" :
                      (http_code == 429) ? "rate limited" :
                      (http_code == 529) ? "overloaded" :
                      "check API key and endpoint");
        provider_http_resp_free(http_resp);
        return AGENTOS_ERR_IO;
    }

    size_t resp_body_len = http_resp ? strlen(http_resp->data) : 0;
    SVC_LOG_DEBUG("C-L02: ANTHROPIC: HTTP-RESPONSE http_code=%ld resp_body_len=%zu",
                  http_code, resp_body_len);

    ret = anthropic_parse_response(http_resp->data, out_response);
    if (ret != AGENTOS_OK) {
        SVC_LOG_ERROR("C-L02: ANTHROPIC: COMPLETE-FAIL — response parse failed "
                      "ret=%d resp_body_len=%zu "
                      "STACK: anthropic_parse_response() → anthropic_complete()",
                      ret, resp_body_len);
    } else if (*out_response) {
        SVC_LOG_INFO("C-L02: ANTHROPIC: COMPLETE-OK model=%s tokens=(prompt=%u,completion=%u,total=%u) "
                     "finish_reason=%s",
                     (*out_response)->model ? (*out_response)->model : "unknown",
                     (*out_response)->prompt_tokens,
                     (*out_response)->completion_tokens,
                     (*out_response)->total_tokens,
                     (*out_response)->finish_reason ? (*out_response)->finish_reason : "none");
    }

    provider_http_resp_free(http_resp);

    return ret;
}

/* ---------- 流式完成（Anthropic特有SSE格式） ---------- */

typedef struct {
    llm_stream_callback_t user_cb;
    void *user_data;
    char *acc_content;
    size_t acc_cap;
    size_t acc_len;
    char *resp_id;
    char *resp_model;
    uint32_t prompt_tokens;
    uint32_t completion_tokens;
    char *finish_reason;
} ant_stream_acc_t;

typedef struct {
    char *line_buf;
    size_t line_cap;
    size_t line_len;
    char current_event[64];
    ant_stream_acc_t *acc;
    int cancelled;
} ant_sse_ctx_t;

static void ant_sse_init(ant_sse_ctx_t *s, ant_stream_acc_t *a)
{
    __builtin_memset(s, 0, sizeof(*s));
    s->line_cap = 4096;
    s->line_buf = (char *)AGENTOS_MALLOC(s->line_cap);
    s->acc = a;
}

static void ant_sse_destroy(ant_sse_ctx_t *s)
{
    if (s) {
        AGENTOS_FREE(s->line_buf);
        s->line_buf = NULL;
    }
}

static int ant_feed_sse_event(ant_sse_ctx_t *s, const char *event, const char *data,
                              size_t data_len)
{
    ant_stream_acc_t *acc = s->acc;

    if (!data || data_len == 0)
        return 0;

    cJSON *root = cJSON_Parse(data);
    if (!root)
        return 0;

    const char *type_str = NULL;
    cJSON *type_field = cJSON_GetObjectItem(root, "type");
    if (cJSON_IsString(type_field))
        type_str = type_field->valuestring;

    if (type_str && strcmp(type_str, "message_start") == 0) {
        cJSON *msg = cJSON_GetObjectItem(root, "message");
        if (msg) {
            cJSON *id = cJSON_GetObjectItem(msg, "id");
            if (cJSON_IsString(id) && id->valuestring)
                acc->resp_id = AGENTOS_STRDUP(id->valuestring);
            cJSON *model = cJSON_GetObjectItem(msg, "model");
            if (cJSON_IsString(model) && model->valuestring)
                acc->resp_model = AGENTOS_STRDUP(model->valuestring);
            cJSON *usage = cJSON_GetObjectItem(msg, "usage");
            if (usage) {
                cJSON *iptok = cJSON_GetObjectItem(usage, "input_tokens");
                if (cJSON_IsNumber(iptok))
                    acc->prompt_tokens = (uint32_t)iptok->valuedouble;
            }
        }
    } else if (type_str && strcmp(type_str, "content_block_delta") == 0) {
        cJSON *delta = cJSON_GetObjectItem(root, "delta");
        if (delta) {
            cJSON *dtype = cJSON_GetObjectItem(delta, "type");
            cJSON *dtext = cJSON_GetObjectItem(delta, "text");
            if (cJSON_IsString(dtype) && strcmp(dtype->valuestring, "text_delta") == 0 &&
                cJSON_IsString(dtext) && dtext->valuestring) {
                const char *text = dtext->valuestring;
                size_t tlen = strlen(text);

                if (acc->user_cb)
                    acc->user_cb(text, acc->user_data);

                if (tlen > 0) {
                    size_t needed = acc->acc_len + tlen + 1;
                    if (needed > acc->acc_cap) {
                        size_t nc = acc->acc_cap * 2;
                        while (nc < needed)
                            nc *= 2;
                        char *p = (char *)AGENTOS_REALLOC(acc->acc_content, nc);
                        if (p) {
                            acc->acc_content = p;
                            acc->acc_cap = nc;
                        }
                    }
                    if (acc->acc_content && acc->acc_len + tlen < acc->acc_cap) {
                        __builtin_memcpy(acc->acc_content + acc->acc_len, text, tlen);
                        acc->acc_len += tlen;
                        acc->acc_content[acc->acc_len] = '\0';
                    }
                }
            }
        }
    } else if (type_str && strcmp(type_str, "message_delta") == 0) {
        cJSON *delta = cJSON_GetObjectItem(root, "delta");
        if (delta) {
            cJSON *fr = cJSON_GetObjectItem(delta, "stop_reason");
            if (cJSON_IsString(fr) && fr->valuestring) {
                AGENTOS_FREE(acc->finish_reason);
                acc->finish_reason = AGENTOS_STRDUP(fr->valuestring);
            }
        }
        cJSON *usage = cJSON_GetObjectItem(root, "usage");
        if (usage) {
            cJSON *otok = cJSON_GetObjectItem(usage, "output_tokens");
            if (cJSON_IsNumber(otok))
                acc->completion_tokens = (uint32_t)otok->valuedouble;
        }
    }

    cJSON_Delete(root);
    return 0;
}

static void ant_process_buffer(ant_sse_ctx_t *s)
{
    if (s->line_len == 0)
        return;

    char *p = s->line_buf;
    char *end = p + s->line_len;

    while (p < end) {
        char *nl = (char *)memchr(p, '\n', (size_t)(end - p));
        if (!nl)
            break;

        size_t llen = (size_t)(nl - p);
        if (llen > 0 && *(nl - 1) == '\r')
            llen--;

        if (llen > 0) {
            if (llen >= 6 && memcmp(p, "event:", 6) == 0) {
                const char *ev = p + 6;
                while (*ev == ' ' || *ev == '\t')
                    ev++;
                size_t elen = llen - (size_t)(ev - p);
                if (elen >= sizeof(s->current_event))
                    elen = sizeof(s->current_event) - 1;
                __builtin_memcpy(s->current_event, ev, elen);
                s->current_event[elen] = '\0';
            } else if (llen >= 5 && memcmp(p, "data:", 5) == 0) {
                const char *ds = p + 5;
                while (*ds == ' ' || *ds == '\t')
                    ds++;
                size_t dlen = llen - (size_t)(ds - p);
                ant_feed_sse_event(s, s->current_event[0] ? s->current_event : NULL, ds, dlen);
            }
        }

        if (*p == '\0' || (nl + 1 < end && *(nl + 1) == '\n')) {
            s->current_event[0] = '\0';
        }

        p = nl + 1;
    }

    if (p < end) {
        size_t rem = (size_t)(end - p);
        __builtin_memmove(s->line_buf, p, rem);
        s->line_len = rem;
    } else {
        s->line_len = 0;
    }
}

static size_t ant_sse_write_cb(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    ant_sse_ctx_t *s = (ant_sse_ctx_t *)userp;
    if (s->cancelled)
        return 0;

    size_t needed = s->line_len + realsize + 1;
    if (needed > s->line_cap) {
        size_t nc = s->line_cap * 2;
        while (nc < needed)
            nc *= 2;
        char *ptr = (char *)AGENTOS_REALLOC(s->line_buf, nc);
        if (!ptr)
            return 0;
        s->line_buf = ptr;
        s->line_cap = nc;
    }

    __builtin_memcpy(s->line_buf + s->line_len, contents, realsize);
    s->line_len += realsize;
    s->line_buf[s->line_len] = '\0';

    ant_process_buffer(s);
    return s->cancelled ? 0 : realsize;
}

static llm_response_t *ant_build_stream_response(ant_stream_acc_t *acc)
{
    llm_response_t *r = (llm_response_t *)AGENTOS_CALLOC(1, sizeof(llm_response_t));
    if (!r) {
        AGENTOS_ERROR_NULL(AGENTOS_ERR_UNKNOWN, "validation failed");
    }

    r->id = acc->resp_id ? acc->resp_id : AGENTOS_STRDUP("");
    acc->resp_id = NULL;
    r->model = acc->resp_model ? acc->resp_model : AGENTOS_STRDUP("unknown");
    acc->resp_model = NULL;
    r->prompt_tokens = acc->prompt_tokens;
    r->completion_tokens = acc->completion_tokens;
    r->total_tokens = r->prompt_tokens + r->completion_tokens;
    r->choices = (llm_message_t *)AGENTOS_CALLOC(1, sizeof(llm_message_t));
    if (r->choices) {
        r->choice_count = 1;
        r->choices[0].role = AGENTOS_STRDUP("assistant");
        r->choices[0].content = acc->acc_content;
        acc->acc_content = NULL;
    } else {
        r->choice_count = 0;
    }
    r->finish_reason = acc->finish_reason ? acc->finish_reason : AGENTOS_STRDUP("end_turn");
    acc->finish_reason = NULL;
    return r;
}

static int anthropic_complete_stream(provider_ctx_t *ctx_ptr, const llm_request_config_t *manager,
                                     llm_stream_callback_t callback, void *user_data,
                                     llm_response_t **out_response)
{
    if (!ctx_ptr || !manager || !callback) {
        SVC_LOG_ERROR("C-L02: ANTHROPIC: STREAM-FAIL — invalid params "
                      "ctx=%p manager=%p callback=%p",
                      (void *)ctx_ptr, (void *)manager, (void *)(uintptr_t)callback);
        return AGENTOS_ERR_INVALID_PARAM;
    }

    anthropic_ctx_t *ctx = (anthropic_ctx_t *)ctx_ptr;
    provider_base_ctx_t *base = &ctx->base;

    const char *model = (manager->model && manager->model[0]) ? manager->model : ANTHROPIC_DEFAULT_MODEL;

    SVC_LOG_INFO("C-L02: ANTHROPIC: STREAM-START model=%s msgs=%zu max_tokens=%d temp=%.2f",
                 model, manager->message_count, manager->max_tokens, manager->temperature);

    llm_request_config_t stream_cfg = *manager;
    stream_cfg.stream = 1;

    char *req_body = anthropic_build_request(&stream_cfg);
    if (!req_body) {
        SVC_LOG_ERROR("C-L02: ANTHROPIC: STREAM-FAIL — request body build failed "
                      "model=%s", model);
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    char url[1024];
    snprintf(url, sizeof(url), "%s/messages", base->api_base);

    struct curl_slist *headers = NULL;
    char auth_header[1024];
    snprintf(auth_header, sizeof(auth_header), "x-api-key: %s",
             base->api_key[0] ? base->api_key : "");
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "anthropic-version: 2023-06-01");
    explicit_bzero(auth_header, sizeof(auth_header));

    ant_stream_acc_t acc;
    __builtin_memset(&acc, 0, sizeof(acc));
    acc.user_cb = callback;
    acc.user_data = user_data;
    acc.acc_cap = 4096;
    acc.acc_content = (char *)AGENTOS_MALLOC(acc.acc_cap);

    ant_sse_ctx_t sse;
    ant_sse_init(&sse, &acc);
    if (!sse.line_buf) {
        SVC_LOG_ERROR("C-L02: ANTHROPIC: STREAM-FAIL — SSE buffer alloc failed (OOM)");
        AGENTOS_FREE(req_body);
        curl_slist_free_all(headers);
        AGENTOS_FREE(acc.acc_content);
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    SVC_LOG_DEBUG("C-L02: ANTHROPIC: STREAM-HTTP-POST url=%s body_len=%zu timeout=%.1fs",
                  url, strlen(req_body), base->timeout_sec);

    CURL *curl = curl_easy_init();
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

        if (sse.line_len > 0)
            ant_process_buffer(&sse);

        if (cres == CURLE_OK)
            ret = AGENTOS_OK;
        else
            SVC_LOG_WARN("C-L02: ANTHROPIC: STREAM — curl error: %s (code=%d)",
                         curl_easy_strerror(cres), (int)cres);
    } else {
        SVC_LOG_ERROR("C-L02: ANTHROPIC: STREAM-FAIL — curl_easy_init() failed");
    }

    ant_sse_destroy(&sse);
    curl_slist_free_all(headers);
    AGENTOS_FREE(req_body);

    if (ret != AGENTOS_OK) {
        SVC_LOG_ERROR("C-L02: ANTHROPIC: STREAM-FAIL — HTTP stream error "
                      "url=%s http_code=%ld ret=%d timeout=%.1fs "
                      "STACK: curl_easy_perform() → anthropic_complete_stream()",
                      url, http_code, ret, base->timeout_sec);
        AGENTOS_FREE(acc.acc_content);
        AGENTOS_FREE(acc.resp_id);
        AGENTOS_FREE(acc.resp_model);
        AGENTOS_FREE(acc.finish_reason);
        return ret;
    }

    llm_response_t *resp = ant_build_stream_response(&acc);
    AGENTOS_FREE(acc.acc_content);
    AGENTOS_FREE(acc.resp_id);
    AGENTOS_FREE(acc.resp_model);
    AGENTOS_FREE(acc.finish_reason);

    if (resp) {
        SVC_LOG_INFO("C-L02: ANTHROPIC: STREAM-OK model=%s tokens=(prompt=%u,completion=%u,total=%u) "
                     "finish_reason=%s acc_len=%zu",
                     resp->model ? resp->model : "unknown",
                     resp->prompt_tokens, resp->completion_tokens, resp->total_tokens,
                     resp->finish_reason ? resp->finish_reason : "none",
                     resp->choices && resp->choices[0].content ? strlen(resp->choices[0].content) : 0);
    } else {
        SVC_LOG_WARN("C-L02: ANTHROPIC: STREAM — null response built");
    }

    if (out_response)
        *out_response = resp;
    else if (resp)
        llm_response_free(resp);

    return AGENTOS_OK;
}

/* ---------- 操作表 ---------- */

const provider_ops_t anthropic_ops = {.init = anthropic_init,
                                      .destroy = anthropic_destroy,
                                      .complete = anthropic_complete,
                                      .complete_stream = anthropic_complete_stream,
                                      .name = "anthropic",
                                      .default_model = ANTHROPIC_DEFAULT_MODEL,
                                      .default_base_url = ANTHROPIC_DEFAULT_BASE};

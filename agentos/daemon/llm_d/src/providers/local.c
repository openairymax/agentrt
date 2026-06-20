#include "memory_compat.h"

#include <cjson/cJSON.h>
#include "error.h"
/**
 * @file local.c
 * @brief 本地模型适配器（兼容 OpenAI 格式）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 改进说明：
 * 1. 使用公共 Provider 基础设施
 * 2. 代码量从 370 行减少到约 140 行
 * 3. 消除了与 openai.c/deepseek.c 的重复代码
 */

#include "daemon_errors.h"
#include "platform.h"
#include "provider.h"
#include "svc_logger.h"

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOCAL_DEFAULT_BASE "http://localhost:8080/v1"
#define LOCAL_DEFAULT_MODEL "gpt-3.5-turbo"
#define LOCAL_DEFAULT_TIMEOUT 60.0

/* ---------- 上下文 ---------- */

typedef struct {
    provider_base_ctx_t base;
} local_ctx_t;

/* ---------- 生命周期 ---------- */

static provider_ctx_t *local_init(const char *name __attribute__((unused)),
                                  const char *api_key __attribute__((unused)), const char *api_base,
                                  const char *organization __attribute__((unused)),
                                  double timeout_sec, int max_retries)
{

    local_ctx_t *ctx = (local_ctx_t *)AGENTOS_CALLOC(1, sizeof(local_ctx_t));
    if (!ctx) {
        SVC_LOG_ERROR("C-L02: LOCAL: INIT-FAIL — OOM allocating ctx (size=%zu)",
                      sizeof(local_ctx_t));
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "null parameter");
        return NULL;
    }

    double timeout = timeout_sec > 0 ? timeout_sec : LOCAL_DEFAULT_TIMEOUT;
    provider_base_init(&ctx->base, NULL, api_base, NULL, timeout, max_retries, LOCAL_DEFAULT_BASE);

    SVC_LOG_INFO("C-L02: LOCAL: INIT api_base=%s model=%s timeout=%.1fs max_retries=%d "
                 "has_api_key=%d (no auth, local endpoint, higher default timeout)",
                 ctx->base.api_base,
                 LOCAL_DEFAULT_MODEL,
                 timeout, max_retries, 0);

    return (provider_ctx_t *)ctx;
}

static void local_destroy(provider_ctx_t *ctx_ptr)
{
    if (ctx_ptr) {
        SVC_LOG_DEBUG("C-L02: LOCAL: DESTROY ctx=%p", (void *)ctx_ptr);
        AGENTOS_FREE(ctx_ptr);
    }
}

/* ---------- 同步完成 ---------- */

static int local_complete(provider_ctx_t *ctx_ptr, const llm_request_config_t *manager,
                          llm_response_t **out_response)
{
    if (!ctx_ptr || !manager || !out_response) {
        SVC_LOG_ERROR("C-L02: LOCAL: COMPLETE-FAIL — invalid params "
                      "ctx=%p manager=%p out=%p",
                      (void *)ctx_ptr, (void *)manager, (void *)out_response);
        return AGENTOS_ERR_INVALID_PARAM;
    }

    local_ctx_t *ctx = (local_ctx_t *)ctx_ptr;
    provider_base_ctx_t *base = &ctx->base;

    const char *model = (manager->model && manager->model[0]) ? manager->model : LOCAL_DEFAULT_MODEL;

    SVC_LOG_INFO("C-L02: LOCAL: COMPLETE-START model=%s msgs=%zu max_tokens=%d temp=%.2f "
                 "stream=%d (no auth, local endpoint)",
                 model, manager->message_count, manager->max_tokens,
                 manager->temperature, manager->stream);

    char *req_body = provider_build_openai_request(manager, LOCAL_DEFAULT_MODEL);
    if (!req_body) {
        SVC_LOG_ERROR("C-L02: LOCAL: COMPLETE-FAIL — request body build failed (OOM) "
                      "model=%s", model);
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    size_t req_body_len = strlen(req_body);

    char url[1024];
    snprintf(url, sizeof(url), "%s/chat/completions", base->api_base);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    provider_http_resp_t *http_resp = NULL;
    long http_code = 0;

    SVC_LOG_DEBUG("C-L02: LOCAL: HTTP-POST url=%s body_len=%zu timeout=%.1fs retries=%d "
                  "(no auth, local endpoint)",
                  url, req_body_len, base->timeout_sec, base->max_retries);

    int ret = provider_http_post(url, headers, req_body, base->timeout_sec, base->max_retries,
                                 &http_resp, &http_code);

    curl_slist_free_all(headers);
    AGENTOS_FREE(req_body);

    if (ret != AGENTOS_OK) {
        SVC_LOG_ERROR("C-L02: LOCAL: COMPLETE-FAIL — HTTP request failed "
                      "url=%s http_code=%ld ret=%d timeout=%.1fs "
                      "STACK: provider_http_post() → local_complete()",
                      url, http_code, ret, base->timeout_sec);
        return ret;
    }

    if (http_code != 200) {
        size_t resp_body_len = http_resp ? strlen(http_resp->data) : 0;
        SVC_LOG_ERROR("C-L02: LOCAL: COMPLETE-FAIL — HTTP error "
                      "url=%s http_code=%ld resp_body_len=%zu "
                      "DIAGNOSIS: %s "
                      "STACK: provider_http_post() → local_complete()",
                      url, http_code, resp_body_len,
                      (http_code == 401) ? "invalid API key (but local should not use auth)" :
                      (http_code == 429) ? "rate limited" :
                      (http_code == 500) ? "local server internal error" :
                      (http_code == 503) ? "local service unavailable — check if model server is running" :
                      "check local endpoint URL and model server status");
        provider_http_resp_free(http_resp);
        return AGENTOS_ERR_IO;
    }

    size_t resp_body_len = http_resp ? strlen(http_resp->data) : 0;
    SVC_LOG_DEBUG("C-L02: LOCAL: HTTP-RESPONSE http_code=%ld resp_body_len=%zu",
                  http_code, resp_body_len);

    ret = provider_parse_openai_response(http_resp->data, out_response);
    if (ret != AGENTOS_OK) {
        SVC_LOG_ERROR("C-L02: LOCAL: COMPLETE-FAIL — response parse failed "
                      "ret=%d resp_body_len=%zu "
                      "STACK: provider_parse_openai_response() → local_complete()",
                      ret, resp_body_len);
    } else if (*out_response) {
        SVC_LOG_INFO("C-L02: LOCAL: COMPLETE-OK model=%s tokens=(prompt=%u,completion=%u,total=%u) "
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

/* ---------- 流式完成（SSE, OpenAI兼容格式） ---------- */

typedef struct {
    llm_stream_callback_t user_cb;
    void *user_data;
    char *acc_content;
    size_t acc_cap;
    size_t acc_len;
    char *resp_id;
    char *resp_model;
    uint64_t resp_created;
    char *finish_reason;
} loc_stream_acc_t;

static int loc_stream_on_chunk(const char *json_line, void *userdata)
{
    loc_stream_acc_t *acc = (loc_stream_acc_t *)userdata;

    cJSON *root = cJSON_Parse(json_line);
    if (!root)
        return 0;

    if (!acc->resp_id) {
        cJSON *id = cJSON_GetObjectItem(root, "id");
        if (cJSON_IsString(id) && id->valuestring)
            acc->resp_id = AGENTOS_STRDUP(id->valuestring);
    }

    if (!acc->resp_model) {
        cJSON *model = cJSON_GetObjectItem(root, "model");
        if (cJSON_IsString(model) && model->valuestring)
            acc->resp_model = AGENTOS_STRDUP(model->valuestring);
    }

    cJSON *created = cJSON_GetObjectItem(root, "created");
    if (cJSON_IsNumber(created) && acc->resp_created == 0)
        acc->resp_created = (uint64_t)created->valuedouble;

    cJSON *choices = cJSON_GetObjectItem(root, "choices");
    if (cJSON_IsArray(choices) && cJSON_GetArraySize(choices) > 0) {
        cJSON *choice = cJSON_GetArrayItem(choices, 0);
        cJSON *delta = cJSON_GetObjectItem(choice, "delta");
        if (delta) {
            cJSON *content = cJSON_GetObjectItem(delta, "content");
            if (cJSON_IsString(content) && content->valuestring) {
                const char *text = content->valuestring;
                size_t tlen = strlen(text);

                if (acc->user_cb)
                    acc->user_cb(text, acc->user_data);

                if (tlen > 0) {
                    size_t needed = acc->acc_len + tlen + 1;
                    if (needed > acc->acc_cap) {
                        size_t new_cap = acc->acc_cap * 2;
                        while (new_cap < needed)
                            new_cap *= 2;
                        char *ptr = (char *)AGENTOS_REALLOC(acc->acc_content, new_cap);
                        if (ptr) {
                            acc->acc_content = ptr;
                            acc->acc_cap = new_cap;
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

        cJSON *fr = cJSON_GetObjectItem(choice, "finish_reason");
        if (cJSON_IsString(fr) && fr->valuestring && strcmp(fr->valuestring, "null") != 0) {
            AGENTOS_FREE(acc->finish_reason);
            acc->finish_reason = AGENTOS_STRDUP(fr->valuestring);
        }
    }

    cJSON_Delete(root);
    return 0;
}

static llm_response_t *loc_build_stream_response(loc_stream_acc_t *acc)
{
    llm_response_t *resp = (llm_response_t *)AGENTOS_CALLOC(1, sizeof(llm_response_t));
    if (!resp) {
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_UNKNOWN, "validation failed");

        return NULL;
    }

    resp->id = acc->resp_id ? acc->resp_id : AGENTOS_STRDUP("");
    acc->resp_id = NULL;
    resp->model = acc->resp_model ? acc->resp_model : AGENTOS_STRDUP("unknown");
    acc->resp_model = NULL;
    resp->created = acc->resp_created;
    resp->choices = (llm_message_t *)AGENTOS_CALLOC(1, sizeof(llm_message_t));
    if (resp->choices) {
        resp->choice_count = 1;
        resp->choices[0].role = AGENTOS_STRDUP("assistant");
        resp->choices[0].content = acc->acc_content;
        acc->acc_content = NULL;
    } else {
        resp->choice_count = 0;
    }
    resp->finish_reason = acc->finish_reason ? acc->finish_reason : AGENTOS_STRDUP("stop");
    acc->finish_reason = NULL;
    return resp;
}

static int local_complete_stream(provider_ctx_t *ctx_ptr, const llm_request_config_t *manager,
                                 llm_stream_callback_t callback, void *user_data,
                                 llm_response_t **out_response)
{
    if (!ctx_ptr || !manager || !callback) {
        SVC_LOG_ERROR("C-L02: LOCAL: STREAM-FAIL — invalid params "
                      "ctx=%p manager=%p callback=%p",
                      (void *)ctx_ptr, (void *)manager, (void *)callback);
        return AGENTOS_ERR_INVALID_PARAM;
    }

    local_ctx_t *ctx = (local_ctx_t *)ctx_ptr;
    provider_base_ctx_t *base = &ctx->base;

    const char *model = (manager->model && manager->model[0]) ? manager->model : LOCAL_DEFAULT_MODEL;

    SVC_LOG_INFO("C-L02: LOCAL: STREAM-START model=%s msgs=%zu max_tokens=%d temp=%.2f "
                 "(no auth, local endpoint)",
                 model, manager->message_count, manager->max_tokens, manager->temperature);

    llm_request_config_t stream_cfg = *manager;
    stream_cfg.stream = 1;

    char *req_body = provider_build_openai_request(&stream_cfg, LOCAL_DEFAULT_MODEL);
    if (!req_body) {
        SVC_LOG_ERROR("C-L02: LOCAL: STREAM-FAIL — request body build failed (OOM) "
                      "model=%s", model);
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    char url[1024];
    snprintf(url, sizeof(url), "%s/chat/completions", base->api_base);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    loc_stream_acc_t acc;
    __builtin_memset(&acc, 0, sizeof(acc));
    acc.user_cb = callback;
    acc.user_data = user_data;
    acc.acc_cap = 4096;
    acc.acc_content = (char *)AGENTOS_MALLOC(acc.acc_cap);

    SVC_LOG_DEBUG("C-L02: LOCAL: STREAM-HTTP-POST url=%s body_len=%zu timeout=%.1fs "
                  "(no auth, local endpoint)",
                  url, strlen(req_body), base->timeout_sec);

    long http_code = 0;
    int ret = provider_http_post_stream(url, headers, req_body, base->timeout_sec,
                                        loc_stream_on_chunk, &acc, &http_code);

    curl_slist_free_all(headers);
    AGENTOS_FREE(req_body);

    if (ret != AGENTOS_OK) {
        SVC_LOG_ERROR("C-L02: LOCAL: STREAM-FAIL — HTTP stream error "
                      "url=%s http_code=%ld ret=%d timeout=%.1fs "
                      "STACK: provider_http_post_stream() → local_complete_stream()",
                      url, http_code, ret, base->timeout_sec);
        AGENTOS_FREE(acc.acc_content);
        AGENTOS_FREE(acc.resp_id);
        AGENTOS_FREE(acc.resp_model);
        AGENTOS_FREE(acc.finish_reason);
        return ret;
    }

    llm_response_t *resp = loc_build_stream_response(&acc);
    AGENTOS_FREE(acc.acc_content);
    AGENTOS_FREE(acc.resp_id);
    AGENTOS_FREE(acc.resp_model);
    AGENTOS_FREE(acc.finish_reason);

    if (resp) {
        SVC_LOG_INFO("C-L02: LOCAL: STREAM-OK model=%s tokens=(prompt=%u,completion=%u,total=%u) "
                     "finish_reason=%s acc_len=%zu",
                     resp->model ? resp->model : "unknown",
                     resp->prompt_tokens, resp->completion_tokens, resp->total_tokens,
                     resp->finish_reason ? resp->finish_reason : "none",
                     resp->choices && resp->choices[0].content ? strlen(resp->choices[0].content) : 0);
    } else {
        SVC_LOG_WARN("C-L02: LOCAL: STREAM — null response built");
    }

    if (out_response)
        *out_response = resp;
    else if (resp)
        llm_response_free(resp);

    return AGENTOS_OK;
}

/* ---------- 操作表 ---------- */

const provider_ops_t local_ops = {.init = local_init,
                                  .destroy = local_destroy,
                                  .complete = local_complete,
                                  .complete_stream = local_complete_stream,
                                  .name = "local",
                                  .default_model = LOCAL_DEFAULT_MODEL,
                                  .default_base_url = LOCAL_DEFAULT_BASE};

/**
 * @file google.c
 * @brief Google Gemini 适配器实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * Gemini API 特点：
 * 1. 认证使用 x-goog-api-key 头（非 Bearer token）
 * 2. 请求格式：contents 数组 + systemInstruction
 * 3. 响应格式：candidates[].content.parts[].text
 * 4. SSE 流式：使用自定义 SSE 解析器处理 Gemini 格式
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

#define GOOGLE_DEFAULT_BASE "https://generativelanguage.googleapis.com/v1beta"
#define GOOGLE_DEFAULT_MODEL "gemini-2.0-flash"

typedef struct {
    provider_base_ctx_t base;
} google_ctx_t;

static provider_ctx_t* google_init(const char* name,
                                   const char* api_key,
                                   const char* api_base,
                                   const char* organization,
                                   double timeout_sec,
                                   int max_retries) {
    (void)name;
    (void)organization;

    google_ctx_t* ctx = (google_ctx_t*)calloc(1, sizeof(google_ctx_t));
    if (!ctx) {
        return NULL;
    }

    provider_base_init(&ctx->base, api_key, api_base, organization,
                      timeout_sec, max_retries, GOOGLE_DEFAULT_BASE);

    return (provider_ctx_t*)ctx;
}

static void google_destroy(provider_ctx_t* ctx_ptr) {
    if (ctx_ptr) {
        free(ctx_ptr);
    }
}

static char* google_build_request(const llm_request_config_t* manager) {
    if (!manager) return NULL;

    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;

    cJSON* contents = cJSON_CreateArray();
    char* system_instruction_text = NULL;

    for (size_t i = 0; i < manager->message_count; ++i) {
        const char* role = manager->messages[i].role ? manager->messages[i].role : "user";
        const char* content = manager->messages[i].content ? manager->messages[i].content : "";

        if (strcmp(role, "system") == 0) {
            system_instruction_text = strdup(content);
            continue;
        }

        cJSON* entry = cJSON_CreateObject();
        const char* gemini_role = (strcmp(role, "assistant") == 0) ? "model" : "user";
        cJSON_AddStringToObject(entry, "role", gemini_role);

        cJSON* parts = cJSON_CreateArray();
        cJSON* part = cJSON_CreateObject();
        cJSON_AddStringToObject(part, "text", content);
        cJSON_AddItemToArray(parts, part);
        cJSON_AddItemToObject(entry, "parts", parts);
        cJSON_AddItemToArray(contents, entry);
    }

    cJSON_AddItemToObject(root, "contents", contents);

    if (system_instruction_text) {
        cJSON* si = cJSON_CreateObject();
        cJSON* si_parts = cJSON_CreateArray();
        cJSON* si_part = cJSON_CreateObject();
        cJSON_AddStringToObject(si_part, "text", system_instruction_text);
        cJSON_AddItemToArray(si_parts, si_part);
        cJSON_AddItemToObject(si, "parts", si_parts);
        cJSON_AddItemToObject(root, "systemInstruction", si);
        free(system_instruction_text);
    }

    cJSON* gen_config = cJSON_CreateObject();
    if (manager->temperature > 0) {
        cJSON_AddNumberToObject(gen_config, "temperature", manager->temperature);
    }
    if (manager->max_tokens > 0) {
        cJSON_AddNumberToObject(gen_config, "maxOutputTokens", manager->max_tokens);
    }
    cJSON_AddItemToObject(root, "generationConfig", gen_config);

    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

static int google_parse_response(const char* body, llm_response_t** out) {
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

    cJSON* candidates = cJSON_GetObjectItem(root, "candidates");
    if (cJSON_IsArray(candidates) && cJSON_GetArraySize(candidates) > 0) {
        cJSON* first = cJSON_GetArrayItem(candidates, 0);
        cJSON* content_obj = cJSON_GetObjectItem(first, "content");
        if (content_obj) {
            cJSON* parts = cJSON_GetObjectItem(content_obj, "parts");
            if (cJSON_IsArray(parts) && cJSON_GetArraySize(parts) > 0) {
                cJSON* part0 = cJSON_GetArrayItem(parts, 0);
                cJSON* text = cJSON_GetObjectItem(part0, "text");
                if (cJSON_IsString(text) && text->valuestring) {
                    resp->choice_count = 1;
                    resp->choices = (llm_message_t*)calloc(1, sizeof(llm_message_t));
                    if (resp->choices) {
                        resp->choices[0].role = strdup("assistant");
                        resp->choices[0].content = strdup(text->valuestring);
                    }
                }
            }
        }

        cJSON* finish = cJSON_GetObjectItem(first, "finishReason");
        if (cJSON_IsString(finish) && finish->valuestring) {
            const char* fr = finish->valuestring;
            if (strcmp(fr, "STOP") == 0) {
                resp->finish_reason = strdup("stop");
            } else if (strcmp(fr, "MAX_TOKENS") == 0) {
                resp->finish_reason = strdup("length");
            } else if (strcmp(fr, "SAFETY") == 0) {
                resp->finish_reason = strdup("content_filter");
            } else {
                resp->finish_reason = strdup(fr);
            }
        }
    }

    cJSON* model = cJSON_GetObjectItem(root, "modelVersion");
    if (cJSON_IsString(model) && model->valuestring) {
        resp->model = strdup(model->valuestring);
    }

    cJSON* usage = cJSON_GetObjectItem(root, "usageMetadata");
    if (usage) {
        cJSON* pt = cJSON_GetObjectItem(usage, "promptTokenCount");
        cJSON* ct = cJSON_GetObjectItem(usage, "candidatesTokenCount");
        if (cJSON_IsNumber(pt)) resp->prompt_tokens = (uint32_t)pt->valuedouble;
        if (cJSON_IsNumber(ct)) resp->completion_tokens = (uint32_t)ct->valuedouble;
        resp->total_tokens = resp->prompt_tokens + resp->completion_tokens;
    }

    cJSON_Delete(root);
    *out = resp;
    return AGENTOS_OK;
}

static int google_complete(provider_ctx_t* ctx_ptr,
                           const llm_request_config_t* manager,
                           llm_response_t** out_response) {
    if (!ctx_ptr || !manager || !out_response) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    google_ctx_t* ctx = (google_ctx_t*)ctx_ptr;
    provider_base_ctx_t* base = &ctx->base;

    const char* model = (manager->model && manager->model[0]) ?
                        manager->model : GOOGLE_DEFAULT_MODEL;

    char* req_body = google_build_request(manager);
    if (!req_body) {
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    char url[1024];
    snprintf(url, sizeof(url), "%s/models/%s:generateContent",
             base->api_base, model);

    struct curl_slist* headers = NULL;
    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "x-goog-api-key: %s",
             base->api_key[0] ? base->api_key : "");
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: application/json");
    explicit_bzero(auth_header, sizeof(auth_header));

    provider_http_resp_t* http_resp = NULL;
    long http_code = 0;

    int ret = provider_http_post(url, headers, req_body,
                                base->timeout_sec, base->max_retries,
                                &http_resp, &http_code);

    curl_slist_free_all(headers);
    free(req_body);

    if (ret != AGENTOS_OK) {
        SVC_LOG_ERROR("google: HTTP request failed, status=%ld", http_code);
        return ret;
    }

    if (http_code != 200) {
        SVC_LOG_ERROR("google: HTTP error, status=%ld", http_code);
        provider_http_resp_free(http_resp);
        return AGENTOS_ERR_IO;
    }

    ret = google_parse_response(http_resp->data, out_response);
    provider_http_resp_free(http_resp);

    return ret;
}

typedef struct {
    llm_stream_callback_t user_cb;
    void*                 user_data;
    char*                 acc_content;
    size_t                acc_cap;
    size_t                acc_len;
    char*                 resp_model;
    uint32_t              prompt_tokens;
    uint32_t              completion_tokens;
    char*                 finish_reason;
} gg_stream_acc_t;

typedef struct {
    char*   line_buf;
    size_t  line_cap;
    size_t  line_len;
    gg_stream_acc_t* acc;
    int     cancelled;
} gg_sse_ctx_t;

static void gg_sse_init(gg_sse_ctx_t* s, gg_stream_acc_t* a) {
    memset(s, 0, sizeof(*s));
    s->line_cap = 4096;
    s->line_buf = (char*)malloc(s->line_cap);
    s->acc = a;
}

static void gg_sse_destroy(gg_sse_ctx_t* s) {
    if (s) { free(s->line_buf); s->line_buf = NULL; }
}

static int gg_feed_sse_data(gg_sse_ctx_t* s, const char* data, size_t data_len) {
    gg_stream_acc_t* acc = s->acc;

    if (!data || data_len == 0) return 0;

    cJSON* root = cJSON_Parse(data);
    if (!root) return 0;

    cJSON* candidates = cJSON_GetObjectItem(root, "candidates");
    if (cJSON_IsArray(candidates) && cJSON_GetArraySize(candidates) > 0) {
        cJSON* first = cJSON_GetArrayItem(candidates, 0);
        cJSON* content_obj = cJSON_GetObjectItem(first, "content");
        if (content_obj) {
            cJSON* parts = cJSON_GetObjectItem(content_obj, "parts");
            if (cJSON_IsArray(parts)) {
                for (int i = 0; i < cJSON_GetArraySize(parts); i++) {
                    cJSON* part = cJSON_GetArrayItem(parts, i);
                    cJSON* text = cJSON_GetObjectItem(part, "text");
                    if (cJSON_IsString(text) && text->valuestring) {
                        const char* chunk = text->valuestring;
                        size_t clen = strlen(chunk);

                        if (acc->user_cb)
                            acc->user_cb(chunk, acc->user_data);

                        if (clen > 0) {
                            size_t needed = acc->acc_len + clen + 1;
                            if (needed > acc->acc_cap) {
                                size_t nc = acc->acc_cap * 2;
                                while (nc < needed) nc *= 2;
                                char* p = (char*)realloc(acc->acc_content, nc);
                                if (p) { acc->acc_content = p; acc->acc_cap = nc; }
                            }
                            if (acc->acc_content && acc->acc_len + clen < acc->acc_cap) {
                                memcpy(acc->acc_content + acc->acc_len, chunk, clen);
                                acc->acc_len += clen;
                                acc->acc_content[acc->acc_len] = '\0';
                            }
                        }
                    }
                }
            }
        }

        cJSON* finish = cJSON_GetObjectItem(first, "finishReason");
        if (cJSON_IsString(finish) && finish->valuestring) {
            free(acc->finish_reason);
            const char* fr = finish->valuestring;
            if (strcmp(fr, "STOP") == 0) {
                acc->finish_reason = strdup("stop");
            } else if (strcmp(fr, "MAX_TOKENS") == 0) {
                acc->finish_reason = strdup("length");
            } else {
                acc->finish_reason = strdup(fr);
            }
        }
    }

    cJSON* usage = cJSON_GetObjectItem(root, "usageMetadata");
    if (usage) {
        cJSON* pt = cJSON_GetObjectItem(usage, "promptTokenCount");
        cJSON* ct = cJSON_GetObjectItem(usage, "candidatesTokenCount");
        if (cJSON_IsNumber(pt)) acc->prompt_tokens = (uint32_t)pt->valuedouble;
        if (cJSON_IsNumber(ct)) acc->completion_tokens = (uint32_t)ct->valuedouble;
    }

    cJSON* mv = cJSON_GetObjectItem(root, "modelVersion");
    if (cJSON_IsString(mv) && mv->valuestring && !acc->resp_model) {
        acc->resp_model = strdup(mv->valuestring);
    }

    cJSON_Delete(root);
    return 0;
}

static void gg_process_buffer(gg_sse_ctx_t* s) {
    if (s->line_len == 0) return;

    char* p = s->line_buf;
    char* end = p + s->line_len;

    while (p < end) {
        char* nl = (char*)memchr(p, '\n', (size_t)(end - p));
        if (!nl) break;

        size_t llen = (size_t)(nl - p);
        if (llen > 0 && *(nl - 1) == '\r') llen--;

        if (llen >= 5 && memcmp(p, "data:", 5) == 0) {
            const char* ds = p + 5;
            while (*ds == ' ' || *ds == '\t') ds++;
            size_t dlen = llen - (size_t)(ds - p);
            char* data_copy = (char*)malloc(dlen + 1);
            if (data_copy) {
                memcpy(data_copy, ds, dlen);
                data_copy[dlen] = '\0';
                gg_feed_sse_data(s, data_copy, dlen);
                free(data_copy);
            }
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

static size_t gg_sse_write_cb(void* contents, size_t size, size_t nmemb,
                              void* userp) {
    size_t realsize = size * nmemb;
    gg_sse_ctx_t* s = (gg_sse_ctx_t*)userp;
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

    gg_process_buffer(s);
    return s->cancelled ? 0 : realsize;
}

static llm_response_t* gg_build_stream_response(gg_stream_acc_t* acc) {
    llm_response_t* r = (llm_response_t*)calloc(1, sizeof(llm_response_t));
    if (!r) return NULL;

    r->id = strdup("");
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
    r->finish_reason = acc->finish_reason ? acc->finish_reason : strdup("stop");
    acc->finish_reason = NULL;
    return r;
}

static int google_complete_stream(provider_ctx_t* ctx_ptr,
                                  const llm_request_config_t* manager,
                                  llm_stream_callback_t callback,
                                  void* user_data,
                                  llm_response_t** out_response) {
    if (!ctx_ptr || !manager || !callback)
        return AGENTOS_ERR_INVALID_PARAM;

    google_ctx_t* ctx = (google_ctx_t*)ctx_ptr;
    provider_base_ctx_t* base = &ctx->base;

    const char* model = (manager->model && manager->model[0]) ?
                        manager->model : GOOGLE_DEFAULT_MODEL;

    llm_request_config_t stream_cfg = *manager;
    stream_cfg.stream = 1;

    char* req_body = google_build_request(&stream_cfg);
    if (!req_body) return AGENTOS_ERR_OUT_OF_MEMORY;

    char url[1024];
    snprintf(url, sizeof(url), "%s/models/%s:streamGenerateContent?alt=sse",
             base->api_base, model);

    struct curl_slist* headers = NULL;
    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "x-goog-api-key: %s",
             base->api_key[0] ? base->api_key : "");
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: application/json");
    explicit_bzero(auth_header, sizeof(auth_header));

    gg_stream_acc_t acc;
    memset(&acc, 0, sizeof(acc));
    acc.user_cb = callback;
    acc.user_data = user_data;
    acc.acc_cap = 4096;
    acc.acc_content = (char*)malloc(acc.acc_cap);

    gg_sse_ctx_t sse;
    gg_sse_init(&sse, &acc);
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
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, gg_sse_write_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sse);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)base->timeout_sec);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

        CURLcode cres = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        curl_easy_cleanup(curl);

        if (sse.line_len > 0) gg_process_buffer(&sse);

        if (cres == CURLE_OK) ret = AGENTOS_OK;
        else SVC_LOG_WARN("google: Stream curl error: %s", curl_easy_strerror(cres));
    }

    gg_sse_destroy(&sse);
    curl_slist_free_all(headers);
    free(req_body);

    if (ret != AGENTOS_OK) {
        SVC_LOG_ERROR("google: Stream HTTP error, status=%ld", http_code);
        free(acc.acc_content); free(acc.resp_model);
        free(acc.finish_reason);
        return ret;
    }

    llm_response_t* resp = gg_build_stream_response(&acc);
    free(acc.acc_content); free(acc.resp_model);
    free(acc.finish_reason);

    if (out_response) *out_response = resp;
    else if (resp) llm_response_free(resp);

    return AGENTOS_OK;
}

const provider_ops_t google_ops = {
    .init = google_init,
    .destroy = google_destroy,
    .complete = google_complete,
    .complete_stream = google_complete_stream,
    .name = "google",
    .default_model = GOOGLE_DEFAULT_MODEL,
    .default_base_url = GOOGLE_DEFAULT_BASE
};

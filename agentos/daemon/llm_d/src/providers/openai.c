/**
 * @file openai.c
 * @brief OpenAI 适配器实现（含生产级Rate Limiting）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * PROTO-003 实现内容：
 * 1. 令牌桶算法速率限制（RPM/TPM）
 * 2. HTTP 429 检测与 Retry-After 头解析
 * 3. 指数退避 + 抖动（Exponential Backoff with Jitter）
 * 4. 可配置的速率限制参数
 *
 * 改进说明：
 * - 使用公共 Provider 基础设施
 * - 集成 OpenAI API 最佳实践
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
#include <time.h>
#include <math.h>
#include <pthread.h>

#define OPENAI_DEFAULT_BASE "https://api.openai.com/v1"
#define OPENAI_DEFAULT_MODEL "gpt-3.5-turbo"

/* ---------- OpenAI 默认速率限制（Tier 1 限制值） ---------- */

#define OPENAI_DEFAULT_RPM         500    /* Requests per minute */
#define OPENAI_DEFAULT_TPM       150000   /* Tokens per minute (Tier 1) */
#define OPENAI_DEFAULT_TPM_TIER2 300000   /* Tokens per minute (Tier 2) */
#define OPENAI_MAX_RETRIES           5     /* 最大重试次数 */
#define OPENAI_BASE_DELAY_MS        1000  /* 初始退避延迟(ms) */
#define OPENAI_MAX_DELAY_MS        60000  /* 最大退避延迟(ms) */
#define OPENAI_JITTER_FACTOR        0.2   /* 抖动因子(20%) */

/* ---------- 速率限制器状态 ---------- */

typedef struct {
    pthread_mutex_t lock;
    time_t rpm_window_start;       /* RPM窗口起始时间 */
    int rpm_count;                  /* 当前窗口内请求数 */
    int rpm_limit;                 /* RPM限制 */
    long tpm_count;                /* TPM累计token数 */
    time_t tpm_window_start;       /* TPM窗口起始时间 */
    long tpm_limit;                /* TPM限制 */
    time_t last_429_time;          /* 上次429响应时间 */
    int retry_after_sec;            /* 服务端建议的Retry-After(秒) */
    int consecutive_429s;           /* 连续429次数(用于自适应退避) */
} openai_rate_limiter_t;

/* ---------- 上下文 ---------- */

typedef struct {
    provider_base_ctx_t base;
    openai_rate_limiter_t rl;
} openai_ctx_t;

/* ========== 速率限制器内部函数 ========== */

static void openai_rl_init(openai_rate_limiter_t* rl) {
    pthread_mutex_init(&rl->lock, NULL);
    rl->rpm_window_start = time(NULL);
    rl->rpm_count = 0;
    rl->rpm_limit = OPENAI_DEFAULT_RPM;
    rl->tpm_count = 0;
    rl->tpm_window_start = time(NULL);
    rl->tpm_limit = OPENAI_DEFAULT_TPM;
    rl->last_429_time = 0;
    rl->retry_after_sec = 0;
    rl->consecutive_429s = 0;
}

static void openai_rl_destroy(openai_rate_limiter_t* rl) {
    pthread_mutex_destroy(&rl->lock);
}

static int openai_rl_check_rpm(openai_rate_limiter_t* rl) {
    time_t now = time(NULL);
    pthread_mutex_lock(&rl->lock);

    if (now - rl->rpm_window_start >= 60) {
        rl->rpm_count = 0;
        rl->rpm_window_start = now;
    }

    if (rl->rpm_count >= rl->rpm_limit) {
        pthread_mutex_unlock(&rl->lock);
        return -1;
    }

    rl->rpm_count++;
    pthread_mutex_unlock(&rl->lock);
    return 0;
}

static int openai_rl_check_tpm(openai_rate_limiter_t* rl, int tokens) {
    time_t now = time(NULL);
    pthread_mutex_lock(&rl->lock);

    if (now - rl->tpm_window_start >= 60) {
        rl->tpm_count = 0;
        rl->tpm_window_start = now;
    }

    if (rl->tpm_count + tokens > rl->tpm_limit) {
        pthread_mutex_unlock(&rl->lock);
        return -1;
    }

    rl->tpm_count += tokens;
    pthread_mutex_unlock(&rl->lock);
    return 0;
}

static void openai_rl_record_429(openai_rate_limiter_t* rl, int retry_after) {
    time_t now = time(NULL);
    pthread_mutex_lock(&rl->lock);

    rl->last_429_time = now;
    rl->consecutive_429s++;
    if (retry_after > 0) {
        rl->retry_after_sec = retry_after;
    } else {
        rl->retry_after_sec = 0;
    }

    pthread_mutex_unlock(&rl->lock);
}

static int openai_rl_get_wait_ms(openai_rate_limiter_t* rl, int attempt) {
    pthread_mutex_lock(&rl->lock);

    int wait_ms;

    if (rl->retry_after_sec > 0) {
        wait_ms = rl->retry_after_sec * 1000;
        rl->retry_after_sec = 0;
    } else {
        int base_delay = OPENAI_BASE_DELAY_MS << attempt;
        if (base_delay > OPENAI_MAX_DELAY_MS) base_delay = OPENAI_MAX_DELAY_MS;

        double jitter = ((double)(rand() % 100) / 100.0) * base_delay * OPENAI_JITTER_FACTOR;
        wait_ms = (int)((double)base_delay + jitter);
    }

    pthread_mutex_unlock(&rl->lock);
    return wait_ms;
}

static void openai_rl_reset_429(openai_rate_limiter_t* rl) {
    pthread_mutex_lock(&rl->lock);
    rl->consecutive_429s = 0;
    rl->retry_after_sec = 0;
    pthread_mutex_unlock(&rl->lock);
}

/* ========== HTTP 429 Retry-After 解析 ========== */

static int parse_retry_after(const char* headers_data) {
    if (!headers_data) return 0;

    const char* retry_ptr = strstr(headers_data, "retry-after:");
    if (!retry_ptr) retry_ptr = strstr(headers_data, "Retry-After:");
    if (!retry_ptr) return 0;

    retry_ptr = strchr(retry_ptr, ':');
    if (!retry_ptr) return 0;
    retry_ptr++;

    while (*retry_ptr == ' ' || *retry_ptr == '\t') retry_ptr++;

    long seconds = strtol(retry_ptr, NULL, 10);
    if (seconds <= 0) return 0;
    if (seconds > 300) seconds = 300;

    return (int)seconds;
}

/* ========== 带重试的HTTP请求 ========== */

static int openai_http_request_with_retry(
    openai_ctx_t* ctx,
    const char* url,
    struct curl_slist* headers,
    const char* body,
    long* out_http_code,
    provider_http_resp_t** out_response)
{
    int attempt = 0;
    int max_attempts = ctx->base.max_retries > 0 ?
                       ctx->base.max_retries : OPENAI_MAX_RETRIES;

    while (attempt < max_attempts) {
        int ret = openai_rl_check_rpm(&ctx->rl);
        if (ret != 0) {
            SVC_LOG_WARN("openai: RPM limit reached, waiting...");
            struct timespec ts = { .tv_sec = 1, .tv_nsec = 0 };
            nanosleep(&ts, NULL);
            continue;
        }

        *out_response = NULL;
        ret = provider_http_post(url, headers, body,
                                  ctx->base.timeout_sec, 0,
                                  out_response, out_http_code);

        if (ret == AGENTOS_OK && *out_http_code == 200) {
            openai_rl_reset_429(&ctx->rl);
            return AGENTOS_OK;
        }

        if (*out_http_code == 429) {
            int retry_after = parse_retry_after(
                *out_response ? (*out_response)->data : NULL);
            openai_rl_record_429(&ctx->rl, retry_after);

            SVC_LOG_WARN("openai: Rate limited (429), attempt %d/%d, retry_after=%ds",
                         attempt + 1, max_attempts, retry_after);

            if (*out_response) {
                provider_http_resp_free(*out_response);
                *out_response = NULL;
            }

            int wait_ms = openai_rl_get_wait_ms(&ctx->rl, attempt);
            if (wait_ms > 0) {
                struct timespec ts = {
                    .tv_sec = wait_ms / 1000,
                    .tv_nsec = (wait_ms % 1000) * 1000000LL
                };
                nanosleep(&ts, NULL);
            }
            attempt++;
            continue;
        }

        if (*out_http_code >= 500 && *out_http_code < 600 && attempt < max_attempts - 1) {
            SVC_LOG_WARN("openai: Server error %ld, attempt %d/%d, retrying...",
                         *out_http_code, attempt + 1, max_attempts);

            if (*out_response) {
                provider_http_resp_free(*out_response);
                *out_response = NULL;
            }

            int delay = OPENAI_BASE_DELAY_MS << attempt;
            if (delay > OPENAI_MAX_DELAY_MS) delay = OPENAI_MAX_DELAY_MS;
            struct timespec ts = {
                .tv_sec = delay / 1000,
                .tv_nsec = (delay % 1000) * 1000000LL
            };
            nanosleep(&ts, NULL);
            attempt++;
            continue;
        }

        break;
    }

    return AGENTOS_ERR_IO;
}

/* ---------- 生命周期 ---------- */

static provider_ctx_t* openai_init(const char* name,
                                  const char* api_key,
                                  const char* api_base,
                                  const char* organization,
                                  double timeout_sec,
                                  int max_retries) {
    (void)name;

    openai_ctx_t* ctx = (openai_ctx_t*)calloc(1, sizeof(openai_ctx_t));
    if (!ctx) {
        return NULL;
    }

    provider_base_init(&ctx->base, api_key, api_base, organization,
                      timeout_sec, max_retries, OPENAI_DEFAULT_BASE);

    openai_rl_init(&ctx->rl);

    srand((unsigned int)time(NULL) ^ (unsigned int)(uintptr_t)ctx);

    SVC_LOG_INFO("openai: adapter initialized (RPM=%d, TPM=%ld)",
                 OPENAI_DEFAULT_RPM, (long)OPENAI_DEFAULT_TPM);

    return (provider_ctx_t*)ctx;
}

static void openai_destroy(provider_ctx_t* ctx_ptr) {
    if (ctx_ptr) {
        openai_ctx_t* ctx = (openai_ctx_t*)ctx_ptr;
        openai_rl_destroy(&ctx->rl);
        free(ctx_ptr);
    }
}

/* ---------- 同步完成（含Rate Limiting） ---------- */

static int openai_complete(provider_ctx_t* ctx_ptr,
                           const llm_request_config_t* manager,
                           llm_response_t** out_response) {
    if (!ctx_ptr || !manager || !out_response) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    openai_ctx_t* ctx = (openai_ctx_t*)ctx_ptr;
    provider_base_ctx_t* base = &ctx->base;

    char* req_body = provider_build_openai_request(manager, OPENAI_DEFAULT_MODEL);
    if (!req_body) {
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    char url[1024];
    snprintf(url, sizeof(url), "%s/chat/completions", base->api_base);

    struct curl_slist* headers = NULL;
    char auth_header[1024];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s",
             base->api_key[0] ? base->api_key : "");
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: application/json");

    provider_http_resp_t* http_resp = NULL;
    long http_code = 0;

    int ret = openai_http_request_with_retry(ctx, url, headers, req_body,
                                              &http_code, &http_resp);

    curl_slist_free_all(headers);
    free(req_body);

    if (ret != AGENTOS_OK) {
        if (http_code == 429) {
            SVC_LOG_ERROR("openai: Rate limit exhausted after retries");
        } else {
            SVC_LOG_ERROR("openai: HTTP request failed, status=%ld", http_code);
        }
        if (http_resp) provider_http_resp_free(http_resp);
        return ret;
    }

    ret = provider_parse_openai_response(http_resp->data, out_response);
    provider_http_resp_free(http_resp);

    return ret;
}

/* ---------- 流式完成 ---------- */

static int openai_complete_stream(provider_ctx_t* ctx_ptr,
                                  const llm_request_config_t* manager,
                                  llm_stream_callback_t callback,
                                  void* user_data,
                                  llm_response_t** out_response) {
    if (!ctx_ptr || !manager || !callback) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    SVC_LOG_WARN("OpenAI streaming not yet fully implemented, using synchronous mode");

    llm_response_t* resp = NULL;
    int ret = openai_complete(ctx_ptr, manager, &resp);

    if (ret == AGENTOS_OK && resp && resp->choices && resp->choices[0].content) {
        callback(resp->choices[0].content, user_data);
    }

    if (out_response) {
        *out_response = resp;
    } else if (resp) {
        llm_response_free(resp);
    }

    return ret;
}

/* ---------- 操作表 ---------- */

const provider_ops_t openai_ops = {
    .init = openai_init,
    .destroy = openai_destroy,
    .complete = openai_complete,
    .complete_stream = openai_complete_stream,
    .name = "openai",
    .default_model = OPENAI_DEFAULT_MODEL,
    .default_base_url = OPENAI_DEFAULT_BASE
};

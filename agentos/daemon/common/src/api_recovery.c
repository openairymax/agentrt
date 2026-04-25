/**
 * @file api_recovery.c
 * @brief API错误恢复系统实现 — 多凭证池 + 降级策略 + 熔断集成
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "api_recovery.h"
#include "daemon_defaults.h"
#include "svc_logger.h"
#include "platform.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

static uint64_t rec_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

static double rec_jitter(float ratio) {
    if (ratio <= 0.0f) return 0.0;
    double r = (double)agentos_random_uint32(0, RAND_MAX) / (double)RAND_MAX * 2.0 - 1.0;
    return r * ratio;
}

static void rec_update_cred_health(api_rec_credential_t* cred, bool success) {
    if (!cred) return;
    cred->total_uses++;
    uint64_t now = rec_timestamp_ms();

    if (success) {
        cred->last_success_time = now;
        cred->consecutive_failures = 0;

        double decay = AGENTOS_API_REC_HEALTH_DECAY;
        cred->health_score = cred->health_score * decay + (1.0 - decay) * 1.0;
        if (cred->health_score > 1.0) cred->health_score = 1.0;
    } else {
        cred->last_failure_time = now;
        cred->consecutive_failures++;
        cred->total_failures++;

        double penalty = AGENTOS_API_REC_HEALTH_PENALTY;
        cred->health_score = cred->health_score * (1.0 - penalty);
        if (cred->health_score < 0.0) cred->health_score = 0.0;

        if (cred->consecutive_failures >= AGENTOS_API_REC_CONSECUTIVE_DISABLE) {
            cred->is_valid = false;
        }
    }
}

static api_rec_error_code_t classify_http_error(long http_code) {
    if (http_code == 429) return API_REC_ERR_RATE_LIMIT;
    if (http_code == 401 || http_code == 403) return API_REC_ERR_AUTH;
    if (http_code >= 500 && http_code < 600) return API_REC_ERR_SERVER;
    if (http_code == 0 || http_code >= 600) return API_REC_ERR_NETWORK;
    return API_REC_ERR_UNKNOWN;
}

/* ==================== Lifecycle ==================== */

api_rec_pool_t* api_rec_pool_create(const char* name) {
    api_rec_pool_t* pool = calloc(1, sizeof(api_rec_pool_t));
    if (!pool) return NULL;

    if (name) {
        strncpy(pool->name, name, sizeof(pool->name) - 1);
        pool->name[sizeof(pool->name) - 1] = '\0';
    }

    pool->cred_count = 0;
    pool->cred_index = 0;
    pool->fallback_count = 0;
    pool->current_fallback_idx = 0;
    pool->current_level = API_REC_DEGRADE_NONE;

    pool->max_retries = API_REC_MAX_RETRY;
    pool->base_delay_ms = API_REC_DEFAULT_BASE_DELAY_MS;
    pool->backoff_factor = AGENTOS_API_REC_BACKOFF_FACTOR;
    pool->jitter_ratio = AGENTOS_API_REC_JITTER_PCT / 100.0f;

    pool->total_calls = 0;
    pool->recovered_calls = 0;
    pool->failed_calls = 0;
    pool->recovery_rate = 0.0;

    agentos_random_init();

    SVC_LOG_INFO("API recovery pool created: %s", name ? name : "(unnamed)");
    return pool;
}

void api_rec_pool_destroy(api_rec_pool_t* pool) {
    if (!pool) return;
    SVC_LOG_INFO("API recovery pool destroyed: %s (calls=%llu recovered=%llu rate=%.1f%%)",
                 pool->name,
                 (unsigned long long)pool->total_calls,
                 (unsigned long long)pool->recovered_calls,
                 pool->recovery_rate * 100.0);
    free(pool);
}

/* ==================== Credential Pool ==================== */

int api_rec_add_credential(api_rec_pool_t* pool, const char* api_key) {
    if (!pool || !api_key) return -1;
    if (pool->cred_count >= API_REC_MAX_CREDENTIALS) return -2;

    size_t idx = pool->cred_count++;
    api_rec_credential_t* cred = &pool->credentials[idx];
    memset(cred, 0, sizeof(*cred));

    size_t klen = strlen(api_key);
    if (klen >= API_REC_MAX_CRED_LEN) klen = API_REC_MAX_CRED_LEN - 1;
    memcpy(cred->key, api_key, klen);
    cred->key[klen] = '\0';
    cred->is_valid = true;
    cred->health_score = 1.0;

    SVC_LOG_INFO("API rec[%s]: credential #%zu added", pool->name, idx);
    return 0;
}

int api_rec_remove_credential(api_rec_pool_t* pool, size_t index) {
    if (!pool || index >= pool->cred_count) return -1;

    for (size_t i = index; i < pool->cred_count - 1; i++) {
        pool->credentials[i] = pool->credentials[i + 1];
    }

    pool->cred_count--;
    memset(&pool->credentials[pool->cred_count], 0, sizeof(api_rec_credential_t));

    if (pool->cred_index >= pool->cred_count && pool->cred_count > 0)
        pool->cred_index = 0;

    return 0;
}

const char* api_rec_next_credential(api_rec_pool_t* pool) {
    if (!pool || pool->cred_count == 0) return NULL;

    size_t attempts = 0;
    size_t start_idx = pool->cred_index;

    do {
        api_rec_credential_t* cred = &pool->credentials[pool->cred_index];

        if (cred->is_valid && cred->health_score > AGENTOS_API_REC_HEALTH_MIN) {
            const char* key = cred->key;
            pool->cred_index = (pool->cred_index + 1) % pool->cred_count;
            return key;
        }

        pool->cred_index = (pool->cred_index + 1) % pool->cred_count;
        attempts++;

        if (attempts > pool->cred_count) break;
    } while (pool->cred_index != start_idx);

    api_rec_credential_t* best = &pool->credentials[0];
    for (size_t i = 1; i < pool->cred_count; i++) {
        if (pool->credentials[i].health_score > best->health_score)
            best = &pool->credentials[i];
    }

    best->is_valid = true;
    best->consecutive_failures = 0;
    pool->cred_index = (size_t)(best - pool->credentials);
    return best->key;
}

int api_rec_mark_cred_success(api_rec_pool_t* pool) {
    if (!pool || pool->cred_count == 0) return -1;
    size_t last_idx = (pool->cred_index == 0) ? pool->cred_count - 1 : pool->cred_index - 1;
    rec_update_cred_health(&pool->credentials[last_idx], true);
    return 0;
}

int api_rec_mark_cred_failure(api_rec_pool_t* pool, api_rec_error_code_t err) {
    if (!pool || pool->cred_count == 0) return -1;

    size_t last_idx = (pool->cred_index == 0) ? pool->cred_count - 1 : pool->cred_index - 1;

    rec_update_cred_health(&pool->credentials[last_idx], false);

    if (err == API_REC_ERR_AUTH) {
        pool->credentials[last_idx].is_valid = false;
        SVC_LOG_WARN("API rec[%s]: credential #%zu disabled (auth failure)", pool->name, last_idx);
    }

    if (err == API_REC_ERR_RATE_LIMIT) {
        pool->cred_index = (last_idx + 1) % pool->cred_count;
        SVC_LOG_DEBUG("API rec[%s]: rotated credential due to rate limit", pool->name);
    }

    return 0;
}

double api_rec_cred_health(const api_rec_pool_t* pool, size_t index) {
    if (!pool || index >= pool->cred_count) return -1.0;
    return pool->credentials[index].health_score;
}

/* ==================== Fallback Models ==================== */

int api_rec_add_fallback_model(api_rec_pool_t* pool, const char* model,
                                 float cost_weight, int priority) {
    if (!pool || !model) return -1;
    if (pool->fallback_count >= API_REC_MAX_FALLBACK_MODELS) return -2;

    size_t idx = pool->fallback_count++;
    api_rec_model_t* m = &pool->fallback_models[idx];
    memset(m, 0, sizeof(*m));

    size_t mlen = strlen(model);
    if (mlen >= API_REC_MAX_MODEL_LEN) mlen = API_REC_MAX_MODEL_LEN - 1;
    memcpy(m->model, model, mlen);
    m->model[mlen] = '\0';
    m->cost_weight = cost_weight;
    m->priority = priority;
    m->available = true;

    SVC_LOG_INFO("API rec[%s]: fallback model '%s' added (priority=%d weight=%.2f)",
                 pool->name, model, priority, cost_weight);
    return 0;
}

const char* api_rec_current_model(api_rec_pool_t* pool) {
    if (!pool) return NULL;

    if (pool->current_level == API_REC_DEGRADE_NONE ||
        pool->current_fallback_idx >= pool->fallback_count) {
        return "primary";
    }

    return pool->fallback_models[pool->current_fallback_idx].model;
}

int api_rec_degrade(api_rec_pool_t* pool) {
    if (!pool) return -1;
    if (pool->current_fallback_idx < pool->fallback_count) {
        pool->current_fallback_idx++;
        if (pool->current_fallback_idx >= pool->fallback_count) {
            pool->current_level = API_REC_DEGRADE_MOCK;
        } else {
            pool->current_level = API_REC_DEGRADE_LOWER_TIER;
        }
    } else {
        pool->current_level = API_REC_DEGRADE_CACHE;
    }

    SVC_LOG_WARN("API rec[%s]: degraded to level=%d model='%s'",
                 pool->name, pool->current_level,
                 api_rec_current_model(pool));
    return 0;
}

int api_rec_upgrade(api_rec_pool_t* pool) {
    if (!pool) return -1;
    if (pool->current_fallback_idx > 0) {
        pool->current_fallback_idx--;
        pool->current_level = (pool->current_fallback_idx == 0)
            ? API_REC_DEGRADE_NONE : API_REC_DEGRADE_LOWER_TIER;
        SVC_LOG_INFO("API rec[%s]: upgraded to level=%d model='%s'",
                     pool->name, pool->current_level, api_rec_current_model(pool));
    }
    return 0;
}

api_rec_degradation_level_t api_rec_current_level(const api_rec_pool_t* pool) {
    return pool ? pool->current_level : API_REC_DEGRADE_NONE;
}

/* ==================== Retry Config ==================== */

int api_rec_set_retry_config(api_rec_pool_t* pool,
                              uint32_t max_retries,
                              uint32_t base_delay_ms,
                              float backoff_factor,
                              float jitter_ratio) {
    if (!pool) return -1;
    pool->max_retries = max_retries > 0 ? max_retries : API_REC_MAX_RETRY;
    pool->base_delay_ms = base_delay_ms > 0 ? base_delay_ms : API_REC_DEFAULT_BASE_DELAY_MS;
    pool->backoff_factor = backoff_factor > 1.0f ? backoff_factor : 2.0f;
    pool->jitter_ratio = jitter_ratio >= 0.0f ? jitter_ratio : 0.1f;
    return 0;
}

/* ==================== Circuit Breaker Bind ==================== */

int api_rec_bind_circuit_breaker(api_rec_pool_t* pool, void* breaker) {
    if (!pool) return -1;
    pool->cb_breaker = breaker;
    return 0;
}

/* ==================== Core: Execute with Recovery ==================== */

int api_rec_execute_with_recovery(api_rec_pool_t* pool,
                                  api_rec_request_fn request_fn,
                                  void* ctx,
                                  const char* url,
                                  const char* body,
                                  char** out_response,
                                  long* out_http_code,
                                  api_rec_result_t* out_result) {
    if (!pool || !request_fn || !url || !body || !out_response) return -1;
    if (out_result) memset(out_result, 0, sizeof(*out_result));

    *out_response = NULL;
    if (out_http_code) *out_http_code = 0;

    pool->total_calls++;

    int ret = -1;
    long http_code = 0;
    char* resp_body = NULL;
    bool recovered = false;

    const char* cred = api_rec_next_credential(pool);
    if (!cred) {
        if (out_result) {
            out_result->rec_code = API_REC_ERR_AUTH;
            out_result->message = "No valid credentials available";
        }
        pool->failed_calls++;
        goto done;
    }

    for (uint32_t attempt = 0; attempt <= pool->max_retries; attempt++) {

        if (attempt > 0) {
            unsigned delay_ms = (unsigned)(pool->base_delay_ms *
                pow((double)pool->backoff_factor, (double)(attempt - 1)));
            delay_ms += (unsigned)(delay_ms * rec_jitter(pool->jitter_ratio));

            SVC_LOG_DEBUG("API rec[%s]: retry #%u in %ums",
                          pool->name, attempt, delay_ms);

            struct timespec ts;
            ts.tv_sec = delay_ms / 1000;
            ts.tv_nsec = (delay_ms % 1000) * 1000000L;
            nanosleep(&ts, NULL);

            if (classify_http_error(http_code) == API_REC_ERR_RATE_LIMIT) {
                cred = api_rec_next_credential(pool);
                if (!cred) {
                    if (out_result) out_result->rec_code = API_REC_ERR_AUTH;
                    goto done;
                }
            }

            if (classify_http_error(http_code) == API_REC_ERR_SERVER &&
                attempt >= 2 && pool->fallback_count > 0 &&
                pool->current_fallback_idx < pool->fallback_count) {
                api_rec_degrade(pool);
            }
        }

        free(resp_body);
        resp_body = NULL;
        http_code = 0;

        ret = request_fn(ctx, url, body, cred, &resp_body, &http_code);

        if (ret == 0 && http_code >= 200 && http_code < 300) {
            api_rec_mark_cred_success(pool);
            if (pool->current_level != API_REC_DEGRADE_NONE) {
                api_rec_upgrade(pool);
            }
            recovered = true;
            break;
        }

        api_rec_error_code_t err = classify_http_error(http_code);
        api_rec_mark_cred_failure(pool, err);

        if (err == API_REC_ERR_AUTH) {
            cred = api_rec_next_credential(pool);
            if (!cred) {
                if (out_result) {
                    out_result->rec_code = err;
                    out_result->should_rotate_cred = false;
                    out_result->message = "All credentials exhausted";
                }
                goto done;
            }
            continue;
        }

        if (err == API_REC_ERR_RATE_LIMIT && attempt < pool->max_retries) {
            continue;
        }

        if (err == API_REC_ERR_SERVER && attempt >= 2 &&
            pool->fallback_count > 0 &&
            pool->current_fallback_idx < pool->fallback_count) {
            api_rec_degrade(pool);
            continue;
        }
    }

done:
    if (recovered) {
        *out_response = resp_body;
        resp_body = NULL;
        if (out_http_code) *out_http_code = http_code;
        pool->recovered_calls++;
        if (out_result) {
            out_result->rec_code = API_REC_ERR_NONE;
            out_result->is_retriable = false;
            out_result->message = "Success";
        }
        ret = 0;
    } else {
        free(resp_body);
        pool->failed_calls++;
        if (out_result) {
            if (!out_result->rec_code)
                out_result->rec_code = classify_http_error(http_code);
            out_result->is_retriable = true;
            if (!out_result->message)
                out_result->message = "All retries exhausted";
        }
        ret = -1;
    }

    uint64_t total = pool->total_calls;
    if (total > 0)
        pool->recovery_rate = (double)pool->recovered_calls / (double)total;

    return ret;
}

/* ==================== Stats ==================== */

void api_rec_get_stats(const api_rec_pool_t* pool,
                       uint64_t* total, uint64_t* recovered,
                       uint64_t* failed, double* rate) {
    if (!pool) return;
    if (total) *total = pool->total_calls;
    if (recovered) *recovered = pool->recovered_calls;
    if (failed) *failed = pool->failed_calls;
    if (rate) *rate = pool->recovery_rate;
}

const char* api_rec_error_string(api_rec_error_code_t code) {
    switch (code) {
        case API_REC_ERR_NONE:     return "none";
        case API_REC_ERR_NETWORK:   return "network";
        case API_REC_ERR_TIMEOUT:   return "timeout";
        case API_REC_ERR_RATE_LIMIT:return "rate_limit";
        case API_REC_ERR_AUTH:      return "auth";
        case API_REC_ERR_SERVER:    return "server";
        default:                   return "unknown";
    }
}

const char* api_rec_degradation_string(api_rec_degradation_level_t level) {
    switch (level) {
        case API_REC_DEGRADE_NONE:       return "none";
        case API_REC_DEGRADE_LOWER_TIER: return "lower_tier";
        case API_REC_DEGRADE_CACHE:      return "cache";
        case API_REC_DEGRADE_MOCK:       return "mock";
        default:                         return "unknown";
    }
}

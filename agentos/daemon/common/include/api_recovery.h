/**
 * @file api_recovery.h
 * @brief API错误恢复系统：多凭证池 + 降级策略 + 熔断集成
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 核心能力：
 * - 多凭证轮换池（Round-Robin + 健康度追踪）
 * - 模型降级链（primary → fallback1 → fallback2 → ...）
 * - 与熔断器深度集成
 * - 指数退避重试 + 抖动
 * - 恢复成功率统计 (>95% 目标)
 */

#ifndef API_RECOVERY_H
#define API_RECOVERY_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define API_REC_MAX_CREDENTIALS     8
#define API_REC_MAX_CRED_LEN        256
#define API_REC_MAX_FALLBACK_MODELS 4
#define API_REC_MAX_MODEL_LEN       64
#define API_REC_MAX_RETRY           5
#define API_REC_DEFAULT_BASE_DELAY_MS 200

typedef enum {
    API_REC_ERR_NONE = 0,
    API_REC_ERR_NETWORK,
    API_REC_ERR_TIMEOUT,
    API_REC_ERR_RATE_LIMIT,
    API_REC_ERR_AUTH,
    API_REC_ERR_SERVER,
    API_REC_ERR_UNKNOWN
} api_rec_error_code_t;

typedef enum {
    API_REC_DEGRADE_NONE = 0,
    API_REC_DEGRADE_LOWER_TIER,
    API_REC_DEGRADE_CACHE,
    API_REC_DEGRADE_MOCK
} api_rec_degradation_level_t;

typedef struct {
    char key[API_REC_MAX_CRED_LEN];
    bool is_valid;
    uint64_t last_success_time;
    uint64_t last_failure_time;
    uint32_t consecutive_failures;
    uint32_t total_uses;
    uint32_t total_failures;
    double health_score;
} api_rec_credential_t;

typedef struct {
    char model[API_REC_MAX_MODEL_LEN];
    float cost_weight;
    int priority;
    bool available;
} api_rec_model_t;

typedef struct {
    char name[64];

    api_rec_credential_t credentials[API_REC_MAX_CREDENTIALS];
    size_t cred_count;
    size_t cred_index;

    api_rec_model_t fallback_models[API_REC_MAX_FALLBACK_MODELS];
    size_t fallback_count;
    size_t current_fallback_idx;

    uint32_t max_retries;
    uint32_t base_delay_ms;
    float backoff_factor;
    float jitter_ratio;

    api_rec_degradation_level_t current_level;

    uint64_t total_calls;
    uint64_t recovered_calls;
    uint64_t failed_calls;
    double recovery_rate;

    void* cb_breaker;
    void* user_context;
} api_rec_pool_t;

typedef struct {
    int http_code;
    api_rec_error_code_t rec_code;
    bool is_retriable;
    bool should_rotate_cred;
    bool should_degrade;
    const char* message;
} api_rec_result_t;

typedef int (*api_rec_request_fn)(void* ctx, const char* url,
                                    const char* body, const char* cred,
                                    char** resp_body, long* http_code);

api_rec_pool_t* api_rec_pool_create(const char* name);
void api_rec_pool_destroy(api_rec_pool_t* pool);

int api_rec_add_credential(api_rec_pool_t* pool, const char* api_key);
int api_rec_remove_credential(api_rec_pool_t* pool, size_t index);
const char* api_rec_next_credential(api_rec_pool_t* pool);
int api_rec_mark_cred_success(api_rec_pool_t* pool);
int api_rec_mark_cred_failure(api_rec_pool_t* pool, api_rec_error_code_t err);
double api_rec_cred_health(const api_rec_pool_t* pool, size_t index);

int api_rec_add_fallback_model(api_rec_pool_t* pool, const char* model,
                                 float cost_weight, int priority);
const char* api_rec_current_model(api_rec_pool_t* pool);
int api_rec_degrade(api_rec_pool_t* pool);
int api_rec_upgrade(api_rec_pool_t* pool);
api_rec_degradation_level_t api_rec_current_level(const api_rec_pool_t* pool);

int api_rec_execute_with_recovery(api_rec_pool_t* pool,
                                  api_rec_request_fn request_fn,
                                  void* ctx,
                                  const char* url,
                                  const char* body,
                                  char** out_response,
                                  long* out_http_code,
                                  api_rec_result_t* out_result);

int api_rec_set_retry_config(api_rec_pool_t* pool,
                              uint32_t max_retries,
                              uint32_t base_delay_ms,
                              float backoff_factor,
                              float jitter_ratio);

int api_rec_bind_circuit_breaker(api_rec_pool_t* pool, void* breaker);

void api_rec_get_stats(const api_rec_pool_t* pool,
                       uint64_t* total, uint64_t* recovered,
                       uint64_t* failed, double* rate);

const char* api_rec_error_string(api_rec_error_code_t code);
const char* api_rec_degradation_string(api_rec_degradation_level_t level);

#ifdef __cplusplus
}
#endif

#endif /* API_RECOVERY_H */

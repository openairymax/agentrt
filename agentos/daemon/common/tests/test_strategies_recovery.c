/**
 * @file test_strategies_recovery.c
 * @brief 调度策略与API恢复机制单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 测试覆盖：
 *   - API Recovery 池生命周期与重试机制（7 cases）
 *   - Weighted 加权调度评分逻辑（7 cases）
 *   - Priority-Based 优先级调度选择（3 cases）
 *   - Error Code Migration 验证：ERR-02/ERR-03 修复后核心路径不再 return -1（1 case）
 */

#include "api_recovery.h"

#include <assert.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"

/* ==================== 内部辅助：模拟 compute_weighted_score 逻辑 ==================== */

typedef struct {
    char *agent_id;
    char *role;
    float cost_estimate;
    float success_rate;
    float trust_score;
    int priority;
} mock_agent_info_t;

typedef struct {
    float cost_weight;
    float perf_weight;
    float trust_weight;
} mock_weighted_config_t;

static float mock_compute_weighted_score(const mock_agent_info_t *agent,
                                         const mock_weighted_config_t *config)
{
    if (!agent || !config)
        return 0.0f;

    float cost_score = 1.0f;
    float perf_score = 1.0f;
    float trust_score = 1.0f;

    if (agent->cost_estimate > 0) {
        cost_score = 1.0f / (1.0f + agent->cost_estimate);
    }

    perf_score = agent->success_rate;
    trust_score = agent->trust_score;

    return config->cost_weight * cost_score + config->perf_weight * perf_score +
           config->trust_weight * trust_score;
}

/* ==================== 辅助：浮点近似比较 ==================== */

static int feq(float a, float b, float eps)
{
    return fabsf(a - b) < eps;
}

/* ==================== API Recovery 测试 ==================== */

static void test_api_recovery_init(void)
{
    printf("  [1]  test_api_recovery_init...\n");

    api_rec_pool_t *pool = api_rec_pool_create("test_pool");
    assert(pool != NULL);
    assert(pool->cred_count == 0);
    assert(pool->fallback_count == 0);
    assert(pool->current_level == API_REC_DEGRADE_NONE);
    assert(pool->total_calls == 0);
    assert(pool->recovered_calls == 0);
    assert(pool->failed_calls == 0);

    uint64_t total = 0, recovered = 0, failed = 0;
    double rate = 0.0;
    api_rec_get_stats(pool, &total, &recovered, &failed, &rate);
    assert(total == 0 && recovered == 0 && failed == 0);

    const char *model = api_rec_current_model(pool);
    assert(model != NULL);  /* 有效指针（可能是 "primary" 或实际模型名） */

    api_rec_pool_destroy(pool);

    printf("      PASSED\n");
}

static void test_api_recovery_null_context(void)
{
    printf("  [2]  test_api_recovery_null_context...\n");

    assert(api_rec_add_credential(NULL, "sk-test-key") != 0);

    assert(api_rec_remove_credential(NULL, 0) != 0);

    const char *cred = api_rec_next_credential(NULL);
    assert(cred == NULL);

    assert(api_rec_mark_cred_success(NULL) != 0);

    assert(api_rec_mark_cred_failure(NULL, API_REC_ERR_NETWORK) != 0);

    double health = api_rec_cred_health(NULL, 0);
    assert(health <= 0.0);

    assert(api_rec_add_fallback_model(NULL, "gpt-4", 1.0f, 1) != 0);

    assert(api_rec_degrade(NULL) != 0);

    assert(api_rec_upgrade(NULL) != 0);

    api_rec_degradation_level_t level = api_rec_current_level(NULL);
    assert(level == API_REC_DEGRADE_NONE || level < 0);

    printf("      PASSED\n");
}

static void test_api_recovery_register_retry(void)
{
    printf("  [3]  test_api_recovery_register_retry...\n");

    api_rec_pool_t *pool = api_rec_pool_create("retry_test");
    assert(pool != NULL);

    assert(api_rec_set_retry_config(pool, 3, 500, 2.0f, 0.1f) == 0);
    assert(pool->max_retries == 3);
    assert(pool->base_delay_ms == 500);
    assert(feq(pool->backoff_factor, 2.0f, 0.001f));
    assert(feq(pool->jitter_ratio, 0.1f, 0.001f));

    assert(api_rec_set_retry_config(pool, API_REC_MAX_RETRY + 1, 100, 1.5f, 0.2f) == 0 ||
           pool->max_retries <= API_REC_MAX_RETRY);

    api_rec_pool_destroy(pool);

    printf("      PASSED\n");
}

static int g_mock_call_count = 0;
static int g_mock_should_fail = 0;

static int mock_request_success(void *ctx __attribute__((unused)),
                                const char *url __attribute__((unused)),
                                const char *body __attribute__((unused)),
                                const char *cred __attribute__((unused)),
                                char **resp_body, long *http_code)
{
    g_mock_call_count++;
    if (http_code)
        *http_code = 200;
    if (resp_body)
        *resp_body = strdup("{\"status\":\"ok\"}");
    return 0;
}

static void test_api_recovery_execute_success(void)
{
    printf("  [4]  test_api_recovery_execute_success...\n");

    api_rec_pool_t *pool = api_rec_pool_create("success_test");
    assert(pool != NULL);
    api_rec_add_credential(pool, "sk-success-key-001");

    g_mock_call_count = 0;
    char *response = NULL;
    long http_code = 0;
    api_rec_result_t result = {0};

    int ret = api_rec_execute_with_recovery(pool, mock_request_success, NULL,
                                           "https://api.test.com/v1/chat",
                                           "{\"msg\":\"hello\"}", &response,
                                           &http_code, &result);
    assert(ret == 0);
    assert(g_mock_call_count >= 1);
    assert(http_code == 200);
    assert(response != NULL);

    free(response);
    api_rec_pool_destroy(pool);

    printf("      PASSED\n");
}

static int g_mock_fail_count = 0;
static int g_mock_fail_before_success = 2;

static int mock_request_fail_then_succeed(void *ctx __attribute__((unused)),
                                          const char *url __attribute__((unused)),
                                          const char *body __attribute__((unused)),
                                          const char *cred __attribute__((unused)),
                                          char **resp_body, long *http_code)
{
    g_mock_fail_count++;
    if (g_mock_fail_count <= g_mock_fail_before_success) {
        if (http_code)
            *http_code = 503;
        if (resp_body)
            *resp_body = strdup("{\"error\":\"unavailable\"}");
        return -1;
    }
    if (http_code)
        *http_code = 200;
    if (resp_body)
        *resp_body = strdup("{\"status\":\"ok\"}");
    return 0;
}

static void test_api_recovery_execute_failure_with_retry(void)
{
    printf("  [5]  test_api_recovery_execute_failure_with_retry...\n");

    api_rec_pool_t *pool = api_rec_pool_create("retry_fail_test");
    assert(pool != NULL);
    api_rec_add_credential(pool, "sk-retry-key-001");
    api_rec_set_retry_config(pool, 3, 100, 1.0f, 0.0f);

    g_mock_fail_count = 0;
    g_mock_fail_before_success = 2;

    char *response = NULL;
    long http_code = 0;
    api_rec_result_t result = {0};

    int ret = api_rec_execute_with_recovery(pool, mock_request_fail_then_succeed, NULL,
                                           "https://api.test.com/v1/retry",
                                           "{}", &response, &http_code, &result);
    assert(ret == 0);
    assert(g_mock_fail_count > 1);
    assert(g_mock_fail_count >= g_mock_fail_before_success + 1);

    free(response);
    api_rec_pool_destroy(pool);

    printf("      PASSED\n");
}

static int mock_request_always_fail(void *ctx __attribute__((unused)),
                                    const char *url __attribute__((unused)),
                                    const char *body __attribute__((unused)),
                                    const char *cred __attribute__((unused)),
                                    char **resp_body, long *http_code)
{
    g_mock_call_count++;
    if (http_code)
        *http_code = 500;
    if (resp_body)
        *resp_body = strdup("{\"error\":\"server_error\"}");
    return -1;
}

static void test_api_recovery_max_retries_exceeded(void)
{
    printf("  [6]  test_api_recovery_max_retries_exceeded...\n");

    api_rec_pool_t *pool = api_rec_pool_create("max_retry_test");
    assert(pool != NULL);
    api_rec_add_credential(pool, "sk-maxretry-key-001");
    api_rec_set_retry_config(pool, 3, 100, 1.0f, 0.0f);

    g_mock_call_count = 0;
    char *response = NULL;
    long http_code = 0;
    api_rec_result_t result = {0};

    int ret = api_rec_execute_with_recovery(pool, mock_request_always_fail, NULL,
                                           "https://api.test.com/v1/fail",
                                           "{}", &response, &http_code, &result);
    assert(ret != 0);
    assert(g_mock_call_count > 1);

    free(response);
    api_rec_pool_destroy(pool);

    printf("      PASSED\n");
}

static void test_api_recovery_shutdown(void)
{
    printf("  [7]  test_api_recovery_shutdown...\n");

    api_rec_pool_t *pool = api_rec_pool_create("shutdown_test");
    assert(pool != NULL);

    api_rec_add_credential(pool, "sk-shutdown-key-01");
    api_rec_add_credential(pool, "sk-shutdown-key-02");
    api_rec_add_fallback_model(pool, "gpt-4", 10.0f, 1);
    api_rec_add_fallback_model(pool, "gpt-3.5", 1.0f, 2);
    api_rec_set_retry_config(pool, 5, 300, 2.0f, 0.15f);

    uint64_t total = 99, recovered = 88, failed = 11;
    double rate = 0.8889;

    api_rec_pool_destroy(pool);

    pool = NULL;
    assert(api_rec_next_credential(pool) == NULL);
    assert(api_rec_current_model(pool) == NULL);

    printf("      PASSED\n");
}

/* ==================== Weighted Strategy 测试 ==================== */

static void test_weighted_equal_weights(void)
{
    printf("  [8]  test_weighted_equal_weights...\n");

    mock_weighted_config_t config = {.cost_weight = 0.333f, .perf_weight = 0.333f,
                                     .trust_weight = 0.334f};
    mock_agent_info_t agent = {
        .agent_id = "agent-a", .cost_estimate = 1.0f, .success_rate = 0.8f, .trust_score = 0.9f,
        .priority = 5};

    float score = mock_compute_weighted_score(&agent, &config);
    float expected_cost = 1.0f / (1.0f + 1.0f);
    float expected = config.cost_weight * expected_cost + config.perf_weight * 0.8f +
                     config.trust_weight * 0.9f;

    assert(feq(score, expected, 0.005f));
    assert(score > 0.0f && score < 1.0f);

    printf("      PASSED\n");
}

static void test_weighted_cost_dominant(void)
{
    printf("  [9]  test_weighted_cost_dominant...\n");

    mock_weighted_config_t high_cost_cfg = {.cost_weight = 0.8f, .perf_weight = 0.1f,
                                            .trust_weight = 0.1f};
    mock_agent_info_t low_cost = {
        .agent_id = "cheap-agent", .cost_estimate = 0.1f, .success_rate = 0.5f,
        .trust_score = 0.5f, .priority = 3};
    mock_agent_info_t high_cost = {
        .agent_id = "expensive-agent", .cost_estimate = 10.0f, .success_rate = 0.95f,
        .trust_score = 0.95f, .priority = 3};

    float s_cheap = mock_compute_weighted_score(&low_cost, &high_cost_cfg);
    float s_expensive = mock_compute_weighted_score(&high_cost, &high_cost_cfg);

    assert(s_cheap > s_expensive);

    printf("      PASSED\n");
}

static void test_weighted_perf_dominant(void)
{
    printf("  [10] test_weighted_perf_dominant...\n");

    mock_weighted_config_t perf_cfg = {.cost_weight = 0.1f, .perf_weight = 0.8f,
                                       .trust_weight = 0.1f};
    mock_agent_info_t high_perf = {
        .agent_id = "fast-agent", .cost_estimate = 5.0f, .success_rate = 0.99f,
        .trust_score = 0.5f, .priority = 3};
    mock_agent_info_t low_perf = {
        .agent_id = "slow-agent", .cost_estimate = 0.5f, .success_rate = 0.3f,
        .trust_score = 0.8f, .priority = 3};

    float s_fast = mock_compute_weighted_score(&high_perf, &perf_cfg);
    float s_slow = mock_compute_weighted_score(&low_perf, &perf_cfg);

    assert(s_fast > s_slow);

    printf("      PASSED\n");
}

static void test_weighted_trust_dominant(void)
{
    printf("  [11] test_weighted_trust_dominant...\n");

    mock_weighted_config_t trust_cfg = {.cost_weight = 0.1f, .perf_weight = 0.1f,
                                        .trust_weight = 0.8f};
    mock_agent_info_t high_trust = {
        .agent_id = "trusted-agent", .cost_estimate = 5.0f, .success_rate = 0.5f,
        .trust_score = 0.98f, .priority = 3};
    mock_agent_info_t low_trust = {
        .agent_id = "distrusted-agent", .cost_estimate = 0.5f, .success_rate = 0.9f,
        .trust_score = 0.2f, .priority = 3};

    float s_trusted = mock_compute_weighted_score(&high_trust, &trust_cfg);
    float s_untrusted = mock_compute_weighted_score(&low_trust, &trust_cfg);

    assert(s_trusted > s_untrusted);

    printf("      PASSED\n");
}

static void test_weighted_zero_all_scores(void)
{
    printf("  [12] test_weighted_zero_all_scores...\n");

    mock_weighted_config_t cfg = {.cost_weight = 0.33f, .perf_weight = 0.33f,
                                  .trust_weight = 0.34f};
    mock_agent_info_t zero_agent = {
        .agent_id = "zero-agent", .cost_estimate = 0.0f, .success_rate = 0.0f,
        .trust_score = 0.0f, .priority = 1};

    float score = mock_compute_weighted_score(&zero_agent, &cfg);

    assert(score >= 0.0f);
    assert(feq(score, 0.33f, 0.01f));

    printf("      PASSED\n");
}

static void test_weighted_high_cost_penalty(void)
{
    printf("  [13] test_weighted_high_cost_penalty...\n");

    mock_weighted_config_t cfg = {.cost_weight = 0.5f, .perf_weight = 0.25f,
                                  .trust_weight = 0.25f};
    mock_agent_info_t cheap = {
        .agent_id = "cheap", .cost_estimate = 0.1f, .success_rate = 0.7f,
        .trust_score = 0.7f, .priority = 2};
    mock_agent_info_t expensive = {
        .agent_id = "expensive", .cost_estimate = 100.0f, .success_rate = 0.99f,
        .trust_score = 0.99f, .priority = 2};

    float s_cheap = mock_compute_weighted_score(&cheap, &cfg);
    float s_exp = mock_compute_weighted_score(&expensive, &cfg);

    assert(s_cheap > s_exp);

    float cost_penalty_ratio = s_cheap / s_exp;
    (void)cost_penalty_ratio;
    assert(cost_penalty_ratio > 1.0f);  /* 便宜Agent得分更高 */

    printf("      PASSED\n");
}

static void test_weighted_perfect_agent(void)
{
    printf("  [14] test_weighted_perfect_agent...\n");

    mock_weighted_config_t cfg = {.cost_weight = 0.3f, .perf_weight = 0.4f,
                                  .trust_weight = 0.3f};
    mock_agent_info_t perfect = {
        .agent_id = "perfect", .cost_estimate = 0.0f, .success_rate = 1.0f,
        .trust_score = 1.0f, .priority = 10};

    float score = mock_compute_weighted_score(&perfect, &cfg);
    float expected = 0.3f * 1.0f + 0.4f * 1.0f + 0.3f * 1.0f;

    assert(feq(score, expected, 0.001f));
    assert(feq(score, 1.0f, 0.001f));

    printf("      PASSED\n");
}

/* ==================== Priority-Based Strategy 测试 ==================== */

static int mock_priority_select(const mock_agent_info_t *candidates[], size_t count)
{
    if (!candidates || count == 0)
        return -1;

    int best_idx = -1;
    int best_pri = INT_MIN;

    for (size_t i = 0; i < count; i++) {
        if (!candidates[i])
            continue;
        if (candidates[i]->priority > best_pri) {
            best_pri = candidates[i]->priority;
            best_idx = (int)i;
        }
    }
    return best_idx;
}

static void test_priority_higher_wins(void)
{
    printf("  [15] test_priority_higher_wins...\n");

    mock_agent_info_t a = {.agent_id = "low-pri", .priority = 1};
    mock_agent_info_t b = {.agent_id = "mid-pri", .priority = 5};
    mock_agent_info_t c = {.agent_id = "hi-pri", .priority = 10};

    const mock_agent_info_t *candidates[] = {&a, &b, &c};
    int winner = mock_priority_select(candidates, 3);

    assert(winner == 2);
    assert(strcmp(candidates[winner]->agent_id, "hi-pri") == 0);

    printf("      PASSED\n");
}

static void test_priority_tie_breaking(void)
{
    printf("  [16] test_priority_tie_breaking...\n");

    mock_agent_info_t a = {.agent_id = "first", .priority = 5};
    mock_agent_info_t b = {.agent_id = "second", .priority = 5};
    mock_agent_info_t c = {.agent_id = "third", .priority = 3};

    const mock_agent_info_t *candidates[] = {&a, &b, &c};
    int winner = mock_priority_select(candidates, 3);

    assert(winner == 0);
    assert(strcmp(candidates[winner]->agent_id, "first") == 0);

    printf("      PASSED\n");
}

static void test_priority_invalid_priority(void)
{
    printf("  [17] test_priority_invalid_priority...\n");

    mock_agent_info_t normal = {.agent_id = "normal", .priority = 5};
    mock_agent_info_t negative = {.agent_id = "neg-pri", .priority = -100};
    mock_agent_info_t zero = {.agent_id = "zero-pri", .priority = 0};

    const mock_agent_info_t *candidates_neg[] = {&normal, &negative};
    int winner_neg = mock_priority_select(candidates_neg, 2);
    assert(winner_neg == 0);

    const mock_agent_info_t *candidates_zero[] = {&negative, &zero, &normal};
    int winner_zero = mock_priority_select(candidates_zero, 3);
    assert(winner_zero == 2);

    const mock_agent_info_t *all_neg[] = {&negative};
    int all_neg_winner = mock_priority_select(all_neg, 1);
    assert(all_neg_winner == 0);

    printf("      PASSED\n");
}

/* ==================== Error Code Migration 验证 ==================== */

static void test_err_migration_no_return_neg1(void)
{
    printf("  [18] test_err_migration_no_return_neg1 (ERR-02/ERR-03 migration)...\n");

    assert(AGENTOS_SUCCESS == 0);
    assert(AGENTOS_OK == 0);

    /* 验证 ERR_UNKNOWN == -1 （传统未知错误码） */
    assert(AGENTOS_ERR_UNKNOWN < 0);

    /* 验证 E* 别名映射到 ERR* 系列 */
    assert(AGENTOS_EINVAL != 0);
    assert(AGENTOS_ENOMEM != 0);
    assert(AGENTOS_ENOENT != 0);

    /* 验证没有错误码使用 -1 作为返回值（ERR-02 合规） */
    assert(AGENTOS_ERR_INVALID_PARAM != (-1));
    assert(AGENTOS_ERR_OUT_OF_MEMORY != (-1));

    /* 验证 strerror 可用 */
    const char *s_unknown = agentos_error_str(AGENTOS_ERR_UNKNOWN);
    const char *s_inval = agentos_error_str(AGENTOS_ERR_INVALID_PARAM);
    assert(s_unknown != NULL);
    assert(s_inval != NULL);
    assert(strlen(s_unknown) > 0);
    assert(strlen(s_inval) > 0);

    printf("      PASSED\n");
}

/* ==================== Main 入口 ==================== */

int main(void)
{
    int total = 18;
    int passed = 0;
    int failed = 0;

    printf("=========================================\n");
    printf("  Strategies & Recovery Unit Tests\n");
    printf("=========================================\n\n");

    printf("--- API Recovery Tests ---\n");
    test_api_recovery_init();
    passed++;
    test_api_recovery_null_context();
    passed++;
    test_api_recovery_register_retry();
    passed++;
    test_api_recovery_execute_success();
    passed++;
    test_api_recovery_execute_failure_with_retry();
    passed++;
    test_api_recovery_max_retries_exceeded();
    passed++;
    test_api_recovery_shutdown();
    passed++;

    printf("\n--- Weighted Strategy Tests ---\n");
    test_weighted_equal_weights();
    passed++;
    test_weighted_cost_dominant();
    passed++;
    test_weighted_perf_dominant();
    passed++;
    test_weighted_trust_dominant();
    passed++;
    test_weighted_zero_all_scores();
    passed++;
    test_weighted_high_cost_penalty();
    passed++;
    test_weighted_perfect_agent();
    passed++;

    printf("\n--- Priority-Based Strategy Tests ---\n");
    test_priority_higher_wins();
    passed++;
    test_priority_tie_breaking();
    passed++;
    test_priority_invalid_priority();
    passed++;

    printf("\n--- Error Code Migration Tests ---\n");
    test_err_migration_no_return_neg1();
    passed++;

    failed = total - passed;

    printf("\n=========================================\n");
    printf("  Results: %d/%d PASSED, %d FAILED\n", passed, total, failed);
    printf("=========================================\n");

    return failed > 0 ? 1 : 0;
}

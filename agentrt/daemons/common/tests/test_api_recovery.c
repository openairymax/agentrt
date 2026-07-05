/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 * test_api_recovery.c - API Recovery Module Unit Tests
 */

#include "../include/api_recovery.h"
#include "error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { tests_run++; printf("  %-50s", name); } while(0)
#define PASS() do { tests_passed++; printf("[PASS]\n"); } while(0)
#define FAIL(msg) do { printf("[FAIL] %s\n", msg); return; } while(0)
#define ASSERT(cond, msg) do { if (!(cond)) { FAIL(msg); } } while(0)

/* ==================== Pool Lifecycle ==================== */

static void test_pool_create_null_name(void)
{
    TEST("Pool create with NULL name");
    api_rec_pool_t *pool = api_rec_pool_create(NULL);
    ASSERT(pool != NULL, "create with NULL name should succeed");
    ASSERT(pool->cred_count == 0, "cred_count should be 0");
    ASSERT(pool->fallback_count == 0, "fallback_count should be 0");
    ASSERT(pool->current_level == API_REC_DEGRADE_NONE, "level should be NONE");
    api_rec_pool_destroy(pool);
    PASS();
}

static void test_pool_create_with_name(void)
{
    TEST("Pool create with name");
    api_rec_pool_t *pool = api_rec_pool_create("test_pool");
    ASSERT(pool != NULL, "create should succeed");
    ASSERT(strcmp(pool->name, "test_pool") == 0, "name should match");
    ASSERT(pool->total_calls == 0, "total_calls should be 0");
    ASSERT(pool->recovered_calls == 0, "recovered_calls should be 0");
    ASSERT(pool->failed_calls == 0, "failed_calls should be 0");
    api_rec_pool_destroy(pool);
    PASS();
}

static void test_pool_destroy_null_safe(void)
{
    TEST("Pool destroy NULL is safe");
    api_rec_pool_destroy(NULL);
    api_rec_pool_destroy(NULL);
    PASS();
}

static void test_pool_multiple_create_destroy(void)
{
    TEST("Pool multiple create/destroy cycles");
    for (int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "cycle_%d", i);
        api_rec_pool_t *pool = api_rec_pool_create(name);
        ASSERT(pool != NULL, "create should succeed");
        ASSERT(api_rec_add_credential(pool, "sk-cyclic-key") == 0, "add credential should succeed");
        api_rec_pool_destroy(pool);
    }
    PASS();
}

/* ==================== Credential Add/Remove/Rotate ==================== */

static void test_add_credential(void)
{
    TEST("Add credential");
    api_rec_pool_t *pool = api_rec_pool_create("cred_test");
    ASSERT(pool != NULL, "create pool");

    int ret = api_rec_add_credential(pool, "sk-api-key-001");
    ASSERT(ret == 0, "add first credential should succeed");
    ASSERT(pool->cred_count == 1, "cred_count should be 1");
    ASSERT(pool->credentials[0].health_score == 1.0, "initial health should be 1.0");
    ASSERT(pool->credentials[0].is_valid == true, "credential should be valid");

    ret = api_rec_add_credential(pool, "sk-api-key-002");
    ASSERT(ret == 0, "add second credential should succeed");
    ASSERT(pool->cred_count == 2, "cred_count should be 2");

    api_rec_pool_destroy(pool);
    PASS();
}

static void test_add_credential_null_pool(void)
{
    TEST("Add credential to NULL pool");
    int ret = api_rec_add_credential(NULL, "sk-key");
    ASSERT(ret == AGENTRT_ERR_INVALID_PARAM, "should return INVALID_PARAM");
    PASS();
}

static void test_add_credential_null_key(void)
{
    TEST("Add credential with NULL key");
    api_rec_pool_t *pool = api_rec_pool_create("null_key_test");
    ASSERT(pool != NULL, "create pool");

    int ret = api_rec_add_credential(pool, NULL);
    ASSERT(ret == AGENTRT_ERR_INVALID_PARAM, "should return INVALID_PARAM");

    api_rec_pool_destroy(pool);
    PASS();
}

static void test_add_credential_overflow(void)
{
    TEST("Add credential beyond max limit");
    api_rec_pool_t *pool = api_rec_pool_create("overflow_test");
    ASSERT(pool != NULL, "create pool");

    char key[64];
    int ret;
    for (size_t i = 0; i < API_REC_MAX_CREDENTIALS; i++) {
        snprintf(key, sizeof(key), "sk-overflow-key-%zu", i);
        ret = api_rec_add_credential(pool, key);
        ASSERT(ret == 0, "add within limit should succeed");
    }
    ASSERT(pool->cred_count == API_REC_MAX_CREDENTIALS, "cred_count should be at max");

    ret = api_rec_add_credential(pool, "sk-one-too-many");
    ASSERT(ret == AGENTRT_ERR_OVERFLOW, "should return OVERFLOW when full");

    api_rec_pool_destroy(pool);
    PASS();
}

static void test_remove_credential(void)
{
    TEST("Remove credential");
    api_rec_pool_t *pool = api_rec_pool_create("remove_test");
    ASSERT(pool != NULL, "create pool");

    api_rec_add_credential(pool, "sk-key-001");
    api_rec_add_credential(pool, "sk-key-002");
    api_rec_add_credential(pool, "sk-key-003");
    ASSERT(pool->cred_count == 3, "should have 3 credentials");

    int ret = api_rec_remove_credential(pool, 1);
    ASSERT(ret == 0, "remove middle credential should succeed");
    ASSERT(pool->cred_count == 2, "cred_count should be 2");

    ret = api_rec_remove_credential(pool, 0);
    ASSERT(ret == 0, "remove first credential should succeed");
    ASSERT(pool->cred_count == 1, "cred_count should be 1");

    ret = api_rec_remove_credential(pool, 0);
    ASSERT(ret == 0, "remove last credential should succeed");
    ASSERT(pool->cred_count == 0, "cred_count should be 0");

    api_rec_pool_destroy(pool);
    PASS();
}

static void test_remove_credential_null_pool(void)
{
    TEST("Remove credential from NULL pool");
    int ret = api_rec_remove_credential(NULL, 0);
    ASSERT(ret == AGENTRT_ERR_INVALID_PARAM, "should return INVALID_PARAM");
    PASS();
}

static void test_remove_credential_out_of_range(void)
{
    TEST("Remove credential out of range");
    api_rec_pool_t *pool = api_rec_pool_create("oor_test");
    ASSERT(pool != NULL, "create pool");

    api_rec_add_credential(pool, "sk-key-001");
    ASSERT(pool->cred_count == 1, "should have 1 credential");

    int ret = api_rec_remove_credential(pool, 5);
    ASSERT(ret == AGENTRT_ERR_INVALID_PARAM, "should return INVALID_PARAM for out of range");

    ret = api_rec_remove_credential(pool, 1);
    ASSERT(ret == AGENTRT_ERR_INVALID_PARAM, "should return INVALID_PARAM for index == count");

    api_rec_pool_destroy(pool);
    PASS();
}

/* ==================== Credential Rotation ==================== */

static void test_next_credential_rotation(void)
{
    TEST("Next credential rotation");
    api_rec_pool_t *pool = api_rec_pool_create("rotation_test");
    ASSERT(pool != NULL, "create pool");

    api_rec_add_credential(pool, "sk-key-A");
    api_rec_add_credential(pool, "sk-key-B");
    api_rec_add_credential(pool, "sk-key-C");

    const char *cred1 = api_rec_next_credential(pool);
    ASSERT(cred1 != NULL, "first credential should not be NULL");
    ASSERT(strcmp(cred1, "sk-key-A") == 0, "first should be key-A");

    const char *cred2 = api_rec_next_credential(pool);
    ASSERT(cred2 != NULL, "second credential should not be NULL");
    ASSERT(strcmp(cred2, "sk-key-B") == 0, "second should be key-B");

    const char *cred3 = api_rec_next_credential(pool);
    ASSERT(cred3 != NULL, "third credential should not be NULL");
    ASSERT(strcmp(cred3, "sk-key-C") == 0, "third should be key-C");

    const char *cred4 = api_rec_next_credential(pool);
    ASSERT(cred4 != NULL, "wraparound should not be NULL");
    ASSERT(strcmp(cred4, "sk-key-A") == 0, "wraparound should be key-A");

    api_rec_pool_destroy(pool);
    PASS();
}

static void test_next_credential_null_pool(void)
{
    TEST("Next credential on NULL pool");
    const char *cred = api_rec_next_credential(NULL);
    ASSERT(cred == NULL, "should return NULL for NULL pool");
    PASS();
}

static void test_next_credential_empty_pool(void)
{
    TEST("Next credential on empty pool");
    api_rec_pool_t *pool = api_rec_pool_create("empty_cred_test");
    ASSERT(pool != NULL, "create pool");

    const char *cred = api_rec_next_credential(pool);
    ASSERT(cred == NULL, "should return NULL for empty pool");

    api_rec_pool_destroy(pool);
    PASS();
}

/* ==================== Credential Health Tracking ==================== */

static void test_mark_cred_success(void)
{
    TEST("Mark credential success");
    api_rec_pool_t *pool = api_rec_pool_create("mark_success_test");
    ASSERT(pool != NULL, "create pool");

    api_rec_add_credential(pool, "sk-mark-success-001");
    api_rec_next_credential(pool);

    int ret = api_rec_mark_cred_success(pool);
    ASSERT(ret == 0, "mark success should succeed");

    double health = api_rec_cred_health(pool, 0);
    ASSERT(health > 0.9, "health should remain high after success");

    api_rec_pool_destroy(pool);
    PASS();
}

static void test_mark_cred_failure(void)
{
    TEST("Mark credential failure");
    api_rec_pool_t *pool = api_rec_pool_create("mark_failure_test");
    ASSERT(pool != NULL, "create pool");

    api_rec_add_credential(pool, "sk-mark-fail-001");
    api_rec_next_credential(pool);

    int ret = api_rec_mark_cred_failure(pool, API_REC_ERR_NETWORK);
    ASSERT(ret == 0, "mark failure should succeed");

    double health = api_rec_cred_health(pool, 0);
    ASSERT(health < 1.0, "health should decrease after failure");

    api_rec_pool_destroy(pool);
    PASS();
}

static void test_mark_cred_success_null_pool(void)
{
    TEST("Mark cred success on NULL pool");
    int ret = api_rec_mark_cred_success(NULL);
    ASSERT(ret == AGENTRT_ERR_INVALID_PARAM, "should return INVALID_PARAM");
    PASS();
}

static void test_mark_cred_failure_null_pool(void)
{
    TEST("Mark cred failure on NULL pool");
    int ret = api_rec_mark_cred_failure(NULL, API_REC_ERR_NETWORK);
    ASSERT(ret == AGENTRT_ERR_INVALID_PARAM, "should return INVALID_PARAM");
    PASS();
}

static void test_mark_cred_empty_pool(void)
{
    TEST("Mark cred on empty pool");
    api_rec_pool_t *pool = api_rec_pool_create("empty_mark_test");
    ASSERT(pool != NULL, "create pool");

    int ret = api_rec_mark_cred_success(pool);
    ASSERT(ret == AGENTRT_ERR_INVALID_PARAM, "mark success on empty should fail");

    ret = api_rec_mark_cred_failure(pool, API_REC_ERR_NETWORK);
    ASSERT(ret == AGENTRT_ERR_INVALID_PARAM, "mark failure on empty should fail");

    api_rec_pool_destroy(pool);
    PASS();
}

static void test_cred_health(void)
{
    TEST("Credential health query");
    api_rec_pool_t *pool = api_rec_pool_create("health_query_test");
    ASSERT(pool != NULL, "create pool");

    api_rec_add_credential(pool, "sk-health-001");
    api_rec_add_credential(pool, "sk-health-002");

    double health0 = api_rec_cred_health(pool, 0);
    ASSERT(health0 == 1.0, "initial health should be 1.0");

    double health1 = api_rec_cred_health(pool, 1);
    ASSERT(health1 == 1.0, "initial health should be 1.0");

    api_rec_pool_destroy(pool);
    PASS();
}

static void test_cred_health_null_pool(void)
{
    TEST("Credential health on NULL pool");
    double health = api_rec_cred_health(NULL, 0);
    ASSERT(health < 0.0, "should return negative for NULL pool");
    PASS();
}

static void test_cred_health_out_of_range(void)
{
    TEST("Credential health out of range");
    api_rec_pool_t *pool = api_rec_pool_create("health_oor_test");
    ASSERT(pool != NULL, "create pool");

    api_rec_add_credential(pool, "sk-health-oor-001");

    double health = api_rec_cred_health(pool, 5);
    ASSERT(health < 0.0, "should return negative for out of range");

    health = api_rec_cred_health(pool, 1);
    ASSERT(health < 0.0, "should return negative for index == count");

    api_rec_pool_destroy(pool);
    PASS();
}

/* ==================== Fallback Models ==================== */

static void test_add_fallback_model(void)
{
    TEST("Add fallback model");
    api_rec_pool_t *pool = api_rec_pool_create("fallback_test");
    ASSERT(pool != NULL, "create pool");

    int ret = api_rec_add_fallback_model(pool, "gpt-4", 10.0f, 1);
    ASSERT(ret == 0, "add first fallback should succeed");
    ASSERT(pool->fallback_count == 1, "fallback_count should be 1");
    ASSERT(strcmp(pool->fallback_models[0].model, "gpt-4") == 0, "model name should match");
    ASSERT(pool->fallback_models[0].available == true, "model should be available");

    ret = api_rec_add_fallback_model(pool, "gpt-3.5-turbo", 1.0f, 2);
    ASSERT(ret == 0, "add second fallback should succeed");
    ASSERT(pool->fallback_count == 2, "fallback_count should be 2");

    api_rec_pool_destroy(pool);
    PASS();
}

static void test_add_fallback_model_null_pool(void)
{
    TEST("Add fallback model to NULL pool");
    int ret = api_rec_add_fallback_model(NULL, "gpt-4", 1.0f, 1);
    ASSERT(ret == AGENTRT_ERR_INVALID_PARAM, "should return INVALID_PARAM");
    PASS();
}

static void test_add_fallback_model_null_name(void)
{
    TEST("Add fallback model with NULL name");
    api_rec_pool_t *pool = api_rec_pool_create("fallback_null_test");
    ASSERT(pool != NULL, "create pool");

    int ret = api_rec_add_fallback_model(pool, NULL, 1.0f, 1);
    ASSERT(ret == AGENTRT_ERR_INVALID_PARAM, "should return INVALID_PARAM");

    api_rec_pool_destroy(pool);
    PASS();
}

static void test_add_fallback_model_overflow(void)
{
    TEST("Add fallback model beyond max");
    api_rec_pool_t *pool = api_rec_pool_create("fallback_overflow");
    ASSERT(pool != NULL, "create pool");

    char model_name[64];
    int ret;
    for (size_t i = 0; i < API_REC_MAX_FALLBACK_MODELS; i++) {
        snprintf(model_name, sizeof(model_name), "fallback-model-%zu", i);
        ret = api_rec_add_fallback_model(pool, model_name, (float)i, (int)i);
        ASSERT(ret == 0, "add within limit should succeed");
    }
    ASSERT(pool->fallback_count == API_REC_MAX_FALLBACK_MODELS, "should be at max");

    ret = api_rec_add_fallback_model(pool, "one-too-many", 1.0f, 99);
    ASSERT(ret == AGENTRT_ERR_OVERFLOW, "should return OVERFLOW when full");

    api_rec_pool_destroy(pool);
    PASS();
}

/* ==================== Degradation / Upgrade / Level ==================== */

static void test_current_model_primary(void)
{
    TEST("Current model when no degradation");
    api_rec_pool_t *pool = api_rec_pool_create("current_model_test");
    ASSERT(pool != NULL, "create pool");

    const char *model = api_rec_current_model(pool);
    ASSERT(model != NULL, "current model should not be NULL");
    ASSERT(strcmp(model, "primary") == 0, "should be 'primary' when no degradation");

    api_rec_pool_destroy(pool);
    PASS();
}

static void test_current_model_null_pool(void)
{
    TEST("Current model on NULL pool");
    const char *model = api_rec_current_model(NULL);
    ASSERT(model == NULL, "should return NULL for NULL pool");
    PASS();
}

static void test_degrade_upgrade_cycle(void)
{
    TEST("Degrade and upgrade cycle");
    api_rec_pool_t *pool = api_rec_pool_create("degrade_test");
    ASSERT(pool != NULL, "create pool");

    api_rec_add_fallback_model(pool, "fallback-gpt-4", 10.0f, 1);
    api_rec_add_fallback_model(pool, "fallback-gpt-3.5", 1.0f, 2);

    api_rec_degradation_level_t level = api_rec_current_level(pool);
    ASSERT(level == API_REC_DEGRADE_NONE, "initial level should be NONE");

    int ret = api_rec_degrade(pool);
    ASSERT(ret == 0, "first degrade should succeed");
    level = api_rec_current_level(pool);
    ASSERT(level == API_REC_DEGRADE_LOWER_TIER, "level should be LOWER_TIER after one degrade");

    const char *model = api_rec_current_model(pool);
    ASSERT(strcmp(model, "fallback-gpt-3.5") == 0, "should use second fallback model after one degrade");

    ret = api_rec_degrade(pool);
    ASSERT(ret == 0, "second degrade should succeed");
    level = api_rec_current_level(pool);
    ASSERT(level == API_REC_DEGRADE_CACHE, "level should be CACHE after second degrade");

    model = api_rec_current_model(pool);
    ASSERT(strcmp(model, "primary") == 0, "should return primary after all fallbacks exhausted");

    ret = api_rec_upgrade(pool);
    ASSERT(ret == 0, "first upgrade should succeed");
    level = api_rec_current_level(pool);
    ASSERT(level == API_REC_DEGRADE_LOWER_TIER, "level should be LOWER_TIER after upgrade");

    ret = api_rec_upgrade(pool);
    ASSERT(ret == 0, "second upgrade should succeed");
    level = api_rec_current_level(pool);
    ASSERT(level == API_REC_DEGRADE_NONE, "level should be NONE after full upgrade");

    api_rec_pool_destroy(pool);
    PASS();
}

static void test_degrade_null_pool(void)
{
    TEST("Degrade on NULL pool");
    int ret = api_rec_degrade(NULL);
    ASSERT(ret == AGENTRT_ERR_INVALID_PARAM, "should return INVALID_PARAM");
    PASS();
}

static void test_upgrade_null_pool(void)
{
    TEST("Upgrade on NULL pool");
    int ret = api_rec_upgrade(NULL);
    ASSERT(ret == AGENTRT_ERR_INVALID_PARAM, "should return INVALID_PARAM");
    PASS();
}

static void test_degrade_without_fallbacks(void)
{
    TEST("Degrade without fallback models");
    api_rec_pool_t *pool = api_rec_pool_create("no_fallback_test");
    ASSERT(pool != NULL, "create pool");

    api_rec_degradation_level_t level = api_rec_current_level(pool);
    ASSERT(level == API_REC_DEGRADE_NONE, "initial level should be NONE");

    int ret = api_rec_degrade(pool);
    ASSERT(ret == 0, "degrade should succeed even without fallbacks");
    level = api_rec_current_level(pool);
    ASSERT(level == API_REC_DEGRADE_CACHE, "should go directly to CACHE when no fallbacks");

    api_rec_pool_destroy(pool);
    PASS();
}

static void test_upgrade_at_top(void)
{
    TEST("Upgrade when already at top level");
    api_rec_pool_t *pool = api_rec_pool_create("upgrade_top_test");
    ASSERT(pool != NULL, "create pool");

    api_rec_add_fallback_model(pool, "fallback-gpt-4", 10.0f, 1);

    int ret = api_rec_upgrade(pool);
    ASSERT(ret == 0, "upgrade at top should succeed (no-op)");
    api_rec_degradation_level_t level = api_rec_current_level(pool);
    ASSERT(level == API_REC_DEGRADE_NONE, "level should still be NONE");

    api_rec_pool_destroy(pool);
    PASS();
}

static void test_current_level_null_pool(void)
{
    TEST("Current level on NULL pool");
    api_rec_degradation_level_t level = api_rec_current_level(NULL);
    ASSERT(level == API_REC_DEGRADE_NONE, "should return NONE for NULL pool");
    PASS();
}

/* ==================== Retry Configuration ==================== */

static void test_set_retry_config(void)
{
    TEST("Set retry configuration");
    api_rec_pool_t *pool = api_rec_pool_create("retry_config_test");
    ASSERT(pool != NULL, "create pool");

    int ret = api_rec_set_retry_config(pool, 3, 500, 2.0f, 0.1f);
    ASSERT(ret == 0, "set retry config should succeed");
    ASSERT(pool->max_retries == 3, "max_retries should be 3");
    ASSERT(pool->base_delay_ms == 500, "base_delay_ms should be 500");
    ASSERT(pool->backoff_factor == 2.0f, "backoff_factor should be 2.0");
    ASSERT(pool->jitter_ratio == 0.1f, "jitter_ratio should be 0.1");

    api_rec_pool_destroy(pool);
    PASS();
}

static void test_set_retry_config_null_pool(void)
{
    TEST("Set retry config on NULL pool");
    int ret = api_rec_set_retry_config(NULL, 3, 500, 2.0f, 0.1f);
    ASSERT(ret == AGENTRT_ERR_INVALID_PARAM, "should return INVALID_PARAM");
    PASS();
}

static void test_set_retry_config_edge_values(void)
{
    TEST("Set retry config edge values");
    api_rec_pool_t *pool = api_rec_pool_create("retry_edge_test");
    ASSERT(pool != NULL, "create pool");

    int ret = api_rec_set_retry_config(pool, 0, 0, 0.0f, 0.0f);
    ASSERT(ret == 0, "set zero values should succeed");
    ASSERT(pool->max_retries == API_REC_MAX_RETRY, "zero max_retries should fallback to default");
    ASSERT(pool->base_delay_ms == API_REC_DEFAULT_BASE_DELAY_MS, "zero delay should fallback to default");
    ASSERT(pool->backoff_factor == 2.0f, "zero backoff should fallback to 2.0");
    ASSERT(pool->jitter_ratio == 0.0f, "zero jitter should be accepted");

    ret = api_rec_set_retry_config(pool, 10, 10000, 3.0f, 0.5f);
    ASSERT(ret == 0, "set large values should succeed");
    ASSERT(pool->max_retries == 10, "max_retries should be 10");
    ASSERT(pool->base_delay_ms == 10000, "base_delay_ms should be 10000");

    api_rec_pool_destroy(pool);
    PASS();
}

/* ==================== Circuit Breaker Binding ==================== */

static void test_bind_circuit_breaker(void)
{
    TEST("Bind circuit breaker");
    api_rec_pool_t *pool = api_rec_pool_create("cb_bind_test");
    ASSERT(pool != NULL, "create pool");

    int dummy_cb = 42;
    int ret = api_rec_bind_circuit_breaker(pool, &dummy_cb);
    ASSERT(ret == 0, "bind circuit breaker should succeed");
    ASSERT(pool->cb_breaker == &dummy_cb, "cb_breaker should point to dummy");

    api_rec_bind_circuit_breaker(pool, NULL);
    ASSERT(pool->cb_breaker == NULL, "unbind should set cb_breaker to NULL");

    api_rec_pool_destroy(pool);
    PASS();
}

static void test_bind_circuit_breaker_null_pool(void)
{
    TEST("Bind circuit breaker to NULL pool");
    int dummy_cb = 42;
    int ret = api_rec_bind_circuit_breaker(NULL, &dummy_cb);
    ASSERT(ret == AGENTRT_ERR_INVALID_PARAM, "should return INVALID_PARAM");
    PASS();
}

/* ==================== Statistics ==================== */

static void test_get_stats(void)
{
    TEST("Get statistics");
    api_rec_pool_t *pool = api_rec_pool_create("stats_test");
    ASSERT(pool != NULL, "create pool");

    uint64_t total = 0, recovered = 0, failed = 0;
    double rate = 0.0;

    api_rec_get_stats(pool, &total, &recovered, &failed, &rate);
    ASSERT(total == 0, "initial total should be 0");
    ASSERT(recovered == 0, "initial recovered should be 0");
    ASSERT(failed == 0, "initial failed should be 0");
    ASSERT(rate == 0.0, "initial rate should be 0.0");

    api_rec_pool_destroy(pool);
    PASS();
}

static void test_get_stats_null_pool(void)
{
    TEST("Get statistics on NULL pool");
    uint64_t total = 99, recovered = 99, failed = 99;
    double rate = 99.0;

    api_rec_get_stats(NULL, &total, &recovered, &failed, &rate);
    ASSERT(total == 99, "total should be unchanged on NULL pool");
    ASSERT(recovered == 99, "recovered should be unchanged");
    ASSERT(failed == 99, "failed should be unchanged");
    ASSERT(rate == 99.0, "rate should be unchanged");

    PASS();
}

static void test_get_stats_null_output(void)
{
    TEST("Get statistics with NULL output pointers");
    api_rec_pool_t *pool = api_rec_pool_create("stats_null_test");
    ASSERT(pool != NULL, "create pool");

    api_rec_get_stats(pool, NULL, NULL, NULL, NULL);

    uint64_t total = 0;
    double rate = 0.0;
    api_rec_get_stats(pool, &total, NULL, NULL, &rate);
    ASSERT(total == 0, "total should be populated");
    ASSERT(rate == 0.0, "rate should be populated");

    api_rec_pool_destroy(pool);
    PASS();
}

/* ==================== String Conversion ==================== */

static void test_error_string(void)
{
    TEST("Error code to string conversion");
    ASSERT(strcmp(api_rec_error_string(API_REC_ERR_NONE), "none") == 0, "NONE -> none");
    ASSERT(strcmp(api_rec_error_string(API_REC_ERR_NETWORK), "network") == 0, "NETWORK -> network");
    ASSERT(strcmp(api_rec_error_string(API_REC_ERR_TIMEOUT), "timeout") == 0, "TIMEOUT -> timeout");
    ASSERT(strcmp(api_rec_error_string(API_REC_ERR_RATE_LIMIT), "rate_limit") == 0, "RATE_LIMIT -> rate_limit");
    ASSERT(strcmp(api_rec_error_string(API_REC_ERR_AUTH), "auth") == 0, "AUTH -> auth");
    ASSERT(strcmp(api_rec_error_string(API_REC_ERR_SERVER), "server") == 0, "SERVER -> server");
    ASSERT(strcmp(api_rec_error_string(API_REC_ERR_UNKNOWN), "unknown") == 0, "UNKNOWN -> unknown");
    ASSERT(strcmp(api_rec_error_string((api_rec_error_code_t)999), "unknown") == 0, "invalid -> unknown");
    PASS();
}

static void test_degradation_string(void)
{
    TEST("Degradation level to string conversion");
    ASSERT(strcmp(api_rec_degradation_string(API_REC_DEGRADE_NONE), "none") == 0, "NONE -> none");
    ASSERT(strcmp(api_rec_degradation_string(API_REC_DEGRADE_LOWER_TIER), "lower_tier") == 0, "LOWER_TIER -> lower_tier");
    ASSERT(strcmp(api_rec_degradation_string(API_REC_DEGRADE_CACHE), "cache") == 0, "CACHE -> cache");
    ASSERT(strcmp(api_rec_degradation_string((api_rec_degradation_level_t)999), "unknown") == 0, "invalid -> unknown");
    PASS();
}

/* ==================== Comprehensive Scenario Tests ==================== */

static void test_credential_health_decay_after_failures(void)
{
    TEST("Credential health decays after multiple failures");
    api_rec_pool_t *pool = api_rec_pool_create("health_decay_test");
    ASSERT(pool != NULL, "create pool");

    api_rec_add_credential(pool, "sk-decay-001");
    api_rec_next_credential(pool);

    double initial_health = api_rec_cred_health(pool, 0);
    ASSERT(initial_health == 1.0, "initial health should be 1.0");

    api_rec_mark_cred_failure(pool, API_REC_ERR_NETWORK);
    double health1 = api_rec_cred_health(pool, 0);
    ASSERT(health1 < 1.0, "health should decrease after first failure");

    api_rec_next_credential(pool);
    api_rec_mark_cred_failure(pool, API_REC_ERR_NETWORK);
    double health2 = api_rec_cred_health(pool, 0);
    ASSERT(health2 < health1, "health should decrease further after second failure");

    api_rec_pool_destroy(pool);
    PASS();
}

static void test_credential_health_recovery_on_success(void)
{
    TEST("Credential health recovers after success");
    api_rec_pool_t *pool = api_rec_pool_create("health_recovery_test");
    ASSERT(pool != NULL, "create pool");

    api_rec_add_credential(pool, "sk-recover-001");
    api_rec_next_credential(pool);
    api_rec_mark_cred_failure(pool, API_REC_ERR_NETWORK);
    double health_after_fail = api_rec_cred_health(pool, 0);

    api_rec_next_credential(pool);
    api_rec_mark_cred_success(pool);
    double health_after_success = api_rec_cred_health(pool, 0);
    ASSERT(health_after_success > health_after_fail, "health should improve after success");

    api_rec_pool_destroy(pool);
    PASS();
}

static void test_credential_auth_failure_disables(void)
{
    TEST("Auth failure disables credential");
    api_rec_pool_t *pool = api_rec_pool_create("auth_disable_test");
    ASSERT(pool != NULL, "create pool");

    api_rec_add_credential(pool, "sk-auth-disable-001");
    api_rec_next_credential(pool);

    int ret = api_rec_mark_cred_failure(pool, API_REC_ERR_AUTH);
    ASSERT(ret == 0, "mark auth failure should succeed");

    ASSERT(pool->credentials[0].is_valid == false, "credential should be disabled after auth failure");

    api_rec_pool_destroy(pool);
    PASS();
}

static void test_remove_credential_adjusts_index(void)
{
    TEST("Remove credential adjusts cred_index");
    api_rec_pool_t *pool = api_rec_pool_create("remove_index_test");
    ASSERT(pool != NULL, "create pool");

    api_rec_add_credential(pool, "sk-rm-idx-A");
    api_rec_add_credential(pool, "sk-rm-idx-B");
    api_rec_add_credential(pool, "sk-rm-idx-C");

    api_rec_next_credential(pool);
    api_rec_next_credential(pool);
    api_rec_next_credential(pool);

    api_rec_remove_credential(pool, pool->cred_count - 1);
    api_rec_next_credential(pool);

    api_rec_pool_destroy(pool);
    PASS();
}

static void test_full_workflow(void)
{
    TEST("Full API recovery workflow");
    api_rec_pool_t *pool = api_rec_pool_create("full_workflow");
    ASSERT(pool != NULL, "create pool");

    api_rec_add_credential(pool, "sk-primary-key-001");
    api_rec_add_credential(pool, "sk-backup-key-002");
    api_rec_add_fallback_model(pool, "gpt-4", 10.0f, 1);
    api_rec_add_fallback_model(pool, "gpt-3.5-turbo", 1.0f, 2);
    api_rec_set_retry_config(pool, 3, 200, 2.0f, 0.15f);

    int dummy_cb = 42;
    api_rec_bind_circuit_breaker(pool, &dummy_cb);

    const char *cred = api_rec_next_credential(pool);
    ASSERT(cred != NULL, "should get credential");
    ASSERT(strcmp(cred, "sk-primary-key-001") == 0, "first credential should be primary");

    api_rec_mark_cred_success(pool);
    ASSERT(api_rec_cred_health(pool, 0) > 0.9, "primary health should be high");

    api_rec_degrade(pool);
    api_rec_degradation_level_t level = api_rec_current_level(pool);
    ASSERT(level == API_REC_DEGRADE_LOWER_TIER, "should be LOWER_TIER after degrade");

    const char *model = api_rec_current_model(pool);
    ASSERT(strcmp(model, "gpt-3.5-turbo") == 0, "should use second fallback after degrade");

    api_rec_upgrade(pool);
    level = api_rec_current_level(pool);
    ASSERT(level == API_REC_DEGRADE_NONE, "should be back to NONE");

    uint64_t total = 0, recovered = 0, failed = 0;
    double rate = 0.0;
    api_rec_get_stats(pool, &total, &recovered, &failed, &rate);
    ASSERT(total == 0, "total should be 0");

    api_rec_pool_destroy(pool);
    PASS();
}

int main(void)
{
    printf("\n=== API Recovery Module Unit Tests ===\n\n");

    test_pool_create_null_name();
    test_pool_create_with_name();
    test_pool_destroy_null_safe();
    test_pool_multiple_create_destroy();
    test_add_credential();
    test_add_credential_null_pool();
    test_add_credential_null_key();
    test_add_credential_overflow();
    test_remove_credential();
    test_remove_credential_null_pool();
    test_remove_credential_out_of_range();
    test_next_credential_rotation();
    test_next_credential_null_pool();
    test_next_credential_empty_pool();
    test_mark_cred_success();
    test_mark_cred_failure();
    test_mark_cred_success_null_pool();
    test_mark_cred_failure_null_pool();
    test_mark_cred_empty_pool();
    test_cred_health();
    test_cred_health_null_pool();
    test_cred_health_out_of_range();
    test_add_fallback_model();
    test_add_fallback_model_null_pool();
    test_add_fallback_model_null_name();
    test_add_fallback_model_overflow();
    test_current_model_primary();
    test_current_model_null_pool();
    test_degrade_upgrade_cycle();
    test_degrade_null_pool();
    test_upgrade_null_pool();
    test_degrade_without_fallbacks();
    test_upgrade_at_top();
    test_current_level_null_pool();
    test_set_retry_config();
    test_set_retry_config_null_pool();
    test_set_retry_config_edge_values();
    test_bind_circuit_breaker();
    test_bind_circuit_breaker_null_pool();
    test_get_stats();
    test_get_stats_null_pool();
    test_get_stats_null_output();
    test_error_string();
    test_degradation_string();
    test_credential_health_decay_after_failures();
    test_credential_health_recovery_on_success();
    test_credential_auth_failure_disables();
    test_remove_credential_adjusts_index();
    test_full_workflow();

    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
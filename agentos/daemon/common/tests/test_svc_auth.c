/**
 * @file test_svc_auth.c
 * @brief 认证中间件单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "svc_auth.h"
#include "error.h"

/* ==================== 测试辅助宏 ==================== */

#define TEST_ASSERT(condition, msg) \
    do { \
        if (!(condition)) { \
            printf("  ✗ FAIL: %s (line %d)\n", msg, __LINE__); \
            return -1; \
        } \
    } while(0)

#define TEST_PASS(msg) printf("  ✓ PASS: %s\n", msg)

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define RUN_TEST(test_func) \
    do { \
        printf("\n[TEST] %s\n", #test_func); \
        int ret = test_func(); \
        if (ret == 0) { g_tests_passed++; } \
        else { g_tests_failed++; } \
    } while(0)

/* ==================== JWT 测试 ==================== */

/**
 * @brief 测试 JWT 初始化
 */
int test_jwt_init(void) {
    jwt_config_t config = {
        .secret = "test-secret-key-for-unit-testing",
        .secret_len = 35,
        .token_ttl_sec = 3600,
        .refresh_threshold_sec = 300,
        .issuer = "agentos-test"
    };

    int ret = auth_jwt_init(&config);
    TEST_ASSERT(ret == AUTH_SUCCESS, "JWT init should succeed");
    TEST_PASS("JWT initialization");

    /* 清理 */
    auth_jwt_cleanup();
    return 0;
}

/**
 * @brief 测试 JWT Token 生成
 */
int test_jwt_generate_token(void) {
    /* 先初始化 */
    jwt_config_t config = {
        .secret = "test-secret-key",
        .secret_len = 16,
        .token_ttl_sec = 3600,
        .refresh_threshold_sec = 300,
        .issuer = "agentos-test"
    };
    auth_jwt_init(&config);

    char* token = NULL;
    int ret = auth_jwt_generate_token("user-001", "admin", &token);

    TEST_ASSERT(ret == AUTH_SUCCESS, "Token generation should succeed");
    TEST_ASSERT(token != NULL, "Token should not be NULL");
    TEST_ASSERT(strlen(token) > 0, "Token should not be empty");

    if (token) {
        TEST_PASS("Token generation with valid parameters");
        free(token);
    }

    /* 测试无效参数 */
    ret = auth_jwt_generate_token(NULL, "admin", &token);
    TEST_ASSERT(ret != AUTH_SUCCESS, "Should fail with NULL subject");
    TEST_PASS("Token generation rejects NULL subject");

    auth_jwt_cleanup();
    return 0;
}

/**
 * @brief 测试 JWT Token 验证
 */
int test_jwt_verify_token(void) {
    /* 初始化 */
    jwt_config_t config = {
        .secret = "test-verify-secret",
        .secret_len = 19,
        .token_ttl_sec = 3600,
        .refresh_threshold_sec = 300,
        .issuer = "agentos-test"
    };
    auth_jwt_init(&config);

    /* 生成 Token */
    char* token = NULL;
    auth_jwt_generate_token("user-002", "user", &token);
    TEST_ASSERT(token != NULL, "Token should be generated");

    /* 验证有效 Token */
    auth_result_t result;
    int ret = auth_jwt_verify_token(token, &result);
    TEST_ASSERT(ret == AUTH_SUCCESS, "Valid token should be verified");
    TEST_ASSERT(result.status == AUTH_SUCCESS, "Result status should be success");
    TEST_ASSERT(strcmp(result.subject, "user-002") == 0, "Subject should match");
    TEST_PASS("Token verification for valid token");

    /* 验证无效 Token */
    ret = auth_jwt_verify_token("invalid.token.here", &result);
    TEST_ASSERT(ret != AUTH_SUCCESS, "Invalid token should fail verification");
    TEST_PASS("Token verification rejects invalid token");

    /* 验证空 Token */
    ret = auth_jwt_verify_token(NULL, &result);
    TEST_ASSERT(ret != AUTH_SUCCESS, "NULL token should fail");
    TEST_PASS("Token verification rejects NULL token");

    if (token) free(token);
    auth_jwt_cleanup();
    return 0;
}

/**
 * @brief 测试 JWT Token 刷新
 */
int test_jwt_refresh_token(void) {
    /* 初始化 */
    jwt_config_t config = {
        .secret = "test-refresh-secret",
        .secret_len = 20,
        .token_ttl_sec = 3600,
        .refresh_threshold_sec = 300,
        .issuer = "agentos-test"
    };
    auth_jwt_init(&config);

    /* 生成旧 Token */
    char* old_token = NULL;
    auth_jwt_generate_token("user-003", "agent", &old_token);
    TEST_ASSERT(old_token != NULL, "Old token should be generated");

    /* 等待1秒确保时间戳不同 */
    sleep(1);

    /* 刷新 Token */
    char* new_token = NULL;
    int ret = auth_jwt_refresh_token(old_token, &new_token);
    TEST_ASSERT(ret == AUTH_SUCCESS, "Token refresh should succeed");
    TEST_ASSERT(new_token != NULL, "New token should not be NULL");
    TEST_ASSERT(strcmp(old_token, new_token) != 0, "Tokens should be different");
    TEST_PASS("Token refresh generates new token");

    /* 验证新 Token */
    auth_result_t result;
    ret = auth_jwt_verify_token(new_token, &result);
    TEST_ASSERT(ret == AUTH_SUCCESS, "Refreshed token should be valid");
    TEST_PASS("Refreshed token is valid");

    if (old_token) free(old_token);
    if (new_token) free(new_token);
    auth_jwt_cleanup();
    return 0;
}

/* ==================== API Key 测试 ==================== */

/**
 * @brief 测试 API Key 初始化和验证
 */
int test_apikey_init_and_verify(void) {
    const char* allowed_keys[] = {
        "apikey-1234567890",
        "apikey-abcdef1234",
        "apikey-testkey999"
    };

    apikey_config_t config = {
        .allowed_keys = allowed_keys,
        .key_count = 3,
        .enable_key_rotation = false
    };

    int ret = auth_apikey_init(&config);
    TEST_ASSERT(ret == AUTH_SUCCESS, "API Key init should succeed");
    TEST_PASS("API Key initialization");

    /* 验证有效的 Key */
    auth_result_t result;
    ret = auth_apikey_verify("apikey-1234567890", &result);
    TEST_ASSERT(ret == AUTH_SUCCESS, "Valid API key should be verified");
    TEST_ASSERT(result.status == AUTH_SUCCESS, "Result should indicate success");
    TEST_PASS("API Key verification for valid key");

    /* 验证无效的 Key */
    ret = auth_apikey_verify("invalid-key", &result);
    TEST_ASSERT(ret == AUTH_APIKEY_INVALID, "Invalid API key should fail");
    TEST_PASS("API Key verification rejects invalid key");

    /* 验证空 Key */
    ret = auth_apikey_verify(NULL, &result);
    TEST_ASSERT(ret != AUTH_SUCCESS, "NULL key should fail");
    TEST_PASS("API Key verification rejects NULL key");

    auth_apikey_cleanup();
    return 0;
}

/**
 * @brief 测试 API Key 动态添加和移除
 */
int test_apikey_add_remove(void) {
    const char* initial_keys[] = {"initial-key-1"};
    
    apikey_config_t config = {
        .allowed_keys = initial_keys,
        .key_count = 1,
        .enable_key_rotation = true
    };
    auth_apikey_init(&config);

    /* 添加新 Key */
    int ret = auth_apikey_add("new-key-12345");
    TEST_ASSERT(ret == AUTH_SUCCESS, "Adding new key should succeed");
    TEST_PASS("API Key addition");

    /* 验证新添加的 Key */
    auth_result_t result;
    ret = auth_apikey_verify("new-key-12345", &result);
    TEST_ASSERT(ret == AUTH_SUCCESS, "Newly added key should work");
    TEST_PASS("Verification of newly added key");

    /* 添加重复的 Key */
    ret = auth_apikey_add("new-key-12345");
    TEST_ASSERT(ret == AGENTOS_ERR_ALREADY_EXISTS, "Duplicate key should fail");
    TEST_PASS("API Key duplicate rejection");

    /* 移除 Key */
    ret = auth_apikey_remove("new-key-12345");
    TEST_ASSERT(ret == AUTH_SUCCESS, "Key removal should succeed");
    TEST_PASS("API Key removal");

    /* 验证已移除的 Key */
    ret = auth_apikey_verify("new-key-12345", &result);
    TEST_ASSERT(ret == AUTH_APIKEY_INVALID, "Removed key should fail");
    TEST_PASS("Verification of removed key fails");

    auth_apikey_cleanup();
    return 0;
}

/* ==================== 速率限制测试 ==================== */

/**
 * @brief 测试速率限制器初始化和检查
 */
int test_ratelimit_init_and_check(void) {
    rate_limit_config_t config = {
        .requests_per_sec = 10,
        .burst_size = 5,
        .max_clients = 100
    };

    int ret = auth_ratelimit_init(&config);
    TEST_ASSERT(ret == AUTH_SUCCESS, "Rate limiter init should succeed");
    TEST_PASS("Rate limiter initialization");

    /* 允许请求（在限制内） */
    for (int i = 0; i < 5; i++) {
        ret = auth_ratelimit_check("client-001");
        TEST_ASSERT(ret == AUTH_SUCCESS, "Request within limit should be allowed");
    }
    TEST_PASS("Requests within burst limit are allowed");

    /* 超出突发大小后应该拒绝（或接近拒绝） */
    bool exceeded = false;
    for (int i = 0; i < 10; i++) {
        ret = auth_ratelimit_check("client-001");
        if (ret == AUTH_RATE_LIMIT_EXCEEDED) {
            exceeded = true;
            break;
        }
    }
    TEST_ASSERT(exceeded, "Should eventually exceed rate limit");
    TEST_PASS("Rate limit enforcement works");

    auth_ratelimit_cleanup();
    return 0;
}

/**
 * @brief 测试速率限制统计信息
 */
int test_ratelimit_stats(void) {
    rate_limit_config_t config = {
        .requests_per_sec = 100,
        .burst_size = 50,
        .max_clients = 100
    };
    auth_ratelimit_init(&config);

    /* 消耗一些令牌 */
    for (int i = 0; i < 5; i++) {
        auth_ratelimit_check("client-stats");
    }

    /* 获取统计信息 */
    uint32_t remaining = 0;
    int64_t reset_time = 0;
    int ret = auth_ratelimit_get_stats("client-stats", &remaining, &reset_time);
    TEST_ASSERT(ret == AUTH_SUCCESS, "Stats retrieval should succeed");
    TEST_ASSERT(remaining > 0 && remaining <= 50, "Remaining should be in valid range");
    TEST_PASS("Rate limit stats retrieval");

    /* 重置计数器 */
    ret = auth_ratelimit_reset("client-stats");
    TEST_ASSERT(ret == AUTH_SUCCESS, "Reset should succeed");
    TEST_PASS("Rate limit reset");

    auth_ratelimit_cleanup();
    return 0;
}

/* ==================== 统一认证入口测试 ==================== */

/**
 * @brief 测试统一认证流程
 */
int test_unified_authenticate(void) {
    auth_config_t config = {
        .jwt.secret = "unified-auth-secret",
        .jwt.secret_len = 21,
        .jwt.token_ttl_sec = 3600,
        .jwt.refresh_threshold_sec = 300,
        .jwt.issuer = "agentos-unified",
        .apikey.allowed_keys = (const char*[]){"unified-api-key"},
        .apikey.key_count = 1,
        .ratelimit.requests_per_sec = 100,
        .ratelimit.burst_size = 20,
        .enable_jwt = true,
        .enable_apikey = true,
        .enable_ratelimit = true
    };

    int ret = auth_init(&config);
    TEST_ASSERT(ret == AUTH_SUCCESS, "Unified auth init should succeed");
    TEST_PASS("Unified authentication initialization");

    /* 测试 Bearer Token 认证 */
    char* token = NULL;
    auth_jwt_generate_token("unified-user", "admin", &token);
    TEST_ASSERT(token != NULL, "Token should be generated");

    char bearer_header[512];
    snprintf(bearer_header, sizeof(bearer_header), "Bearer %s", token);

    auth_result_t result;
    ret = auth_authenticate(bearer_header, "test-client", &result);
    TEST_ASSERT(ret == AUTH_SUCCESS, "Bearer token authentication should succeed");
    TEST_PASS("Bearer token authentication via unified entry");

    free(token);

    /* 测试 API Key 认证 */
    const char* apikey_header = "ApiKey unified-api-key";
    ret = auth_authenticate(apikey_header, "test-client-2", &result);
    TEST_ASSERT(ret == AUTH_SUCCESS, "API Key authentication should succeed");
    TEST_PASS("API Key authentication via unified entry");

    /* 测试无效认证头 */
    ret = auth_authenticate("InvalidScheme invalid-token", "test-client-3", &result);
    TEST_ASSERT(ret == AUTH_MISSING_CREDENTIALS, "Invalid scheme should fail");
    TEST_PASS("Invalid authentication header rejected");

    /* 测试空认证头 */
    ret = auth_authenticate(NULL, "test-client-4", &result);
    TEST_ASSERT(ret == AUTH_MISSING_CREDENTIALS, "NULL header should fail");
    TEST_PASS("NULL authentication header rejected");

    auth_cleanup();
    return 0;
}

/* ==================== 边界条件测试 ==================== */

/**
 * @brief 测试边界条件和错误处理
 */
int test_edge_cases(void) {
    /* 双重初始化检测 */
    jwt_config_t config = {
        .secret = "edge-case-secret",
        .secret_len = 17,
        .token_ttl_sec = 3600,
        .refresh_threshold_sec = 300,
        .issuer = "edge-test"
    };
    auth_jwt_init(&config);
    int ret = auth_jwt_init(&config);  /* 再次初始化 */
    TEST_ASSERT(ret == AGENTOS_ERR_ALREADY_INIT || ret == 0, 
              "Double init should handle gracefully");
    TEST_PASS("Double initialization handling");
    auth_jwt_cleanup();

    /* 未初始化时调用其他函数 */
    auth_result_t result;
    ret = auth_jwt_verify_token("some-token", &result);
    TEST_ASSERT(ret != AUTH_SUCCESS, "Verify without init should fail");
    TEST_PASS("Operation without initialization fails");

    /* 空配置初始化 */
    ret = auth_jwt_init(NULL);
    TEST_ASSERT(ret != AUTH_SUCCESS, "NULL config should fail");
    TEST_PASS("NULL configuration handling");

    return 0;
}

/* ==================== 主函数 ==================== */

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    printf("\n");
    printf("======================================================\n");
    printf("  AgentOS Daemon Authentication Module Unit Tests\n");
    printf("======================================================\n");

    /* JWT 测试 */
    RUN_TEST(test_jwt_init);
    RUN_TEST(test_jwt_generate_token);
    RUN_TEST(test_jwt_verify_token);
    RUN_TEST(test_jwt_refresh_token);

    /* API Key 测试 */
    RUN_TEST(test_apikey_init_and_verify);
    RUN_TEST(test_apikey_add_remove);

    /* 速率限制测试 */
    RUN_TEST(test_ratelimit_init_and_check);
    RUN_TEST(test_ratelimit_stats);

    /* 统一认证测试 */
    RUN_TEST(test_unified_authenticate);

    /* 边界条件测试 */
    RUN_TEST(test_edge_cases);

    /* 结果汇总 */
    printf("\n");
    printf("======================================================\n");
    printf("  Test Results: %d passed, %d failed\n", 
           g_tests_passed, g_tests_failed);
    printf("======================================================\n\n");

    return g_tests_failed > 0 ? 1 : 0;
}

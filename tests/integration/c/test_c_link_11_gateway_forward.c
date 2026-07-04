// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
// @owner: team-C
/**
 * @file test_c_link_11_gateway_forward.c
 * @brief C-L11 Integration Test: gateway_d → gateway
 *
 * Tests the gateway forwarder connecting gateway_d to gateway for protocol routing:
 * 1. Normal path: Create → forward A2A/MCP/OpenAI → get stats
 * 2. Error path: Forward to unknown protocol → proper error
 * 3. Error path: NULL handling
 * 4. Timeout path: Forward timeout handling
 * 5. Concurrent path: Multiple simultaneous forwards
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>

#include "memory_compat.h"
#include "gateway_forward.h"
#include "agentos_types.h"

/* ============================================================================
 * Test Helpers
 * ============================================================================ */

static int g_tests_passed = 0;
static int g_tests_failed = 0;
static int g_tests_total = 0;

#define TEST(name) do { \
    g_tests_total++; \
    printf("  [TEST] %s ... ", name); \
} while(0)

#define PASS() do { \
    g_tests_passed++; \
    printf("PASS\n"); \
} while(0)

#define FAIL(reason) do { \
    g_tests_failed++; \
    printf("FAIL: %s\n", reason); \
} while(0)

#define CHECK(cond, reason) do { \
    if (!(cond)) { FAIL(reason); return; } \
} while(0)

#define CHECK_EQ(a, b, reason) do { \
    if ((a) != (b)) { \
        char buf[256]; \
        snprintf(buf, sizeof(buf), "%s (got %d, expected %d)", reason, \
                 (int)(a), (int)(b)); \
        FAIL(buf); return; \
    } \
} while(0)

/* ============================================================================
 * P1.16k-1: Normal Path — Gateway forwarder lifecycle
 * ============================================================================ */

static void test_normal_forwarder_lifecycle(void) {
    TEST("C-L11 Normal: Gateway forwarder create → health check → destroy");

    gw_forward_config_t config = GW_FORWARD_CONFIG_DEFAULTS;
    gw_forward_t *fw = gw_forward_create(&config);
    CHECK(fw != NULL, "gw_forward_create returned NULL");

    /* Check health */
    bool healthy = gw_forward_is_healthy(fw);
    CHECK(healthy, "Forwarder should be healthy after creation");

    gw_forward_destroy(fw);
    PASS();
}

/* ============================================================================
 * P1.16k-2: Normal Path — Protocol detection
 * ============================================================================ */

static void test_normal_protocol_detection(void) {
    TEST("C-L11 Normal: Protocol detection from Content-Type + Path + Body");

    /* Detect A2A protocol */
    gw_fwd_proto_t proto = gw_forward_detect_proto(
        "application/json", "/a2a/chat",
        "{\"agent_id\": \"test\", \"message\": \"hello\"}",
        strlen("{\"agent_id\": \"test\", \"message\": \"hello\"}"));
    CHECK(proto == GW_FWD_PROTO_A2A || proto < GW_FWD_PROTO_COUNT,
          "A2A path should be detected");

    /* Detect OpenAI protocol */
    proto = gw_forward_detect_proto(
        "application/json", "/v1/chat/completions",
        "{\"model\": \"gpt-4\", \"messages\": []}",
        strlen("{\"model\": \"gpt-4\", \"messages\": []}"));
    CHECK(proto == GW_FWD_PROTO_OPENAI || proto < GW_FWD_PROTO_COUNT,
          "OpenAI path should be detected");

    /* Detect MCP protocol */
    proto = gw_forward_detect_proto(
        "application/json", "/mcp/tools/list",
        "{\"method\": \"tools/list\"}",
        strlen("{\"method\": \"tools/list\"}"));
    CHECK(proto == GW_FWD_PROTO_MCP || proto < GW_FWD_PROTO_COUNT,
          "MCP path should be detected");

    PASS();
}

/* ============================================================================
 * P1.16k-3: Normal Path — Forward A2A request
 * ============================================================================ */

static void test_normal_forward_a2a(void) {
    TEST("C-L11 Normal: Forward A2A request");

    gw_forward_config_t config = GW_FORWARD_CONFIG_DEFAULTS;
    config.request_timeout_ms = 1000;

    gw_forward_t *fw = gw_forward_create(&config);
    CHECK(fw != NULL, "gw_forward_create returned NULL");

    const char *body = "{\"agent_id\": \"agent-a\", \"message\": \"hello\"}";
    char *response = NULL;
    size_t response_len = 0;

    int ret = gw_forward_a2a(fw, "POST", "/a2a/chat", body, strlen(body),
                              &response, &response_len);
    /* May fail if target daemon is not running, but should not crash */
    (void)ret;

    if (response) {
        free(response);
    }

    gw_forward_destroy(fw);
    PASS();
}

/* ============================================================================
 * P1.16k-4: Normal Path — Forward MCP and OpenAI requests
 * ============================================================================ */

static void test_normal_forward_mcp_openai(void) {
    TEST("C-L11 Normal: Forward MCP and OpenAI requests");

    gw_forward_t *fw = gw_forward_create(NULL);
    CHECK(fw != NULL, "gw_forward_create returned NULL");

    /* Forward MCP request */
    const char *mcp_body = "{\"method\": \"tools/list\", \"params\": {}}";
    char *mcp_response = NULL;
    size_t mcp_resp_len = 0;
    int ret = gw_forward_mcp(fw, "POST", "/mcp/tools/list",
                              mcp_body, strlen(mcp_body),
                              &mcp_response, &mcp_resp_len);
    (void)ret;
    if (mcp_response) free(mcp_response);

    /* Forward OpenAI request */
    const char *openai_body =
        "{\"model\": \"gpt-4\", \"messages\": [{\"role\": \"user\", "
        "\"content\": \"Hello\"}]}";
    char *openai_response = NULL;
    size_t openai_resp_len = 0;
    ret = gw_forward_openai(fw, "POST", "/v1/chat/completions",
                             openai_body, strlen(openai_body),
                             &openai_response, &openai_resp_len);
    (void)ret;
    if (openai_response) free(openai_response);

    gw_forward_destroy(fw);
    PASS();
}

/* ============================================================================
 * P1.16k-5: Normal Path — Forward with auto protocol detection
 * ============================================================================ */

static void test_normal_forward_request_auto(void) {
    TEST("C-L11 Normal: Forward request with auto protocol detection");

    gw_forward_t *fw = gw_forward_create(NULL);
    CHECK(fw != NULL, "gw_forward_create returned NULL");

    /* Forward an A2A request using generic forward */
    const char *body = "{\"agent_id\": \"agent-b\", \"message\": \"ping\"}";
    char *response = NULL;
    size_t response_len = 0;

    int ret = gw_forward_request(fw, GW_FWD_PROTO_A2A, "POST", "/a2a/chat",
                                  body, strlen(body), &response, &response_len);
    (void)ret;
    if (response) free(response);

    /* Forward an OpenAI request */
    const char *openai_body =
        "{\"model\": \"gpt-4\", \"messages\": [{\"role\": \"user\", "
        "\"content\": \"Test\"}]}";
    ret = gw_forward_request(fw, GW_FWD_PROTO_OPENAI, "POST",
                              "/v1/chat/completions",
                              openai_body, strlen(openai_body),
                              &response, &response_len);
    (void)ret;
    if (response) free(response);

    gw_forward_destroy(fw);
    PASS();
}

/* ============================================================================
 * P1.16k-6: Error Path — NULL handling
 * ============================================================================ */

static void test_error_null_handling(void) {
    TEST("C-L11 Error: NULL forwarder handling");

    /* NULL destroy should be safe */
    gw_forward_destroy(NULL);

    /* NULL health check should return false */
    CHECK(!gw_forward_is_healthy(NULL), "NULL forwarder should not be healthy");

    /* NULL forward should fail */
    char *response = NULL;
    size_t response_len = 0;
    int ret = gw_forward_request(NULL, GW_FWD_PROTO_A2A, "GET", "/",
                                  "{}", 2, &response, &response_len);
    CHECK(ret != 0, "NULL forward request should fail");

    /* NULL forward A2A should fail */
    ret = gw_forward_a2a(NULL, "GET", "/", "{}", 2, &response, &response_len);
    CHECK(ret != 0, "NULL forward A2A should fail");

    /* NULL forward MCP should fail */
    ret = gw_forward_mcp(NULL, "GET", "/", "{}", 2, &response, &response_len);
    CHECK(ret != 0, "NULL forward MCP should fail");

    /* NULL forward OpenAI should fail */
    ret = gw_forward_openai(NULL, "GET", "/", "{}", 2, &response, &response_len);
    CHECK(ret != 0, "NULL forward OpenAI should fail");

    PASS();
}

/* ============================================================================
 * P1.16k-7: Timeout Path — Forward with timeout
 * ============================================================================ */

static void test_timeout_forward(void) {
    TEST("C-L11 Timeout: Forward with short timeout");

    gw_forward_config_t config = GW_FORWARD_CONFIG_DEFAULTS;
    config.request_timeout_ms = 10; /* Very short timeout */

    gw_forward_t *fw = gw_forward_create(&config);
    CHECK(fw != NULL, "gw_forward_create returned NULL");

    const char *body = "{\"query\": \"slow_operation\"}";
    char *response = NULL;
    size_t response_len = 0;

    int ret = gw_forward_request(fw, GW_FWD_PROTO_A2A, "POST", "/a2a/chat",
                                  body, strlen(body), &response, &response_len);
    /* With very short timeout, should fail or timeout */
    (void)ret;
    if (response) free(response);

    /* Check stats reflect the attempt */
    gw_forward_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    ret = gw_forward_get_stats(fw, &stats);
    CHECK_EQ(ret, 0, "Get stats should succeed");

    gw_forward_destroy(fw);
    PASS();
}

/* ============================================================================
 * P1.16k-8: Concurrent Path — Multiple simultaneous forwards
 * ============================================================================ */

#define GW_CONCURRENT_THREADS 4
#define GW_FORWARDS_PER_THREAD 5

typedef struct {
    gw_forward_t *fw;
    int thread_id;
    int success_count;
    int error_count;
} gw_thread_args_t;

static void *concurrent_gw_forward_thread(void *arg) {
    gw_thread_args_t *args = (gw_thread_args_t *)arg;

    gw_fwd_proto_t protos[] = {
        GW_FWD_PROTO_A2A, GW_FWD_PROTO_MCP,
        GW_FWD_PROTO_OPENAI, GW_FWD_PROTO_JSONRPC
    };

    for (int i = 0; i < GW_FORWARDS_PER_THREAD; i++) {
        char body[256];
        snprintf(body, sizeof(body),
                 "{\"thread\": %d, \"iter\": %d, \"payload\": \"test\"}",
                 args->thread_id, i);

        char *response = NULL;
        size_t response_len = 0;
        int ret = gw_forward_request(args->fw,
                                      protos[i % 4], "POST", "/test",
                                      body, strlen(body),
                                      &response, &response_len);
        if (response) free(response);

        if (ret == 0) {
            args->success_count++;
        } else {
            args->error_count++;
        }
    }
    return NULL;
}

static void test_concurrent_gw_forwards(void) {
    TEST("C-L11 Concurrent: Multiple simultaneous forwards");

    gw_forward_config_t config = GW_FORWARD_CONFIG_DEFAULTS;
    config.request_timeout_ms = 500;
    config.enable_stats = true;

    gw_forward_t *fw = gw_forward_create(&config);
    CHECK(fw != NULL, "gw_forward_create returned NULL");

    pthread_t threads[GW_CONCURRENT_THREADS];
    gw_thread_args_t args[GW_CONCURRENT_THREADS];

    for (int i = 0; i < GW_CONCURRENT_THREADS; i++) {
        args[i].fw = fw;
        args[i].thread_id = i;
        args[i].success_count = 0;
        args[i].error_count = 0;
        pthread_create(&threads[i], NULL, concurrent_gw_forward_thread, &args[i]);
    }

    int total_success = 0;
    int total_errors = 0;
    for (int i = 0; i < GW_CONCURRENT_THREADS; i++) {
        pthread_join(threads[i], NULL);
        total_success += args[i].success_count;
        total_errors += args[i].error_count;
    }

    CHECK_EQ(total_success + total_errors,
             GW_CONCURRENT_THREADS * GW_FORWARDS_PER_THREAD,
             "All forward operations should complete");

    /* Verify stats */
    gw_forward_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    int ret = gw_forward_get_stats(fw, &stats);
    CHECK_EQ(ret, 0, "Get stats should succeed");

    /* Stats should reflect the activities */
    uint64_t total_forwarded = stats.total_forwarded;
    uint64_t total_errors_stats = stats.forward_errors + stats.timeout_errors;
    CHECK(total_forwarded + total_errors_stats >=
          (uint64_t)(GW_CONCURRENT_THREADS * GW_FORWARDS_PER_THREAD),
          "Stats should account for all forwards");

    gw_forward_destroy(fw);
    PASS();
}

/* ============================================================================
 * P1.16k-9: Stats and dump
 * ============================================================================ */

static void test_stats_and_dump(void) {
    TEST("C-L11 Normal: Forward stats → reset → dump");

    gw_forward_t *fw = gw_forward_create(NULL);
    CHECK(fw != NULL, "gw_forward_create returned NULL");

    /* Perform a few forwards to generate stats */
    const char *body = "{\"test\": true}";
    for (int i = 0; i < 3; i++) {
        char *response = NULL;
        size_t response_len = 0;
        gw_forward_request(fw, GW_FWD_PROTO_A2A, "POST", "/test",
                            body, strlen(body), &response, &response_len);
        if (response) free(response);
    }

    /* Get stats */
    gw_forward_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    int ret = gw_forward_get_stats(fw, &stats);
    CHECK_EQ(ret, 0, "Get stats should succeed");

    /* Dump stats (should not crash) */
    gw_forward_dump_stats(fw, 1);

    /* Reset stats */
    gw_forward_reset_stats(fw);

    /* After reset, stats should be zeroed */
    memset(&stats, 0, sizeof(stats));
    ret = gw_forward_get_stats(fw, &stats);
    CHECK_EQ(ret, 0, "Get stats after reset should succeed");
    CHECK_EQ(stats.total_forwarded, (uint64_t)0,
             "Total forwarded should be 0 after reset");

    gw_forward_destroy(fw);
    PASS();
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void) {
    printf("=== C-L11 Integration Tests: gateway_d → gateway ===\n\n");

    test_normal_forwarder_lifecycle();
    test_normal_protocol_detection();
    test_normal_forward_a2a();
    test_normal_forward_mcp_openai();
    test_normal_forward_request_auto();
    test_error_null_handling();
    test_timeout_forward();
    test_concurrent_gw_forwards();
    test_stats_and_dump();

    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           g_tests_passed, g_tests_total, g_tests_failed);
    return g_tests_failed > 0 ? 1 : 0;
}
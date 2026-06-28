// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: Apache-2.0
// @owner: team-C
/**
 * @file test_c_link_09_ipc_bus.c
 * @brief C-L09 Integration Test: IPC Bus → all daemons
 *
 * Tests the IPC Bus helper connecting all daemons via message routing:
 * 1. Normal path: Init → register channel → send message → shutdown
 * 2. Error path: Send to non-existent target → proper error
 * 3. Error path: NULL handling
 * 4. Timeout path: Request timeout handling
 * 5. Concurrent path: Multiple channels and messages
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>

#include "memory_compat.h"
#include "ipc_bus_helper.h"
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
 * Message handler for testing
 * ============================================================================ */

static int g_handler_call_count = 0;

static int test_message_handler(ipc_bus_channel_t channel, const ipc_bus_message_t *msg,
                                             void *user_data) {
    (void)channel;
    (void)msg;
    (void)user_data;
    g_handler_call_count++;
    return 0;
}

/* ============================================================================
 * P1.16i-1: Normal Path — IPC Bus helper lifecycle
 * ============================================================================ */

static void test_normal_ipc_bus_lifecycle(void) {
    TEST("C-L09 Normal: IPC Bus helper init → register channel → shutdown");

    ipc_bus_helper_t *ibh = ipc_bus_helper_init("test_daemon", NULL);
    CHECK(ibh != NULL, "ipc_bus_helper_init returned NULL");

    CHECK(ipc_bus_helper_is_running(ibh), "IPC Bus should be running after init");

    /* Register a channel for this daemon */
    int ret = ipc_bus_helper_register_channel(ibh, "test", IPC_BUS_PROTO_JSON_RPC);
    CHECK_EQ(ret, 0, "Register channel should succeed");

    /* Register a message handler */
    g_handler_call_count = 0;
    ret = ipc_bus_helper_register_handler(ibh, test_message_handler, NULL);
    CHECK_EQ(ret, 0, "Register handler should succeed");

    ipc_bus_helper_shutdown(ibh);
    PASS();
}

/* ============================================================================
 * P1.16i-2: Normal Path — Register endpoint and send message
 * ============================================================================ */

static void test_normal_register_endpoint(void) {
    TEST("C-L09 Normal: Register endpoint → send notification");

    ipc_bus_helper_t *ibh = ipc_bus_helper_init("endpoint_daemon", NULL);
    CHECK(ibh != NULL, "ipc_bus_helper_init returned NULL");

    /* Register channel */
    int ret = ipc_bus_helper_register_channel(ibh, "endpoint", IPC_BUS_PROTO_JSON_RPC);
    CHECK_EQ(ret, 0, "Register channel should succeed");

    /* Register endpoint */
    ipc_bus_proto_t protocols[] = { IPC_BUS_PROTO_JSON_RPC, IPC_BUS_PROTO_MCP };
    ret = ipc_bus_helper_register_endpoint(ibh, "endpoint_svc",
                                            "127.0.0.1:9000",
                                            protocols, 2);
    CHECK_EQ(ret, 0, "Register endpoint should succeed");

    /* Send a notification (fire-and-forget) */
    const char *payload = "{\"type\": \"heartbeat\"}";
    ret = ipc_bus_helper_notify(ibh, "endpoint_svc", payload, strlen(payload),
                                 IPC_BUS_PROTO_JSON_RPC);
    /* May succeed or fail depending on target availability */
    (void)ret;

    ipc_bus_helper_shutdown(ibh);
    PASS();
}

/* ============================================================================
 * P1.16i-3: Normal Path — Multiple channels and auto route
 * ============================================================================ */

static void test_normal_multiple_channels(void) {
    TEST("C-L09 Normal: Multiple channels → auto route");

    ipc_bus_helper_t *ibh = ipc_bus_helper_init("multi_channel_daemon", NULL);
    CHECK(ibh != NULL, "ipc_bus_helper_init returned NULL");

    /* Register multiple channels */
    const char *channels[] = { "llm", "tool", "agent", "market" };
    ipc_bus_proto_t protos[] = {
        IPC_BUS_PROTO_JSON_RPC,
        IPC_BUS_PROTO_MCP,
        IPC_BUS_PROTO_A2A,
        IPC_BUS_PROTO_OPENAI
    };

    for (int i = 0; i < 4; i++) {
        int ret = ipc_bus_helper_register_channel(ibh, channels[i], protos[i]);
        CHECK_EQ(ret, 0, "Register channel should succeed");
    }

    /* Try auto route to a service */
    const char *payload = "{\"query\": \"test\"}";
    int ret = ipc_bus_helper_route_auto(ibh, "llm_d", payload, strlen(payload));
    /* May succeed or fail depending on target availability */
    (void)ret;

    ipc_bus_helper_shutdown(ibh);
    PASS();
}

/* ============================================================================
 * P1.16i-4: Error Path — NULL handling
 * ============================================================================ */

static void test_error_null_handling(void) {
    TEST("C-L09 Error: NULL IPC Bus helper handling");

    /* NULL shutdown should be safe */
    ipc_bus_helper_shutdown(NULL);

    /* NULL is_running should return false */
    CHECK(!ipc_bus_helper_is_running(NULL), "NULL ibh should not be running");

    /* NULL register channel should fail */
    int ret = ipc_bus_helper_register_channel(NULL, "test", IPC_BUS_PROTO_JSON_RPC);
    CHECK(ret != 0, "NULL ibh register channel should fail");

    /* NULL register handler should fail */
    ret = ipc_bus_helper_register_handler(NULL, test_message_handler, NULL);
    CHECK(ret != 0, "NULL ibh register handler should fail");

    /* NULL send should fail */
    ret = ipc_bus_helper_send(NULL, "target", IPC_BUS_MSG_REQUEST,
                               IPC_BUS_PROTO_JSON_RPC, "data", 4);
    CHECK(ret != 0, "NULL ibh send should fail");

    PASS();
}

/* ============================================================================
 * P1.16i-5: Error Path — Invalid channel operations
 * ============================================================================ */

static void test_error_invalid_channel(void) {
    TEST("C-L09 Error: Invalid channel operations");

    ipc_bus_helper_t *ibh = ipc_bus_helper_init("invalid_daemon", NULL);
    CHECK(ibh != NULL, "ipc_bus_helper_init returned NULL");

    /* Register with NULL channel name should fail */
    int ret = ipc_bus_helper_register_channel(ibh, NULL, IPC_BUS_PROTO_JSON_RPC);
    CHECK(ret != 0, "NULL channel name should fail");

    /* Register with NULL handler should fail */
    ret = ipc_bus_helper_register_handler(ibh, NULL, NULL);
    CHECK(ret != 0, "NULL handler should fail");

    /* Send to NULL target should fail */
    ret = ipc_bus_helper_send(ibh, NULL, IPC_BUS_MSG_REQUEST,
                               IPC_BUS_PROTO_JSON_RPC, "data", 4);
    CHECK(ret != 0, "Send to NULL target should fail");

    ipc_bus_helper_shutdown(ibh);
    PASS();
}

/* ============================================================================
 * P1.16i-6: Timeout Path — Request timeout handling
 * ============================================================================ */

static void test_timeout_request(void) {
    TEST("C-L09 Timeout: Request with timeout");

    ipc_bus_helper_t *ibh = ipc_bus_helper_init("timeout_daemon", NULL);
    CHECK(ibh != NULL, "ipc_bus_helper_init returned NULL");

    int ret = ipc_bus_helper_register_channel(ibh, "timeout", IPC_BUS_PROTO_JSON_RPC);
    CHECK_EQ(ret, 0, "Register channel should succeed");

    /* Send a request with very short timeout */
    ipc_bus_message_t request;
    memset(&request, 0, sizeof(request));
    request.header.msg_type = IPC_BUS_MSG_REQUEST;
    request.header.protocol = IPC_BUS_PROTO_JSON_RPC;
    const char *payload_str = "{\"op\": \"ping\"}";
    request.payload = (void *)payload_str;
    request.payload_size = strlen(payload_str);

    ipc_bus_message_t response;
    memset(&response, 0, sizeof(response));

    ret = ipc_bus_helper_request(ibh, "non_existent_service", &request,
                                  &response, 100);
    /* Should timeout or fail since target doesn't exist */
    CHECK(ret != 0, "Request to non-existent service should fail or timeout");

    ipc_bus_helper_shutdown(ibh);
    PASS();
}

/* ============================================================================
 * P1.16i-7: Concurrent Path — Multiple bus helpers
 * ============================================================================ */

#define IPC_CONCURRENT_INSTANCES 4

static void test_concurrent_ipc_bus_helpers(void) {
    TEST("C-L09 Concurrent: Multiple IPC Bus helper instances");

    ipc_bus_helper_t *helpers[IPC_CONCURRENT_INSTANCES];
    const char *names[] = { "daemon_a", "daemon_b", "daemon_c", "daemon_d" };

    /* Create and init multiple instances */
    for (int i = 0; i < IPC_CONCURRENT_INSTANCES; i++) {
        helpers[i] = ipc_bus_helper_init(names[i], NULL);
        CHECK(helpers[i] != NULL, "ipc_bus_helper_init returned NULL");

        ipc_bus_proto_t protos[] = { IPC_BUS_PROTO_JSON_RPC };
        int ret = ipc_bus_helper_register_channel(helpers[i], names[i],
                                                   IPC_BUS_PROTO_JSON_RPC);
        CHECK_EQ(ret, 0, "Register channel should succeed");

        ret = ipc_bus_helper_register_endpoint(helpers[i], names[i],
                                                "127.0.0.1:9000",
                                                protos, 1);
        CHECK_EQ(ret, 0, "Register endpoint should succeed");

        CHECK(ipc_bus_helper_is_running(helpers[i]),
              "Helper should be running");
    }

    /* Verify all have valid bus handles */
    for (int i = 0; i < IPC_CONCURRENT_INSTANCES; i++) {
        ipc_service_bus_t bus = ipc_bus_helper_get_bus(helpers[i]);
        CHECK(bus != NULL, "Should have valid bus handle");
    }

    /* Cleanup */
    for (int i = 0; i < IPC_CONCURRENT_INSTANCES; i++) {
        ipc_bus_helper_shutdown(helpers[i]);
    }

    PASS();
}

/* ============================================================================
 * P1.16i-8: Backpressure integration
 * ============================================================================ */

static void test_backpressure_integration(void) {
    TEST("C-L09 Normal: Backpressure control enable and check");

    ipc_bus_helper_t *ibh = ipc_bus_helper_init("bp_daemon", NULL);
    CHECK(ibh != NULL, "ipc_bus_helper_init returned NULL");

    int ret = ipc_bus_helper_register_channel(ibh, "bp", IPC_BUS_PROTO_JSON_RPC);
    CHECK_EQ(ret, 0, "Register channel should succeed");

    /* Enable backpressure */
    ret = ipc_bus_helper_enable_backpressure(ibh, NULL);
    CHECK_EQ(ret, 0, "Enable backpressure should succeed");

    /* Check initial backpressure level */
    ipc_bp_level_t level = ipc_bus_helper_get_bp_level(ibh);
    CHECK_EQ(level, IPC_BP_NORMAL, "Initial backpressure should be NORMAL");

    /* Update backpressure with low queue depth */
    level = ipc_bus_helper_update_backpressure(ibh, 10);
    CHECK_EQ(level, IPC_BP_NORMAL, "Low queue depth should keep NORMAL level");

    /* Check should accept connection */
    CHECK(ipc_bus_helper_should_accept_connection(ibh),
          "Should accept connections at NORMAL level");

    /* Get backpressure stats */
    ipc_bp_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    ret = ipc_bus_helper_get_bp_stats(ibh, &stats);
    CHECK_EQ(ret, 0, "Get BP stats should succeed");

    /* Send with backpressure check */
    const char *payload = "{\"type\": \"test\"}";
    ret = ipc_bus_helper_send_with_bp(ibh, "target", IPC_BUS_MSG_REQUEST,
                                       IPC_BUS_PROTO_JSON_RPC,
                                       payload, strlen(payload), false);
    /* May succeed or drop depending on target availability */
    (void)ret;

    ipc_bus_helper_shutdown(ibh);
    PASS();
}

/* ============================================================================
 * P1.16i-9: Routing statistics
 * ============================================================================ */

static void test_routing_statistics(void) {
    TEST("C-L09 Normal: Routing statistics query");

    ipc_bus_helper_t *ibh = ipc_bus_helper_init("stats_daemon", NULL);
    CHECK(ibh != NULL, "ipc_bus_helper_init returned NULL");

    int ret = ipc_bus_helper_register_channel(ibh, "stats", IPC_BUS_PROTO_JSON_RPC);
    CHECK_EQ(ret, 0, "Register channel should succeed");

    /* Get routing stats */
    uint64_t total_sends = 0, total_routes = 0, fallbacks = 0;
    uint64_t failures = 0, bp_drops = 0, bp_rejects = 0;

    ret = ipc_bus_helper_get_routing_stats(ibh, &total_sends, &total_routes,
                                            &fallbacks, &failures,
                                            &bp_drops, &bp_rejects);
    CHECK_EQ(ret, 0, "Get routing stats should succeed");

    /* Initial stats should be 0 */
    CHECK_EQ(total_sends, (uint64_t)0, "Initial total_sends should be 0");
    CHECK_EQ(failures, (uint64_t)0, "Initial failures should be 0");

    ipc_bus_helper_shutdown(ibh);
    PASS();
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void) {
    printf("=== C-L09 Integration Tests: IPC Bus → all daemons ===\n\n");

    test_normal_ipc_bus_lifecycle();
    test_normal_register_endpoint();
    test_normal_multiple_channels();
    test_error_null_handling();
    test_error_invalid_channel();
    test_timeout_request();
    test_concurrent_ipc_bus_helpers();
    test_backpressure_integration();
    test_routing_statistics();

    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           g_tests_passed, g_tests_total, g_tests_failed);
    return g_tests_failed > 0 ? 1 : 0;
}
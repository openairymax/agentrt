/**
 * @file test_openclaw_adapter.c
 * @brief OpenClaw Adapter Unit Tests
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */
// @owner: team-B

#include "openclaw_adapter.h"

#include <assert.h>
#include "logging_compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name)                       \
    do {                                 \
        AGENTOS_LOG_INFO("  TEST: %s ... ", name); \
    } while (0)
#define PASS()            \
    do {                  \
        AGENTOS_LOG_INFO("PASS"); \
        tests_passed++;   \
    } while (0)
#define FAIL(msg)                  \
    do {                           \
        AGENTOS_LOG_ERROR("FAIL: %s", msg); \
        tests_failed++;            \
    } while (0)
#define ASSERT_TRUE(cond, msg) \
    do {                       \
        if (!(cond)) {         \
            FAIL(msg);         \
            return;            \
        }                      \
    } while (0)
#define ASSERT_EQ(a, b, msg) \
    do {                     \
        if ((a) != (b)) {    \
            FAIL(msg);       \
            return;          \
        }                    \
    } while (0)
#define ASSERT_NEQ(a, b, msg) \
    do {                      \
        if ((a) == (b)) {     \
            FAIL(msg);        \
            return;           \
        }                     \
    } while (0)
#define ASSERT_NULL(p, msg) \
    do {                    \
        if ((p) != NULL) {  \
            FAIL(msg);      \
            return;         \
        }                   \
    } while (0)
#define ASSERT_NOT_NULL(p, msg) \
    do {                        \
        if ((p) == NULL) {      \
            FAIL(msg);          \
            return;             \
        }                       \
    } while (0)

static void test_config_default(void)
{
    TEST("config_default returns valid config");
    openclaw_config_t cfg = openclaw_config_default();
    ASSERT_TRUE(cfg.heartbeat_interval_sec > 0 || cfg.request_timeout_ms > 0,
                "default config should have valid intervals");
    PASS();
}

static void test_adapter_create_destroy(void)
{
    TEST("adapter create and destroy");
    openclaw_config_t cfg = openclaw_config_default();
    openclaw_adapter_context_t *ctx = openclaw_adapter_create(&cfg);
    ASSERT_NOT_NULL(ctx, "create should return non-NULL");
    openclaw_adapter_destroy(ctx);
    PASS();
}

static void test_adapter_create_null_config(void)
{
    TEST("adapter create with NULL config returns NULL");
    openclaw_adapter_context_t *ctx = openclaw_adapter_create(NULL);
    ASSERT_NULL(ctx, "create with NULL config should return NULL");
    PASS();
}

static void test_adapter_destroy_null(void)
{
    TEST("adapter destroy NULL does not crash");
    openclaw_adapter_destroy(NULL);
    PASS();
}

static void test_is_initialized_fresh(void)
{
    TEST("adapter create initializes context");
    openclaw_config_t cfg = openclaw_config_default();
    openclaw_adapter_context_t *ctx = openclaw_adapter_create(&cfg);
    ASSERT_NOT_NULL(ctx, "create should succeed");
    bool init = openclaw_adapter_is_initialized(ctx);
    ASSERT_TRUE(init, "adapter after create should be initialized");
    openclaw_adapter_destroy(ctx);
    PASS();
}

static void test_is_initialized_null(void)
{
    TEST("is_initialized with NULL returns false");
    bool init = openclaw_adapter_is_initialized(NULL);
    ASSERT_TRUE(!init, "NULL context should return false");
    PASS();
}

static void test_is_connected_fresh(void)
{
    TEST("fresh adapter is not connected");
    openclaw_config_t cfg = openclaw_config_default();
    openclaw_adapter_context_t *ctx = openclaw_adapter_create(&cfg);
    ASSERT_NOT_NULL(ctx, "create should succeed");
    bool conn = openclaw_is_connected(ctx);
    ASSERT_TRUE(!conn, "fresh adapter should not be connected");
    openclaw_adapter_destroy(ctx);
    PASS();
}

static void test_connect_without_init(void)
{
    TEST("connect without init returns error");
    openclaw_config_t cfg = openclaw_config_default();
    openclaw_adapter_context_t *ctx = openclaw_adapter_create(&cfg);
    ASSERT_NOT_NULL(ctx, "create should succeed");
    int rc = openclaw_connect(ctx);
    ASSERT_TRUE(rc != 0, "connect without init should fail");
    openclaw_adapter_destroy(ctx);
    PASS();
}

static void test_disconnect_not_connected(void)
{
    TEST("disconnect when not connected returns error or ok");
    openclaw_config_t cfg = openclaw_config_default();
    openclaw_adapter_context_t *ctx = openclaw_adapter_create(&cfg);
    ASSERT_NOT_NULL(ctx, "create should succeed");
    openclaw_disconnect(ctx);
    openclaw_adapter_destroy(ctx);
    PASS();
}

static void test_register_agent_null(void)
{
    TEST("register_agent with NULL returns error");
    openclaw_config_t cfg = openclaw_config_default();
    openclaw_adapter_context_t *ctx = openclaw_adapter_create(&cfg);
    ASSERT_NOT_NULL(ctx, "create should succeed");
    int rc = openclaw_register_agent(NULL, NULL);
    ASSERT_TRUE(rc != 0, "register_agent NULL should fail");
    openclaw_adapter_destroy(ctx);
    PASS();
}

static void test_register_tool_null(void)
{
    TEST("register_tool with NULL returns error");
    int rc = openclaw_register_tool(NULL, NULL);
    ASSERT_TRUE(rc != 0, "register_tool NULL should fail");
    PASS();
}

static void test_create_session_null(void)
{
    TEST("create_session with NULL returns error");
    int rc = openclaw_create_session(NULL, NULL, NULL);
    ASSERT_TRUE(rc != 0, "create_session NULL should fail");
    PASS();
}

static void test_send_message_null(void)
{
    TEST("send_message with NULL returns error");
    int rc = openclaw_send_message(NULL, NULL, NULL);
    ASSERT_TRUE(rc != 0, "send_message NULL should fail");
    PASS();
}

static void test_delegate_task_null(void)
{
    TEST("delegate_task with NULL returns error");
    int rc = openclaw_delegate_task(NULL, NULL, NULL, NULL);
    ASSERT_TRUE(rc != 0, "delegate_task NULL should fail");
    PASS();
}

static void test_query_task_null(void)
{
    TEST("query_task with NULL returns error");
    int rc = openclaw_query_task(NULL, NULL, NULL);
    ASSERT_TRUE(rc != 0, "query_task NULL should fail");
    PASS();
}

static void test_get_statistics_null(void)
{
    TEST("get_statistics with NULL returns error");
    int rc = openclaw_get_statistics(NULL, NULL, 0);
    ASSERT_TRUE(rc != 0, "get_statistics NULL should fail");
    PASS();
}

static void test_heartbeat_not_connected(void)
{
    TEST("heartbeat when not connected returns error");
    openclaw_config_t cfg = openclaw_config_default();
    openclaw_adapter_context_t *ctx = openclaw_adapter_create(&cfg);
    ASSERT_NOT_NULL(ctx, "create should succeed");
    int rc = openclaw_send_heartbeat(ctx);
    ASSERT_TRUE(rc != 0, "heartbeat without connection should fail");
    openclaw_adapter_destroy(ctx);
    PASS();
}

static void test_version_constants(void)
{
    TEST("version constants are valid");
    ASSERT_TRUE(strlen(OPENCLAW_ADAPTER_VERSION) > 0, "adapter version should not be empty");
    ASSERT_TRUE(strlen(OPENCLAW_PLATFORM_VERSION) > 0, "platform version should not be empty");
    ASSERT_TRUE(OPENCLAW_MAX_AGENTS > 0, "max agents should be positive");
    ASSERT_TRUE(OPENCLAW_MAX_TOOLS > 0, "max tools should be positive");
    ASSERT_TRUE(OPENCLAW_MAX_SESSIONS > 0, "max sessions should be positive");
    PASS();
}

static void test_agent_card_destroy_null(void)
{
    TEST("agent_card_destroy NULL does not crash");
    openclaw_agent_card_destroy(NULL);
    PASS();
}

static void test_tool_info_destroy_null(void)
{
    TEST("tool_info_destroy NULL does not crash");
    openclaw_tool_info_destroy(NULL);
    PASS();
}

static void test_session_destroy_null(void)
{
    TEST("session_destroy NULL does not crash");
    openclaw_session_destroy(NULL);
    PASS();
}

static void test_message_destroy_null(void)
{
    TEST("message_destroy NULL does not crash");
    openclaw_message_destroy(NULL);
    PASS();
}

static void test_task_destroy_null(void)
{
    TEST("task_destroy NULL does not crash");
    openclaw_task_destroy(NULL);
    PASS();
}

static void test_cluster_status_destroy_null(void)
{
    TEST("cluster_status_destroy NULL does not crash");
    openclaw_cluster_status_destroy(NULL);
    PASS();
}

static void test_set_message_handler_null(void)
{
    TEST("set_message_handler with NULL ctx returns error");
    int rc = openclaw_set_message_handler(NULL, NULL, NULL);
    ASSERT_TRUE(rc != 0, "set_message_handler NULL should fail");
    PASS();
}

static void test_set_task_handler_null(void)
{
    TEST("set_task_handler with NULL ctx returns error");
    int rc = openclaw_set_task_handler(NULL, NULL, NULL);
    ASSERT_TRUE(rc != 0, "set_task_handler NULL should fail");
    PASS();
}

int main(void)
{
    AGENTOS_LOG_INFO("=== OpenClaw Adapter Unit Tests ===\n\n");

    test_config_default();
    test_adapter_create_destroy();
    test_adapter_create_null_config();
    test_adapter_destroy_null();
    test_is_initialized_fresh();
    test_is_initialized_null();
    test_is_connected_fresh();
    test_connect_without_init();
    test_disconnect_not_connected();
    test_register_agent_null();
    test_register_tool_null();
    test_create_session_null();
    test_send_message_null();
    test_delegate_task_null();
    test_query_task_null();
    test_get_statistics_null();
    test_heartbeat_not_connected();
    test_version_constants();
    test_agent_card_destroy_null();
    test_tool_info_destroy_null();
    test_session_destroy_null();
    test_message_destroy_null();
    test_task_destroy_null();
    test_cluster_status_destroy_null();
    test_set_message_handler_null();
    test_set_task_handler_null();

    AGENTOS_LOG_INFO("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}

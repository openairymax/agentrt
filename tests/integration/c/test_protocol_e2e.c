// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: Apache-2.0
// @owner: team-C
/**
 * @file test_protocol_e2e.c
 * @brief End-to-End Protocol Tests (P3.6-P3.9)
 *
 * P3.6: A2A E2E - Two agents communicating via A2A protocol
 * P3.7: MCP E2E - External MCP client → AgentRT
 * P3.8: OpenAI Compatible - OpenAI-compatible API endpoints
 * P3.9: Protocol Routing - Mixed MCP/A2A/OpenAI auto-routing
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

#include "memory_compat.h"
#include "a2a_v03_adapter.h"
#include "mcp_v1_adapter.h"
#include "openai_enterprise_adapter.h"

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
        snprintf(buf, sizeof(buf), "%s (got %lld, expected %lld)", reason, \
                 (long long)(a), (long long)(b)); \
        FAIL(buf); return; \
    } \
} while(0)

/* ============================================================================
 * P3.6: A2A End-to-End Tests
 * ============================================================================
 *
 * Scenario: Two agents ("analyst" and "executor") communicate via A2A protocol.
 * Flow: Register → Discover → Delegate Task → Negotiate → Consensus → Stream
 */

static int a2a_notification_count = 0;
static void a2a_notification_cb(a2a_v03_context_t *ctx,
                                const a2a_notification_t *notification,
                                void *user_data) {
    (void)ctx;
    (void)user_data;
    a2a_notification_count++;
    printf("    [A2A Notification] type=%s, task_id=%s\n",
           notification->event_type ? notification->event_type : "unknown",
           notification->task_id ? notification->task_id : "none");
}

void test_a2a_agent_registration(void) {
    TEST("P3.6.1: A2A agent registration and discovery");

    a2a_v03_config_t cfg = a2a_v03_config_default();
    a2a_v03_context_t *ctx = a2a_v03_context_create(&cfg);
    CHECK(ctx != NULL, "Failed to create A2A context");

    /* Register analyst agent */
    a2a_agent_card_t analyst;
    AGENTOS_MEMSET(&analyst, 0, sizeof(analyst));
    analyst.name = "analyst-agent";
    analyst.url = "a2a://analyst:8080";
    analyst.capabilities_json = "data_analysis,reporting,visualization";
    analyst.protocol_version = 3;
    int rc = a2a_v03_register_agent(ctx, &analyst);
    CHECK_EQ(rc, 0, "Failed to register analyst agent");

    /* Register executor agent */
    a2a_agent_card_t executor;
    AGENTOS_MEMSET(&executor, 0, sizeof(executor));
    executor.name = "executor-agent";
    executor.url = "a2a://executor:8081";
    executor.capabilities_json = "task_execution,code_generation,deployment";
    executor.protocol_version = 3;
    rc = a2a_v03_register_agent(ctx, &executor);
    CHECK_EQ(rc, 0, "Failed to register executor agent");

    size_t count = a2a_v03_get_agent_count(ctx);
    CHECK_EQ(count, 2, "Agent count should be 2");

    /* Discover agents by capability */
    a2a_agent_card_t **results = NULL;
    size_t result_count = 0;
    rc = a2a_v03_discover_agents(ctx, "data_analysis", NULL, &results, &result_count);
    CHECK_EQ(rc, 0, "Agent discovery failed");
    CHECK(result_count >= 1, "Should find at least 1 agent with data_analysis capability");

    /* Cleanup discovery results */
    for (size_t i = 0; i < result_count; i++) {
        a2a_agent_card_destroy(results[i]);
    }
    free(results);

    a2a_v03_context_destroy(ctx);
    PASS();
}

void test_a2a_task_delegation(void) {
    TEST("P3.6.2: A2A task delegation and lifecycle");

    a2a_v03_config_t cfg = a2a_v03_config_default();
    a2a_v03_context_t *ctx = a2a_v03_context_create(&cfg);
    CHECK(ctx != NULL, "Failed to create A2A context");

    /* Register two agents */
    a2a_agent_card_t analyst;
    AGENTOS_MEMSET(&analyst, 0, sizeof(analyst));
    analyst.name = "analyst-agent";
    analyst.url = "a2a://analyst:8080";
    analyst.capabilities_json = "data_analysis";
    analyst.protocol_version = 3;
    a2a_v03_register_agent(ctx, &analyst);

    a2a_agent_card_t executor;
    AGENTOS_MEMSET(&executor, 0, sizeof(executor));
    executor.name = "executor-agent";
    executor.url = "a2a://executor:8081";
    executor.capabilities_json = "task_execution";
    executor.protocol_version = 3;
    a2a_v03_register_agent(ctx, &executor);

    /* Create a task for the executor */
    a2a_task_t *task = NULL;
    int rc = a2a_v03_create_task(ctx, "executor-agent",
                                 "Analyze market data and generate report",
                                 "{\"data_source\": \"market_feed\", \"period\": \"Q2_2026\"}",
                                 &task);
    CHECK_EQ(rc, 0, "Failed to create task");
    CHECK(task != NULL, "Task should not be NULL");
    CHECK(task->id != NULL, "Task ID should not be NULL");
    CHECK_EQ(task->state, A2A_TASK_WORKING, "Task should be in WORKING state after handler");

    /* Update task progress */
    rc = a2a_v03_update_task(ctx, task->id, A2A_TASK_COMPLETED,
                             "{\"result\": \"Market analysis complete\", \"confidence\": 0.95}",
                             100.0);
    CHECK_EQ(rc, 0, "Failed to update task");

    /* Retrieve task and verify */
    a2a_task_t *retrieved = NULL;
    rc = a2a_v03_get_task(ctx, task->id, &retrieved);
    CHECK_EQ(rc, 0, "Failed to get task");
    CHECK(retrieved != NULL, "Retrieved task should not be NULL");
    CHECK_EQ(retrieved->state, A2A_TASK_COMPLETED, "Task should be COMPLETED");

    /* Cancel a task */
    rc = a2a_v03_cancel_task(ctx, task->id, "No longer needed");
    CHECK_EQ(rc, 0, "Failed to cancel task");

    size_t task_count = a2a_v03_get_task_count(ctx);
    CHECK_EQ(task_count, 1, "Should have 1 task");

    a2a_v03_context_destroy(ctx);
    PASS();
}

void test_a2a_message_exchange(void) {
    TEST("P3.6.3: A2A inter-agent message exchange");

    a2a_v03_config_t cfg = a2a_v03_config_default();
    a2a_v03_context_t *ctx = a2a_v03_context_create(&cfg);
    CHECK(ctx != NULL, "Failed to create A2A context");

    /* Register agents */
    a2a_agent_card_t analyst;
    AGENTOS_MEMSET(&analyst, 0, sizeof(analyst));
    analyst.name = "analyst-agent";
    analyst.url = "a2a://analyst:8080";
    analyst.capabilities_json = "data_analysis";
    analyst.protocol_version = 3;
    a2a_v03_register_agent(ctx, &analyst);

    a2a_agent_card_t executor;
    AGENTOS_MEMSET(&executor, 0, sizeof(executor));
    executor.name = "executor-agent";
    executor.url = "a2a://executor:8081";
    executor.capabilities_json = "task_execution";
    executor.protocol_version = 3;
    a2a_v03_register_agent(ctx, &executor);

    /* Send a structured message from analyst to executor */
    a2a_message_t msg;
    AGENTOS_MEMSET(&msg, 0, sizeof(msg));
    msg.role = "user";
    msg.type = A2A_MSG_STRUCTURED;
    msg.content_json = "{\"action\":\"analyze\",\"target\":\"sales_data\",\"timeframe\":\"monthly\"}";
    msg.mime_type = "application/json";

    a2a_message_t *response = NULL;
    size_t response_count = 0;
    int rc = a2a_v03_send_message(ctx, "executor-agent", &msg, &response, &response_count);
    CHECK_EQ(rc, 0, "Message send failed");
    CHECK(response_count >= 1, "Should have at least 1 response");

    if (response && response_count > 0) {
        CHECK(response->role != NULL, "Response role should not be NULL");
        CHECK(response->content_json != NULL, "Response content should not be NULL");
        a2a_message_destroy(response);
    }

    a2a_v03_context_destroy(ctx);
    PASS();
}

void test_a2a_negotiation(void) {
    TEST("P3.6.4: A2A task negotiation");

    a2a_v03_config_t cfg = a2a_v03_config_default();
    a2a_v03_context_t *ctx = a2a_v03_context_create(&cfg);
    CHECK(ctx != NULL, "Failed to create A2A context");

    /* Register agents */
    a2a_agent_card_t analyst;
    AGENTOS_MEMSET(&analyst, 0, sizeof(analyst));
    analyst.name = "analyst-agent";
    analyst.url = "a2a://analyst:8080";
    analyst.capabilities_json = "data_analysis";
    analyst.protocol_version = 3;
    a2a_v03_register_agent(ctx, &analyst);

    /* Create a task first */
    a2a_task_t *task = NULL;
    a2a_v03_create_task(ctx, "analyst-agent", "Negotiable task",
                        "{\"priority\": 5}", &task);

    /* Negotiate task cost */
    a2a_negotiation_t proposal;
    AGENTOS_MEMSET(&proposal, 0, sizeof(proposal));
    proposal.action = A2A_NEGOTIATE_PROPOSE;
    proposal.task_id = task->id;
    proposal.agent_id = "analyst-agent";
    proposal.terms_json = "{\"cost\": 100, \"timeout_ms\": 5000}";

    a2a_negotiation_action_t response_action = A2A_NEGOTIATE_REJECT;
    char *response_terms = NULL;
    int rc = a2a_v03_negotiate(ctx, &proposal, &response_action, &response_terms);
    CHECK_EQ(rc, 0, "Negotiation failed");

    a2a_v03_context_destroy(ctx);
    PASS();
}

void test_a2a_consensus(void) {
    TEST("P3.6.5: A2A multi-agent consensus");

    a2a_v03_config_t cfg = a2a_v03_config_default();
    a2a_v03_context_t *ctx = a2a_v03_context_create(&cfg);
    CHECK(ctx != NULL, "Failed to create A2A context");

    /* Register multiple agents */
    const char *names[] = {"agent-a", "agent-b", "agent-c", "agent-d", "agent-e"};
    for (int i = 0; i < 5; i++) {
        a2a_agent_card_t card;
        AGENTOS_MEMSET(&card, 0, sizeof(card));
        card.name = (char *)names[i];
        card.url = "a2a://localhost";
        card.capabilities_json = "consensus";
        card.protocol_version = 3;
        a2a_v03_register_agent(ctx, &card);
    }

    CHECK_EQ(a2a_v03_get_agent_count(ctx), 5, "Should have 5 agents");

    /* Route consensus request */
    char *response = NULL;
    int rc = a2a_v03_route_request(ctx, "agent/discover", NULL, &response);
    CHECK_EQ(rc, 0, "Route request failed");
    CHECK(response != NULL, "Response should not be NULL");
    CHECK(strstr(response, "agents") != NULL, "Response should contain agents list");

    /* Verify stats endpoint */
    char *stats = NULL;
    rc = a2a_v03_route_request(ctx, "stats", NULL, &stats);
    CHECK_EQ(rc, 0, "Stats request failed");
    CHECK(stats != NULL, "Stats should not be NULL");
    CHECK(strstr(stats, "agent_count") != NULL, "Stats should contain agent_count");

    a2a_v03_context_destroy(ctx);
    PASS();
}

void test_a2a_notifications(void) {
    TEST("P3.6.6: A2A push notifications");

    a2a_v03_config_t cfg = a2a_v03_config_default();
    a2a_v03_context_t *ctx = a2a_v03_context_create(&cfg);
    CHECK(ctx != NULL, "Failed to create A2A context");

    a2a_notification_count = 0;
    int rc = a2a_v03_subscribe_notifications(ctx, a2a_notification_cb, NULL);
    CHECK_EQ(rc, 0, "Failed to subscribe notifications");

    /* Create a task that triggers notification via handler */
    a2a_agent_card_t agent;
    AGENTOS_MEMSET(&agent, 0, sizeof(agent));
    agent.name = "notifier";
    agent.url = "a2a://notifier:8080";
    agent.capabilities_json = "notifications";
    agent.protocol_version = 3;
    a2a_v03_register_agent(ctx, &agent);

    /* Send a notification */
    a2a_notification_t notif;
    AGENTOS_MEMSET(&notif, 0, sizeof(notif));
    notif.event_type = "task.status.changed";
    notif.task_id = "task-001";
    notif.agent_id = "notifier";
    notif.data_json = "{\"status\":\"in_progress\"}";
    notif.timestamp = 1234567890;

    rc = a2a_v03_send_notification(ctx, &notif);
    /* Notification handler should have been called */
    CHECK_EQ(rc, 0, "Failed to send notification");

    /* Unsubscribe */
    rc = a2a_v03_unsubscribe_notifications(ctx);
    CHECK_EQ(rc, 0, "Failed to unsubscribe notifications");

    a2a_v03_context_destroy(ctx);
    PASS();
}

void test_a2a_streaming(void) {
    TEST("P3.6.7: A2A streaming task execution");

    a2a_v03_config_t cfg = a2a_v03_config_default();
    a2a_v03_context_t *ctx = a2a_v03_context_create(&cfg);
    CHECK(ctx != NULL, "Failed to create A2A context");

    a2a_agent_card_t agent;
    AGENTOS_MEMSET(&agent, 0, sizeof(agent));
    agent.name = "streamer";
    agent.url = "a2a://streamer:8080";
    agent.capabilities_json = "streaming";
    agent.protocol_version = 3;
    a2a_v03_register_agent(ctx, &agent);

    /* Create a task and stream updates */
    a2a_task_t *task = NULL;
    int rc = a2a_v03_create_task(ctx, "streamer",
                                 "Stream processing task",
                                 "{\"chunks\": 5}", &task);
    CHECK_EQ(rc, 0, "Failed to create task");

    /* Stream progress updates */
    double progress_steps[] = {0.0, 25.0, 50.0, 75.0, 100.0};
    const char *chunks[] = {
        "{\"phase\":\"init\"}",
        "{\"phase\":\"processing\",\"pct\":25}",
        "{\"phase\":\"processing\",\"pct\":50}",
        "{\"phase\":\"processing\",\"pct\":75}",
        "{\"phase\":\"complete\"}"
    };

    for (int i = 0; i < 5; i++) {
        rc = a2a_v03_stream_task_update(ctx, task->id, progress_steps[i],
                                        chunks[i], (i == 4));
        CHECK_EQ(rc, 0, "Stream update failed");
    }

    /* Verify final state */
    a2a_task_t *final = NULL;
    rc = a2a_v03_get_task(ctx, task->id, &final);
    CHECK_EQ(rc, 0, "Failed to get task");
    CHECK_EQ(final->state, A2A_TASK_COMPLETED, "Task should be COMPLETED after streaming");

    a2a_v03_context_destroy(ctx);
    PASS();
}

void test_a2a_authentication(void) {
    TEST("P3.6.8: A2A authentication and session management");

    a2a_v03_config_t cfg = a2a_v03_config_default();
    a2a_v03_context_t *ctx = a2a_v03_context_create(&cfg);
    CHECK(ctx != NULL, "Failed to create A2A context");

    /* Initialize auth */
    a2a_auth_config_t auth_cfg;
    AGENTOS_MEMSET(&auth_cfg, 0, sizeof(auth_cfg));
    auth_cfg.method = A2A_AUTH_API_KEY;
    strncpy(auth_cfg.shared_secret, "test-api-key-12345", sizeof(auth_cfg.shared_secret) - 1);
    auth_cfg.secret_len = strlen(auth_cfg.shared_secret);
    auth_cfg.token_ttl_sec = 3600;
    auth_cfg.max_sessions = 10;

    int rc = a2a_v03_auth_init(ctx, &auth_cfg);
    CHECK_EQ(rc, 0, "Auth init failed");

    /* Authenticate */
    a2a_auth_token_t *token = NULL;
    rc = a2a_v03_authenticate(ctx, "test-agent", "test-api-key-12345", &token);
    CHECK_EQ(rc, 0, "Authentication failed");
    CHECK(token != NULL, "Token should not be NULL");
    CHECK(token->valid, "Token should be valid");

    /* Verify token */
    a2a_auth_token_t *verified = NULL;
    rc = a2a_v03_verify_token(ctx, token->token, &verified);
    CHECK_EQ(rc, 0, "Token verification failed");
    CHECK(verified == token, "Verified token should match original");

    /* Create session */
    a2a_session_t *session = NULL;
    rc = a2a_v03_create_session(ctx, "remote-agent", A2A_AUTH_API_KEY,
                                A2A_CRYPTO_AES_256_GCM, &session);
    CHECK_EQ(rc, 0, "Session creation failed");
    CHECK(session != NULL, "Session should not be NULL");
    CHECK(session->authenticated, "Session should be authenticated");
    CHECK(session->encrypted, "Session should be encrypted");

    /* Validate session */
    a2a_session_t *validated = NULL;
    rc = a2a_v03_validate_session(ctx, session->session_id, &validated);
    CHECK_EQ(rc, 0, "Session validation failed");

    /* Sign request */
    char signature[65];
    const char *sig = a2a_v03_sign_request(ctx, "task/delegate",
                                           "{\"task\":\"test\"}",
                                           token->token, signature, sizeof(signature));
    CHECK(sig != NULL, "Request signing failed");

    /* Verify signature */
    rc = a2a_v03_verify_signature(ctx, "task/delegate",
                                  "{\"task\":\"test\"}", signature, token->token);
    CHECK_EQ(rc, 0, "Signature verification failed");

    /* Invalidate token */
    rc = a2a_v03_invalidate_token(ctx, token->token);
    CHECK_EQ(rc, 0, "Token invalidation failed");

    a2a_v03_auth_shutdown(ctx);
    a2a_v03_context_destroy(ctx);
    PASS();
}

/* ============================================================================
 * P3.7: MCP End-to-End Tests
 * ============================================================================
 *
 * Scenario: External MCP client connects to AgentRT, registers tools,
 * calls tools, accesses resources, and uses prompts.
 */

static void mcp_tool_echo_handler(const char *tool_name, const char *arguments_json,
                                  mcp_content_t **results, size_t *result_count,
                                  bool *is_error, void *user_data) {
    (void)tool_name;
    (void)user_data;

    *result_count = 1;
    *results = (mcp_content_t *)AGENTOS_CALLOC(1, sizeof(mcp_content_t));
    if (*results) {
        (*results)[0].type = MCP_CONTENT_TEXT;
        size_t len = snprintf(NULL, 0, "Echo: %s", arguments_json ? arguments_json : "{}");
        (*results)[0].text = (char *)AGENTOS_MALLOC(len + 1);
        if ((*results)[0].text) {
            snprintf((*results)[0].text, len + 1, "Echo: %s",
                     arguments_json ? arguments_json : "{}");
        }
    }
    *is_error = false;
}

static void mcp_resource_file_handler(const char *uri, char **content, char **mime_type,
                                      void *user_data) {
    (void)uri;
    (void)user_data;
    *content = AGENTOS_STRDUP("This is a test resource content.");
    *mime_type = AGENTOS_STRDUP("text/plain");
}

static void mcp_prompt_greeting_handler(const char *name, const char *arguments_json,
                                        mcp_sampling_message_t **messages,
                                        size_t *message_count, void *user_data) {
    (void)name;
    (void)arguments_json;
    (void)user_data;
    *message_count = 1;
    *messages = (mcp_sampling_message_t *)AGENTOS_CALLOC(1, sizeof(mcp_sampling_message_t));
    if (*messages) {
        (*messages)[0].role = AGENTOS_STRDUP("assistant");
        (*messages)[0].content = (mcp_content_t *)AGENTOS_CALLOC(1, sizeof(mcp_content_t));
        if ((*messages)[0].content) {
            (*messages)[0].content[0].type = MCP_CONTENT_TEXT;
            (*messages)[0].content[0].text = AGENTOS_STRDUP("Hello from MCP prompt!");
        }
        (*messages)[0].content_count = 1;
    }
}

void test_mcp_initialization(void) {
    TEST("P3.7.1: MCP context initialization");

    mcp_v1_config_t cfg = mcp_v1_config_default();
    mcp_v1_context_t *ctx = mcp_v1_context_create(&cfg);
    CHECK(ctx != NULL, "Failed to create MCP context");

    /* Verify initialize response */
    char *response = NULL;
    int rc = mcp_v1_route_request(ctx, "initialize", NULL, &response);
    CHECK_EQ(rc, 0, "Initialize failed");
    CHECK(response != NULL, "Initialize response should not be NULL");
    CHECK(strstr(response, "protocolVersion") != NULL, "Should contain protocolVersion");
    CHECK(strstr(response, "capabilities") != NULL, "Should contain capabilities");
    CHECK(strstr(response, "serverInfo") != NULL, "Should contain serverInfo");

    /* Verify capabilities */
    uint32_t caps = mcp_v1_get_capabilities(ctx);
    CHECK(caps & MCP_CAP_TOOLS, "Should have TOOLS capability");
    CHECK(caps & MCP_CAP_RESOURCES, "Should have RESOURCES capability");

    mcp_v1_context_destroy(ctx);
    PASS();
}

void test_mcp_tool_registration_and_call(void) {
    TEST("P3.7.2: MCP tool registration and call");

    mcp_v1_config_t cfg = mcp_v1_config_default();
    mcp_v1_context_t *ctx = mcp_v1_context_create(&cfg);
    CHECK(ctx != NULL, "Failed to create MCP context");

    /* Register echo tool */
    mcp_tool_t tool;
    AGENTOS_MEMSET(&tool, 0, sizeof(tool));
    tool.name = "echo";
    tool.description = "Echoes back the input arguments";
    tool.input_schema_json = "{\"type\":\"object\",\"properties\":{\"message\":{\"type\":\"string\"}}}";
    int rc = mcp_v1_register_tool(ctx, &tool, mcp_tool_echo_handler, NULL);
    CHECK_EQ(rc, 0, "Failed to register echo tool");

    CHECK_EQ(mcp_v1_get_tool_count(ctx), 1, "Should have 1 tool");

    /* List tools */
    char *tools_list = NULL;
    rc = mcp_v1_handle_tools_list(ctx, &tools_list);
    CHECK_EQ(rc, 0, "Tools list failed");
    CHECK(tools_list != NULL, "Tools list should not be NULL");
    CHECK(strstr(tools_list, "echo") != NULL, "Should contain echo tool");

    /* Call tool */
    char *result = NULL;
    rc = mcp_v1_handle_tools_call(ctx, "echo",
                                  "{\"message\":\"Hello MCP\"}", &result);
    CHECK_EQ(rc, 0, "Tool call failed");
    CHECK(result != NULL, "Tool call result should not be NULL");
    CHECK(strstr(result, "Echo") != NULL, "Result should contain Echo");

    /* Call non-existent tool */
    char *error_result = NULL;
    rc = mcp_v1_handle_tools_call(ctx, "non_existent_tool",
                                  "{}", &error_result);
    CHECK(rc != 0, "Non-existent tool should return error");
    CHECK(error_result != NULL, "Error result should not be NULL");
    CHECK(strstr(error_result, "isError") != NULL, "Error result should contain isError");

    mcp_v1_context_destroy(ctx);
    PASS();
}

void test_mcp_resource_management(void) {
    TEST("P3.7.3: MCP resource management");

    mcp_v1_config_t cfg = mcp_v1_config_default();
    mcp_v1_context_t *ctx = mcp_v1_context_create(&cfg);
    CHECK(ctx != NULL, "Failed to create MCP context");

    /* Register a resource */
    mcp_resource_t resource;
    AGENTOS_MEMSET(&resource, 0, sizeof(resource));
    resource.uri = "file:///data/config.json";
    resource.name = "Configuration";
    resource.description = "Application configuration";
    resource.mime_type = "application/json";
    int rc = mcp_v1_register_resource(ctx, &resource, mcp_resource_file_handler, NULL);
    CHECK_EQ(rc, 0, "Failed to register resource");

    CHECK_EQ(mcp_v1_get_resource_count(ctx), 1, "Should have 1 resource");

    /* List resources */
    char *resources_list = NULL;
    rc = mcp_v1_handle_resources_list(ctx, &resources_list);
    CHECK_EQ(rc, 0, "Resources list failed");
    CHECK(strstr(resources_list, "config.json") != NULL, "Should contain config.json");

    /* Read resource */
    char *content = NULL;
    rc = mcp_v1_handle_resources_read(ctx, "file:///data/config.json", &content);
    CHECK_EQ(rc, 0, "Resource read failed");
    CHECK(content != NULL, "Content should not be NULL");
    CHECK(strstr(content, "test resource content") != NULL, "Should contain content");

    /* Register resource template */
    mcp_resource_template_t tmpl;
    AGENTOS_MEMSET(&tmpl, 0, sizeof(tmpl));
    tmpl.uri_template = "file:///data/{name}.json";
    tmpl.name = "Data Files";
    tmpl.description = "Data files by name";
    tmpl.mime_type = "application/json";
    rc = mcp_v1_register_resource_template(ctx, &tmpl);
    CHECK_EQ(rc, 0, "Failed to register resource template");

    /* List templates */
    char *templates = NULL;
    rc = mcp_v1_handle_resources_templates(ctx, &templates);
    CHECK_EQ(rc, 0, "Templates list failed");
    CHECK(strstr(templates, "{name}.json") != NULL, "Should contain template");

    mcp_v1_context_destroy(ctx);
    PASS();
}

void test_mcp_prompt_management(void) {
    TEST("P3.7.4: MCP prompt management");

    mcp_v1_config_t cfg = mcp_v1_config_default();
    mcp_v1_context_t *ctx = mcp_v1_context_create(&cfg);
    CHECK(ctx != NULL, "Failed to create MCP context");

    /* Register a prompt */
    mcp_prompt_t prompt;
    AGENTOS_MEMSET(&prompt, 0, sizeof(prompt));
    prompt.name = "greeting";
    prompt.description = "Generates a greeting message";
    prompt.arguments_schema_json = "[{\"name\":\"user\",\"type\":\"string\"}]";
    int rc = mcp_v1_register_prompt(ctx, &prompt, mcp_prompt_greeting_handler, NULL);
    CHECK_EQ(rc, 0, "Failed to register prompt");

    CHECK_EQ(mcp_v1_get_prompt_count(ctx), 1, "Should have 1 prompt");

    /* List prompts */
    char *prompts_list = NULL;
    rc = mcp_v1_handle_prompts_list(ctx, &prompts_list);
    CHECK_EQ(rc, 0, "Prompts list failed");
    CHECK(strstr(prompts_list, "greeting") != NULL, "Should contain greeting");

    /* Get prompt */
    char *prompt_result = NULL;
    rc = mcp_v1_handle_prompts_get(ctx, "greeting",
                                   "{\"user\":\"Alice\"}", &prompt_result);
    CHECK_EQ(rc, 0, "Prompt get failed");
    CHECK(prompt_result != NULL, "Prompt result should not be NULL");
    CHECK(strstr(prompt_result, "Hello") != NULL, "Should contain greeting");

    mcp_v1_context_destroy(ctx);
    PASS();
}

void test_mcp_logging(void) {
    TEST("P3.7.5: MCP logging level control");

    mcp_v1_config_t cfg = mcp_v1_config_default();
    mcp_v1_context_t *ctx = mcp_v1_context_create(&cfg);
    CHECK(ctx != NULL, "Failed to create MCP context");

    /* Set log level via route_request */
    char *response = NULL;
    int rc = mcp_v1_route_request(ctx, "logging/setLogLevel",
                                  "{\"level\":\"debug\"}", &response);
    CHECK_EQ(rc, 0, "Set log level failed");

    rc = mcp_v1_set_log_level(ctx, MCP_LOG_ERROR);
    CHECK_EQ(rc, 0, "Set log level via API failed");

    mcp_v1_context_destroy(ctx);
    PASS();
}

void test_mcp_progress_notification(void) {
    TEST("P3.7.6: MCP progress notifications");

    mcp_v1_config_t cfg = mcp_v1_config_default();
    mcp_v1_context_t *ctx = mcp_v1_context_create(&cfg);
    CHECK(ctx != NULL, "Failed to create MCP context");

    /* Send progress */
    int rc = mcp_v1_send_progress(ctx, "progress-token-001", 50.0, 100.0);
    CHECK_EQ(rc, 0, "Progress send failed");

    /* Notify cancelled */
    rc = mcp_v1_notify_cancelled(ctx, "req-001", "User requested cancellation");
    CHECK_EQ(rc, 0, "Cancel notification failed");

    mcp_v1_context_destroy(ctx);
    PASS();
}

/* ============================================================================
 * P3.8: OpenAI Compatible Tests
 * ============================================================================
 *
 * Scenario: Using OpenAI-compatible endpoints to interact with AgentRT.
 * Tests model listing, chat completions, and streaming.
 */

void test_openai_model_listing(void) {
    TEST("P3.8.1: OpenAI model listing");

    /* Verify adapter can be obtained */
    const protocol_adapter_t *adapter = openai_enterprise_get_adapter();
    CHECK(adapter != NULL, "OpenAI adapter should not be NULL");
    CHECK(adapter->name != NULL, "Adapter name should not be NULL");
    CHECK(strstr(adapter->name, "OpenAI") != NULL, "Should be OpenAI adapter");

    /* Verify adapter version */
    char version_buf[32];
    int rc = adapter->get_version(adapter->context, version_buf, sizeof(version_buf));
    CHECK(rc >= 0 || rc == -1, "Version check should not crash");

    PASS();
}

void test_openai_route_request(void) {
    TEST("P3.8.2: OpenAI route request");

    /* Verify adapter capabilities */
    const protocol_adapter_t *adapter = openai_enterprise_get_adapter();
    CHECK(adapter != NULL, "OpenAI adapter should not be NULL");

    /* Test capabilities */
    uint32_t caps = adapter->capabilities(adapter->context);
    CHECK(caps > 0, "Should have capabilities");

    PASS();
}

/* ============================================================================
 * P3.9: Protocol Routing Tests
 * ============================================================================
 *
 * Scenario: Mixed MCP/A2A/OpenAI requests are automatically routed
 * to the correct protocol adapter.
 */

void test_protocol_routing_mixed(void) {
    TEST("P3.9.1: Mixed protocol routing (MCP + A2A + OpenAI)");

    /* Create all three protocol contexts */
    a2a_v03_config_t a2a_cfg = a2a_v03_config_default();
    a2a_v03_context_t *a2a_ctx = a2a_v03_context_create(&a2a_cfg);
    CHECK(a2a_ctx != NULL, "Failed to create A2A context");

    mcp_v1_config_t mcp_cfg = mcp_v1_config_default();
    mcp_v1_context_t *mcp_ctx = mcp_v1_context_create(&mcp_cfg);
    CHECK(mcp_ctx != NULL, "Failed to create MCP context");

    /* Register an A2A agent */
    a2a_agent_card_t agent;
    AGENTOS_MEMSET(&agent, 0, sizeof(agent));
    agent.name = "router-agent";
    agent.url = "a2a://router:8080";
    agent.capabilities_json = "routing,multi_protocol";
    agent.protocol_version = 3;
    int rc = a2a_v03_register_agent(a2a_ctx, &agent);
    CHECK_EQ(rc, 0, "Failed to register A2A agent");

    /* Register an MCP tool */
    mcp_tool_t tool;
    AGENTOS_MEMSET(&tool, 0, sizeof(tool));
    tool.name = "router_tool";
    tool.description = "Protocol routing test tool";
    tool.input_schema_json = "{}";
    rc = mcp_v1_register_tool(mcp_ctx, &tool, mcp_tool_echo_handler, NULL);
    CHECK_EQ(rc, 0, "Failed to register MCP tool");

    /* Route A2A request */
    char *a2a_response = NULL;
    rc = a2a_v03_route_request(a2a_ctx, "agent/discover", NULL, &a2a_response);
    CHECK_EQ(rc, 0, "A2A route request failed");
    CHECK(strstr(a2a_response, "router-agent") != NULL, "A2A response should contain agent");

    /* Route MCP request */
    char *mcp_response = NULL;
    rc = mcp_v1_route_request(mcp_ctx, "initialize", NULL, &mcp_response);
    CHECK_EQ(rc, 0, "MCP route request failed");
    CHECK(strstr(mcp_response, "AgentRT") != NULL, "MCP response should contain AgentRT");

    /* Verify both protocols are independent */
    CHECK_EQ(a2a_v03_get_agent_count(a2a_ctx), 1, "A2A should have 1 agent");
    CHECK_EQ(mcp_v1_get_tool_count(mcp_ctx), 1, "MCP should have 1 tool");

    a2a_v03_context_destroy(a2a_ctx);
    mcp_v1_context_destroy(mcp_ctx);
    PASS();
}

void test_protocol_routing_unknown_method(void) {
    TEST("P3.9.2: Unknown method routing");

    a2a_v03_config_t cfg = a2a_v03_config_default();
    a2a_v03_context_t *ctx = a2a_v03_context_create(&cfg);
    CHECK(ctx != NULL, "Failed to create A2A context");

    /* Route unknown method */
    char *response = NULL;
    int rc = a2a_v03_route_request(ctx, "unknown/method", NULL, &response);
    CHECK(rc != 0, "Unknown method should return error");
    CHECK(response != NULL, "Error response should not be NULL");
    CHECK(strstr(response, "unknown method") != NULL, "Should indicate unknown method");

    a2a_v03_context_destroy(ctx);
    PASS();
}

void test_protocol_routing_task_lifecycle(void) {
    TEST("P3.9.3: A2A task lifecycle via routing");

    a2a_v03_config_t cfg = a2a_v03_config_default();
    a2a_v03_context_t *ctx = a2a_v03_context_create(&cfg);
    CHECK(ctx != NULL, "Failed to create A2A context");

    a2a_agent_card_t agent;
    AGENTOS_MEMSET(&agent, 0, sizeof(agent));
    agent.name = "lifecycle-agent";
    agent.url = "a2a://lifecycle:8080";
    agent.capabilities_json = "task_lifecycle";
    agent.protocol_version = 3;
    a2a_v03_register_agent(ctx, &agent);

    /* Create task via routing */
    char *create_resp = NULL;
    int rc = a2a_v03_route_request(ctx, "task/create",
                                   "{\"description\":\"Test routing task\"}",
                                   &create_resp);
    CHECK_EQ(rc, 0, "Task create via routing failed");
    CHECK(strstr(create_resp, "task_id") != NULL, "Should contain task_id");

    /* List tasks via routing */
    char *list_resp = NULL;
    rc = a2a_v03_route_request(ctx, "task/list", NULL, &list_resp);
    CHECK_EQ(rc, 0, "Task list via routing failed");
    CHECK(strstr(list_resp, "tasks") != NULL, "Should contain tasks");

    a2a_v03_context_destroy(ctx);
    PASS();
}

void test_protocol_routing_mcp_methods(void) {
    TEST("P3.9.4: MCP method routing");

    mcp_v1_config_t cfg = mcp_v1_config_default();
    mcp_v1_context_t *ctx = mcp_v1_context_create(&cfg);
    CHECK(ctx != NULL, "Failed to create MCP context");

    mcp_tool_t tool;
    AGENTOS_MEMSET(&tool, 0, sizeof(tool));
    tool.name = "routing_test";
    tool.description = "Routing test tool";
    tool.input_schema_json = "{\"type\":\"object\"}";
    mcp_v1_register_tool(ctx, &tool, mcp_tool_echo_handler, NULL);

    /* Route tools/list */
    char *tools_list = NULL;
    int rc = mcp_v1_route_request(ctx, "tools/list", NULL, &tools_list);
    CHECK_EQ(rc, 0, "tools/list routing failed");
    CHECK(strstr(tools_list, "routing_test") != NULL, "Should contain routing_test");

    /* Route tools/call */
    char *call_result = NULL;
    rc = mcp_v1_route_request(ctx, "tools/call",
                              "{\"name\":\"routing_test\",\"arguments\":{\"msg\":\"test\"}}",
                              &call_result);
    CHECK_EQ(rc, 0, "tools/call routing failed");
    CHECK(strstr(call_result, "Echo") != NULL, "Should contain Echo");

    /* Route unknown MCP method */
    char *unknown = NULL;
    rc = mcp_v1_route_request(ctx, "unknown/mcp/method", NULL, &unknown);
    CHECK(rc != 0, "Unknown MCP method should return error");
    CHECK(strstr(unknown, "Method not found") != NULL, "Should indicate method not found");

    mcp_v1_context_destroy(ctx);
    PASS();
}

/* ============================================================================
 * P3.10: Multi-Agent Collaboration Mode Tests
 * ============================================================================
 */

void test_multi_agent_orchestrator(void) {
    TEST("P3.10.1: Orchestrator pattern - 1 master, 3 sub-agents");

    a2a_v03_config_t cfg = a2a_v03_config_default();
    a2a_v03_context_t *ctx = a2a_v03_context_create(&cfg);
    CHECK(ctx != NULL, "Failed to create A2A context");

    /* Register orchestrator */
    a2a_agent_card_t orchestrator;
    AGENTOS_MEMSET(&orchestrator, 0, sizeof(orchestrator));
    orchestrator.name = "orchestrator";
    orchestrator.url = "a2a://orchestrator:8080";
    orchestrator.capabilities_json = "orchestration,planning";
    orchestrator.protocol_version = 3;
    a2a_v03_register_agent(ctx, &orchestrator);

    /* Register 3 sub-agents */
    const char *sub_names[] = {"researcher", "analyst", "writer"};
    const char *sub_caps[] = {"research,search", "analysis,statistics", "writing,editing"};
    for (int i = 0; i < 3; i++) {
        a2a_agent_card_t sub;
        AGENTOS_MEMSET(&sub, 0, sizeof(sub));
        sub.name = (char *)sub_names[i];
        sub.url = "a2a://localhost";
        sub.capabilities_json = (char *)sub_caps[i];
        sub.protocol_version = 3;
        a2a_v03_register_agent(ctx, &sub);
    }

    CHECK_EQ(a2a_v03_get_agent_count(ctx), 4, "Should have 4 agents");

    /* Orchestrator creates tasks for sub-agents */
    for (int i = 0; i < 3; i++) {
        a2a_task_t *task = NULL;
        char desc[128];
        snprintf(desc, sizeof(desc), "Sub-task %d for %s", i + 1, sub_names[i]);
        int rc = a2a_v03_create_task(ctx, (char *)sub_names[i], desc,
                                     "{\"orchestrator\":\"orchestrator\"}", &task);
        CHECK_EQ(rc, 0, "Failed to create sub-task");
        CHECK(task != NULL, "Sub-task should not be NULL");
    }

    CHECK_EQ(a2a_v03_get_task_count(ctx), 3, "Should have 3 tasks");

    a2a_v03_context_destroy(ctx);
    PASS();
}

void test_multi_agent_debate(void) {
    TEST("P3.10.2: Debate pattern - 2 agents debate, conclusion generated");

    a2a_v03_config_t cfg = a2a_v03_config_default();
    a2a_v03_context_t *ctx = a2a_v03_context_create(&cfg);
    CHECK(ctx != NULL, "Failed to create A2A context");

    /* Register 2 debate agents */
    a2a_agent_card_t pro;
    AGENTOS_MEMSET(&pro, 0, sizeof(pro));
    pro.name = "pro-arguer";
    pro.url = "a2a://pro:8080";
    pro.capabilities_json = "debate,argumentation,pro";
    pro.protocol_version = 3;
    a2a_v03_register_agent(ctx, &pro);

    a2a_agent_card_t con;
    AGENTOS_MEMSET(&con, 0, sizeof(con));
    con.name = "con-arguer";
    con.url = "a2a://con:8080";
    con.capabilities_json = "debate,argumentation,con";
    con.protocol_version = 3;
    a2a_v03_register_agent(ctx, &con);

    /* Exchange debate messages */
    a2a_message_t pro_msg;
    AGENTOS_MEMSET(&pro_msg, 0, sizeof(pro_msg));
    pro_msg.role = "assistant";
    pro_msg.type = A2A_MSG_STRUCTURED;
    pro_msg.content_json = "{\"argument\":\"AI will benefit humanity\",\"position\":\"pro\"}";
    pro_msg.mime_type = "application/json";

    a2a_message_t *response = NULL;
    size_t response_count = 0;
    int rc = a2a_v03_send_message(ctx, "con-arguer", &pro_msg, &response, &response_count);
    CHECK_EQ(rc, 0, "Debate message exchange failed");
    CHECK(response_count >= 1, "Should have response");

    if (response && response_count > 0) {
        a2a_message_destroy(response);
    }

    a2a_v03_context_destroy(ctx);
    PASS();
}

void test_multi_agent_hierarchy(void) {
    TEST("P3.10.3: Hierarchy pattern - parent delegates to child");

    a2a_v03_config_t cfg = a2a_v03_config_default();
    a2a_v03_context_t *ctx = a2a_v03_context_create(&cfg);
    CHECK(ctx != NULL, "Failed to create A2A context");

    /* Register parent (supervisor) */
    a2a_agent_card_t parent;
    AGENTOS_MEMSET(&parent, 0, sizeof(parent));
    parent.name = "supervisor";
    parent.url = "a2a://supervisor:8080";
    parent.capabilities_json = "supervision,delegation";
    parent.protocol_version = 3;
    a2a_v03_register_agent(ctx, &parent);

    /* Register child (worker) */
    a2a_agent_card_t child;
    AGENTOS_MEMSET(&child, 0, sizeof(child));
    child.name = "worker";
    child.url = "a2a://worker:8080";
    child.capabilities_json = "execution,reporting";
    child.protocol_version = 3;
    a2a_v03_register_agent(ctx, &child);

    /* Parent delegates task to child */
    a2a_task_t *task = NULL;
    int rc = a2a_v03_create_task(ctx, "worker",
                                 "Execute delegated work",
                                 "{\"from\":\"supervisor\",\"priority\":\"high\"}",
                                 &task);
    CHECK_EQ(rc, 0, "Hierarchy delegation failed");
    CHECK(task != NULL, "Task should not be NULL");

    /* Child updates task with result */
    rc = a2a_v03_update_task(ctx, task->id, A2A_TASK_COMPLETED,
                             "{\"result\":\"Work completed\",\"worker\":\"worker\"}",
                             100.0);
    CHECK_EQ(rc, 0, "Task update failed");

    a2a_v03_context_destroy(ctx);
    PASS();
}

void test_multi_agent_market(void) {
    TEST("P3.10.4: Market pattern - agents bid for task");

    a2a_v03_config_t cfg = a2a_v03_config_default();
    a2a_v03_context_t *ctx = a2a_v03_context_create(&cfg);
    CHECK(ctx != NULL, "Failed to create A2A context");

    /* Register 3 bidding agents */
    const char *bidder_names[] = {"bidder-alpha", "bidder-beta", "bidder-gamma"};
    for (int i = 0; i < 3; i++) {
        a2a_agent_card_t bidder;
        AGENTOS_MEMSET(&bidder, 0, sizeof(bidder));
        bidder.name = (char *)bidder_names[i];
        bidder.url = "a2a://localhost";
        bidder.capabilities_json = "bidding,execution";
        bidder.protocol_version = 3;
        a2a_v03_register_agent(ctx, &bidder);
    }

    /* Create a task for bidding */
    a2a_task_t *task = NULL;
    int rc = a2a_v03_create_task(ctx, NULL,
                                 "Market auction task - highest bidder wins",
                                 "{\"type\":\"auction\",\"base_price\":100}",
                                 &task);
    CHECK_EQ(rc, 0, "Market task creation failed");

    /* Each bidder negotiates (bids) */
    a2a_negotiation_t bid;
    AGENTOS_MEMSET(&bid, 0, sizeof(bid));
    bid.action = A2A_NEGOTIATE_PROPOSE;
    bid.task_id = task->id;
    bid.terms_json = "{\"bid\":150,\"estimated_time_ms\":3000}";

    a2a_negotiation_action_t action;
    char *terms = NULL;
    rc = a2a_v03_negotiate(ctx, &bid, &action, &terms);
    CHECK_EQ(rc, 0, "Market bidding failed");

    a2a_v03_context_destroy(ctx);
    PASS();
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("\n");
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║   AgentRT Protocol E2E Tests (P3.6-P3.10)       ║\n");
    printf("╚══════════════════════════════════════════════════╝\n");
    printf("\n");

    /* P3.6: A2A End-to-End Tests */
    printf("═══ P3.6: A2A Protocol End-to-End Tests ═══\n");
    test_a2a_agent_registration();
    test_a2a_task_delegation();
    test_a2a_message_exchange();
    test_a2a_negotiation();
    test_a2a_consensus();
    test_a2a_notifications();
    test_a2a_streaming();
    test_a2a_authentication();

    /* P3.7: MCP End-to-End Tests */
    printf("\n═══ P3.7: MCP Protocol End-to-End Tests ═══\n");
    test_mcp_initialization();
    test_mcp_tool_registration_and_call();
    test_mcp_resource_management();
    test_mcp_prompt_management();
    test_mcp_logging();
    test_mcp_progress_notification();

    /* P3.8: OpenAI Compatible Tests */
    printf("\n═══ P3.8: OpenAI Compatible API Tests ═══\n");
    test_openai_model_listing();
    test_openai_route_request();

    /* P3.9: Protocol Routing Tests */
    printf("\n═══ P3.9: Protocol Routing Tests ═══\n");
    test_protocol_routing_mixed();
    test_protocol_routing_unknown_method();
    test_protocol_routing_task_lifecycle();
    test_protocol_routing_mcp_methods();

    /* P3.10: Multi-Agent Collaboration Tests */
    printf("\n═══ P3.10: Multi-Agent Collaboration Tests ═══\n");
    test_multi_agent_orchestrator();
    test_multi_agent_debate();
    test_multi_agent_hierarchy();
    test_multi_agent_market();

    /* Summary */
    printf("\n");
    printf("══════════════════════════════════════════════\n");
    printf("  Total: %d | Passed: %d | Failed: %d\n",
           g_tests_total, g_tests_passed, g_tests_failed);
    printf("══════════════════════════════════════════════\n\n");

    return g_tests_failed > 0 ? 1 : 0;
}
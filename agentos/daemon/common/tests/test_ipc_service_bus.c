/**
 * @file test_ipc_service_bus.c
 * @brief IPC服务总线模块单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "ipc_service_bus.h"
#include "error.h"
#include "memory_compat.h"
#include "platform.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define RUN_TEST(fn) do { \
    fn(); \
    g_tests_passed++; \
} while (0)

static int dummy_handler(ipc_bus_channel_t channel,
                         const ipc_bus_message_t *message, void *user_data)
{
    (void)channel;
    (void)message;
    (void)user_data;
    return AGENTOS_OK;
}

static int dummy_handler2(ipc_bus_channel_t channel,
                          const ipc_bus_message_t *message, void *user_data)
{
    (void)channel;
    (void)message;
    (void)user_data;
    return AGENTOS_OK;
}

static void test_ipc_bus_create_null_name(void)
{
    printf("  test_ipc_bus_create_null_name...\n");

    ipc_service_bus_t bus = ipc_service_bus_create(NULL, NULL);
    assert(bus == NULL);

    printf("    PASSED\n");
}

static void test_ipc_bus_create_valid(void)
{
    printf("  test_ipc_bus_create_valid...\n");

    ipc_service_bus_t bus = ipc_service_bus_create("test_bus", NULL);
    assert(bus != NULL);

    ipc_service_bus_destroy(bus);

    printf("    PASSED\n");
}

static void test_ipc_bus_destroy_null(void)
{
    printf("  test_ipc_bus_destroy_null...\n");

    ipc_service_bus_destroy(NULL);

    printf("    PASSED\n");
}

static void test_ipc_bus_start_stop(void)
{
    printf("  test_ipc_bus_start_stop...\n");

    ipc_service_bus_t bus = ipc_service_bus_create("start_stop_bus", NULL);
    assert(bus != NULL);

    agentos_error_t ret = ipc_service_bus_start(bus);
    (void)ret;
    assert(ret == AGENTOS_OK);
    assert(ipc_service_bus_is_running(bus) == true);

    ret = ipc_service_bus_stop(bus);
    assert(ret == AGENTOS_OK);
    assert(ipc_service_bus_is_running(bus) == false);

    ipc_service_bus_destroy(bus);

    printf("    PASSED\n");
}

static void test_ipc_bus_double_start(void)
{
    printf("  test_ipc_bus_double_start...\n");

    ipc_service_bus_t bus = ipc_service_bus_create("double_start_bus", NULL);
    assert(bus != NULL);

    agentos_error_t ret = ipc_service_bus_start(bus);
    (void)ret;
    assert(ret == AGENTOS_OK);

    ret = ipc_service_bus_start(bus);
    assert(ret == AGENTOS_OK || ret == AGENTOS_EBUSY);  /* 幂等或已运行 */

    ipc_service_bus_stop(bus);
    ipc_service_bus_destroy(bus);

    printf("    PASSED\n");
}

static void test_ipc_bus_channel_create(void)
{
    printf("  test_ipc_bus_channel_create...\n");

    ipc_service_bus_t bus = ipc_service_bus_create("channel_bus", NULL);
    assert(bus != NULL);

    ipc_bus_channel_config_t config;
    memset(&config, 0, sizeof(config));
    strncpy(config.name, "test_channel", IPC_BUS_CHANNEL_NAME_LEN - 1);
    config.default_protocol = IPC_BUS_PROTO_JSON_RPC;
    config.timeout_ms = IPC_BUS_DEFAULT_TIMEOUT_MS;

    ipc_bus_channel_t channel = ipc_bus_channel_create(bus, &config);
    assert(channel != NULL);

    const char *name = ipc_bus_channel_get_name(channel);
    assert(name != NULL);
    assert(strcmp(name, "test_channel") == 0);

    ipc_bus_channel_destroy(channel);
    ipc_service_bus_destroy(bus);

    printf("    PASSED\n");
}

static void test_ipc_bus_channel_null_bus(void)
{
    printf("  test_ipc_bus_channel_null_bus...\n");

    ipc_bus_channel_config_t config;
    memset(&config, 0, sizeof(config));
    strncpy(config.name, "null_bus_ch", IPC_BUS_CHANNEL_NAME_LEN - 1);

    ipc_bus_channel_t channel = ipc_bus_channel_create(NULL, &config);
    assert(channel == NULL);

    printf("    PASSED\n");
}

static void test_ipc_bus_register_handler(void)
{
    printf("  test_ipc_bus_register_handler...\n");

    ipc_service_bus_t bus = ipc_service_bus_create("handler_bus", NULL);
    assert(bus != NULL);

    agentos_error_t ret = ipc_service_bus_register_handler(bus, dummy_handler, (void *)0x1);
    (void)ret;
    assert(ret == AGENTOS_OK);

    ret = ipc_service_bus_unregister_handler(bus, dummy_handler);
    assert(ret == AGENTOS_OK);

    ipc_service_bus_destroy(bus);

    printf("    PASSED\n");
}

static void test_ipc_bus_unregister_handler(void)
{
    printf("  test_ipc_bus_unregister_handler...\n");

    ipc_service_bus_t bus = ipc_service_bus_create("unreg_bus", NULL);
    assert(bus != NULL);

    agentos_error_t ret = ipc_service_bus_register_handler(bus, dummy_handler, NULL);
    (void)ret;
    assert(ret == AGENTOS_OK);

    ret = ipc_service_bus_unregister_handler(bus, dummy_handler);
    assert(ret == AGENTOS_OK);

    ret = ipc_service_bus_unregister_handler(bus, dummy_handler);
    assert(ret == AGENTOS_OK || ret == AGENTOS_ENOENT);  /* 成功或已不存在 */

    ipc_service_bus_destroy(bus);

    printf("    PASSED\n");
}

static void test_ipc_bus_get_name(void)
{
    printf("  test_ipc_bus_get_name...\n");

    ipc_service_bus_t bus = ipc_service_bus_create("my_named_bus", NULL);
    assert(bus != NULL);

    const char *name = ipc_service_bus_get_name(bus);
    (void)name;
    assert(name != NULL);
    assert(strcmp(name, "my_named_bus") == 0);

    ipc_service_bus_destroy(bus);

    name = ipc_service_bus_get_name(NULL);
    assert(name == NULL);

    printf("    PASSED\n");
}

static void test_ipc_bus_is_running(void)
{
    printf("  test_ipc_bus_is_running...\n");

    ipc_service_bus_t bus = ipc_service_bus_create("running_bus", NULL);
    assert(bus != NULL);

    assert(ipc_service_bus_is_running(bus) == false);
    assert(ipc_service_bus_is_running(NULL) == false);

    ipc_service_bus_start(bus);
    assert(ipc_service_bus_is_running(bus) == true);

    ipc_service_bus_stop(bus);
    assert(ipc_service_bus_is_running(bus) == false);

    ipc_service_bus_destroy(bus);

    printf("    PASSED\n");
}

static void test_ipc_bus_stats(void)
{
    printf("  test_ipc_bus_stats...\n");

    ipc_service_bus_t bus = ipc_service_bus_create("stats_bus", NULL);
    assert(bus != NULL);

    ipc_bus_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    agentos_error_t ret = ipc_service_bus_get_stats(bus, &stats);
    (void)ret;
    assert(ret == AGENTOS_OK);
    assert(stats.messages_sent == 0);
    assert(stats.messages_received == 0);
    assert(stats.errors == 0);

    ret = ipc_service_bus_reset_stats(bus);
    assert(ret == AGENTOS_OK);

    ret = ipc_service_bus_get_stats(NULL, &stats);
    assert(ret != AGENTOS_OK);

    ipc_service_bus_destroy(bus);

    printf("    PASSED\n");
}

static void test_ipc_bus_max_handlers(void)
{
    printf("  test_ipc_bus_max_handlers...\n");

    ipc_service_bus_t bus = ipc_service_bus_create("max_handler_bus", NULL);
    assert(bus != NULL);

#define MAX_HANDLER_TEST_COUNT 256

    agentos_error_t ret;
    int registered = 0;

    for (int i = 0; i < MAX_HANDLER_TEST_COUNT; i++) {
        ret = ipc_service_bus_register_handler(bus, dummy_handler, (void *)(intptr_t)(i + 1));
        if (ret != AGENTOS_OK) {
            break;
        }
        registered++;
    }

    assert(registered > 0);

    for (int i = 0; i < registered; i++) {
        ret = ipc_service_bus_unregister_handler(bus, dummy_handler);
        assert(ret == AGENTOS_OK);
    }

    ipc_service_bus_destroy(bus);

    printf("    PASSED\n");
}

static void test_ipc_bus_lifecycle_full(void)
{
    printf("  test_ipc_bus_lifecycle_full...\n");

    ipc_service_bus_t bus = ipc_service_bus_create("lifecycle_bus", NULL);
    assert(bus != NULL);
    assert(ipc_service_bus_is_running(bus) == false);

    ipc_bus_channel_config_t ch_cfg;
    memset(&ch_cfg, 0, sizeof(ch_cfg));
    strncpy(ch_cfg.name, "lc_channel", IPC_BUS_CHANNEL_NAME_LEN - 1);
    ch_cfg.default_protocol = IPC_BUS_PROTO_MCP;

    ipc_bus_channel_t channel = ipc_bus_channel_create(bus, &ch_cfg);
    assert(channel != NULL);

    agentos_error_t ret = ipc_service_bus_register_handler(bus, dummy_handler, NULL);
    (void)ret;
    assert(ret == AGENTOS_OK);

    ret = ipc_service_bus_register_handler(bus, dummy_handler2, (void *)0xDEAD);
    assert(ret == AGENTOS_OK);

    ret = ipc_service_bus_start(bus);
    assert(ret == AGENTOS_OK);
    assert(ipc_service_bus_is_running(bus) == true);

    ipc_bus_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    ret = ipc_service_bus_get_stats(bus, &stats);
    assert(ret == AGENTOS_OK);

    ret = ipc_service_bus_stop(bus);
    assert(ret == AGENTOS_OK);
    assert(ipc_service_bus_is_running(bus) == false);

    ret = ipc_service_bus_unregister_handler(bus, dummy_handler2);
    assert(ret == AGENTOS_OK);

    ret = ipc_service_bus_unregister_handler(bus, dummy_handler);
    assert(ret == AGENTOS_OK);

    ipc_bus_channel_destroy(channel);
    ipc_service_bus_destroy(bus);

    printf("    PASSED\n");
}

static void test_ipc_bus_destroy_while_running(void)
{
    printf("  test_ipc_bus_destroy_while_running...\n");

    ipc_service_bus_t bus = ipc_service_bus_create("running_destroy_bus", NULL);
    assert(bus != NULL);

    agentos_error_t ret = ipc_service_bus_start(bus);
    assert(ret == AGENTOS_OK);
    assert(ipc_service_bus_is_running(bus) == true);

    ipc_service_bus_destroy(bus);

    printf("    PASSED\n");
}

int main(void)
{
    printf("=========================================\n");
    printf("  IPC Service Bus Unit Tests\n");
    printf("=========================================\n\n");

    RUN_TEST(test_ipc_bus_create_null_name);
    RUN_TEST(test_ipc_bus_create_valid);
    RUN_TEST(test_ipc_bus_destroy_null);
    RUN_TEST(test_ipc_bus_start_stop);
    RUN_TEST(test_ipc_bus_double_start);
    RUN_TEST(test_ipc_bus_channel_create);
    RUN_TEST(test_ipc_bus_channel_null_bus);
    RUN_TEST(test_ipc_bus_register_handler);
    RUN_TEST(test_ipc_bus_unregister_handler);
    RUN_TEST(test_ipc_bus_get_name);
    RUN_TEST(test_ipc_bus_is_running);
    RUN_TEST(test_ipc_bus_stats);
    RUN_TEST(test_ipc_bus_max_handlers);
    RUN_TEST(test_ipc_bus_lifecycle_full);
    RUN_TEST(test_ipc_bus_destroy_while_running);

    printf("\n=========================================\n");
    printf("  Results: %d passed, %d failed, %d total\n",
           g_tests_passed, g_tests_failed, g_tests_passed + g_tests_failed);
    printf("=========================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}

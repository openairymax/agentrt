/**
 * @file test_service.c
 * @brief Tool 服务核心功能单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "tool_service.h"
#include "service.h"

static void test_service_create_destroy(void) {
    printf("  test_service_create_destroy...\n");

    tool_service_t* svc = tool_service_create(NULL);
    assert(svc != NULL);

    tool_service_destroy(svc);

    printf("    PASSED\n");
}

static void test_service_config(void) {
    printf("  test_service_config...\n");

    tool_service_config_t manager = {
        .max_concurrent = 10,
        .default_timeout_ms = 30000,
        .cache_enabled = 1,
        .cache_ttl_sec = 3600,
        .log_level = 1
    };

    tool_service_t* svc = tool_service_create(&manager);
    assert(svc != NULL);

    tool_service_destroy(svc);

    printf("    PASSED\n");
}

static void test_service_register_tool(void) {
    printf("  test_service_register_tool...\n");

    tool_service_t* svc = tool_service_create(NULL);
    assert(svc != NULL);

    tool_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.name = "test_tool";
    meta.version = "1.0.0";
    meta.description = "A test tool";
    meta.executable = "/usr/bin/echo";

    int ret = tool_service_register(svc, &meta);
    assert(ret == 0);

    tool_service_destroy(svc);

    printf("    PASSED\n");
}

static void test_service_list_tools(void) {
    printf("  test_service_list_tools...\n");

    tool_service_t* svc = tool_service_create(NULL);
    assert(svc != NULL);

    tool_meta_t meta1;
    memset(&meta1, 0, sizeof(meta1));
    meta1.name = "tool1";
    meta1.version = "1.0.0";
    meta1.executable = "/usr/bin/echo";

    tool_meta_t meta2;
    memset(&meta2, 0, sizeof(meta2));
    meta2.name = "tool2";
    meta2.version = "2.0.0";
    meta2.executable = "/usr/bin/cat";

    tool_service_register(svc, &meta1);
    tool_service_register(svc, &meta2);

    char** tools = NULL;
    size_t count = 0;
    int ret = tool_service_list(svc, &tools, &count);
    assert(ret == 0);
    assert(count == 2);

    for (size_t i = 0; i < count; i++) {
        free(tools[i]);
    }
    free(tools);

    tool_service_destroy(svc);

    printf("    PASSED\n");
}

static void test_service_get_tool(void) {
    printf("  test_service_get_tool...\n");

    tool_service_t* svc = tool_service_create(NULL);
    assert(svc != NULL);

    tool_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.name = "get_test_tool";
    meta.version = "1.0.0";
    meta.executable = "/usr/bin/echo";

    tool_service_register(svc, &meta);

    const tool_meta_t* found = tool_service_get(svc, "get_test_tool");
    assert(found != NULL);
    assert(strcmp(found->name, "get_test_tool") == 0);

    tool_service_destroy(svc);

    printf("    PASSED\n");
}

static void test_service_unregister_tool(void) {
    printf("  test_service_unregister_tool...\n");

    tool_service_t* svc = tool_service_create(NULL);
    assert(svc != NULL);

    tool_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.name = "unregister_test";
    meta.version = "1.0.0";
    meta.executable = "/usr/bin/echo";

    tool_service_register(svc, &meta);

    int ret = tool_service_unregister(svc, "unregister_test");
    assert(ret == 0);

    const tool_meta_t* found = tool_service_get(svc, "unregister_test");
    assert(found == NULL);

    tool_service_destroy(svc);

    printf("    PASSED\n");
}

int main(void) {
    printf("=========================================\n");
    printf("  Tool Service Unit Tests\n");
    printf("=========================================\n");

    test_service_create_destroy();
    test_service_config();
    test_service_register_tool();
    test_service_list_tools();
    test_service_get_tool();
    test_service_unregister_tool();

    printf("\n✅ All tool service tests PASSED\n");
    return 0;
}
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

static void test_service_create_destroy(void) {
    printf("  test_service_create_destroy...\n");

    tool_service_t* svc = tool_service_create(NULL);
    assert(svc != NULL);

    tool_service_destroy(svc);

    printf("    PASSED\n");
}

static void test_service_register_tool(void) {
    printf("  test_service_register_tool...\n");

    tool_service_t* svc = tool_service_create(NULL);
    assert(svc != NULL);

    tool_metadata_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.id = "test_tool";
    meta.name = "test_tool";
    meta.description = "A test tool";
    meta.executable = "/usr/bin/echo";
    meta.timeout_sec = 10;

    int ret = tool_service_register(svc, &meta);
    assert(ret == 0);

    tool_service_destroy(svc);

    printf("    PASSED\n");
}

static void test_service_list_tools(void) {
    printf("  test_service_list_tools...\n");

    tool_service_t* svc = tool_service_create(NULL);
    assert(svc != NULL);

    tool_metadata_t meta1;
    memset(&meta1, 0, sizeof(meta1));
    meta1.id = "tool1";
    meta1.name = "tool1";
    meta1.executable = "/usr/bin/echo";
    meta1.timeout_sec = 10;

    tool_metadata_t meta2;
    memset(&meta2, 0, sizeof(meta2));
    meta2.id = "tool2";
    meta2.name = "tool2";
    meta2.executable = "/usr/bin/cat";
    meta2.timeout_sec = 10;

    tool_service_register(svc, &meta1);
    tool_service_register(svc, &meta2);

    char* tools_json = tool_service_list(svc);
    assert(tools_json != NULL);
    printf("    Tools: %s\n", tools_json);
    free(tools_json);

    tool_service_destroy(svc);

    printf("    PASSED\n");
}

static void test_service_get_tool(void) {
    printf("  test_service_get_tool...\n");

    tool_service_t* svc = tool_service_create(NULL);
    assert(svc != NULL);

    tool_metadata_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.id = "get_test_tool";
    meta.name = "get_test_tool";
    meta.executable = "/usr/bin/echo";
    meta.timeout_sec = 10;

    tool_service_register(svc, &meta);

    tool_metadata_t* found = tool_service_get(svc, "get_test_tool");
    assert(found != NULL);
    assert(strcmp(found->name, "get_test_tool") == 0);

    tool_service_destroy(svc);

    printf("    PASSED\n");
}

static void test_service_unregister_tool(void) {
    printf("  test_service_unregister_tool...\n");

    tool_service_t* svc = tool_service_create(NULL);
    assert(svc != NULL);

    tool_metadata_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.id = "unregister_test";
    meta.name = "unregister_test";
    meta.executable = "/usr/bin/echo";
    meta.timeout_sec = 10;

    tool_service_register(svc, &meta);

    int ret = tool_service_unregister(svc, "unregister_test");
    assert(ret == 0);

    tool_metadata_t* found = tool_service_get(svc, "unregister_test");
    assert(found == NULL);

    tool_service_destroy(svc);

    printf("    PASSED\n");
}

int main(void) {
    printf("=========================================\n");
    printf("  Tool Service Unit Tests\n");
    printf("=========================================\n");

    test_service_create_destroy();
    test_service_register_tool();
    test_service_list_tools();
    test_service_get_tool();
    test_service_unregister_tool();

    printf("\nAll tool service tests PASSED\n");
    return 0;
}

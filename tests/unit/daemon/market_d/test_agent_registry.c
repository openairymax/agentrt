/**
 * @file test_agent_registry.c
 * @brief Agent注册表单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "market_service.h"

static void test_registry_create_destroy(void) {
    printf("  test_registry_create_destroy...\n");

    agent_registry_t* reg = agent_registry_create();
    assert(reg != NULL);

    agent_registry_destroy(reg);

    printf("    PASSED\n");
}

static void test_registry_register_agent(void) {
    printf("  test_registry_register_agent...\n");

    agent_registry_t* reg = agent_registry_create();
    assert(reg != NULL);

    agent_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.id = "test_agent_001";
    meta.name = "Test Agent";
    meta.version = "1.0.0";
    meta.description = "A test agent";

    int ret = agent_registry_register(reg, &meta);
    assert(ret == 0);

    agent_registry_destroy(reg);

    printf("    PASSED\n");
}

static void test_registry_get_agent(void) {
    printf("  test_registry_get_agent...\n");

    agent_registry_t* reg = agent_registry_create();
    assert(reg != NULL);

    agent_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.id = "get_test_agent";
    meta.name = "Get Test Agent";
    meta.version = "1.0.0";

    agent_registry_register(reg, &meta);

    const agent_meta_t* found = agent_registry_get(reg, "get_test_agent");
    assert(found != NULL);
    assert(strcmp(found->id, "get_test_agent") == 0);

    agent_registry_destroy(reg);

    printf("    PASSED\n");
}

static void test_registry_unregister_agent(void) {
    printf("  test_registry_unregister_agent...\n");

    agent_registry_t* reg = agent_registry_create();
    assert(reg != NULL);

    agent_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.id = "unregister_test";
    meta.name = "Unregister Test";
    meta.version = "1.0.0";

    agent_registry_register(reg, &meta);

    int ret = agent_registry_unregister(reg, "unregister_test");
    assert(ret == 0);

    const agent_meta_t* found = agent_registry_get(reg, "unregister_test");
    assert(found == NULL);

    agent_registry_destroy(reg);

    printf("    PASSED\n");
}

static void test_registry_list_agents(void) {
    printf("  test_registry_list_agents...\n");

    agent_registry_t* reg = agent_registry_create();
    assert(reg != NULL);

    agent_meta_t meta1;
    memset(&meta1, 0, sizeof(meta1));
    meta1.id = "list_agent_1";
    meta1.name = "List Agent 1";
    meta1.version = "1.0.0";

    agent_meta_t meta2;
    memset(&meta2, 0, sizeof(meta2));
    meta2.id = "list_agent_2";
    meta2.name = "List Agent 2";
    meta2.version = "1.0.0";

    agent_registry_register(reg, &meta1);
    agent_registry_register(reg, &meta2);

    char** agents = NULL;
    size_t count = 0;
    int ret = agent_registry_list(reg, &agents, &count);
    assert(ret == 0);
    assert(count == 2);

    for (size_t i = 0; i < count; i++) {
        free(agents[i]);
    }
    free(agents);

    agent_registry_destroy(reg);

    printf("    PASSED\n");
}

static void test_registry_search_agents(void) {
    printf("  test_registry_search_agents...\n");

    agent_registry_t* reg = agent_registry_create();
    assert(reg != NULL);

    agent_meta_t meta1;
    memset(&meta1, 0, sizeof(meta1));
    meta1.id = "search_agent_1";
    meta1.name = "Search Test Agent";
    meta1.version = "1.0.0";
    meta1.description = "A searchable agent";

    agent_meta_t meta2;
    memset(&meta2, 0, sizeof(meta2));
    meta2.id = "search_agent_2";
    meta2.name = "Another Agent";
    meta2.version = "1.0.0";
    meta2.description = "Not searchable";

    agent_registry_register(reg, &meta1);
    agent_registry_register(reg, &meta2);

    char** results = NULL;
    size_t count = 0;
    int ret = agent_registry_search(reg, "Search", &results, &count);
    assert(ret == 0);
    assert(count >= 1);

    for (size_t i = 0; i < count; i++) {
        free(results[i]);
    }
    free(results);

    agent_registry_destroy(reg);

    printf("    PASSED\n");
}

static void test_version_comparison(void) {
    printf("  test_version_comparison...\n");

    assert(agent_registry_compare_versions("1.0.0", "1.0.0") == 0);
    assert(agent_registry_compare_versions("1.0.1", "1.0.0") > 0);
    assert(agent_registry_compare_versions("1.0.0", "1.0.1") < 0);
    assert(agent_registry_compare_versions("2.0.0", "1.9.9") > 0);
    assert(agent_registry_compare_versions("1.10.0", "1.9.0") > 0);

    printf("    PASSED\n");
}

int main(void) {
    printf("=========================================\n");
    printf("  Agent Registry Unit Tests\n");
    printf("=========================================\n");

    test_registry_create_destroy();
    test_registry_register_agent();
    test_registry_get_agent();
    test_registry_unregister_agent();
    test_registry_list_agents();
    test_registry_search_agents();
    test_version_comparison();

    printf("\n✅ All agent registry tests PASSED\n");
    return 0;
}

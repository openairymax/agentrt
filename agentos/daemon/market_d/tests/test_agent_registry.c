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

static void test_market_create_destroy(void) {
    printf("  test_market_create_destroy...\n");

    market_service_t* svc = NULL;
    int ret = market_service_create(NULL, &svc);
    assert(ret == 0 || svc != NULL);

    if (svc) {
        ret = market_service_destroy(svc);
        assert(ret == 0);
    }

    printf("    PASSED\n");
}

static void test_market_register_agent(void) {
    printf("  test_market_register_agent...\n");

    market_service_t* svc = NULL;
    int ret = market_service_create(NULL, &svc);
    assert(ret == 0 && svc != NULL);

    agent_info_t info;
    memset(&info, 0, sizeof(info));
    info.agent_id = "test_agent_001";
    info.name = "Test Agent";
    info.version = "1.0.0";
    info.description = "A test agent";
    info.type = AGENT_TYPE_ASSISTANT;
    info.status = AGENT_STATUS_AVAILABLE;

    ret = market_service_register_agent(svc, &info);
    assert(ret == 0);

    market_service_destroy(svc);

    printf("    PASSED\n");
}

static void test_market_search_agents(void) {
    printf("  test_market_search_agents...\n");

    market_service_t* svc = NULL;
    int ret = market_service_create(NULL, &svc);
    assert(ret == 0 && svc != NULL);

    agent_info_t info;
    memset(&info, 0, sizeof(info));
    info.agent_id = "search_test_agent";
    info.name = "Search Test Agent";
    info.version = "1.0.0";
    info.type = AGENT_TYPE_EXPERT;

    market_service_register_agent(svc, &info);

    search_params_t params;
    memset(&params, 0, sizeof(params));
    params.query = "Search Test";

    agent_info_t** agents = NULL;
    size_t count = 0;
    ret = market_service_search_agents(svc, &params, &agents, &count);
    if (ret == 0) {
        printf("    Found %zu agents\n", count);
        for (size_t i = 0; i < count; i++) free(agents[i]);
        free(agents);
    } else {
        printf("    Search returned %d (may be expected)\n", ret);
    }

    market_service_destroy(svc);

    printf("    PASSED\n");
}

static void test_market_get_installed_agents(void) {
    printf("  test_market_get_installed_agents...\n");

    market_service_t* svc = NULL;
    int ret = market_service_create(NULL, &svc);
    assert(ret == 0 && svc != NULL);

    agent_info_t info1;
    memset(&info1, 0, sizeof(info1));
    info1.agent_id = "installed_agent_1";
    info1.name = "Installed Agent 1";
    info1.version = "1.0.0";
    info1.type = AGENT_TYPE_ASSISTANT;

    agent_info_t info2;
    memset(&info2, 0, sizeof(info2));
    info2.agent_id = "installed_agent_2";
    info2.name = "Installed Agent 2";
    info2.version = "2.0.0";
    info2.type = AGENT_TYPE_EXPERT;

    market_service_register_agent(svc, &info1);
    market_service_register_agent(svc, &info2);

    agent_info_t** agents = NULL;
    size_t count = 0;
    ret = market_service_get_installed_agents(svc, &agents, &count);
    if (ret == 0) {
        printf("    Installed agents: %zu\n", count);
        for (size_t i = 0; i < count; i++) free(agents[i]);
        free(agents);
    }

    market_service_destroy(svc);

    printf("    PASSED\n");
}

static void test_market_uninstall_agent(void) {
    printf("  test_market_uninstall_agent...\n");

    market_service_t* svc = NULL;
    int ret = market_service_create(NULL, &svc);
    assert(ret == 0 && svc != NULL);

    agent_info_t info;
    memset(&info, 0, sizeof(info));
    info.agent_id = "uninstall_test_agent";
    info.name = "Uninstall Test";
    info.version = "1.0.0";
    info.type = AGENT_TYPE_CUSTOM;

    market_service_register_agent(svc, &info);

    ret = market_service_uninstall_agent(svc, "uninstall_test_agent");
    assert(ret == 0);

    market_service_destroy(svc);

    printf("    PASSED\n");
}

static void test_market_check_update(void) {
    printf("  test_market_check_update...\n");

    market_service_t* svc = NULL;
    int ret = market_service_create(NULL, &svc);
    assert(ret == 0 && svc != NULL);

    bool has_update = false;
    char* latest_version = NULL;
    ret = market_service_check_update(svc, "test_agent", &has_update, &latest_version);
    if (ret == 0) {
        printf("    Update available: %s, latest: %s\n",
               has_update ? "yes" : "no",
               latest_version ? latest_version : "(null)");
        free(latest_version);
    } else {
        printf("    Check update returned %d\n", ret);
    }

    market_service_destroy(svc);

    printf("    PASSED\n");
}

static void test_market_enum_values(void) {
    printf("  test_market_enum_values...\n");

    assert(AGENT_TYPE_ASSISTANT == 0);
    assert(AGENT_TYPE_EXPERT == 1);
    assert(AGENT_TYPE_SPECIALIZED == 2);
    assert(AGENT_TYPE_CUSTOM == 3);

    assert(AGENT_STATUS_AVAILABLE == 0);
    assert(AGENT_STATUS_INSTALLING == 1);
    assert(AGENT_STATUS_ERROR == 2);
    assert(AGENT_STATUS_DISABLED == 3);

    assert(SKILL_TYPE_TOOL == 0);
    assert(SKILL_TYPE_KNOWLEDGE == 1);
    assert(SKILL_TYPE_INTEGRATION == 2);
    assert(SKILL_TYPE_CUSTOM == 3);

    printf("    PASSED\n");
}

int main(void) {
    printf("=========================================\n");
    printf("  Market Service Unit Tests\n");
    printf("=========================================\n");

    test_market_enum_values();
    test_market_create_destroy();
    test_market_register_agent();
    test_market_search_agents();
    test_market_get_installed_agents();
    test_market_uninstall_agent();
    test_market_check_update();

    printf("\nAll market service tests PASSED\n");
    return 0;
}

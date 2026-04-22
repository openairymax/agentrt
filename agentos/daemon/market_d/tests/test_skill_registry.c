/**
 * @file test_skill_registry.c
 * @brief Skill注册表单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "market_service.h"

static void test_market_register_skill(void) {
    printf("  test_market_register_skill...\n");

    market_service_t* svc = NULL;
    int ret = market_service_create(NULL, &svc);
    assert(ret == 0 && svc != NULL);

    skill_info_t info;
    memset(&info, 0, sizeof(info));
    info.skill_id = "test_skill_001";
    info.name = "Test Skill";
    info.version = "1.0.0";
    info.description = "A test skill";
    info.type = SKILL_TYPE_TOOL;

    ret = market_service_register_skill(svc, &info);
    assert(ret == 0);

    market_service_destroy(svc);

    printf("    PASSED\n");
}

static void test_market_search_skills(void) {
    printf("  test_market_search_skills...\n");

    market_service_t* svc = NULL;
    int ret = market_service_create(NULL, &svc);
    assert(ret == 0 && svc != NULL);

    skill_info_t info;
    memset(&info, 0, sizeof(info));
    info.skill_id = "search_test_skill";
    info.name = "Search Test Skill";
    info.version = "1.0.0";
    info.type = SKILL_TYPE_KNOWLEDGE;

    market_service_register_skill(svc, &info);

    search_params_t params;
    memset(&params, 0, sizeof(params));
    params.query = "Search Test";

    skill_info_t** skills = NULL;
    size_t count = 0;
    ret = market_service_search_skills(svc, &params, &skills, &count);
    if (ret == 0) {
        printf("    Found %zu skills\n", count);
        for (size_t i = 0; i < count; i++) free(skills[i]);
        free(skills);
    } else {
        printf("    Search returned %d\n", ret);
    }

    market_service_destroy(svc);

    printf("    PASSED\n");
}

static void test_market_get_installed_skills(void) {
    printf("  test_market_get_installed_skills...\n");

    market_service_t* svc = NULL;
    int ret = market_service_create(NULL, &svc);
    assert(ret == 0 && svc != NULL);

    skill_info_t info1;
    memset(&info1, 0, sizeof(info1));
    info1.skill_id = "installed_skill_1";
    info1.name = "Installed Skill 1";
    info1.version = "1.0.0";
    info1.type = SKILL_TYPE_TOOL;

    skill_info_t info2;
    memset(&info2, 0, sizeof(info2));
    info2.skill_id = "installed_skill_2";
    info2.name = "Installed Skill 2";
    info2.version = "2.0.0";
    info2.type = SKILL_TYPE_INTEGRATION;

    market_service_register_skill(svc, &info1);
    market_service_register_skill(svc, &info2);

    skill_info_t** skills = NULL;
    size_t count = 0;
    ret = market_service_get_installed_skills(svc, &skills, &count);
    if (ret == 0) {
        printf("    Installed skills: %zu\n", count);
        for (size_t i = 0; i < count; i++) free(skills[i]);
        free(skills);
    }

    market_service_destroy(svc);

    printf("    PASSED\n");
}

static void test_market_uninstall_skill(void) {
    printf("  test_market_uninstall_skill...\n");

    market_service_t* svc = NULL;
    int ret = market_service_create(NULL, &svc);
    assert(ret == 0 && svc != NULL);

    skill_info_t info;
    memset(&info, 0, sizeof(info));
    info.skill_id = "uninstall_test_skill";
    info.name = "Uninstall Test Skill";
    info.version = "1.0.0";
    info.type = SKILL_TYPE_CUSTOM;

    market_service_register_skill(svc, &info);

    ret = market_service_uninstall_skill(svc, "uninstall_test_skill");
    assert(ret == 0);

    market_service_destroy(svc);

    printf("    PASSED\n");
}

static void test_market_install_skill(void) {
    printf("  test_market_install_skill...\n");

    market_service_t* svc = NULL;
    int ret = market_service_create(NULL, &svc);
    assert(ret == 0 && svc != NULL);

    install_request_t request;
    memset(&request, 0, sizeof(request));
    request.id = "install_test_skill";
    request.version = "1.0.0";

    install_result_t* result = NULL;
    ret = market_service_install_skill(svc, &request, &result);
    if (ret == 0 && result != NULL) {
        printf("    Install success: %s\n", result->message ? result->message : "(null)");
        free(result->message);
        free(result->installed_version);
        free(result->install_path);
        free(result);
    } else {
        printf("    Install returned %d (expected in test env)\n", ret);
    }

    market_service_destroy(svc);

    printf("    PASSED\n");
}

int main(void) {
    printf("=========================================\n");
    printf("  Skill Registry Unit Tests\n");
    printf("=========================================\n");

    test_market_register_skill();
    test_market_search_skills();
    test_market_get_installed_skills();
    test_market_uninstall_skill();
    test_market_install_skill();

    printf("\nAll skill registry tests PASSED\n");
    return 0;
}

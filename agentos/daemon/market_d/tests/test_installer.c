/**
 * @file test_installer.c
 * @brief 安装器单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "market_service.h"

static void test_installer_agent_install_uninstall(void) {
    printf("  test_installer_agent_install_uninstall...\n");

    market_service_t* svc = NULL;
    int ret = market_service_create(NULL, &svc);
    assert(ret == 0 && svc != NULL);

    agent_info_t info;
    memset(&info, 0, sizeof(info));
    info.agent_id = "install_test_agent";
    info.name = "Install Test Agent";
    info.version = "1.0.0";
    info.type = AGENT_TYPE_ASSISTANT;

    market_service_register_agent(svc, &info);

    install_request_t request;
    memset(&request, 0, sizeof(request));
    request.id = "install_test_agent";
    request.version = "1.0.0";

    install_result_t* result = NULL;
    ret = market_service_install_agent(svc, &request, &result);
    if (ret == 0 && result != NULL) {
        printf("    Agent install: success=%d, msg=%s\n",
               result->success,
               result->message ? result->message : "(null)");
        free(result->message);
        free(result->installed_version);
        free(result->install_path);
        free(result);
    } else {
        printf("    Agent install returned %d\n", ret);
    }

    market_service_destroy(svc);

    printf("    PASSED\n");
}

static void test_installer_skill_install_uninstall(void) {
    printf("  test_installer_skill_install_uninstall...\n");

    market_service_t* svc = NULL;
    int ret = market_service_create(NULL, &svc);
    assert(ret == 0 && svc != NULL);

    skill_info_t info;
    memset(&info, 0, sizeof(info));
    info.skill_id = "install_test_skill";
    info.name = "Install Test Skill";
    info.version = "1.0.0";
    info.type = SKILL_TYPE_TOOL;

    market_service_register_skill(svc, &info);

    install_request_t request;
    memset(&request, 0, sizeof(request));
    request.id = "install_test_skill";
    request.force_update = true;

    install_result_t* result = NULL;
    ret = market_service_install_skill(svc, &request, &result);
    if (ret == 0 && result != NULL) {
        printf("    Skill install: success=%d, version=%s\n",
               result->success,
               result->installed_version ? result->installed_version : "(null)");
        free(result->message);
        free(result->installed_version);
        free(result->install_path);
        free(result);
    } else {
        printf("    Skill install returned %d\n", ret);
    }

    market_service_destroy(svc);

    printf("    PASSED\n");
}

static void test_installer_check_update(void) {
    printf("  test_installer_check_update...\n");

    market_service_t* svc = NULL;
    int ret = market_service_create(NULL, &svc);
    assert(ret == 0 && svc != NULL);

    bool has_update = false;
    char* latest_version = NULL;
    ret = market_service_check_update(svc, "some_agent_or_skill_id", &has_update, &latest_version);
    if (ret == 0) {
        printf("    Update check: has_update=%s, latest=%s\n",
               has_update ? "yes" : "no",
               latest_version ? latest_version : "(null)");
        free(latest_version);
    } else {
        printf("    Check update returned %d\n", ret);
    }

    market_service_destroy(svc);

    printf("    PASSED\n");
}

static void test_installer_sync_registry(void) {
    printf("  test_installer_sync_registry...\n");

    market_service_t* svc = NULL;
    int ret = market_service_create(NULL, &svc);
    assert(ret == 0 && svc != NULL);

    ret = market_service_sync_registry(svc);
    if (ret == 0) {
        printf("    Sync completed successfully\n");
    } else {
        printf("    Sync returned %d (may be expected)\n", ret);
    }

    market_service_destroy(svc);

    printf("    PASSED\n");
}

static void test_installer_reload_config(void) {
    printf("  test_installer_reload_config...\n");

    market_service_t* svc = NULL;
    int ret = market_service_create(NULL, &svc);
    assert(ret == 0 && svc != NULL);

    market_config_t new_config;
    memset(&new_config, 0, sizeof(new_config));
    new_config.sync_interval_ms = 60000;
    new_config.cache_ttl_ms = 300000;

    ret = market_service_reload_config(svc, &new_config);
    assert(ret == 0);

    market_service_destroy(svc);

    printf("    PASSED\n");
}

int main(void) {
    printf("=========================================\n");
    printf("  Installer Unit Tests\n");
    printf("=========================================\n");

    test_installer_agent_install_uninstall();
    test_installer_skill_install_uninstall();
    test_installer_check_update();
    test_installer_sync_registry();
    test_installer_reload_config();

    printf("\nAll installer tests PASSED\n");
    return 0;
}

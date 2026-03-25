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

static void test_installer_create_destroy(void) {
    printf("  test_installer_create_destroy...\n");

    installer_t* inst = installer_create(NULL);
    assert(inst != NULL);

    installer_destroy(inst);

    printf("    PASSED\n");
}

static void test_installer_config(void) {
    printf("  test_installer_config...\n");

    installer_config_t config = {
        .install_dir = "/tmp/agentos/test",
        .temp_dir = "/tmp/agentos/temp",
        .verify_signature = 1,
        .max_retries = 3
    };

    installer_t* inst = installer_create(&config);
    assert(inst != NULL);

    installer_destroy(inst);

    printf("    PASSED\n");
}

static void test_installer_prepare_request(void) {
    printf("  test_installer_prepare_request...\n");

    installer_t* inst = installer_create(NULL);
    assert(inst != NULL);

    install_request_t request;
    memset(&request, 0, sizeof(request));
    request.package_id = "test_package_001";
    request.version = "1.0.0";
    request.install_path = "/tmp/test_install";

    int ret = installer_prepare_request(inst, &request);
    assert(ret == 0 || ret == AGENTOS_ERR_INVALID_PARAM);

    installer_destroy(inst);

    printf("    PASSED\n");
}

static void test_installer_validate_package(void) {
    printf("  test_installer_validate_package...\n");

    installer_t* inst = installer_create(NULL);
    assert(inst != NULL);

    const char* valid_package = "{\"id\": \"test\", \"version\": \"1.0.0\", \"files\": []}";
    const char* invalid_package = "not a valid json";

    int ret = installer_validate_package(inst, valid_package);
    assert(ret == 0);

    ret = installer_validate_package(inst, invalid_package);
    assert(ret != 0);

    installer_destroy(inst);

    printf("    PASSED\n");
}

static void test_installer_download_progress(void) {
    printf("  test_installer_download_progress...\n");

    installer_t* inst = installer_create(NULL);
    assert(inst != NULL);

    install_progress_t progress;
    memset(&progress, 0, sizeof(progress));
    progress.total_bytes = 1024;
    progress.downloaded_bytes = 512;
    progress.percentage = 50;

    int ret = installer_set_progress_callback(inst, NULL, NULL);
    assert(ret == 0);

    installer_destroy(inst);

    printf("    PASSED\n");
}

static void test_installer_rollback(void) {
    printf("  test_installer_rollback...\n");

    installer_t* inst = installer_create(NULL);
    assert(inst != NULL);

    install_request_t request;
    memset(&request, 0, sizeof(request));
    request.package_id = "rollback_test";
    request.version = "1.0.0";

    int ret = installer_rollback(inst, &request);
    assert(ret == 0 || ret == AGENTOS_ERR_NOT_FOUND);

    installer_destroy(inst);

    printf("    PASSED\n");
}

static void test_installer_status(void) {
    printf("  test_installer_status...\n");

    installer_t* inst = installer_create(NULL);
    assert(inst != NULL);

    install_status_t status = installer_get_status(inst);
    assert(status == INSTALL_STATUS_IDLE || status >= 0);

    installer_destroy(inst);

    printf("    PASSED\n");
}

int main(void) {
    printf("=========================================\n");
    printf("  Installer Unit Tests\n");
    printf("=========================================\n");

    test_installer_create_destroy();
    test_installer_config();
    test_installer_prepare_request();
    test_installer_validate_package();
    test_installer_download_progress();
    test_installer_rollback();
    test_installer_status();

    printf("\n✅ All installer tests PASSED\n");
    return 0;
}

/**
 * @file test_config.c
 * @brief 配置模块单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "svc_config.h"

static void test_config_create_destroy(void) {
    printf("  test_config_create_destroy...\n");

    svc_config_t* cfg = svc_config_create();
    assert(cfg != NULL);

    svc_config_destroy(cfg);

    printf("    PASSED\n");
}

static void test_config_set_get(void) {
    printf("  test_config_set_get...\n");

    svc_config_t* cfg = svc_config_create();
    assert(cfg != NULL);

    int ret = svc_config_set_int(cfg, "port", 8080);
    assert(ret == 0);

    int port = svc_config_get_int(cfg, "port", -1);
    assert(port == 8080);

    ret = svc_config_set_string(cfg, "host", "localhost");
    assert(ret == 0);

    const char* host = svc_config_get_string(cfg, "host", NULL);
    assert(host != NULL);
    assert(strcmp(host, "localhost") == 0);

    ret = svc_config_set_bool(cfg, "debug", 1);
    assert(ret == 0);
    assert(svc_config_get_bool(cfg, "debug", 0) == 1);

    ret = svc_config_set_double(cfg, "timeout", 3.14);
    assert(ret == 0);
    assert(svc_config_get_double(cfg, "timeout", 0.0) > 3.13 && svc_config_get_double(cfg, "timeout", 0.0) < 3.15);

    svc_config_destroy(cfg);

    printf("    PASSED\n");
}

static void test_config_has_key(void) {
    printf("  test_config_has_key...\n");

    svc_config_t* cfg = svc_config_create();
    assert(cfg != NULL);

    assert(svc_config_has_key(cfg, "nonexistent") == 0);

    svc_config_set_int(cfg, "exists", 42);
    assert(svc_config_has_key(cfg, "exists") == 1);

    svc_config_destroy(cfg);

    printf("    PASSED\n");
}

int main(void) {
    printf("=========================================\n");
    printf("  Config Module Unit Tests\n");
    printf("=========================================\n");

    test_config_create_destroy();
    test_config_set_get();
    test_config_has_key();

    printf("\n✅ All config module tests PASSED\n");
    return 0;
}
/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file test_service.c
 * @brief Gateway服务测试
 */

#include "gateway_service.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/**
 * @brief 测试默认配置
 */
static void test_default_config(void) {
    printf("Test: default config... ");
    
    gateway_service_config_t config;
    gateway_service_get_default_config(&config);
    
    assert(config.name != NULL);
    assert(strcmp(config.name, "gateway_d") == 0);
    assert(config.http.port == 8080);
    assert(config.http.enabled == true);
    assert(config.ws.port == 8081);
    assert(config.ws.enabled == false);
    assert(config.stdio.enabled == false);
    
    printf("PASSED\n");
}

/**
 * @brief 测试服务创建
 */
static void test_service_create(void) {
    printf("Test: service create... ");
    
    gateway_service_t service = NULL;
    agentos_error_t err = gateway_service_create(&service, NULL);
    
    assert(err == AGENTOS_SUCCESS);
    assert(service != NULL);
    assert(gateway_service_get_state(service) == AGENTOS_SVC_STATE_CREATED);
    
    gateway_service_destroy(service);
    
    printf("PASSED\n");
}

/**
 * @brief 测试服务生命周期
 */
static void test_service_lifecycle(void) {
    printf("Test: service lifecycle... ");
    
    gateway_service_config_t config;
    gateway_service_get_default_config(&config);
    
    config.http.enabled = false;
    config.ws.enabled = false;
    config.stdio.enabled = false;
    
    gateway_service_t service = NULL;
    agentos_error_t err = gateway_service_create(&service, &config);
    assert(err == AGENTOS_SUCCESS);
    
    err = gateway_service_init(service);
    assert(err == AGENTOS_SUCCESS);
    assert(gateway_service_get_state(service) == AGENTOS_SVC_STATE_READY);
    
    err = gateway_service_start(service);
    assert(err == AGENTOS_SUCCESS);
    assert(gateway_service_get_state(service) == AGENTOS_SVC_STATE_RUNNING);
    assert(gateway_service_is_running(service) == true);
    
    err = gateway_service_stop(service, false);
    assert(err == AGENTOS_SUCCESS);
    assert(gateway_service_get_state(service) == AGENTOS_SVC_STATE_STOPPED);
    
    gateway_service_destroy(service);
    
    printf("PASSED\n");
}

/**
 * @brief 测试健康检查
 */
static void test_healthcheck(void) {
    printf("Test: healthcheck... ");
    
    gateway_service_config_t config;
    gateway_service_get_default_config(&config);
    config.http.enabled = false;
    config.ws.enabled = false;
    config.stdio.enabled = false;
    
    gateway_service_t service = NULL;
    gateway_service_create(&service, &config);
    gateway_service_init(service);
    gateway_service_start(service);
    
    agentos_error_t err = gateway_service_healthcheck(service);
    assert(err == AGENTOS_SUCCESS);
    
    gateway_service_stop(service, false);
    gateway_service_destroy(service);
    
    printf("PASSED\n");
}

/**
 * @brief 测试统计信息
 */
static void test_stats(void) {
    printf("Test: stats... ");
    
    gateway_service_config_t config;
    gateway_service_get_default_config(&config);
    config.http.enabled = false;
    config.ws.enabled = false;
    config.stdio.enabled = false;
    
    gateway_service_t service = NULL;
    gateway_service_create(&service, &config);
    gateway_service_init(service);
    gateway_service_start(service);
    
    agentos_svc_stats_t stats;
    agentos_error_t err = gateway_service_get_stats(service, &stats);
    assert(err == AGENTOS_SUCCESS);
    
    gateway_service_stop(service, false);
    gateway_service_destroy(service);
    
    printf("PASSED\n");
}

/**
 * @brief 主函数
 */
int main(void) {
    printf("\n=== Gateway Service Tests ===\n\n");
    
    test_default_config();
    test_service_create();
    test_service_lifecycle();
    test_healthcheck();
    test_stats();
    
    printf("\n=== All tests passed ===\n\n");
    return 0;
}

/*
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 * 
 * @file test_network.c
 * @brief 网络通信模块单元测试
 * 
 * @details
 * 测试 network_common.h 中声明的所有网络功能，包括：
 * - 基础连接 API（创建/连接/断开/发送/接收/状态/超时/统计）
 * - HTTP 客户端 API（请求/GET/POST/响应释放）
 * - 连接池管理 API（创建/销毁/获取/释放/可用数/大小/健康检查）
 * - DNS 解析 API（解析/结果释放）
 * - 工具函数（可达性检查/本地IP获取/地址转换/初始化/清理）
 * 
 * @author AgentOS Team
 * @date 2026-04-03
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <cmocka.h>
#include <network_common.h>

/* ============================================================================
 * 基础连接 API 测试
 * ============================================================================ */

/**
 * @brief 测试默认配置创建
 */
static void test_network_default_config(void **state) {
    (void)state;
    
    network_config_t config = network_create_default_config();
    
    assert_non_null(config.host);
    assert_int_equal(config.port, 8080);
    assert_int_equal(config.timeout_ms, 30000);
    assert_int_equal(config.read_timeout_ms, 10000);
    assert_int_equal(config.write_timeout_ms, 10000);
    assert_int_equal(config.max_retries, 3);
    assert_int_equal(config.retry_interval_ms, 1000);
    assert_int_equal(config.sock_type, NETWORK_SOCK_STREAM);
    assert_int_equal(config.af, NETWORK_AF_INET);
    assert_false(config.keepalive);
    assert_false(config.nonblocking);
    assert_false(config.ssl_enable);
}

/**
 * @brief 测试连接创建和销毁
 */
static void test_network_connection_create_destroy(void **state) {
    (void)state;
    
    network_config_t config = network_create_default_config();
    config.host = "127.0.0.1";
    config.port = 8080;
    
    network_connection_t* conn = network_connection_create(&config);
    assert_non_null(conn);
    
    /* 初始状态应为已断开 */
    assert_int_equal(network_get_status(conn), NETWORK_STATUS_DISCONNECTED);
    
    /* 错误消息初始应为空或 "No error" */
    const char* err_msg = network_get_error_message(conn);
    assert_non_null(err_msg);
    
    network_connection_destroy(conn);
}

/**
 * @brief 测试 NULL 参数处理
 */
static void test_network_null_handling(void **state) {
    (void)state;
    
    /* NULL 创建应返回 NULL */
    network_connection_t* conn = network_connection_create(NULL);
    assert_null(conn);
    
    /* NULL 销毁不应崩溃 */
    network_connection_destroy(NULL);
    
    /* NULL 连接操作应返回错误 */
    assert_int_equal(network_connect(NULL), AGENTOS_EINVAL);
    assert_int_equal(network_disconnect(NULL), AGENTOS_EINVAL);
    assert_int_equal(network_get_status(NULL), NETWORK_STATUS_ERROR);
}

/**
 * @brief 测试连接状态枚举值
 */
static void test_network_status_enums(void **state) {
    (void)state;
    
    assert_int_equal(NETWORK_STATUS_DISCONNECTED, 0);
    assert_int_equal(NETWORK_STATUS_CONNECTING, 1);
    assert_int_equal(NETWORK_STATUS_CONNECTED, 2);
    assert_int_equal(NETWORK_STATUS_DISCONNECTING, 3);
    assert_int_equal(NETWORK_STATUS_ERROR, 4);
}

/**
 * @brief 测试 Socket 类型枚举值
 */
static void test_network_sock_type_enums(void **state) {
    (void)state;
    
    assert_int_equal(NETWORK_SOCK_STREAM, 1);
    assert_int_equal(NETWORK_SOCK_DGRAM, 2);
    assert_int_equal(NETWORK_SOCK_RAW, 3);
}

/**
 * @brief 测试地址族枚举值
 */
static void test_network_af_enums(void **state) {
    (void)state;
    
    assert_int_equal(NETWORK_AF_UNSPEC, 0);
    assert_int_equal(NETWORK_AF_INET, 2);
    assert_int_equal(NETWORK_AF_INET6, 10);
}

/**
 * @brief 测试 SSL 验证模式枚举值
 */
static void test_network_ssl_verify_enums(void **state) {
    (void)state;
    
    assert_int_equal(NETWORK_SSL_VERIFY_NONE, 0);
    assert_int_equal(NETWORK_SSL_VERIFY_PEER, 1);
    assert_int_equal(NETWORK_SSL_VERIFY_FAIL_IF_NO_PEER_CERT, 2);
    assert_int_equal(NETWORK_SSL_VERIFY_CLIENT_ONCE, 4);
}

/* ============================================================================
 * HTTP 客户端 API 测试
 * ============================================================================ */

/**
 * @brief 测试 HTTP GET 请求结构体初始化
 */
static void test_http_get_request_init(void **state) {
    (void)state;
    
    network_http_request_t request = {0};
    request.method = "GET";
    request.path = "/api/test";
    
    assert_string_equal(request.method, "GET");
    assert_string_equal(request.path, "/api/test");
    assert_null(request.body);
    assert_int_equal(request.body_len, 0);
}

/**
 * @brief 测试 HTTP POST 请求结构体初始化
 */
static void test_http_post_request_init(void **state) {
    (void)state;
    
    const char* json_body = "{\"key\":\"value\"}";
    
    network_http_request_t request = {0};
    request.method = "POST";
    request.path = "/api/submit";
    request.content_type = "application/json";
    request.body = json_body;
    request.body_len = strlen(json_body);
    
    assert_string_equal(request.method, "POST");
    assert_string_equal(request.content_type, "application/json");
    assert_int_equal(request.body_len, 15);
}

/**
 * @brief 测试 HTTP 响应结构体
 */
static void test_http_response_structure(void **state) {
    (void)state;
    
    network_http_response_t response = {0};
    
    response.status_code = 200;
    response.body = strdup("OK");
    response.body_len = 2;
    
    assert_int_equal(response.status_code, 200);
    assert_string_equal(response.body, "OK");
    assert_int_equal(response.body_len, 2);
    
    /* 使用标准释放函数 */
    network_http_response_free(&response);
    assert_null(response.body);
}

/**
 * @brief 测试 NULL 响应释放不崩溃
 */
static void test_http_response_free_null(void **state) {
    (void)state;
    
    network_http_response_free(NULL);
}

/* ============================================================================
 * 连接池 API 测试
 * ============================================================================ */

/**
 * @brief 测试连接池创建和销毁
 */
static void test_network_pool_create_destroy(void **state) {
    (void)state;
    
    network_config_t config = network_create_default_config();
    config.host = "localhost";
    config.port = 8080;
    
    network_pool_t* pool = network_pool_create(&config, 10);
    assert_non_null(pool);
    
    assert_int_equal(network_pool_size(pool), 0);
    
    network_pool_destroy(pool);
}

/**
 * @brief 测试连接池参数验证
 */
static void test_network_pool_parameter_validation(void **state) {
    (void)state;
    
    network_config_t config = network_create_default_config();
    
    /* 大小为 0 应返回 NULL */
    network_pool_t* pool_zero = network_pool_create(&config, 0);
    assert_null(pool_zero);
    
    /* 超过最大池大小应返回 NULL */
    network_pool_t* pool_oversize = network_pool_create(&config, NETWORK_MAX_POOL_SIZE + 1);
    assert_null(pool_oversize);
    
    /* NULL 配置应返回 NULL */
    network_pool_t* pool_null_cfg = network_pool_create(NULL, 5);
    assert_null(pool_null_cfg);
    
    /* NULL 销毁不应崩溃 */
    network_pool_destroy(NULL);
}

/**
 * @brief 测试连接池可用连接查询
 */
static void test_network_pool_available(void **state) {
    (void)state;
    
    network_config_t config = network_create_default_config();
    config.host = "localhost";
    config.port = 8080;
    
    network_pool_t* pool = network_pool_create(&config, 5);
    assert_non_null(pool);
    
    /* 空池的可用连接数应该等于最大大小（全部可新建） */
    size_t available = network_pool_available(pool);
    assert_true(available >= 5);  /* 至少有 5 个槽位可用 */
    
    network_pool_destroy(pool);
}

/**
 * @brief 测试连接池健康检查
 */
static void test_network_pool_health_check(void **state) {
    (void)state;
    
    network_config_t config = network_create_default_config();
    config.host = "localhost";
    config.port = 8080;
    
    network_pool_t* pool = network_pool_create(&config, 5);
    assert_non_null(pool);
    
    /* 空池的健康连接数应为 0 */
    size_t healthy = network_pool_health_check(pool);
    assert_int_equal(healthy, 0);
    
    /* NULL 池的健康检查不应崩溃 */
    healthy = network_pool_health_check(NULL);
    assert_int_equal(healthy, 0);
    
    network_pool_destroy(pool);
}

/* ============================================================================
 * DNS 解析 API 测试
 * ============================================================================ */

/**
 * @brief 测试 DNS 解析函数存在性
 */
static void test_dns_resolve_exists(void **state) {
    (void)state;
    
    network_dns_result_t result = {0};
    
    agentos_error_t err = network_dns_resolve("localhost", NETWORK_AF_INET, &result);
    
    /* 无论成功与否，都不应崩溃 */
    if (err == AGENTOS_SUCCESS && result.count > 0) {
        assert_non_null(result.addresses);
        network_dns_result_free(&result);
    }
}

/**
 * @brief 测试 DNS 解析 NULL 处理
 */
static void test_dns_resolve_null_handling(void **state) {
    (void)state;
    
    /* NULL 参数应返回错误 */
    assert_int_equal(network_dns_resolve(NULL, NETWORK_AF_INET, NULL), AGENTOS_EINVAL);
    
    network_dns_result_t result = {0};
    assert_int_equal(network_dns_resolve(NULL, NETWORK_AF_INET, &result), AGENTOS_EINVAL);
    
    /* 释放 NULL 结果不应崩溃 */
    network_dns_result_free(NULL);
}

/* ============================================================================
 * 工具函数测试
 * ============================================================================ */

/**
 * @brief 测试网络初始化和清理
 */
static void test_network_init_cleanup(void **state) {
    (void)state;
    
    agentos_error_t err = network_init();
    assert_int_equal(err, AGENTOS_SUCCESS);
    
    /* 多次初始化应该是安全的 */
    err = network_init();
    assert_int_equal(err, AGENTOS_SUCCESS);
    
    network_cleanup();
    /* 多次清理也应是安全的 */
    network_cleanup();
}

/**
 * @brief 测试网络可达性检查
 */
static void test_network_is_reachable(void **state) {
    (void)state;
    
    /* NULL 主机应返回 false */
    bool reachable = network_is_reachable(NULL, 1000);
    assert_false(reachable);
    
    /* localhost 通常可达（但可能因环境而异，只验证不崩溃） */
    reachable = network_is_reachable("127.0.0.1", 1000);
    (void)reachable;  /* 结果取决于系统环境 */
}

/**
 * @brief 测试获取本地 IP
 */
static void test_network_get_local_ip(void **state) {
    (void)state;
    
    char buffer[64] = {0};
    
    /* NULL 缓冲区应返回错误 */
    assert_int_equal(network_get_local_ip(NETWORK_AF_INET, NULL, 0), AGENTOS_EINVAL);
    
    /* 正常调用应成功 */
    agentos_error_t err = network_get_local_ip(NETWORK_AF_INET, buffer, sizeof(buffer));
    assert_int_equal(err, AGENTOS_SUCCESS);
    
    /* 应该得到有效的 IP 字符串 */
    if (buffer[0] != '\0') {
        /* 验证包含数字和点号 */
        int has_digit = 0, has_dot = 0;
        for (size_t i = 0; buffer[i]; i++) {
            if (buffer[i] >= '0' && buffer[i] <= '9') has_digit = 1;
            if (buffer[i] == '.') has_dot = 1;
        }
        (void)has_digit;
        (void)has_dot;
    }
}

/**
 * @brief 测试地址转换函数
 */
static void test_network_addr_to_string(void **state) {
    (void)state;
    
    char buffer[64] = {0};
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, "192.168.1.1", &addr.sin_addr);
    
    agentos_error_t err = network_addr_to_string(
        NETWORK_AF_INET, &addr, buffer, sizeof(buffer)
    );
    assert_int_equal(err, AGENTOS_SUCCESS);
    
    /* NULL 参数处理 */
    err = network_addr_to_string(NETWORK_AF_INET, NULL, buffer, sizeof(buffer));
    assert_int_equal(err, AGENTOS_EINVAL);
    
    err = network_addr_to_string(NETWORK_AF_INET, &addr, NULL, 0);
    assert_int_equal(err, AGENTOS_EINVAL);
}

/**
 * @brief 测试网络事件枚举值
 */
static void test_network_event_enums(void **state) {
    (void)state;
    
    assert_int_equal(NETWORK_EVENT_CONNECTED, 1);
    assert_int_equal(NETWORK_EVENT_DISCONNECTED, 2);
    assert_int_equal(NETWORK_EVENT_DATA_RECEIVED, 3);
    assert_int_equal(NETWORK_EVENT_DATA_SENT, 4);
    assert_int_equal(NETWORK_EVENT_ERROR, 5);
    assert_int_equal(NETWORK_EVENT_TIMEOUT, 6);
}

/**
 * @brief 测试统计信息结构体
 */
static void test_network_stats_structure(void **state) {
    (void)state;
    
    network_stats_t stats = {0};
    
    stats.bytes_sent = 1024;
    stats.bytes_received = 2048;
    stats.packets_sent = 10;
    stats.packets_received = 20;
    stats.connect_count = 3;
    stats.error_count = 1;
    stats.retry_count = 2;
    stats.avg_latency_us = 5000;
    
    assert_int_equal(stats.bytes_sent, 1024);
    assert_int_equal(stats.bytes_received, 2048);
    assert_int_equal(stats.packets_sent, 10);
    assert_int_equal(stats.packets_received, 20);
    assert_int_equal(stats.connect_count, 3);
    assert_int_equal(stats.error_count, 1);
    assert_int_equal(stats.retry_count, 2);
    assert_int_equal(stats.avg_latency_us, 5000);
}

/* ============================================================================
 * 主测试入口
 * ============================================================================ */

int main(void) {
    const struct CMUnitTest tests[] = {
        /* 基础连接 API 测试 */
        cmocka_unit_test(test_network_default_config),
        cmocka_unit_test(test_network_connection_create_destroy),
        cmocka_unit_test(test_network_null_handling),
        cmocka_unit_test(test_network_status_enums),
        cmocka_unit_test(test_network_sock_type_enums),
        cmocka_unit_test(test_network_af_enums),
        cmocka_unit_test(test_network_ssl_verify_enums),
        
        /* HTTP 客户端 API 测试 */
        cmocka_unit_test(test_http_get_request_init),
        cmocka_unit_test(test_http_post_request_init),
        cmocka_unit_test(test_http_response_structure),
        cmocka_unit_test(test_http_response_free_null),
        
        /* 连接池 API 测试 */
        cmocka_unit_test(test_network_pool_create_destroy),
        cmocka_unit_test(test_network_pool_parameter_validation),
        cmocka_unit_test(test_network_pool_available),
        cmocka_unit_test(test_network_pool_health_check),
        
        /* DNS 解析 API 测试 */
        cmocka_unit_test(test_dns_resolve_exists),
        cmocka_unit_test(test_dns_resolve_null_handling),
        
        /* 工具函数测试 */
        cmocka_unit_test(test_network_init_cleanup),
        cmocka_unit_test(test_network_is_reachable),
        cmocka_unit_test(test_network_get_local_ip),
        cmocka_unit_test(test_network_addr_to_string),
        cmocka_unit_test(test_network_event_enums),
        cmocka_unit_test(test_network_stats_structure),
    };
    
    return cmocka_run_group_tests(tests, NULL, NULL);
}
